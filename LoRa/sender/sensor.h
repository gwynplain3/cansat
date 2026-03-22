#ifndef SENSOR_H
#define SENSOR_H

#include <Wire.h>
#include <DHT.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <TinyGPS++.h>
#include "../common/config.h"
#include "../common/datawrap.h"

DHT dht(DHT_PIN, DHT22);
Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;
BH1750 lightMeter;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

void initSensors() {
    Wire.begin();
    dht.begin();
    mpu.begin();
    bmp.begin(0x76);
    lightMeter.begin();
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    pinMode(DUST_LED_PIN, OUTPUT);
    pinMode(VOLT_PIN, INPUT);
}

void wrapData(FlightData &packet, uint16_t id) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    packet.timestamp = millis();
    packet.accX = a.acceleration.x;
    packet.accY = a.acceleration.y;
    packet.accZ = a.acceleration.z;
    
    packet.lat = gps.location.lat();
    packet.lng = gps.location.lng();
    
    // Mission 3: Battery Energy
    // Assumes a voltage divider (e.g. two 10k resistors)
    int rawV = analogRead(VOLT_PIN);
    packet.voltage = (rawV * 3.3 / 4095.0) * 2.0; 
    packet.current = 120.5; // Placeholder (use INA219 for real current)

    packet.altitude = bmp.readAltitude(SEALEVEL_HPA);
    packet.pressure = bmp.readPressure() / 100.0F;
    packet.speed = gps.speed.kmph(); // Or vertical speed logic

    packet.temp = dht.readTemperature();
    packet.hum = dht.readHumidity();
    packet.lux = lightMeter.readLightLevel();
    packet.packetID = id;
}
#endif