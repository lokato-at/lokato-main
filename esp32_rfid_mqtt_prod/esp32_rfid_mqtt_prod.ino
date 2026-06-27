#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "R200_3.h"

// =========================
// Configuration — fill in your own values before flashing.
// Do NOT commit real WiFi/MQTT credentials back into git.
// =========================
static const char* DEVICE_KEY = "<unique-device-key>";   // e.g. "RaspberryChild02" — one per ESP32

// Hostname is derived from DEVICE_KEY so each ESP32 is unique on the network.
static char WIFI_HOSTNAME[48] = { 0 };
static const char* WIFI_SSID     = "<your-wifi-ssid>";
static const char* WIFI_PASSWORD = "<your-wifi-password>";

// Prefer a DHCP reservation on your router for this device instead of a hard-coded static IP.
// Optional static IP example (uncomment if you really need it and know your network):
// IPAddress LOCAL_IP(192, 168, 1, 50);
// IPAddress GATEWAY(192, 168, 1, 1);
// IPAddress SUBNET(255, 255, 255, 0);
// IPAddress DNS1(1, 1, 1, 1);
// IPAddress DNS2(8, 8, 8, 8);

static const char* MQTT_HOST = "<lokato-pi-ip>";           // e.g. "192.168.1.100"
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USERNAME = nullptr;
static const char* MQTT_PASSWORD = nullptr;

static const uint8_t RFID_RX_PIN = 16;
static const uint8_t RFID_TX_PIN = 17;
// ---------- UHF read distance / range ----------
// This module does not offer a true "distance in cm" setting.
// In practice, read distance is mostly influenced by transmit power,
// antenna orientation, tag quality, environment, and mounting.
//
// Recommended range for this reader family is usually about 5..30 dBm.
// Lower value = shorter range / less stray reads.
// Higher value = longer range / easier reads.
//
// Good starting points:
//   5  dBm  -> very short range test setup
//   8  dBm  -> short range
//   12 dBm  -> medium range
//   18 dBm  -> longer range
//   24..27  -> aggressive / may read more than desired
static const uint8_t RFID_POWER_DBM = 20;

// Optional software filter against accidental long-distance reads:
// a tag must be seen N times in a row before it is accepted as "present".
// Increase this to 2..4 if you get false positives from farther away.
static const uint8_t RFID_CONFIRMATION_HITS = 2;

static const uint32_t RFID_POLL_INTERVAL_MS = 250;
static const uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
static const uint32_t MQTT_RETRY_INTERVAL_MS = 3000;
static const uint32_t NTP_FIRST_SYNC_RETRY_MS = 10000;  // solange noch keine Zeit da ist: alle 10s probieren
static const uint32_t NTP_RETRY_INTERVAL_MS = 60000;    // nach Fehlern später: alle 60s
static const uint32_t NTP_RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
static const uint32_t NTP_SYNC_WAIT_MS = 5000;  // statt 1500ms
static const uint32_t DIAG_INTERVAL_MS = 10000;
static const uint32_t STATUS_PUBLISH_INTERVAL_MS = 60000;
static const uint32_t WIFI_HARD_RESET_AFTER_MS = 5UL * 60UL * 1000UL;
static const uint32_t MQTT_HARD_RESET_AFTER_MS = 10UL * 60UL * 1000UL;
static const uint32_t BOOT_STABILIZE_DELAY_MS = 300;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
R200_3 rfid;

static uint8_t currentUid[12] = { 0 };
static uint8_t pendingUid[12] = { 0 };
static uint8_t candidateUid[12] = { 0 };
static uint8_t candidateHits = 0;
static bool pendingPublish = false;
static bool timeSynced = false;
static bool mqttWasConnected = false;

static unsigned long lastPollAt = 0;
static unsigned long lastWifiTryAt = 0;
static unsigned long lastMqttTryAt = 0;
static unsigned long lastNtpTryAt = 0;
static unsigned long lastNtpSyncAt = 0;
static unsigned long lastDiagAt = 0;
static unsigned long lastStatusAt = 0;
static unsigned long wifiDownSinceAt = 0;
static unsigned long mqttDownSinceAt = 0;

