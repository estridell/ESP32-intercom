# ESP32 Bluetooth Intercom (Breadboard v1 Guide)

## Project Overview
This project is a single-endpoint Bluetooth intercom on an ESP32-WROOM-32 DevKit. It is designed to run on the fixed BOM in `PROJECT_SPEC.md`: stream music (A2DP), handle phone-call audio when HFP support is available in the toolchain, and use a breadboard-first wiring approach before any PCB design.

## TODO

### Hardware Assembly
- [ ] Place ESP32, PAM8403 amplifier module, microphone module, and at least 1 speaker on breadboard(s)
- [ ] Wire shared power rails and verify no short between 3V3/5V/GND
- [ ] Wire ESP32 DAC output (`GPIO25`) to PAM8403 analog input
- [ ] Wire microphone analog output to `GPIO34`
- [ ] Wire bond-clear tactile button to `GPIO33` (active LOW)
- [ ] Wire dual-color CC LED with one resistor per color channel (`GPIO18` red, `GPIO19` green)
- [ ] Verify PAM8403 output safety: no speaker terminal to GND, no tied L/R outputs
- [ ] Verify LiPo/USB power routing behavior before long tests

### Firmware and Bring-Up
- [ ] Compile and flash `ESP32-intercom.ino`
- [ ] Confirm boot logs at `115200` show no fatal startup errors
- [ ] Confirm boot log reports output path status (`analog-ok` or blocked)
- [ ] Confirm boot log reports HFP toolchain status (`ok` or blocked)

### Functional Testing
- [ ] Pair from phone Bluetooth settings
- [ ] Verify music playback and volume control
- [ ] Verify call mode behavior (if HFP supported)
- [ ] Verify disconnect/reconnect behavior without reboot
- [ ] Run 30-minute music stability test
- [ ] Run 15-minute call stability test (or mark blocked with evidence)
- [ ] Run at least 20 music<->call handovers

## BUILD (Breadboard-First)

### 1. Parts You Will Use
- ESP32-WROOM-32 DevKit LiPo (main controller)
- Electret microphone module with amplifier/AGC
- PAM8403 analog amplifier module
- 1-2 x 8 ohm / 0.5W speakers (start with 1)
- 1 x tactile switch (bond-clear)
- 1 x red/green 3-pin common-cathode LED
- 2 x current-limiting resistors for LED channels (typical: 220 ohm to 1 kohm)
- Breadboard(s) + jumper wires
- Optional: 1 slide switch for power/input experiments

### 2. Firmware Pinout (Reference)
- `GPIO25` -> speaker analog signal output (DAC1) to PAM8403 input
- `GPIO34` <- microphone analog output
- `GPIO33` <- bond-clear button to GND (INPUT_PULLUP in firmware)
- `GPIO18` -> LED red channel (through resistor)
- `GPIO19` -> LED green channel (through resistor)
- `3V3` -> microphone module VCC
- `GND` -> common ground for ESP32, mic module, and PAM8403 input ground

### 3. Text Wiring Diagram (Netlist Style)
- ESP32 `GPIO25` -> PAM8403 `INL` (or `INR`)
- ESP32 `GND` -> PAM8403 signal `GND`
- ESP32 `GPIO34` <- microphone module `OUT`
- ESP32 `3V3` -> microphone module `VCC`
- ESP32 `GND` -> microphone module `GND`
- ESP32 `GPIO33` -> one side of tactile switch
- ESP32 `GND` -> opposite side of tactile switch
- ESP32 `GPIO18` -> resistor -> LED red anode
- ESP32 `GPIO19` -> resistor -> LED green anode
- ESP32 `GND` -> LED common cathode
- PAM8403 `L+` and `L-` -> speaker #1 terminals
- Optional second speaker: PAM8403 `R+` and `R-` -> speaker #2 terminals

### 4. Breadboard Assembly Steps
1. Put ESP32 across the breadboard center gap so each pin row is isolated and accessible.
2. Reserve one power rail for `GND` and one for logic power (`3V3` for mic/logic).
3. Place microphone module on the breadboard edge. Wire `VCC`, `GND`, and `OUT` per pinout above.
4. Place tactile button so it bridges the center gap. Use diagonal continuity pair; wire to `GPIO33` and `GND`.
5. Place dual-color common-cathode LED. Connect cathode to `GND`. Add one resistor per color channel, then wire to `GPIO18` and `GPIO19`.
6. Place PAM8403 module. Connect signal input ground to ESP32 ground and connect ESP32 `GPIO25` to one analog input channel (`INL` recommended).
7. Connect one speaker to one PAM8403 output pair (`L+`/`L-`). If using two speakers, keep one speaker per channel only.
8. Power check before enabling audio: confirm no PAM8403 speaker negative terminal (`L-`/`R-`) is tied to system ground.
9. Connect USB for programming and serial logs. Add LiPo only after confirming board power-path behavior.

### 5. Critical Safety and Wiring Notes
- PAM8403 output is BTL/floating. Never connect `L-` or `R-` to GND.
- Do not tie left and right outputs together.
- Keep startup volume low; firmware clamps max volume but hardware checks are still required.
- For 8 ohm / 0.5W speakers, avoid sustained high output. Start short and quiet, then increase gradually.
- If VBAT sensing is not wired, low-battery protection remains blocked by design in this BOM-first build.

### 6. Build/Flash How-To

#### Arduino IDE
1. Install board package `esp32 by Espressif Systems` (2.x series).
2. Select board profile (`ESP32 Dev Module` or matching devkit profile).
3. Open `ESP32-intercom.ino`.
4. Select serial port and upload.
5. Open Serial Monitor at `115200`.

#### arduino-cli
```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 .
```

## TESTING

### 1. Power-On and Log Verification
1. Boot device and monitor serial at `115200`.
2. Confirm startup reaches idle state and device becomes discoverable.
3. Confirm logs include HFP status (`ok` or `BLOCKED HFP-TOOLCHAIN`).
4. Confirm logs include audio output path status for PAM8403 analog flow.

### 2. Pairing and Music
1. Pair from phone Bluetooth settings.
2. Start music playback and verify audible speaker output.
3. Change phone volume in steps and confirm monotonic loudness changes.
4. Pause/stop music and verify return to silent idle behavior.

### 3. Call Behavior (If HFP Supported)
1. Place/answer a call while connected.
2. Verify remote voice is heard on speaker.
3. Verify your mic audio reaches the phone peer.
4. End call and confirm transition back to music (if music still active) or idle.

### 4. Stability and Recovery
1. Music soak: run 30 minutes continuously; no crash/reset/stuck state.
2. Call soak: run 15 minutes full-duplex (or record blocked status with logs if HFP unavailable).
3. Handover run: perform at least 20 music<->call transitions without reboot.
4. Disconnect test: force source disconnect in music and call; verify return to idle and reconnect-ready state.
5. Reboot reconnect: run 5 reboot attempts and record reconnect success rate.

### 5. Test Logging
Use `TEST_LOG_TEMPLATE.md` for each run and capture:
- Date/time and source phone type
- Wiring/power preflight notes
- Pass/fail against ACC items
- Any blocked status (HFP toolchain or VBAT sensing)

## Breadboard-First Exit Criteria (Before PCB)
- Wiring is repeatable on breadboard with no safety violations
- Pairing, music, and reconnect behavior verified on hardware
- Call behavior verified or explicitly blocked with evidence
- Long-session tests and handover counts are recorded in test logs
- No unresolved fatal startup/runtime errors in the mixed-use run
