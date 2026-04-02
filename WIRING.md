# Wiring Diagram - ESP32-S3 Walkie Talkie

## Geoptimaliseerde Configuratie (Gedeelde I2S Pins)

```
                    ┌───────────────────────────────┐
                    │      ESP32-S3 Supermini       │
                    │                               │
                    │  ┌─────────────────────────┐  │
                    │  │                         │  │
    ┌───────────────┼──┤ GPIO 4 (I2S BCLK)      │  │
    │               │  │                         │  │
    │  ┌────────────┼──┤ GPIO 5 (I2S WS/LRCLK)  │  │
    │  │            │  │                         │  │
    │  │  ┌─────────┼──┤ GPIO 6 (I2S SD_IN)     │  │
    │  │  │         │  │                         │  │
    │  │  │    ┌────┼──┤ GPIO 7 (I2S SD_OUT)    │  │
    │  │  │    │    │  │                         │  │
    │  │  │    │    │  │ GPIO 0 (BUTTON)        ├──┼───[Button]───GND
    │  │  │    │    │  │                         │  │
    │  │  │    │    │  │ 3.3V ───────────────────┼──┼───────┐
    │  │  │    │    │  │                         │  │       │
    │  │  │    │    │  │ 5V ─────────────────────┼──┼────┐  │
    │  │  │    │    │  │                         │  │    │  │
    │  │  │    │    │  │ GND ────────────────────┼──┼──┐ │  │
    │  │  │    │    │  │                         │  │  │ │  │
    │  │  │    │    │  └─────────────────────────┘  │  │ │  │
    │  │  │    │    └───────────────────────────────┘  │ │  │
    │  │  │    │                                       │ │  │
    │  │  │    │    ┌──────────────────────────┐      │ │  │
    │  │  │    │    │     MAX98357A (Speaker)  │      │ │  │
    │  │  │    │    │                          │      │ │  │
    ├──┼──┼────┼────┤ BCLK                     │      │ │  │
    │  │  │    │    │                          │      │ │  │
    │  └──┼────┼────┤ LRC (LRCLK)              │      │ │  │
    │     │    │    │                          │      │ │  │
    │     │    └────┤ DIN                      │      │ │  │
    │     │         │                          │      │ │  │
    │     │         │ VIN ─────────────────────┼──────┘ │  │
    │     │         │                          │        │  │
    │     │         │ GND ─────────────────────┼────────┘  │
    │     │         │                          │           │
    │     │         │ SD (Shutdown) ───────────┼───> [Optional: to GPIO for control]
    │     │         │                          │           │
    │     │         │ GAIN ─<Configure>────────┼───> GND/Float/VCC
    │     │         │                          │           │
    │     │         └──[Speaker 4-8Ω]──────────┘           │
    │     │                                                 │
    │     │         ┌──────────────────────────┐           │
    │     │         │     INMP441 (Microfoon)  │           │
    │     │         │                          │           │
    └─────┼─────────┤ SCK                      │           │
          │         │                          │           │
          └─────────┤ WS                       │           │
                    │                          │           │
                    │ SD ──────────────────────┤           │
                    │                          │           │
                    │ L/R ─────────────────────┼───> GND   │
                    │                          │           │
                    │ VDD ─────────────────────┼───────────┘
                    │                          │
                    │ GND ─────────────────────┼───────────┐
                    │                          │           │
                    └──────────────────────────┘           │
                                                            │
                                                          [GND]


══════════════════════════════════════════════════════════════════

PINOUT SAMENVATTING:

Gedeelde Signalen (voor BEIDE INMP441 en MAX98357A):
├─ GPIO 4 → I2S BCLK (Bit Clock) ───────┬─→ INMP441 SCK
│                                        └─→ MAX98357A BCLK
│
└─ GPIO 5 → I2S WS/LRCLK (Word Select) ─┬─→ INMP441 WS
                                         └─→ MAX98357A LRC

Aparte Data Signalen:
├─ GPIO 6 → I2S SD_IN ─────────────────────→ INMP441 SD (Input)
└─ GPIO 7 → I2S SD_OUT ────────────────────→ MAX98357A DIN (Output)

Control:
└─ GPIO 0 → Button → GND (Push-to-Talk)

Power:
├─ 3.3V → INMP441 VDD
├─ 5V   → MAX98357A VIN
└─ GND  → Common Ground (alle componenten!)

══════════════════════════════════════════════════════════════════

VOORDELEN VAN DEZE CONFIGURATIE:
✓ Bespaart 2 GPIO pins (van 7 naar 5 pins totaal)
✓ Geen extra componenten nodig
✓ Volledige hardware ondersteuning in ESP32-S3
✓ Perfecte sync tussen microfoon en speaker timing
✓ Ideaal voor half-duplex operatie (push-to-talk)

BELANGRIJKE NOTITIES:
! Alle GND moeten met elkaar verbonden zijn (common ground)
! De 3.3V voor INMP441 moet stabiel zijn
! MAX98357A heeft 5V nodig voor voldoende volume
! Button gebruikt interne pull-up, dus verbind alleen naar GND
! Clock signalen (BCLK, WS) kunnen gedeeld omdat ze unidirectioneel zijn
! Data signalen (SD_IN, SD_OUT) MOETEN apart blijven!

MAX98357A GAIN CONFIGURATIE:
├─ GAIN → GND:   9dB versterking (stil)
├─ GAIN → Float: 12dB versterking (normaal, aanbevolen)
└─ GAIN → VCC:   15dB versterking (luid)
```

## PCB Layout Tips

Als je een PCB wilt maken:

1. **Houd analog en digital gescheiden**: 
   - INMP441 zo dicht mogelijk bij ESP32
   - MAX98357A verder weg met eigen power filtering

2. **Decoupling capacitors**:
   - 0.1µF keramische cap bij elke VCC pin
   - 10µF elektrolytische cap bij MAX98357A VIN

3. **Ground plane**:
   - Gebruik een solid ground plane onder audio componenten
   - Verbind analog en digital ground op één punt (star ground)

4. **Trace routing**:
   - Houd I2S traces kort en parallel
   - Vermijd kruisingen met power traces
   - Gebruik 50Ω impedantie voor I2S signalen (voor PCB)

5. **Speaker output**:
   - Gebruik dikke traces (minimaal 20 mil/0.5mm)
   - Twisted pair of differential routing naar speaker
