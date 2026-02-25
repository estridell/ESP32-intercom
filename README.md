# ESP32 Bluetooth Intercom (v1)

Single-endpoint Bluetooth intercom firmware for ESP32-WROOM-32 DevKit hardware.  
v1 target: phone/computer music sink + hands-free call audio with deterministic mode arbitration (`IDLE`, `MUSIC`, `CALL`) and serial diagnostics.

## Hardware assumptions (BOM-locked)

This repo targets only the purchased BOM from `PROJECT_SPEC.md`:

- ESP32-WROOM-32 DevKit LiPo (main controller)
- Electret microphone module with amplifier + AGC (analog mic input)
- Digital 3W amplifier module (I2S speaker output stage)
- One or two 8 ohm speakers (v1 supports one as minimum)
- Slide switches, tactile buttons, and dual-color common-cathode LEDs for simple controls/indicators
- LiPo battery per BOM

No extra sensors, radios, displays, or co-processors are assumed.

## Default wiring (practical pin map)

| Function | ESP32 pin | Module pin | Notes |
|---|---:|---|---|
| I2S BCLK | GPIO26 | AMP BCLK/SCK | Digital amp clock |
| I2S LRCLK/WS | GPIO25 | AMP LRCLK/WS | Left-right clock |
| I2S DATA OUT | GPIO22 | AMP DIN | Audio stream to amp |
| I2S GND | GND | AMP GND | Common ground |
| I2S power | 5V (or board rail per amp module) | AMP VIN | Match amp module voltage spec |
| Mic analog input | GPIO34 (ADC1_CH6) | MIC OUT | Electret module analog output |
| Mic power | 3V3 + GND | MIC VCC/GND | Shared ground required |
| Bond-clear button | GPIO33 -> GND | Tactile switch | Active LOW; hold at boot to clear bonds |
| Status LED red anode | GPIO18 (via resistor) | LED R | Common cathode to GND |
| Status LED green anode | GPIO19 (via resistor) | LED G | Common cathode to GND |

## Firmware behavior summary

- Mode arbitration is explicit and deterministic in `state_machine.h`:
  - If source is disconnected: `IDLE`
  - Else if call is active: `CALL` (always priority)
  - Else if music is active: `MUSIC`
  - Else: `IDLE`
- Required handover behavior:
  - Incoming call during music: `MUSIC -> CALL`
  - Call end + media still active: `CALL -> MUSIC`
  - Call end + no media: `CALL -> IDLE`
- Startup safety:
  - Starts muted and ramps gain to avoid pop/transient artifacts
  - Output volume is clamped by `kSafeMaxVolumeAbs`
- Connectivity policy:
  - Discoverable/connectable on startup
  - Re-applies discoverable/connectable after disconnect/reconnect failures
  - Uses default bonding persistence
  - Optional deterministic bond reset: hold GPIO33 LOW for 3s during boot to clear all bonds
  - Deterministic single-source policy: newest authenticated bond is kept, older bonds removed
- Logging:
  - Startup
  - Connect/disconnect
  - Mode transitions with direction strings
  - Recoverable warnings
  - Stable `FATAL-xxx` startup failures
  - Heartbeat every 10s

## Build + flash

### Arduino IDE (ESP32 core 2.x)

1. Install board package: `esp32 by Espressif Systems` (2.x line).
2. Select board: `ESP32 Dev Module` (or your specific WROOM-32 devkit profile).
3. Open `esp32_intercom.ino`.
4. Select serial port, then **Upload**.
5. Open Serial Monitor at `115200`.

### arduino-cli examples

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17

# Compile from repo root
arduino-cli compile --fqbn esp32:esp32:esp32 .

# Flash (replace port)
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 .
```

Host-side arbitration test:

```bash
./scripts/run_host_tests.sh
```

## Execution artifacts

- [BUILD_PLAN.md](BUILD_PLAN.md)
- [CHECKLIST.md](CHECKLIST.md)
- [TEST_LOG_TEMPLATE.md](TEST_LOG_TEMPLATE.md)

## Manual validation procedure (acceptance-oriented)

Use this sequence and record results in `CHECKLIST.md`:

1. `ACC-001`, `UX-001`, `FR-PAIR-*`: pair from phone Bluetooth settings with no custom app.
2. `ACC-002`, `FR-MUSIC-*`, `NFR-REL-001`: stream music continuously for 30 minutes.
3. `ACC-003`, `FR-CALL-*`, `NFR-REL-002`: run a 15-minute call and verify two-way audio.
4. `ACC-004`, `FR-MODE-*`, `NFR-REL-003`: perform at least 20 music/call handovers.
5. `ACC-005`, `EC-008`, `TM-002`: power-cycle reconnect tests (minimum 5 attempts summary).
6. `ACC-006`, `FR-REC-*`, `EC-004/005`: disconnect during music/call and verify `IDLE` + reconnect-ready behavior.
7. `ACC-007`, `FR-OBS-*`: confirm logs for startup, transitions, disconnects, and heartbeat.
8. `ACC-008`, `NFR-RES-*`, `TM-003`: 2-hour mixed-use run with reconnects.
9. `ACC-009`, `EC-*`: execute all edge-case scenarios and mark pass/fail.
10. `ACC-010`, `HW-*`, `TM-005`: confirm test run used only approved BOM parts.
