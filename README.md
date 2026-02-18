# ESP32 Intercom

Single-sketch ESP32 firmware for Bluetooth A2DP sink (music) and HFP client
(calls), with runtime I2S clock switching between media and call audio.

## Project Status

The codebase was unified into one production sketch:

- `esp_intercom.ino` is the only file intended to be flashed.
- The repository now only keeps source code, `README.md`, and `LICENSE`.

## Repository Layout

- `esp_intercom.ino`: production firmware.
- `README.md`: project documentation.
- `LICENSE`: MIT license text.

## Features

- A2DP sink playback over I2S (44.1 kHz stereo).
- HFP full-duplex call path over I2S:
  - narrowband CVSD: 8 kHz mono
  - wideband mSBC: 16 kHz mono
- Event-driven mode transitions with serial logs for status and latency.
- Non-blocking I2S callback path to reduce Bluetooth task starvation.
- NVS initialization for Bluetooth pairing persistence.
- Startup error handling with explicit failure logs.

## Hardware

Recommended modules:

- ESP32-WROOM-32
- MAX98357A (I2S DAC/amp output)
- INMP441 (I2S microphone input)

Pin map:

- `BCLK`: GPIO 26
- `WS/LRCK`: GPIO 25
- `DATA_OUT` (to DAC): GPIO 22
- `DATA_IN` (from mic): GPIO 35

## Software Requirements

- Arduino ESP32 Core `2.0.17`
- Partition scheme: `Huge APP` (or equivalent)

No third-party audio libraries are required by the current sketch.

## Flashing

1. Open this folder as an Arduino sketch.
2. Ensure the board is an ESP32 target compatible with Classic Bluetooth.
3. Build and upload `esp_intercom.ino`.
4. Open serial monitor at `115200`.
5. Pair with device name `esp_intercom`.

## Serial Log Signals

- `[STATUS]` profile connection state changes
- `[EVENT]` media/call mode transitions
- `[LATENCY]` transition timing
- `[INFO]` heartbeat with active mode and callback drop/pad counters
- `[ERROR]` initialization/runtime API failures
- `[FATAL]` unrecoverable startup failure

## Known Limits

- This project has not been validated against all phones/headsets.
- Without hardware-in-loop tests, latency and duplex quality are unverified.
- Discoverability is currently left enabled continuously.

## License

MIT. See `LICENSE`.
