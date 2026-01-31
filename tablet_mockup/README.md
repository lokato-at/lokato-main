# Raumbelegung – Grid-Prototyp

Der Prototyp stellt **kein finales Produkt** dar!

---

## Funktionsumfang

### Grid
- Feste Grid-Größe (z. B. 3×4 Felder)
- Dynamische Generierung der Zellen
- Unterstützung gesperrter Felder

### Belegung
- Freie Felder können über ein Plus-Symbol belegt werden
- Auswahl eines Tieres über ein modales Auswahlfenster
- Belegte Felder zeigen das ausgewählte Tier

### Entfernen von Belegungen
- Belegte Felder können durch erneuten Klick geleert werden
- Das Feld wird anschließend wieder als frei markiert

### Eindeutige Belegung
- Jedes Tier kann nur **einmal** im Grid verwendet werden
- Bereits genutzte Tiere sind in der Auswahl deaktiviert

### Belegungsanzeige
- Anzeige der aktuell belegten Plätze
- Optionale Begrenzung der maximalen Belegung



## Technische Umsetzung

### Technologien
- HTML (Struktur)
- CSS (Layout & Styling)
- JavaScript (Interaktionslogik)

### Architektur
- Single-File-Prototyp (eine HTML-Datei)
- Keine externen Abhängigkeiten
- Keine Server- oder Backend-Komponenten

### Zustandsverwaltung
Der Zustand des Grids wird clientseitig verwaltet:

- `null` → freies Feld
- Emoji → belegtes Feld
- `locked` → gesperrtes Feld

Zusätzlich wird eine Liste genutzter Tiere geführt, um Mehrfachbelegungen zu verhindern.

---

## Nutzung

1. Öffnen der HTML-Datei in einem Browser.
2. Freie Felder über das Plus-Symbol belegen.
3. Belegungen durch Klick auf das jeweilige Feld entfernen.
4. Gesperrte Felder sind rein informativ und nicht interaktiv.

Der Prototyp ist vollständig offline nutzbar.

---



---

## Was hier noch zu erledigen ist

- Verbindung zum Backend herstellen
- Design soll den Bildschirmen nach angepasst werden
- Vue integration 
- Funktionsfähige Bildschhirme implementieren


---