static void copyUid(uint8_t dst[12], const uint8_t src[12]) {
  memcpy(dst, src, 12);
}


static bool uidEquals(const uint8_t a[12], const uint8_t b[12]) {
  return memcmp(a, b, 12) == 0;
}

static bool isBlankUid(const uint8_t uid[12]) {
  for (uint8_t i = 0; i < 12; ++i) {
    if (uid[i] != 0x00) {
      return false;
    }
  }
  return true;
}

static void uidToHexString(const uint8_t uid[12], char* out, size_t outSize) {
  if (outSize < 27) {
    if (outSize > 0) {
      out[0] = '\0';
    }
    return;
  }

  out[0] = '0';
  out[1] = 'x';
  static const char hex[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < 12; ++i) {
    out[2 + (i * 2)] = hex[(uid[i] >> 4) & 0x0F];
    out[2 + (i * 2) + 1] = hex[uid[i] & 0x0F];
  }
  out[26] = '\0';
}

static bool getIso8601Local(char* out, size_t outSize) {
  if (outSize > 0) {
    out[0] = '\0';
  }

  if (!timeSynced) {
    return false;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return false;
  }

  const int year = timeinfo.tm_year + 1900;
  if (year < 2024) {
    return false;
  }

  char tmp[40] = { 0 };

  // Beispiel: 2026-05-02T20:15:30+0200
  if (strftime(tmp, sizeof(tmp), "%Y-%m-%dT%H:%M:%S%z", &timeinfo) == 0) {
    return false;
  }

  // +0200 zu +02:00 umbauen
  const size_t len = strlen(tmp);
  if (len >= 5 && outSize >= len + 2) {
    memcpy(out, tmp, len - 2);
    out[len - 2] = ':';
    out[len - 1] = tmp[len - 2];
    out[len] = tmp[len - 1];
    out[len + 1] = '\0';
    return true;
  }

  strncpy(out, tmp, outSize - 1);
  out[outSize - 1] = '\0';
  return true;
}


static void buildHostname(char* out, size_t outSize) {
  const uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  snprintf(out, outSize, "%s-%08lX", DEVICE_KEY, (unsigned long)chipId);
}

static void buildClientId(char* out, size_t outSize) {
  const uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  snprintf(out, outSize, "rfid-%s-%08lX", DEVICE_KEY, (unsigned long)chipId);
}

static void buildStatusTopic(char* out, size_t outSize) {
  snprintf(out, outSize, "devices/%s/status", DEVICE_KEY);
}

static void buildInfoTopic(char* out, size_t outSize) {
  snprintf(out, outSize, "devices/%s/info", DEVICE_KEY);
}

static bool publishDeviceInfo(bool retained = true) {
  if (!mqttClient.connected()) {
    return false;
  }

  char topic[64];
  buildInfoTopic(topic, sizeof(topic));

  StaticJsonDocument<256> doc;
  doc["device_key"] = DEVICE_KEY;
  doc["hostname"] = WIFI_HOSTNAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["uptime_ms"] = millis();
  doc["rfid_power_dbm"] = RFID_POWER_DBM;
  doc["rfid_confirmation_hits"] = RFID_CONFIRMATION_HITS;

  char payload[256];
  const size_t len = serializeJson(doc, payload, sizeof(payload));
  return mqttClient.publish(topic, payload, retained);
}

static bool publishOnlineStatus() {
  if (!mqttClient.connected()) {
    return false;
  }

  char topic[64];
  buildStatusTopic(topic, sizeof(topic));
  return mqttClient.publish(topic, "online", true);
}

static bool publishScanEvent(const uint8_t uid[12]) {
  if (!mqttClient.connected()) {
    return false;
  }

  char uidHex[27];
  uidToHexString(uid, uidHex, sizeof(uidHex));

  StaticJsonDocument<256> doc;
  doc["device_key"] = DEVICE_KEY;
  doc["tracker_uid"] = uidHex;
  doc["wifi_rssi"] = WiFi.RSSI();

  char ts[40];
  if (getIso8601Local(ts, sizeof(ts))) {
    doc["event_time"] = ts;
  } else {
    doc["event_time"] = nullptr;
    doc["time_synced"] = false;
  }

  char payload[256];
  const size_t len = serializeJson(doc, payload, sizeof(payload));

  Serial.print("Publishing MQTT to /api/v1/scan: ");
  Serial.println(payload);

  return mqttClient.publish("/api/v1/scan", payload, false);
}

