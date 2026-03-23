#ifndef SENSOR_H
#define SENSOR_H

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_INA219.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include "../common/config.h"
#include "../common/datawrap.h"

// ==========================================
// 1. SENSOR DEFINITIONS & PINS
// ==========================================

// --- GPS (NEO-6M) ---
#define GPS_RX_PIN 12  // ESP32 RX (to GPS TX)
#define GPS_TX_PIN 13  // ESP32 TX (to GPS RX)
#define GPS_BAUD   9600
TinyGPSPlus gps;
HardwareSerial SerialGPS(2);

// --- IMU (MPU6050) ---
#define MPU_SDA 4
#define MPU_SCL 15
Adafruit_MPU6050 mpu;

// --- POWER (INA219) ---
#define INA_SDA 21
#define INA_SCL 22
Adafruit_INA219 ina219;
float energyMWh = 0.0;
unsigned long lastEnergyTime = 0;


// ==========================================
// 2. SENSOR FUNCTIONS
// ==========================================

void initSensors() {
    // 1. Start Serial for GPS
    SerialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // 2. Start I2C Bus 1 (MPU6050)
    Wire.begin(MPU_SDA, MPU_SCL);
    Wire.setClock(400000); // HARDWARE UPGRADE: Fast I2C (400kHz)
    if (!mpu.begin()) {
        Serial.println("MPU6050 Failed!");
    } else {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    // 3. Start I2C Bus 2 (INA219)
    // ESP32 has built-in TwoWire objects `Wire` and `Wire1`
    Wire1.begin(INA_SDA, INA_SCL);
    if (!ina219.begin(&Wire1)) {
        Serial.println("INA219 Failed!");
    } else {
        ina219.setCalibration_32V_1A();
    }
    
    lastEnergyTime = millis();
}

// Call this continuously in loop() to feed data
void updateSensors() {
    // Feed GPS
    while (SerialGPS.available() > 0) {
        gps.encode(SerialGPS.read());
    }
    
    // Accumulate INA219 energy
    unsigned long now = millis();
    float power_mW = ina219.getBusVoltage_V() * max(0.0f, ina219.getCurrent_mA());
    float dtHours = (now - lastEnergyTime) / 3600000.0;
    energyMWh += power_mW * dtHours;
    lastEnergyTime = now;
}

// Package all sensor data into the payload struct
void wrapData(FlightData &packet, uint16_t id) {
    // Zero out old data or unused sensors to prevent junk bytes
    memset(&packet, 0, sizeof(FlightData));
    
    packet.packetID = id;
    packet.timestamp = millis();

    // -- IMU (MPU6050) --
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    packet.accX = a.acceleration.x;
    packet.accY = a.acceleration.y;
    packet.accZ = a.acceleration.z;
    packet.temp = (int16_t)(temp.temperature * 10); // PACKED MATH: 28.5C becomes 285

    // -- GPS (NEO-6M) --
    if (gps.location.isValid()) {
        packet.lat = gps.location.lat();
        packet.lng = gps.location.lng();
    }
    if (gps.speed.isValid()) {
        packet.speed = gps.speed.kmph();
    }

    // -- Power (INA219) --
    // PACKED MATH: 7.4V becomes 74
    packet.voltage = (uint8_t)(ina219.getBusVoltage_V() * 10);
    packet.current = (uint16_t)max(0.0f, ina219.getCurrent_mA());
}

#endif