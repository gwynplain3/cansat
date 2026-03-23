#include <SPI.h>
#include <LoRa.h>
#include "../common/config.h"
#include "../common/datawrap.h"

// Pin
#define RX_SCK  5
#define RX_MISO 19
#define RX_MOSI 27
#define RX_SS   18
#define RX_RST  14
#define RX_DIO0 26

void setup() {
    Serial.begin(921600); 

    SPI.begin(RX_SCK, RX_MISO, RX_MOSI, RX_SS);
    LoRa.setPins(RX_SS, RX_RST, RX_DIO0);
    
    if (!LoRa.begin(RF_FREQUENCY)) while (1);
    
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(7);
    
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