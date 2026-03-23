const { SerialPort } = require('serialport');
const { Server } = require('socket.io');
const fs = require('fs');
const path = require('path');

// --- 1. SETTINGS ---
const TARGET_PORT = 'COM3'; // Change this to your actual Receiver port
const MOCK_MODE = process.env.MOCK === 'true'; // Manual flag only
const io = new Server(3000, { cors: { origin: "*" } });

// --- PROTOCOL CONFIGURATION ---
// If you ever add or remove a sensor in datawrap.h, change this ONE number!
const PAYLOAD_SIZE = 49;

// Auto-calculated boundaries (Do not change)
const HEADER_SIZE = 2; // Sync bytes (0xAA, 0xAF)
const RSSI_SIZE = 2;   // Appended by reciever.ino
const FOOTER_SIZE = 1; // 0xBB appended by reciever.ino
const TOTAL_PACKET_SIZE = HEADER_SIZE + PAYLOAD_SIZE + RSSI_SIZE + FOOTER_SIZE;
const RSSI_OFFSET = HEADER_SIZE + PAYLOAD_SIZE; // Where the RSSI bytes start

const flightHistory = [];

io.on('connection', (socket) => {
    socket.emit('flight-history', flightHistory);
});
console.log("-----------------------------------------");
console.log("📡 CANSAT MISSION CONTROL BRIDGE");
console.log("-----------------------------------------");

// --- 2. LOGGING SETUP ---
const logDir = path.join(__dirname, 'logs');
if (!fs.existsSync(logDir)) fs.mkdirSync(logDir);
const logFileName = `asteroid_log_${new Date().toISOString().replace(/[:.]/g, '-')}.csv`;
const logPath = path.join(logDir, logFileName);

// PERFORMANCE UPGRADE: Create a single open memory pipe directly to the Hard Drive
const logStream = fs.createWriteStream(logPath, { flags: 'a' });
logStream.write("Timestamp,ID,Alt,Press,Temp,Hum,Lux,AccX,AccY,AccZ,Lat,Lng,Volt,Curr,Power,Speed,RSSI\n");

// --- 3. HARDWARE OR MOCK MODE SELECTION ---
if (MOCK_MODE) {
    console.log("🛠️  MODE: MANUAL SIMULATION (MOCK)");
    startMockLoop();
} else {
    console.log(`🔌 MODE: HARDWARE (Searching for ${TARGET_PORT}...)`);

    const port = new SerialPort({ path: TARGET_PORT, baudRate: 921600 }, (err) => {
        if (err) {
            console.error(`\n❌ ERROR: Port ${TARGET_PORT} not available.`);
            console.error(`📝 REASON: ${err.message}`);
            console.log("\nUSAGE:");
            console.log("  For Hardware: Plug in receiver and check COM port.");
            console.log("  For Mocking:  $env:MOCK='true'; node index.js (Windows)");
            console.log("                MOCK=true node index.js (Mac/Linux)\n");
            process.exit(1);
        }

        console.log(`✅ SUCCESS: Receiver connected on ${TARGET_PORT}`);
        startSerialListener(port);
    });
}

// --- 4. SERIAL LISTENER (THE HARDWARE ENGINE) ---
function calculateCRC(buffer, length) {
    let crc = 0xFFFF;
    for (let i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (let j = 0; j < 8; j++) {
            if ((crc & 1) !== 0) crc = (crc >>> 1) ^ 0xA001;
            else crc >>>= 1;
        }
    }
    return crc;
}

// --- TELEMETRY HELPER FUNCTION ---
function parseTelemetry(p, rssi) {
    if (p.length < PAYLOAD_SIZE) {
        throw new Error(`Buffer too small! Expected ${PAYLOAD_SIZE} bytes but got ${p.length}`);
    }

    const telemetry = {
        // High Precision
        time: p.readUInt32LE(0),
        acc: { x: p.readFloatLE(4).toFixed(2), y: p.readFloatLE(8).toFixed(2), z: p.readFloatLE(12).toFixed(2) },
        gps: { lat: p.readFloatLE(16).toFixed(6), lng: p.readFloatLE(20).toFixed(6) },
        alt: p.readFloatLE(24).toFixed(2),
        press: p.readFloatLE(28).toFixed(2),
        speed: p.readFloatLE(32).toFixed(1),

        // Compressed Integers
        temp: (p.readInt16LE(36) / 10.0).toFixed(1),
        hum: (p.readUInt16LE(38) / 10.0).toFixed(1),
        lux: p.readUInt16LE(40).toFixed(0),
        curr: p.readUInt16LE(42).toFixed(1),
        volt: (p.readUInt8(44) / 10.0).toFixed(1),

        // Metadata
        id: p.readUInt16LE(45),
        rssi: rssi,
        isMock: false
    };

    telemetry.power = (telemetry.volt * (telemetry.curr / 1000)).toFixed(3);
    return telemetry;
}

