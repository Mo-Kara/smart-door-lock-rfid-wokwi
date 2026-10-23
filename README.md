# Smart Door Lock with RFID – ESP32, MQTT (Wokwi simulation)

Second version of my ESP32 smart door lock (first version: [smart-door-lock-esp32](https://github.com/Mo-Kara/smart-door-lock-esp32)). It adds an **RC522 RFID reader** next to the keypad and runs in the **Wokwi** simulator.

## Features
- Unlock with a **4x4 keypad PIN** (`#` = enter, `*` = clear, `*` on an empty entry while open = lock) or an **RFID card** (RC522)
- **SSD1306 OLED** (I2C) shows prompts and status
- **Servo** moves the lock (0° open, 90° locked), non-blocking **auto-lock after 5 s**
- **MQTT** (`broker.hivemq.com`): status, event log and remote commands

| Topic | Use |
|---|---|
| `smartlock/command` | send `lock` or `unlock` |
| `smartlock/status` | `locked` / `unlocked` |
| `smartlock/logs` | event log (PIN, RFID, auto-lock, unknown commands) |

## Pins (Wokwi)
| Part | ESP32 pin |
|---|---|
| Servo | GPIO 16 |
| RC522 SS / RST | GPIO 5 / 27 (SPI default pins) |
| Keypad rows | 32, 33, 25, 26 |
| Keypad columns | 14, 13, 15, 4 |
| OLED | I2C default (SDA 21, SCL 22), address 0x3C |

Keypad pins are remapped so they do not collide with the SPI bus used by the RFID reader.

## Run it
1. Create an ESP32 project in [Wokwi](https://wokwi.com) with the parts above and paste `smart_lock_rfid/smart_lock_rfid.ino`.
2. Libraries: `PubSubClient`, `ESP32Servo`, `Keypad`, `Adafruit GFX`, `Adafruit SSD1306`, `MFRC522`.
3. Start the simulation; Wi-Fi is the built-in `Wokwi-GUEST` network.
4. Test remote control by publishing `unlock` to `smartlock/command` (for example with MQTT Explorer or `mosquitto_pub -h broker.hivemq.com -t smartlock/command -m unlock`).
5. Set `authorizedUID` to your card's UID (printed on the serial monitor as "Scanned UID").

## Notes and limitations
- Wi-Fi uses the open Wokwi network, so there are no credentials in the code.
- The default PIN `1234` and the example UID are placeholders for the simulation.
- MQTT is unencrypted (port 1883) on a public broker without login: anyone who knows the topic can send `unlock`. For a real door I would use TLS (8883), broker credentials and a unique topic prefix.
- The PIN is stored in plain text and there is no lock-out after wrong attempts.
- Not yet tested on real hardware.