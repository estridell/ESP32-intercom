# Project Specification (Maintainer Reality Check)

## 1) Purpose and Scope

- Provide a **single-ESP32 Classic Bluetooth audio endpoint** for phone use-cases:
  - A2DP sink for music playback.
  - HFP client for call audio (mic up-link + speaker down-link).
  - AVRCP absolute volume handshake/control.
- Target hardware is **ESP32-WROOM-class** boards with Bluetooth Classic support.
- Scope is currently one-device-at-a-time pairing/use (iPhone/PC), not mesh/intercom networking between ESP32 units.

Out of scope in current codebase:
- No DSP stack (AEC/NS/AGC), no call controls UI, no battery/power management logic.
- No automated tests, CI, or production release packaging.

## 2) Current Code Variants and Canonical Recommendation

| File | Intent | Status | Recommendation |
|---|---|---|---|
| `esp_intercom.ino` | Main dual-profile build using external I2S audio output | Most coherent baseline; labeled 2.9.0 stable in-file | **Canonical** |
| `code3.ino` | Same flow but routes output through ESP32 internal DAC (GPIO25) | Functional variant, lower-fidelity analog path, hardware-specific | Keep as optional hardware variant |
| `Barebones_CODE.ino` | Minimal BT bring-up + CoD + HFP init/discoverability | Diagnostic/bootstrap only; no media/call audio pipeline | Keep for troubleshooting only |

Canonical recommendation:
- Use `esp_intercom.ino` as the source of truth for v1 stabilization.
- Treat `code3.ino` as a feature branch variant until behavior is parity-tested.

## 3) Hardware Assumptions and Pin Maps per Variant

Common assumptions:
- ESP32 with Bluetooth Classic (not BLE-only parts).
- One I2S digital mic input path and one speaker output path.

| Variant | BCLK | WS/LRCK | Data Out | Data In (Mic) | Audio Out Path |
|---|---:|---:|---:|---:|---|
| `esp_intercom.ino` | GPIO26 | GPIO25 | GPIO22 | GPIO35 | External I2S DAC/amp (example: MAX98357A) |
| `code3.ino` | GPIO32 | GPIO33 | N/A (`I2S_PIN_NO_CHANGE`) | GPIO35 | ESP32 internal DAC on GPIO25 (right channel) |
| `Barebones_CODE.ino` | N/A | N/A | N/A | N/A | No audio pipeline implemented |

Notes:
- `code3.ino` requires hardware compatible with internal DAC mode and an external analog amplifier stage.
- `Barebones_CODE.ino` only validates BT visibility/profile identity behavior.

## 4) Bluetooth Profiles and Runtime State Machine

Profiles used:
- A2DP Sink (`esp_a2d_*`) for media audio.
- HFP Client (`esp_hf_client_*`) for call audio.
- AVRCP CT/TG (`esp_avrc_*`) for absolute volume sync and notifications.

Boot/init sequence in main variants:
1. Serial + NVS init.
2. I2S driver/pins configured.
3. BT controller + Bluedroid started/enabled.
4. Device name set to `esp_intercom`; CoD set to Audio/Video Hands-Free.
5. AVRCP CT/TG init.
6. A2DP init + callback registration.
7. HFP client init + audio callback registration.
8. Discoverable/connectable mode enabled.

Runtime state model (implicit in flags/logging):

| State | Enter Condition | Exit Condition |
|---|---|---|
| `STANDBY_IDLE` | Default after setup, or streaming/call inactive | A2DP stream starts or HFP audio starts |
| `STREAMING_MEDIA` | A2DP audio state active (`isMediaStreaming=true`) | A2DP pause/stop or HFP call audio starts |
| `IN_VOICE_CALL` | HFP audio state active (`isCallActive=true`) | HFP audio inactive |

