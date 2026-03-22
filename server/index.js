const { SerialPort } = require('serialport');
const { Server } = require('socket.io');
const fs = require('fs');
const path = require('path');

// --- 1. SETTINGS ---
const TARGET_PORT = 'COM3'; // Change this to your actual Receiver port
const MOCK_MODE = process.env.MOCK === 'true'; // Manual flag only
const io = new Server(3000, { cors: { origin: "*" } });

console.log("-----------------------------------------");
console.log("📡 CANSAT MISSION CONTROL BRIDGE");
console.log("-----------------------------------------");

// --- 2. LOGGING SETUP ---
const logDir = path.join(__dirname, 'logs');
if (!fs.existsSync(logDir)) fs.mkdirSync(logDir);
const logFileName = `asteroid_log_${new Date().toISOString().replace(/[:.]/g, '-')}.csv`;
const logPath = path.join(logDir, logFileName);
fs.writeFileSync(logPath, "Timestamp,ID,Alt,Press,Temp,Hum,Lux,AccX,AccY,AccZ,Lat,Lng,Volt,Curr,Power,Speed,RSSI\n");

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
function startSerialListener(port) {
    let buffer = Buffer.alloc(0);

    port.on('data', (chunk) => {
        buffer = Buffer.concat([buffer, chunk]);

        // Total Packet: Header(2) + Data(58) + RSSI(2) + Footer(1) = 63 bytes
        while (buffer.length >= 63) {
            const syncIdx = buffer.indexOf(Buffer.from([0xAA, 0xAF]));

            if (syncIdx === -1) { buffer = Buffer.alloc(0); break; }
            if (syncIdx > 0) { buffer = buffer.slice(syncIdx); continue; }

            const p = buffer.slice(2, 60); // The 58 bytes of CanSat data

            const voltage = p.readFloatLE(24);
            const current = p.readFloatLE(28);

            const telemetry = {
                time: p.readUInt32LE(0),
                acc: { x: p.readFloatLE(4).toFixed(2), y: p.readFloatLE(8).toFixed(2), z: p.readFloatLE(12).toFixed(2) },
                gps: { lat: p.readFloatLE(16).toFixed(6), lng: p.readFloatLE(20).toFixed(6) },
                volt: voltage.toFixed(2),
                curr: current.toFixed(1),
                power: (voltage * (current / 1000)).toFixed(3), // Mission 3
                alt: p.readFloatLE(32).toFixed(2),
                press: p.readFloatLE(36).toFixed(2),
                speed: p.readFloatLE(40).toFixed(1),
                temp: p.readFloatLE(44).toFixed(1),
                hum: p.readFloatLE(48).toFixed(1),
                lux: p.readFloatLE(52).toFixed(0),
                id: p.readUInt16LE(56),
                rssi: buffer.readInt16LE(60), // RSSI is at the end
                isMock: false
            };

            io.emit('flight-data', telemetry);
            saveToCSV(telemetry);
            buffer = buffer.slice(63);
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
        io.emit('flight-data', mock);
    }, 500);
}

function saveToCSV(t) {
    const row = `${t.time},${t.id},${t.alt},${t.press},${t.temp},${t.hum},${t.lux},${t.acc.x},${t.acc.y},${t.acc.z},${t.gps.lat},${t.gps.lng},${t.volt},${t.curr},${t.power},${t.speed},${t.rssi}\n`;
    fs.appendFile(logPath, row, () => { });
}