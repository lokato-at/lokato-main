/**
 * R200 RFID + WiFi + MQTT publisher
 * Publishes a scan event when a new tag is detected
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "R200_3.h"

// ---------- Device ----------
static const char* DEVICE_KEY = "RaspberryChild02";

// ---------- WiFi ----------
static const char* WIFI_SSID = "lokato"; //DEIN WLAN
static const char* WIFI_PASSWORD = "lokato123"; //DEIN NETZWERKSCHLÜSSEL

// ---------- MQTT ----------
static const char* MQTT_HOST = "192.168.1.100"; // ipconfig / hostname -I
static const uint16_t MQTT_PORT = 1883;

static const char* MQTT_USERNAME = nullptr; // keep null if allow_anonymous true
static const char* MQTT_PASSWORD = nullptr;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ---------- R200 ----------hostname
unsigned long lastPollAt = 0;
R200_3 rfid;

// Track last published UID to avoid spamming
uint8_t lastPublishedUid[12] = {0};

// ---------- Time (NTP) ----------
static void setupTime() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  Serial.print("Syncing time via NTP");

  unsigned long start = millis();
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);

    // 10 Sekunden Timeout
    if (millis() - start > 10000) {
      Serial.println("\nNTP timeout – continuing without time");
      return;
    }
  }

  Serial.println("\nTime synced.");
}


static String getIso8601Local() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }

  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &timeinfo); // +0100
  String ts(buf);

  // "+0100" -> "+01:00"
  if (ts.length() > 5) {
    String tz = ts.substring(ts.length() - 5);
    String tzFormatted = tz.substring(0, 3) + ":" + tz.substring(3);
    ts = ts.substring(0, ts.length() - 5) + tzFormatted;
  }

  return ts;
}

// ---------- Helpers ----------
static String uidToHexString(const uint8_t uid[12]) {
  String out = "0x";
  for (int i = 0; i < 12; i++) {
    if (uid[i] < 0x10) out += "0";
    out += String(uid[i], HEX);
  }
  out.toUpperCase();
  return out;
}

static String buildScanTopic() {
  return String("/api/v1/scan");
}

// ---------- WiFi ----------
static void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connect failed.");
  }
}

// ---------- MQTT ----------
static void connectMqtt() {
  if (mqttClient.connected()) return;

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  String clientId = String("lokato-") + DEVICE_KEY + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("Connecting MQTT: ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  bool ok = false;

  if (MQTT_USERNAME && MQTT_PASSWORD) {
    ok = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    ok = mqttClient.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("MQTT connected.");
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(mqttClient.state());
  }
}

static bool publishScanEvent(const uint8_t uid[12]) {
  if (!mqttClient.connected()) return false;

  StaticJsonDocument<384> doc;   // etwas größer, sicherer
  doc["device_key"]  = DEVICE_KEY;          // <-- NEU
  doc["tracker_uid"] = uidToHexString(uid);

  String ts = getIso8601Local();
  if (ts.length() > 0) {
    doc["event_time"] = ts;
  }

  char payload[384];             // passend zur Doc-Größe
  size_t len = serializeJson(doc, payload, sizeof(payload));

  String topic = buildScanTopic();

  Serial.print("Publishing MQTT to ");
  Serial.println(topic);
  Serial.println(payload);

  return mqttClient.publish(topic.c_str(), payload, len);
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== R200 + MQTT scanner starts ===");

  // RFID SOFORT initialisieren
  rfid.begin(&Serial2, 115200, 16, 17);
  rfid.dumpModuleInfo();

  rfid.setTransmitPowerDbm(12);   // REICHWEITE

  mqttClient.setBufferSize(512);

  do {
    if (WiFi.status() != WL_CONNECTED) {
      connectWifi();
      if (WiFi.status() == WL_CONNECTED) {
        setupTime();
        Serial.println("WiFi connected and local time synced");
      } else {
        Serial.println("WiFi not connected. Retrying ...");
      }
    } else if (!mqttClient.connected) {
      connectMqtt();
      if (mqttClient.connected) {
        Serial.println("MQTT connected");
      } else {
        Serial.println("MQTT not connected. Retrying ...");
      }
    }

    if (!mqttClient.connected || WiFi.status() != WL_CONNECTED) {
      Serial.println("Retrying in 2000ms");
      delay(2000); // pause for two seconds
    }
  } while (!mqttClient.connected || WiFi.status() != WL_CONNECTED);

  memset(lastPublishedUid, 0, sizeof(lastPublishedUid));

  Serial.println("Setup success. Entering loop()");
}


void loop() {
  // -------- WiFi / MQTT am Leben halten --------
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  // -------- R200 Daten verarbeiten --------
  rfid.loop();

  // -------- alle 300 ms poll --------
  if (millis() - lastPollAt > 300) {
    rfid.poll();
    lastPollAt = millis();
  }

  // -------- UID-Änderung erkennen --------
  if (memcmp(rfid.uid, lastPublishedUid, 12) != 0) {

    bool isBlank = true;
    for (int i = 0; i < 12; i++) {
      if (rfid.uid[i] != 0x00) {
        isBlank = false;
        break;
      }
    }

    if (!isBlank) {
      // -------- UID IMMER im Serial ausgeben --------
      Serial.print("TAG erkannt: ");
      Serial.print("0x");
      for (int i = 0; i < 12; i++) {
        if (rfid.uid[i] < 0x10) Serial.print("0");
        Serial.print(rfid.uid[i], HEX);
      }
      Serial.println();

      // -------- MQTT senden --------
      if (mqttClient.connected()) {
        bool ok = publishScanEvent(rfid.uid);
        if (ok) {
          Serial.println("→ MQTT published");
          memcpy(lastPublishedUid, rfid.uid, 12);
        } else {
          Serial.println("→ MQTT publish FAILED");
        }
      } else {
        Serial.println("→ MQTT nicht verbunden (nur Serial-Ausgabe)");
      }

    } else {
      // -------- Tag entfernt --------
      Serial.println("Tag entfernt");
      memset(lastPublishedUid, 0, sizeof(lastPublishedUid));
    }
  }

  delay(20);
}

