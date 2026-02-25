# ESP32 Bluetooth Intercom (v1)

Single-endpoint Bluetooth intercom firmware for ESP32-WROOM-32 DevKit hardware.

## BOM Reality / Wiring

This repo is locked to the purchased Electrokit BOM in `PROJECT_SPEC.md`.

- Amplifier is **PAM8403 analog input** (SKU `41018541`), not an I2S digital amp.
- Firmware output path targets ESP32 DAC (`GPIO25`) for analog feed into PAM8403 input.
- Microphone path uses MAX9814 module (`41016669`) analog output to `GPIO34`.
- Speaker parts are `8Ω 0.5W`; firmware clamps output volume conservatively and starts muted.
- PAM8403 output stage is BTL/floating: do **not** tie speaker `-` to GND and do **not** tie L/R outputs together.

Practical wiring assumptions used by firmware/docs:

- `GPIO25 (DAC1)` -> PAM8403 `IN` (left or right channel input), shared ground with ESP32.
- MAX9814 `OUT` -> `GPIO34`, MAX9814 powered from `3V3` + `GND`.
- Bond-clear button: tactile switch to `GPIO33` and `GND` (active LOW, hold during boot).
- Slide switch parts are SPDT center-common; map center pin explicitly in harness.
- 6x6 tactile switches have paired legs per side; validate continuity before wiring.
- Dual-color CC LED requires one resistor per color channel (`R` and `G`), cathode to GND.

Battery/jumper caveats:

- Validate board power routing/jumper behavior before acceptance tests.
- Avoid mismatched rails/back-power conditions when USB and LiPo are connected.
- Low-battery protection requires VBAT ADC wiring; without it, firmware logs blocked status.

## Known Limits

- HFP call support is toolchain-dependent: if `esp_hf_client_api.h` is missing, firmware logs `BLOCKED HFP-TOOLCHAIN` and call path stays disabled.
- Analog DAC output is a practical BOM-aligned fallback, not high-fidelity codec output.
- Low-battery cutoff behavior is blocked until VBAT sensing pin/divider is wired and calibrated.
- All hardware acceptance items are intentionally `PENDING-DEVICE` until bench validation is recorded.

## Build + Flash

### Arduino IDE (ESP32 core 2.x)

1. Install board package: `esp32 by Espressif Systems` (2.x line).
2. Select board: `ESP32 Dev Module` (or your exact WROOM-32 devkit profile).
3. Open `ESP32-intercom.ino`.
4. Select serial port, then upload.
5. Open Serial Monitor at `115200`.

### arduino-cli examples

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17

arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 .
```

Host-side arbitration test:

```bash
./scripts/run_host_tests.sh
```

## Execution Artifacts

- [PROJECT_SPEC.md](PROJECT_SPEC.md)
- [SPEC_CONFORMANCE.md](SPEC_CONFORMANCE.md)
- [CHECKLIST.md](CHECKLIST.md)
- [BUILD_PLAN.md](BUILD_PLAN.md)
- [TEST_LOG_TEMPLATE.md](TEST_LOG_TEMPLATE.md)
