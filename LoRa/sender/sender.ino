#include <SPI.h>
#include <LoRa.h>
#include "../common/config.h"
#include "../common/datawrap.h"
#include "sensor.h"

FlightData current;
uint16_t count = 0;
unsigned long lastTx = 0;

void setup() {
    Serial.begin(115200);
    initSensors();
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
    if (!LoRa.begin(RF_FREQUENCY)) while (1);
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
}

void loop() {
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    if (millis() - lastTx > SEND_INTERVAL) {
        lastTx = millis();
        wrapData(current, count++);
        LoRa.beginPacket();
        LoRa.write((uint8_t*)&current, sizeof(current));
        LoRa.endPacket();
    }
}