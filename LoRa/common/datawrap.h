#ifndef DATA_WRAP_H
#define DATA_WRAP_H

#include <Arduino.h>

struct __attribute__((__packed__)) FlightData {
    // Standard Precision Variables (4 bytes each)
    uint32_t timestamp;      // [0] 
    float accX, accY, accZ;  // [4, 8, 12] 
    float lat, lng;          // [16, 20]    
    float altitude;          // [24]        
    float pressure;          // [28]
    float speed;             // [32]        

    // --- Memory Optimized ---
    int16_t temp;            // [36] 
    uint16_t hum;            // [38] 
    uint16_t lux;            // [40] 
    uint16_t current;        // [42] 
    uint8_t voltage;         // [44] 

    // Metadata Headers
    uint16_t packetID;       // [45]
    uint16_t checksum;       // [47] (CRC16)
}; // Total Size: 49 bytes
#endif