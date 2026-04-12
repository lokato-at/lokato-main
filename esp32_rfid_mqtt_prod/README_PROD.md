# ESP32 RFID MQTT Production Build

## Wichtig vor dem Flashen

Passe in `esp32_rfid_mqtt_prod.ino` diese Werte an:

- `DEVICE_KEY`
- `WIFI_HOSTNAME`
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_HOST`
- optional `MQTT_USERNAME` / `MQTT_PASSWORD`

## Was verbessert wurde

- kein blockierender WLAN-Reconnect mehr
- kein blockierender MQTT-Reconnect mehr
- RFID-UART Parser jetzt byteweise und nicht-blockierend
- Buzzer nicht-blockierend
- keine `String`-Objekte in den kritischen Pfaden
- queued publish: wenn MQTT kurz weg ist, wird der letzte erkannte Tag nach Reconnect gesendet
- MQTT Last Will + Online-Status
- periodische Device-Info und Diagnoseausgaben
- WLAN Sleep deaktiviert
- Hostname gesetzt
- NTP robuster und periodisch

## Empfohlene Arduino IDE / PlatformIO Libraries

- PubSubClient
- ArduinoJson
- ESP32 Arduino Core aktuell halten

## Netzwerk-Empfehlung

Nutze möglichst **DHCP Reservation** im Router statt einer fest im Code gesetzten IP.
Dann bleibt die IP stabil, aber DNS/Gateway/Subnet bleiben zentral sauber verwaltet.

## Broker-Empfehlung

Wenn möglich:

- lokalen DNS-Namen für den Broker verwenden, z. B. `mqtt-broker.lan`
- festen Broker-Hostnamen statt wechselnder IP
- MQTT-Authentifizierung aktivieren
- optional TLS, wenn das Netz nicht komplett intern/vertrauenswürdig ist


Fixed deployment values in this build:
- SSID: lokato
- MQTT broker: 192.168.1.100:1883
- Hostname is derived from DEVICE_KEY + chip id
- Auto-recovery: ESP restart after 5 min WiFi-down or 10 min MQTT-down
