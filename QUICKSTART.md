# Quick Start Guide - ESP32-S3 Walkie Talkie

## 🔌 Snelle Bedradings Checklist

### Minimale Bedrading (5 GPIO pins + power):

| Componenten | Verbinding | ESP32-S3 Pin |
|------------|------------|--------------|
| **INMP441 + MAX98357A** | SCK/BCLK (gedeeld) | GPIO 4 |
| **INMP441 + MAX98357A** | WS/LRC (gedeeld) | GPIO 5 |
| **INMP441 alleen** | SD (data in) | GPIO 6 |
| **MAX98357A alleen** | DIN (data out) | GPIO 7 |
| **Button** | Push-to-Talk | GPIO 0 → GND |

### Power:
- INMP441: **3.3V** + GND
- MAX98357A: **5V** + GND  
- Alle GND samen!

---

## ⚡ Snelle Start (PlatformIO)

```bash
# 1. Clone/open project
cd Walkie
code .

# 2. Build
pio run

# 3. Upload
pio run --target upload

# 4. Monitor
pio device monitor
```

---

## 🔧 Arduino IDE (alternatief)

1. Open `Walkie_Talkie_ESP32S3.ino`
2. Board: **ESP32S3 Dev Module**
3. USB CDC: **Enabled**
4. Upload + Serial Monitor (115200)

---

## 🎯 Gebruik

| Actie | Wat te doen |
|-------|-------------|
| **Praten** | Houd button ingedrukt |
| **Luisteren** | Laat button los |
| **Check status** | Serial monitor @ 115200 baud |

---

## 📊 Performance

| Parameter | Waarde |
|-----------|---------|
| Sample rate | 16 kHz (voice quality) |
| Bit depth | 16-bit |
| Latency | ~10-20 ms |
| Range | 30-100+ meter |
| Battery life | ~4-8 uur @ 1000mAh |

---

## 🐛 Snelle Fixes

**Geen geluid?**
```
✓ Check alle GND verbindingen
✓ MAX98357A heeft 5V nodig (niet 3.3V!)
✓ Serial monitor: zie je "TRANSMITTING"?
```

**Vervormd geluid?**
```cpp
// Verlaag gain in code:
int32_t sample = audioBuffer[i] * 1; // Was 2
```

**Te zachte microfoon?**
```cpp
// Verhoog gain in code:
int32_t sample = audioBuffer[i] * 4; // Was 2
```

**Crash/Reset?**
```
✓ Voeding: Gebruik 5V 1A adapter (niet alleen USB)
✓ Serial monitor: Check for "Guru Meditation Error"
```

---

## 📋 Pinout Geheugensteun

```
    ESP32-S3
    ┌──────┐
 4 ─┤●    ●├─ BCLK (beide)
 5 ─┤●    ●├─ WS (beide)  
 6 ─┤●    ●├─ SD (alleen mic)
 7 ─┤●    ●├─ DIN (alleen spk)
 0 ─┤●    ●├─ Button
    └──────┘
    3V3  5V  GND
     │    │   │
    Mic  Spk All
```

---

## 🎛️ Settings Cheat Sheet

### In `src/main.cpp` / `.ino`:

**Hogere kwaliteit** (meer CPU, meer latentie):
```cpp
#define SAMPLE_RATE 22050
#define AUDIO_BUFFER_SIZE 1024
```

**Lagere latentie** (lagere kwaliteit):
```cpp
#define SAMPLE_RATE 8000
#define AUDIO_BUFFER_SIZE 256
```

**WiFi kanaal veranderen**:
```cpp
esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);  // Was 1
```

---

## 📡 MAC Address Checklist

Beide devices moeten:
- ✓ Op hetzelfde WiFi kanaal zijn (default: 1)
- ✓ ESP-NOW succesvol initialiseren
- ✓ Broadcast mode gebruiken (FF:FF:FF:FF:FF:FF)

Check Serial Monitor:
```
MAC Address: XX:XX:XX:XX:XX:XX  ← Noteer deze!
ESP-NOW initialized             ← Moet verschijnen
Walkie Talkie Ready!            ← Gereed voor gebruik
```

---

## 🔋 Power Consumption

| Mode | Current | Tijd @ 1000mAh |
|------|---------|----------------|
| Idle (luisteren) | ~80-120 mA | 8-12 uur |
| TX (praten) | ~180-250 mA | 4-6 uur |
| Sleep (toekomstig) | ~10 mA | 100 uur |

---

## 🛠️ Debug Commando's

Serial monitor @ 115200 baud toont:

```
>>> TRANSMITTING <<<     ← Button ingedrukt
>>> RECEIVING <<<        ← Button losgelaten
Stats - Sent: 123 | Received: 456   ← Elke 5 seconden
```

Als je geen "Sent" stats ziet → Check button
Als je geen "Received" stats ziet → Check andere device

---

## 🚀 Next Steps

1. ✅ Test met 2 devices
2. ✅ Pas gain/volume aan naar wens
3. ✅ Experimenteer met sample rate
4. 🔲 Add LED indicators
5. 🔲 Add battery level monitor
6. 🔲 Add sleep mode

---

**Tip:** Print deze kaart uit en houd bij je breadboard!
