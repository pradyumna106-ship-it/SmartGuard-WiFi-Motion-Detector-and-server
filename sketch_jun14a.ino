#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#define LED_PIN 2
#define PIR_PIN 4
int heartbeat = 10000;
int scanInterval = 100;
int threshold = 5;
#define MOTION_SEND_COOLDOWN_MS 500
#define HTTP_TIMEOUT_MS 3000
int baselineRSSI = 0;
bool lastStrongMotionState = false;
bool lastPirMotionState = false;
unsigned long lastScanMs = 0;
unsigned long lastApiSendMs = 0;
// Add near the top with other globals
unsigned long lastConfigFetchMs = 0;
#define CONFIG_FETCH_INTERVAL_MS 30000  // check every 30 seconds
#define CONFIG_SERVER_IP SERVER_IP
#define CONFIG_SERVER_PORT SERVER_PORT
String wifiSSID = WIFI_SSID;
String wifiPassword = WIFI_PASSWORD;
String apiUrl = API_URL;
String deviceId = DEVICE_ID;

bool detectStrongMotionViaRSSI(int currentRSSI)
{
    int diff = abs(currentRSSI - baselineRSSI);
    return diff >= threshold;
}

bool detectMotionViaPIR()
{
    return digitalRead(PIR_PIN) == HIGH;
}

bool connectWifi(const char *ssid, const char *password)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(1000);

    WiFi.begin(ssid, password);
    Serial.println("SSID:");
    Serial.println(WiFi.SSID());
    Serial.println("HOST:");
    Serial.println(WiFi.getHostname());
    int retry = 0;

    while (WiFi.status() != WL_CONNECTED && retry < 30)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi Connected");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("WiFi Connection Failed");
    return false;
}

bool sendDataToAPI(int rssi, bool strongMotionSense, bool motionDetected)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi disconnected. API send skipped.");
        return false;
    }

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    Serial.print("API URL: ");
    Serial.println(apiUrl);
    http.begin(apiUrl.c_str());
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"deviceId\":\"" + String(deviceId) + "\",";
    payload += "\"rssi\":" + String(rssi) + ",";
    payload += "\"strongMotionSense\":" + String(strongMotionSense ? "true" : "false") + ",";
    payload += "\"motionDetected\":" + String(motionDetected ? "true" : "false") + ",";
    payload += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    payload += "}";

    Serial.println("Sending payload:");
    Serial.println(payload);

    int responseCode = http.POST(payload);

    Serial.print("HTTP Response Code: ");
    Serial.println(responseCode);

    if (responseCode > 0)
    {
        String response = http.getString();
        Serial.println("Response:");
        Serial.println(response);
    }
    else
    {
        Serial.print("HTTP Error: ");
        Serial.println(http.errorToString(responseCode));
    }

    http.end();

    return responseCode >= 200 && responseCode < 300;
}

bool shouldSendToApi(unsigned long now, bool strongMotionSense, bool motionDetected)
{
    bool stateChanged =
        strongMotionSense != lastStrongMotionState ||
        motionDetected != lastPirMotionState;

    bool heartbeatDue = (now - lastApiSendMs >= heartbeat);

    bool motionReportDue =
        (strongMotionSense || motionDetected) &&
        (now - lastApiSendMs >= MOTION_SEND_COOLDOWN_MS);

    return stateChanged || heartbeatDue || motionReportDue;
}

