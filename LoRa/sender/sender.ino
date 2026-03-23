#include <SPI.h>
#include <LoRa.h>
#include "../common/config.h"
#include "../common/datawrap.h"
#include "sensor.h"

// --- Sender Settings ---
#define SEND_INTERVAL 500 // send ทุก 0.5 วิ

// Pins ---
#define TX_LORA_SCK  5
#define TX_LORA_MISO 19
#define TX_LORA_MOSI 27
#define TX_LORA_CS   18
#define TX_LORA_RST  14
#define TX_LORA_IRQ  26

FlightData current;
uint16_t count = 0;
unsigned long lastTx = 0;

uint16_t calculateCRC(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

void setup() {
    Serial.begin(115200);
    
    // Initialize all sensors from sensor.h
    initSensors();

    // Initialize LoRa
    SPI.begin(TX_LORA_SCK, TX_LORA_MISO, TX_LORA_MOSI, TX_LORA_CS);
    LoRa.setPins(TX_LORA_CS, TX_LORA_RST, TX_LORA_IRQ);
    
    if (!LoRa.begin(RF_FREQUENCY)) {
        Serial.println("LoRa Failed!");
        while (1);
    }
    
    // TTGO Specific LoRa Settings
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(7);
    LoRa.setTxPower(20);
    
    Serial.println("=== CANSAT SENDER READY ===");
}

void loop() {
    // Continuously read high-speed sensors (GPS string parsing, Energy accumulation)
    updateSensors();

    // Transmit every SEND_INTERVAL
    if (millis() - lastTx > SEND_INTERVAL) {
        lastTx = millis();
        
        wrapData(current, count++);
        current.checksum = calculateCRC((uint8_t*)&current, sizeof(current) - sizeof(uint16_t));
        
        // ASYNCHRONOUS LORA TRANSMIT (The 'true' parameter is the secret!)
        // This tells the radio chip to transmit in the background. 
        // The ESP32 processor instantly skips to the next line and returns to reading the GPS!
        LoRa.beginPacket();
        LoRa.write((uint8_t*)&current, sizeof(current));
        LoRa.endPacket(true);

        Serial.print("Async TX Packet #");
        Serial.println(count);
    }
}