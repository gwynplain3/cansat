#ifndef DATA_WRAP_H
#define DATA_WRAP_H

#include <Arduino.h>

struct __attribute__((__packed__)) FlightData {
    uint32_t timestamp;      // [0] 
    float accX, accY, accZ;  // [4, 8, 12]
    float lat, lng;          // [16, 20]
    float voltage, current;  // [24, 28]
    float altitude;          // [32]
    float pressure;          // [36]
    float speed;             // [40] 
    float temp, hum, lux;    // [44, 48, 52]
    uint16_t packetID;       // [56]
    // ประมาณ 50-60 byte ต่อ 1 ชุดที่ส้งมา
};

#endif