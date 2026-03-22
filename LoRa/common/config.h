#ifndef CONFIG_H
#define CONFIG_H

// --- MISSION SETTINGS ---
#define RF_FREQUENCY      923.0E6
#define LORA_BW           500E3   // 500kHz for high-speed
#define LORA_SF           7       // SF7 for real-time
#define SEALEVEL_HPA      1013.25 // ความสูงนเำทะเล ต้แงแก้วันจริง
#define SEND_INTERVAL     200     // 5Hz Data Rate

// --- HARDWARE PIN MAPPING (Generic ESP32 / T-Beam) ---
#define LORA_CS           18
#define LORA_RST          23
#define LORA_IRQ          26
#define DHT_PIN           4
#define DUST_LED_PIN      27
#define DUST_ANA_PIN      34
#define VOLT_PIN          35      // Analog pin for Voltage Divider
#define GPS_RX            16
#define GPS_TX            17

#endif