function startSerialListener(port) {
    let buffer = Buffer.alloc(0);

    port.on('data', (chunk) => {
        buffer = Buffer.concat([buffer, chunk]);

        while (buffer.length >= TOTAL_PACKET_SIZE) {
            const syncIdx = buffer.indexOf(Buffer.from([0xAA, 0xAF]));

            if (syncIdx === -1) { buffer = Buffer.alloc(0); break; }
            if (syncIdx > 0) { buffer = buffer.slice(syncIdx); continue; }

            // Slice out the exact CanSat payload data
            const p = buffer.slice(HEADER_SIZE, HEADER_SIZE + PAYLOAD_SIZE);

            // Validate Checksum (The last 2 bytes of the payload)
            const computedCrc = calculateCRC(p, PAYLOAD_SIZE - 2);
            const receivedCrc = p.readUInt16LE(PAYLOAD_SIZE - 2);

            if (computedCrc !== receivedCrc) {
                console.warn(`[!] CRC mismatch. Expected ${computedCrc.toString(16)}, got ${receivedCrc.toString(16)}. Corrupted packet dropped.`);
                buffer = buffer.slice(TOTAL_PACKET_SIZE); // Skip broken packet
                continue;
            }

            // VALIDATION: Check if the Payload Size is out of sync with C++
            const footerByte = buffer.readUInt8(TOTAL_PACKET_SIZE - 1);
            if (footerByte !== 0xBB) {
                console.error(`[FATAL] Footer alignment failed! Expected 0xBB, got 0x${footerByte.toString(16)}.`);
                console.error(`        --> Is your Javascript PAYLOAD_SIZE (${PAYLOAD_SIZE}) perfectly matched with C++ datawrap.h?`);
                buffer = buffer.slice(TOTAL_PACKET_SIZE);
                continue;
            }

            try {
                // Extract RSSI and Feed exactly 49 bytes into the new cleaner helper function
                const rssi = buffer.readInt16LE(RSSI_OFFSET);
                const telemetry = parseTelemetry(p, rssi);

                flightHistory.push(telemetry);
                if (flightHistory.length > 30) flightHistory.shift();

                io.emit('flight-data', telemetry);
                saveToCSV(telemetry);
            } catch (err) {
                console.error(`[ERROR] Crash during packet parsing: Failed extracting ${err.message}`);
                console.error(`        --> Did you add a new float to datawrap.h but forgot to increase PAYLOAD_SIZE?`);
            }

            // Fast-forward buffer safely after attempting to parse
            buffer = buffer.slice(TOTAL_PACKET_SIZE);
        }
    });
}

// --- 5. MOCK LOOP THE SIMULATOR ---
function startMockLoop() {
    setInterval(() => {
        const mock = {
            time: Date.now(),
            alt: (50 + Math.random() * 5).toFixed(2),
            speed: (2.5).toFixed(1),
            temp: 28.5,
            volt: 7.40,
            curr: 120,
            power: (7.4 * 0.12).toFixed(3),
            rssi: -60,
            id: 999,
            isMock: true,
            gps: { lat: 13.75, lng: 100.5 },
            acc: { x: 0, y: 0, z: 9.8 }
        };
        flightHistory.push(mock);
        if (flightHistory.length > 30) flightHistory.shift();
        io.emit('flight-data', mock);
    }, 500);
}

function saveToCSV(t) {
    const row = `${t.time},${t.id},${t.alt},${t.press},${t.temp},${t.hum},${t.lux},${t.acc.x},${t.acc.y},${t.acc.z},${t.gps.lat},${t.gps.lng},${t.volt},${t.curr},${t.power},${t.speed},${t.rssi}\n`;

    // Instantly streams into the memory pipe with literally zero disk-thrashing overhead!
    logStream.write(row);
}