void setup()
{
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    pinMode(PIR_PIN, INPUT);

    deviceId = DEVICE_ID;

    // Step 1: Connect to bootstrap WiFi from config.h
    Serial.println("Connecting to bootstrap WiFi...");
    if (!connectWifi(WIFI_SSID, WIFI_PASSWORD))
    {
        Serial.println("Bootstrap WiFi connection failed");
        return;
    }

    // Step 2: Fetch config from server (gets new SSID if updated)
    Serial.println("Loading configuration...");
    
    // Step 3: If server returned a different SSID, reconnect to it
    if (wifiSSID.length() > 0 &&
        (wifiSSID != String(WIFI_SSID) || wifiPassword != String(WIFI_PASSWORD)))
    {
        Serial.println("New WiFi credentials received. Reconnecting...");
        WiFi.disconnect(true, true);

        if (!connectWifi(wifiSSID.c_str(), wifiPassword.c_str()))
        {
            Serial.println("New WiFi connection failed");
            return;
        }

        // Re-fetch config on new network to get correct apiUrl
            Serial.print("API URL after reconnect: ");
            Serial.println(apiUrl);
    }

    // Step 4: Calculate baseline RSSI
    int sum = 0;
    for (int i = 0; i < 10; i++)
    {
        sum += WiFi.RSSI();
        delay(100);
    }
    baselineRSSI = sum / 10;

    Serial.print("Baseline RSSI: ");
    Serial.println(baselineRSSI);

    lastScanMs = millis();
    lastApiSendMs = 0;
}
void loop()
{
    unsigned long now = millis();
    if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi not connected...");
            delay(2000);
            return;
        }
    if (now - lastScanMs < scanInterval)
    {
        return;
    }
    // Periodic config re-fetch
    if (now - lastConfigFetchMs >= CONFIG_FETCH_INTERVAL_MS) {
        lastConfigFetchMs = now;

        String prevSSID = wifiSSID;

            // If SSID changed, reconnect
            if (wifiSSID.length() > 0 && wifiSSID != prevSSID) {
                Serial.println("WiFi config changed. Reconnecting...");
                WiFi.disconnect(true, true);
                // Step 1: Connect to bootstrap WiFi from config.h
                    Serial.println("Connecting to bootstrap WiFi...");
                    if (!connectWifi(WIFI_SSID, WIFI_PASSWORD))
                    {
                        Serial.println("Bootstrap WiFi connection failed");
                        return;
                    }
                    // ✅ Set fallback apiUrl immediately so sending never fails
                    apiUrl = "http://" + String(CONFIG_SERVER_IP) + ":" + String(CONFIG_SERVER_PORT) + "/wifi/rssi";
                    Serial.println("Default API URL set: " + apiUrl);
                if (connectWifi(wifiSSID.c_str(), wifiPassword.c_str())) {
                    Serial.println("Reconnected to new WiFi");

                    // Recalculate baseline RSSI for new network
                    int sum = 0;
                    for (int i = 0; i < 10; i++) {
                        sum += WiFi.RSSI();
                        delay(100);
                    }
                    baselineRSSI = sum / 10;
                    Serial.print("New Baseline RSSI: ");
                    Serial.println(baselineRSSI);
                } else {
                    Serial.println("Reconnect failed. Staying on old network.");
                }
            }
    }

    lastScanMs = now;

    int currentRSSI = WiFi.RSSI();
    bool strongMotionSense = detectStrongMotionViaRSSI(currentRSSI);
    bool motionDetected = detectMotionViaPIR();

    digitalWrite(LED_PIN, strongMotionSense ? HIGH : LOW);

    if (strongMotionSense != lastStrongMotionState)
    {
        Serial.print(strongMotionSense ? "STRONG MOTION ON  " : "STRONG MOTION OFF ");
        Serial.print("RSSI=");
        Serial.println(currentRSSI);
    }

    if (motionDetected != lastPirMotionState)
    {
        Serial.print(motionDetected ? "PIR MOTION ON  " : "PIR MOTION OFF ");
        Serial.println("(ESP32 proximity)");
    }

    if (shouldSendToApi(now, strongMotionSense, motionDetected))
    {
        bool sent = sendDataToAPI(currentRSSI, strongMotionSense, motionDetected);

        if (sent)
        {
            Serial.println("API sent successfully");
        }
        else
        {
            Serial.println("API send failed");
        }

        lastApiSendMs = now;
        lastStrongMotionState = strongMotionSense;
        lastPirMotionState = motionDetected;
    }
    // while (true) {
    //     delay(100);
    // }
}