static void queuePublishForCurrentTag() {
  if (isBlankUid(currentUid)) {
    return;
  }
  copyUid(pendingUid, currentUid);
  pendingPublish = true;
}

static void tryPublishPendingTag() {
  if (!pendingPublish || !mqttClient.connected()) {
    return;
  }

  if (publishScanEvent(pendingUid)) {
    pendingPublish = false;
    Serial.println("→ MQTT published");
  } else {
    Serial.print("→ MQTT publish failed, rc=");
    Serial.println(mqttClient.state());
  }
}

static void syncTimeIfNeeded(unsigned long nowMs, bool force = false) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  const bool due = force || !timeSynced || (nowMs - lastNtpSyncAt >= NTP_RESYNC_INTERVAL_MS);
  if (!due) {
    return;
  }

  const uint32_t retryInterval = timeSynced ? NTP_RETRY_INTERVAL_MS : NTP_FIRST_SYNC_RETRY_MS;

  if (!force && lastNtpTryAt != 0 && (nowMs - lastNtpTryAt < retryInterval)) {
    return;
  }

  lastNtpTryAt = nowMs;

  struct tm timeinfo;

  // Erst prüfen, ob die lokale Uhr bereits gültig läuft.
  if (getLocalTime(&timeinfo, 100)) {
    const int year = timeinfo.tm_year + 1900;
    if (year >= 2024) {
      if (!timeSynced) {
        Serial.println("Time already valid.");
      }
      timeSynced = true;
      lastNtpSyncAt = nowMs;
      return;
    }
  }

  Serial.println("NTP sync requested...");

  Serial.print("WiFi IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());

  IPAddress ntpIp;
  if (WiFi.hostByName("pool.ntp.org", ntpIp)) {
    Serial.print("NTP DNS ok: pool.ntp.org -> ");
    Serial.println(ntpIp);
  } else {
    Serial.println("NTP DNS failed: pool.ntp.org");
  }

  // Österreich/Wien: CET/CEST mit Sommerzeit
  configTzTime(
    "CET-1CEST,M3.5.0,M10.5.0/3",
    "pool.ntp.org",
    "time.google.com",
    "time.nist.gov");

  if (getLocalTime(&timeinfo, NTP_SYNC_WAIT_MS)) {
    const int year = timeinfo.tm_year + 1900;

    if (year >= 2024) {
      timeSynced = true;
      lastNtpSyncAt = nowMs;

      char buf[40];
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);

      Serial.print("Time synced: ");
      Serial.println(buf);
    } else {
      Serial.print("Time invalid after sync, year=");
      Serial.println(year);
    }
  } else {
    Serial.println("Time not synced yet.");
  }
}

static void beginWifi() {
  static bool wifiInitDone = false;
  if (!wifiInitDone) {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    buildHostname(WIFI_HOSTNAME, sizeof(WIFI_HOSTNAME));
    WiFi.setHostname(WIFI_HOSTNAME);

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Optional static IP configuration:
    // WiFi.config(LOCAL_IP, GATEWAY, SUBNET, DNS1, DNS2);

    wifiInitDone = true;
  }

  Serial.print("Connecting WiFi: ");
  Serial.print(WIFI_SSID);
  Serial.print(" as ");
  Serial.println(WIFI_HOSTNAME);
  WiFi.disconnect(false, true);
  delay(50);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void ensureWifiConnected(unsigned long nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    wifiDownSinceAt = 0;
    return;
  }

  if (wifiDownSinceAt == 0) {
    wifiDownSinceAt = nowMs;
  }

  if (nowMs - lastWifiTryAt >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiTryAt = nowMs;
    beginWifi();
  }

  if (nowMs - wifiDownSinceAt >= WIFI_HARD_RESET_AFTER_MS) {
    Serial.println("WiFi down too long -> restarting ESP");
    delay(100);
    ESP.restart();
  }
}

static bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  char clientId[64];
  char willTopic[64];
  buildClientId(clientId, sizeof(clientId));
  buildStatusTopic(willTopic, sizeof(willTopic));

  Serial.print("Connecting MQTT: ");
  Serial.print(MQTT_HOST);
  Serial.print(':');
  Serial.println(MQTT_PORT);

  bool ok = false;
  if (MQTT_USERNAME != nullptr && MQTT_PASSWORD != nullptr) {
    ok = mqttClient.connect(clientId, MQTT_USERNAME, MQTT_PASSWORD, willTopic, 0, true, "offline");
  } else {
    ok = mqttClient.connect(clientId, willTopic, 0, true, "offline");
  }

  if (ok) {
    Serial.println("MQTT connected.");
    publishOnlineStatus();
    publishDeviceInfo(true);
    lastStatusAt = millis();
    mqttWasConnected = true;
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(mqttClient.state());
  }
  return ok;
}

static void ensureMqttConnected(unsigned long nowMs) {
  if (WiFi.status() != WL_CONNECTED) {
    mqttDownSinceAt = 0;
    return;
  }

  if (mqttClient.connected()) {
    mqttDownSinceAt = 0;
    return;
  }

  if (mqttDownSinceAt == 0) {
    mqttDownSinceAt = nowMs;
  }

  if (nowMs - lastMqttTryAt >= MQTT_RETRY_INTERVAL_MS) {
    lastMqttTryAt = nowMs;
    connectMqtt();
  }

  if (nowMs - mqttDownSinceAt >= MQTT_HARD_RESET_AFTER_MS) {
    Serial.println("MQTT down too long -> restarting ESP");
    delay(100);
    ESP.restart();
  }
}

static void handleTagStateChange() {
  if (uidEquals(rfid.uid, currentUid)) {
    candidateHits = 0;
    memset(candidateUid, 0, sizeof(candidateUid));
    return;
  }

  if (isBlankUid(rfid.uid)) {
    candidateHits = 0;
    memset(candidateUid, 0, sizeof(candidateUid));

    if (!isBlankUid(currentUid)) {
      char uidHex[27];
      uidToHexString(currentUid, uidHex, sizeof(uidHex));
      Serial.print("Tag entfernt: ");
      Serial.println(uidHex);
      memset(currentUid, 0, sizeof(currentUid));
    }
    return;
  }

  if (!uidEquals(rfid.uid, candidateUid)) {
    copyUid(candidateUid, rfid.uid);
    candidateHits = 1;
    return;
  }

  if (candidateHits < 255) {
    candidateHits++;
  }

  if (candidateHits < RFID_CONFIRMATION_HITS) {
    return;
  }

  copyUid(currentUid, candidateUid);
  candidateHits = 0;
  memset(candidateUid, 0, sizeof(candidateUid));

  char uidHex[27];
  uidToHexString(currentUid, uidHex, sizeof(uidHex));
  Serial.print("TAG erkannt: ");
  Serial.print(uidHex);
  Serial.print(" @ ");
  Serial.print(RFID_POWER_DBM);
  Serial.println(" dBm");

  queuePublishForCurrentTag();
  tryPublishPendingTag();
}

static void publishPeriodicStatus(unsigned long nowMs) {
  if (!mqttClient.connected()) {
    return;
  }

  if (nowMs - lastStatusAt < STATUS_PUBLISH_INTERVAL_MS) {
    return;
  }

  lastStatusAt = nowMs;
  publishOnlineStatus();
  publishDeviceInfo(true);
}

