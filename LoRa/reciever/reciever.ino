#include <SPI.h>
#include <LoRa.h>
#include "../common/config.h"
#include "../common/datawrap.h"

void setup() {
    Serial.begin(921600); //เร้วกว่า 112500
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
    if (!LoRa.begin(RF_FREQUENCY)) while (1);
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.receive();
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        Serial.write(0xAA); // Sync 1
        Serial.write(0xAF); // Sync 2
        while (LoRa.available()) Serial.write(LoRa.read());
        int16_t rssi = LoRa.packetRssi();
        Serial.write((uint8_t*)&rssi, 2);
        Serial.write(0xBB); // Footer
    }
}