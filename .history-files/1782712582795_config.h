#ifndef CONFIG_H
#define CONFIG_H
#define WIFI_SSID      "Kodnest_Student"
#define WIFI_PASSWORD  "Welcome@1234"
#define SERVER_IP      "10.123.26.123"
#define SERVER_PORT    "3000"
#define API_URL        "http://" SERVER_IP ":" SERVER_PORT "/wifi/rssi"
#define DEVICE_ID      "ESP32_01"
#define RSSI_THRESHOLD 5
#define CONFIG_HEARTBEAT_MS 10000
#define CONFIG_SCAN_INTERVAL_MS 100
#endif