static void printDiagnostics(unsigned long nowMs) {
  if (nowMs - lastDiagAt < DIAG_INTERVAL_MS) {
    return;
  }

  lastDiagAt = nowMs;
  Serial.print("diag wifi=");
  Serial.print((int)WiFi.status());
  Serial.print(" ip=");
  Serial.print(WiFi.localIP());
  Serial.print(" rssi=");
  Serial.print(WiFi.RSSI());
  Serial.print(" mqtt=");
  Serial.print(mqttClient.connected() ? "1" : "0");
  Serial.print(" mqttState=");
  Serial.print(mqttClient.state());
  Serial.print(" heap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" pending=");
  Serial.print(pendingPublish ? "1" : "0");
  Serial.print(" power_dbm=");
  Serial.print(RFID_POWER_DBM);
  Serial.print(" confirm_hits=");
  Serial.print(RFID_CONFIRMATION_HITS);

  Serial.print(" ntp=");
  Serial.print(timeSynced ? "1" : "0");

  Serial.print(" lastNtpTry=");
  Serial.print(lastNtpTryAt);

  Serial.print(" lastNtpSync=");
  Serial.print(lastNtpSyncAt);

  Serial.println();
}

static const char* wifiDisconnectReasonText(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED:           return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:           return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:            return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:          return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:         return "ASSOC_TOOMANY (AP voll)";
    case WIFI_REASON_NOT_AUTHED:            return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED:           return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE:           return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:      return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:return "4WAY_HANDSHAKE_TIMEOUT (falsches Passwort?)";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:     return "HANDSHAKE_TIMEOUT (falsches Passwort?)";
    case WIFI_REASON_MIC_FAILURE:           return "MIC_FAILURE (falsches Passwort?)";
    case WIFI_REASON_AUTH_FAIL:             return "AUTH_FAIL (falsche Zugangsdaten?)";
    case WIFI_REASON_ASSOC_FAIL:            return "ASSOC_FAIL";
    case WIFI_REASON_BEACON_TIMEOUT:        return "BEACON_TIMEOUT (Signal verloren)";
    case WIFI_REASON_NO_AP_FOUND:           return "NO_AP_FOUND (SSID nicht gefunden / 5GHz / zu weit weg)";
    case WIFI_REASON_CONNECTION_FAIL:       return "CONNECTION_FAIL";
    default:                                return "(siehe esp_wifi_types.h)";
  }
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA connected to AP");
      wifiDownSinceAt = 0;
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] Got IP: ");
      Serial.println(WiFi.localIP());

      Serial.print("[WiFi] Gateway: ");
      Serial.println(WiFi.gatewayIP());

      Serial.print("[WiFi] DNS: ");
      Serial.println(WiFi.dnsIP());

      wifiDownSinceAt = 0;

      // Direkt nach IP-Vergabe NTP versuchen.
      syncTimeIfNeeded(millis(), true);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      const uint8_t reason = info.wifi_sta_disconnected.reason;
      Serial.print("[WiFi] Disconnected, reason=");
      Serial.print(reason);
      Serial.print(" (");
      Serial.print(wifiDisconnectReasonText(reason));
      Serial.println(")");
      mqttWasConnected = false;
      mqttDownSinceAt = 0;
      break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(BOOT_STABILIZE_DELAY_MS);

  Serial.println();
  Serial.println("=== Production RFID + MQTT scanner starts ===");
  Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());

  WiFi.onEvent(onWiFiEvent);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(2);

  rfid.begin(&Serial2, 115200, RFID_RX_PIN, RFID_TX_PIN);
  rfid.dumpModuleInfo();
  delay(80);
  rfid.setTransmitPowerDbm(RFID_POWER_DBM);

  memset(currentUid, 0, sizeof(currentUid));
  memset(pendingUid, 0, sizeof(pendingUid));
  memset(candidateUid, 0, sizeof(candidateUid));
  candidateHits = 0;

  beginWifi();
}

void loop() {
  const unsigned long nowMs = millis();

  ensureWifiConnected(nowMs);
  syncTimeIfNeeded(nowMs);
  ensureMqttConnected(nowMs);

  if (mqttClient.connected()) {
    mqttClient.loop();
  } else if (mqttWasConnected) {
    mqttWasConnected = false;
  }

  rfid.loop();

  if (nowMs - lastPollAt >= RFID_POLL_INTERVAL_MS) {
    lastPollAt = nowMs;
    rfid.poll();
  }

  handleTagStateChange();
  tryPublishPendingTag();
  publishPeriodicStatus(nowMs);
  printDiagnostics(nowMs);

  delay(5);
}