Clock/profile transition behavior:
- Call active: I2S clock switched to **16 kHz mono**.
- Call inactive: I2S clock switched back to **44.1 kHz stereo**.
- Heartbeat log every 5s reports state with `isCallActive` taking priority over `isMediaStreaming`.

## 5) Audio Pipeline for Music and Calls

Music (A2DP) path:
1. PCM arrives in `a2dp_sink_data_cb`.
2. Samples scaled using squared AVRCP volume curve (`volume^2 / 16129`).
3. Scaled PCM written to I2S output.

Call downlink (phone -> headset):
1. HFP incoming callback receives audio frames.
2. Frames written directly to I2S TX.

Call uplink (mic -> phone):
1. HFP outgoing callback reads from I2S RX.
2. Captured frames returned to HFP stack.

Practical behavior:
- Single shared I2S peripheral for both media and call audio.
- No dedicated mixer, no software resampler, no echo cancellation.

## 6) Build/Run Environment Constraints

- Targeted explicitly at **Arduino ESP32 core 2.0.17**.
- Uses legacy ESP-IDF APIs exposed by that core (`driver/i2s.h`, `esp_hf_client_api.h`, etc.).
- Requires board profile with Bluetooth Classic support and enough flash; README suggests large app partition.
- Serial monitor expected at **115200 baud**.

Library reality vs README:
- Main sketches do **not** use `ESP32-A2DP` or `Arduino-Audio-Tools`.
- `Barebones_CODE.ino` uses `BluetoothSerial.h` (from Arduino ESP32 core).

## 7) Gaps / Risks / Technical Debt

- Event handling relies on **magic numeric constants** (`event == 0/1/2`, `state == 1/2`) rather than enum symbols.
- Minimal runtime error checking after many BT/profile API calls.
- `transitionStartTime` is declared but unused; README claims latency instrumentation not present in code.
- Blocking `i2s_read`/`i2s_write` inside BT callbacks may increase underrun/latency risk under load.
- No explicit reconnection policy, pairing management UX, or call-control features.
- High duplication between `esp_intercom.ino` and `code3.ino` increases drift risk.
- No test harness or compatibility matrix (board variants, phones, core versions).

## 8) Concrete Roadmap (Prioritized)

| Priority | Milestone | Concrete Deliverables |
|---|---|---|
| P0 | Canonical stabilization | Freeze `esp_intercom.ino` as baseline; replace magic event/state numbers with named enums; add return-code checks and fatal/log paths; remove dead vars/log claims |
| P0 | Variant hygiene | Refactor shared logic into common module; isolate output backend differences (external I2S DAC vs internal DAC) behind compile-time switch |
| P1 | Audio robustness | Move audio I/O to buffered task model; add underrun/overrun counters; define deterministic behavior during profile handover |
| P1 | Observability | Implement real latency metrics that match README claims; structured log tags for state, errors, and transitions |
| P2 | Compatibility validation | Test matrix: iPhone call/music transitions, reconnect after reboot, long-run playback, multiple ESP32 devkits |
| P2 | Release readiness | Versioned changelog, known-limitations doc, reproducible build settings (board/core/partition) |

## 9) Acceptance Criteria for a Stable v1

A v1 is accepted when all items pass on canonical hardware (ESP32-WROOM + external I2S DAC + digital mic):

1. Builds cleanly on Arduino ESP32 core 2.0.17 with documented board/partition settings.
2. Device is discoverable, pairable, and reconnects after power cycle without reflashing.
3. A2DP playback runs 30 minutes without crash/reset; AVRCP absolute volume changes are reflected in output level.
4. HFP call path is duplex (remote hears mic, local hears remote) and transitions do not require reboot.
5. Entering/leaving calls correctly switches sample-rate/channel mode (16 kHz mono <-> 44.1 kHz stereo).
6. Serial logs report unambiguous state transitions and no recurring fatal errors.
7. `code3.ino` status is explicitly documented as either "supported variant" (with tested limits) or "experimental".
