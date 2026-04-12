# ESP32 RFID MQTT range-control variant

## What you can adjust

The actual UHF read distance cannot be set in centimeters.
The most important software setting is transmit power:

```cpp
static const uint8_t RFID_POWER_DBM = 8;
```

Lower value = shorter range. Higher value = longer range.

Suggested starting values:
- 5 dBm: very short range
- 8 dBm: short range
- 12 dBm: medium range
- 18 dBm: long range
- 24+ dBm: very aggressive, may read unintended tags

## Extra filter

```cpp
static const uint8_t RFID_CONFIRMATION_HITS = 2;
```

A tag must be seen multiple times before it is accepted. This reduces accidental reads from farther away.

## Important

Read distance also depends heavily on:
- antenna orientation
- tag size and quality
- metal nearby
- mounting position
- power supply stability
- environment and reflections

For really short range, lower transmit power first, then change antenna placement.
