# Raspberry Pi Setup Script – Anleitung

Dieses Script richtet eine Entwicklungsumgebung auf **Raspberry Pi OS (64-bit)** ein.  
Es installiert für das Lokato-Platform Projekt **Docker, Docker Compose, Node.js, PHP + Extensions und Composer** sowie **optional Nginx**.

Die Nutzung ist **für lokale Netzwerke gedacht** (keine Domain notwendig).

---

## Voraussetzungen

- Raspberry Pi OS **64-bit** (Desktop oder Lite)
- Raspberry Pi 4 oder neuer
- Internetverbindung
- Ein normaler Benutzer mit `sudo`-Rechten  
  ⚠️ **Script nicht als root ausführen**

---

## Aktuelle Paketlisten laden (wichtig!)

Vor dem ersten Setup:

```bash
sudo apt update
```

---

## Script Location

```
lokato-main/
└── scripts/
    └── rpi-setup.sh
```

---

## Script ausführbar machen

```bash
chmod +x ./scripts/rpi-setup.sh
```

---

## Script benutzen

### 1️⃣ Nur prüfen (empfohlen)

Zeigt an, **was fehlt**, ohne etwas zu installieren:

```bash
./scripts/rpi-setup.sh --check-only
```

Beispiel-Ausgabe:

```
❌ Docker missing
❌ Node.js missing
❌ Composer missing
✅ PHP found
ℹ️ Nginx optional (skipping check)
```

---

### 2️⃣ Standard-Setup (empfohlen)

Installiert/stellt sicher:

* Docker
* Docker Compose
* Node.js (LTS mit Fallback)
* PHP + Extensions
* **Composer (nicht optional)**

**Ohne Nginx**

```bash
./scripts/rpi-setup.sh
```

---

### 3️⃣ Setup inkl. Nginx (optional)

Nur nötig, wenn du:

* einen Reverse Proxy willst
* oder bewusst einen Webserver brauchst

```bash
./scripts/rpi-setup.sh --with-nginx
```

ℹ️ Falls Port 80 bereits belegt ist, wird **Nginx installiert**, aber eventuell **nicht gestartet** (das Script bricht deshalb nicht ab).

---

## Node.js – Besonderheiten

* Das Script versucht zuerst **NodeSource LTS**
* Falls das fehlschlägt, wird automatisch auf die **Raspberry Pi OS / Debian Version** zurückgefallen
* Am Ende wird **verifiziert**, dass `node` oder `nodejs` wirklich existiert

Ausgabe z. B.:

```
✅ Node.js version: v18.x.x
✅ npm version: x.x.x
```

---

## PHP + Extensions

Das Script installiert **PHP + Extensions auch dann**, wenn `php` bereits installiert ist.
Damit fehlen dir später keine typischen Module (xml, curl, zip, mysql, …).

Check:

```bash
php -v
```

---

## Composer
Composer wird **immer** installiert/gesichert (auch wenn PHP schon vorhanden ist).

Check:

```bash
composer --version
```

---

## Docker – wichtig nach der Installation

Wenn Docker neu installiert wurde:

```bash
newgrp docker
```

oder **ab- und wieder anmelden**, damit Docker ohne `sudo` nutzbar ist.

Test:

```bash
docker run hello-world
```

---

## Nginx – optional & lokal

* **Nicht nötig**, wenn:

  * du Docker nutzt
  * oder Apps direkt über Ports aufrufst (`http://pi-ip:3000`)
* Nginx kann später jederzeit installiert werden:

  ```bash
  ./scripts/rpi-setup.sh --with-nginx
  ```

---

## Typische lokale Aufrufe

* Docker / Node App:

  ```
  http://<pi-ip>:3000
  http://<pi-ip>:8080
  ```

* PHP Dev-Server (ohne Nginx):

  ```bash
  php -S 0.0.0.0:8000
  ```

---

## Fehlerbehebung

### Node.js fehlt nach dem Script

```bash
node -v || nodejs -v
apt-cache policy nodejs
```

### Composer fehlt nach dem Script

```bash
composer --version
dpkg -l | grep composer || true
```

### Nginx startet nicht

Meist ist Port 80 belegt:

```bash
sudo ss -tulpn | grep ':80'
```

---

## Wichtige Hinweise nach der Installation

* **Führe vor dem ersten Setup immer ein Update aus:**

  ```bash
  sudo apt update
  ```

  Dadurch werden aktuelle Paketlisten geladen und Installationsfehler vermieden.

* **Starte den Raspberry Pi nach der Installation neu (empfohlen):**

  ```bash
  sudo reboot
  ```

  Das stellt sicher, dass:

  * neue Benutzergruppen (z. B. `docker`) aktiv sind
  * alle Dienste sauber gestartet werden
  * Umgebungsänderungen korrekt übernommen werden

* Alternativ (ohne Reboot) kannst du für Docker ausführen:

  ```bash
  newgrp docker
  ```

  Ein Neustart ist jedoch die **sauberste und zuverlässigste Lösung**.



