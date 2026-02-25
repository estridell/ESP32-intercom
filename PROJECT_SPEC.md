# Product Specification: ESP32 Bluetooth Intercom Endpoint

## 1. Document Purpose

This document defines **what the product must do** for a rebuild from scratch.

- It is implementation-agnostic and does not prescribe code structure, APIs, libraries, or firmware architecture.
- It is normative: requirements using **MUST / MUST NOT / SHOULD** are binding.
- It targets a single-device Bluetooth intercom endpoint built on ESP32-class hardware with Bluetooth Classic support.

## 2. Product Intent and Scope

### 2.1 Product Intent

The product is a Bluetooth audio endpoint that allows a user to:

- stream music from a phone/computer to the device speaker path, and
- conduct two-way phone calls through the device (remote voice playback + local microphone capture).

Primary scenario: helmet/hands-free intercom endpoint paired to one smartphone at a time.

### 2.2 In Scope

- Bluetooth Classic discoverability, pairing, bonding, and reconnect behavior.
- Music playback as a Bluetooth media sink.
- Full-duplex call audio as a Bluetooth hands-free endpoint.
- Volume synchronization with source device media volume commands.
- Deterministic behavior when switching between music and calls.
- Runtime status and error observability via serial logs.

### 2.3 Out of Scope (v1)

- Mesh or peer-to-peer intercom networking between multiple embedded endpoints.
- On-device UI beyond optional minimal status indicators.
- Advanced voice DSP features (echo cancellation, noise suppression, automatic gain control).
- Battery charging/power management policy beyond basic boot/runtime behavior.
- OTA update, cloud services, or mobile companion apps.

## 3. Definitions

- `Source Device`: phone or computer that initiates Bluetooth media/call sessions.
- `Music Session`: media streaming session to the endpoint.
- `Call Session`: active duplex voice session to/from the endpoint.
- `Active Audio Mode`: one of `IDLE`, `MUSIC`, `CALL`.
- `Handover`: transition from one active audio mode to another.

## 4. System Context and Constraints

### 4.1 Platform Constraints

- The product MUST run on ESP32-class hardware that supports Bluetooth Classic audio profiles.
- The product MUST support:
  - one speaker/audio output path, and
  - one microphone/audio input path.
- The product MUST operate as a single endpoint, paired to one source device at a time in v1.

### 4.2 User-Visible Identity Constraints

- The product MUST expose a stable Bluetooth device name configurable at build time.
- The product SHOULD expose class-of-device metadata consistent with hands-free audio equipment.
- The product MUST remain pairable after reboot without reflashing.

### 4.3 Safety and Audio Behavior Constraints

- The product MUST NOT emit high-amplitude transient artifacts during startup, reconnect, or mode handover.
- The product MUST clamp output gain to a safe upper bound to avoid clipping and sudden spikes.
- The product MUST enforce a conservative speaker safety limit suitable for the available 8Ω 0.5W speakers, even though the amplifier module can deliver higher peak power.
- For 8Ω / 0.5W speakers, continuous electrical output MUST be kept at or below ~2.0 Vrms equivalent per channel (or stricter).
- The product MUST include output limiting behavior that prevents sustained overdrive in music/call scenarios.
- The product SHOULD include a protective high-pass behavior (~300–400 Hz region) for tiny speaker load protection.

### 4.4 Fixed Hardware BOM Constraint (Project Scope Lock)

This gymnasiearbete build is constrained to the exact purchased hardware set below.

- The v1 implementation MUST target only the listed hardware SKUs and quantities.
- The v1 implementation MUST NOT assume additional hardware modules beyond this list.
- If a requirement cannot be met with this BOM, the requirement MUST be marked blocked and explicitly documented.

Approved BOM (Electrokit invoice F 2307333):

| SKU | Part | Qty | Constraint |
|---|---|---:|---|
| 41017634 | ESP32-WROOM-32 DevKit LiPo | 1 | Required main controller board |
| 41019154 | LiPo battery 3.7V 1200mAh (JST-PH) | 1 | Required portable power source |
| 41016669 | Electret microphone with amplifier + AGC | 1 | Required microphone input module |
| 41018541 | PAM8403 analog audio amplifier module (3W class) | 1 | Required speaker drive stage; input is analog line-level style (not I2S digital audio) |
| 41023812 | Speaker 8Ω Ø50mm 0.5W | 2 | Available speaker units for output path |
| 41004098 | Mini slide switch 1P ON-ON (2.54mm PCB) | 2 | Available hardware switch inputs |
| 41001412 | Tactile switch PCB 6x6x5mm | 10 | Available button inputs |
| 40307082 | LED 3mm red/green 3-pin diffuse CC | 2 | Available status indicator LEDs |

BOM scope rules:

- `HW-001` Product MUST be buildable and testable with the approved BOM only.
- `HW-002` Product MUST NOT require additional sensors, radios, displays, or external co-processors for v1 acceptance.
- `HW-003` Product SHOULD support operation with one connected speaker unit; using both available speakers is optional unless explicitly required by test scenarios.
- `HW-004` Any optional controls/indicators in firmware MUST be limited to the available slide switches, tactile switches, and dual-color LEDs listed above.
- `HW-005` Audio output implementation MUST be compatible with PAM8403 analog input using approved BOM parts only; v1 MUST NOT require an external I2S DAC module.
- `HW-006` If hands-free profile (HFP) headers/libraries are unavailable in the selected ESP32 toolchain, call features MUST be marked blocked at boot and in acceptance artifacts (no implicit pass).
- `HW-007` Wiring assumptions are locked for validation: slide switch is SPDT center-common, tactile switch uses diagonal pin-pair continuity, and common-cathode dual-color LED channels each use their own current-limiting resistor.
- `HW-010` PAM8403 output-side wiring safety is mandatory: do not connect L-/R- speaker terminals to system GND, do not tie L/R outputs together, and use one speaker load per channel.
- `HW-008` Battery/power caveat MUST be documented and tested: board jumper/power routing must prevent back-power or rail mismatch between USB, LiPo, and amplifier supply.
- `HW-009` Low-battery behavior is required: when battery sensing wiring is present, firmware MUST reduce risk by forcing safe output behavior at/below threshold; when sensing is not wired in BOM-only build, status MUST be explicitly blocked/pending.

## 5. Functional Requirements

### 5.1 Boot, Discoverability, and Pairing

- [ ] `FR-BOOT-001` On boot, the device MUST initialize into `IDLE` mode.
- [ ] `FR-BOOT-002` Within 10 seconds of boot, the device MUST become discoverable and connectable unless explicitly configured otherwise.
- [ ] `FR-PAIR-001` The device MUST support initial pairing and bonding with at least one mainstream smartphone platform.
- [ ] `FR-PAIR-002` Bonding data MUST persist across power cycles.
- [ ] `FR-PAIR-003` If a bonded source device is available, the endpoint SHOULD reconnect automatically after reboot.
- [ ] `FR-PAIR-004` If auto-reconnect fails, the device MUST remain discoverable/connectable for manual reconnection.

### 5.2 Music Playback Behavior

- [ ] `FR-MUSIC-001` The device MUST function as a Bluetooth media sink for stereo music playback.
- [ ] `FR-MUSIC-002` During active music streaming, decoded audio MUST be rendered continuously to the speaker path.
- [ ] `FR-MUSIC-003` Media volume commands from the source device MUST change perceived output loudness monotonically.
- [ ] `FR-MUSIC-004` If media is paused/stopped by the source device, output MUST return to silent `IDLE` behavior without requiring reboot.

### 5.3 Call Audio Behavior

- [ ] `FR-CALL-001` The device MUST function as a hands-free endpoint supporting full-duplex call audio.
- [ ] `FR-CALL-002` During an active call, incoming remote voice MUST be audible on the speaker path.
- [ ] `FR-CALL-003` During an active call, local microphone capture MUST be transmitted to the source device.
- [ ] `FR-CALL-004` Entering or leaving a call MUST NOT require reboot or user re-pairing.

### 5.4 Mode Arbitration and Priority

- [ ] `FR-MODE-001` Active mode MUST be one of exactly: `IDLE`, `MUSIC`, `CALL`.
- [ ] `FR-MODE-002` If both music and call contexts are signaled, `CALL` MUST take priority for speaker output.
- [ ] `FR-MODE-003` On call end, the endpoint MUST return to `MUSIC` if music is still active; otherwise to `IDLE`.
- [ ] `FR-MODE-004` Handover between `MUSIC` and `CALL` MUST be automatic and deterministic.

### 5.5 Audio Format Adaptation

- [ ] `FR-FMT-001` The endpoint MUST support music mode output compatible with 44.1 kHz stereo source streams.
- [ ] `FR-FMT-002` The endpoint MUST support call mode audio compatible with 16 kHz mono voice sessions.
- [ ] `FR-FMT-003` Audio format changes required by mode handover MUST occur automatically and without manual intervention.
- [ ] `FR-FMT-004` After call exit, audio format MUST revert correctly for music playback.

### 5.6 Connection Loss and Recovery

- [ ] `FR-REC-001` Unexpected source disconnect MUST transition the endpoint to `IDLE` within 3 seconds.
- [ ] `FR-REC-002` After disconnect, the endpoint MUST remain available for reconnection without reboot.
- [ ] `FR-REC-003` If reconnect attempts fail repeatedly, the endpoint MUST continue advertising/connectable behavior and log failure reason categories.

### 5.7 Observability and Diagnostics

- [ ] `FR-OBS-001` The device MUST emit runtime logs over serial at startup, connect/disconnect, and mode transitions.
- [ ] `FR-OBS-002` Logs MUST identify current active mode and transition direction (`IDLE->MUSIC`, `MUSIC->CALL`, etc.).
- [ ] `FR-OBS-003` Recoverable errors MUST be logged without halting runtime operation.
- [ ] `FR-OBS-004` Fatal startup failures MUST be clearly logged with a stable error code or category.
- [ ] `FR-OBS-005` A periodic heartbeat log SHOULD report high-level state at least every 10 seconds.

### 5.8 Toolchain Preconditions and Blocked-State Handling

- [ ] `FR-BLOCK-001` Boot logs MUST explicitly report whether HFP client support is available in the active toolchain build.
- [ ] `FR-BLOCK-002` If HFP support is unavailable, the firmware MUST keep call path disabled and MUST mark call-related acceptance items as blocked/pending-device.
- [ ] `FR-BLOCK-003` Boot logs MUST explicitly report whether the configured audio output backend matches PAM8403 analog-input constraints.
- [ ] `FR-BLOCK-004` Firmware and checklist MUST NOT present blocked capabilities as passed.

### 5.9 Power and Battery Protection Behavior

- [ ] `FR-PWR-001` The system MUST document required battery/USB/jumper power configuration before acceptance testing.
- [ ] `FR-PWR-002` If VBAT sensing is wired, firmware MUST detect low-battery threshold crossing and force safe behavior (at minimum reduce output level and exit active playback/call mode).
- [ ] `FR-PWR-003` If VBAT sensing is not wired in BOM-only build, firmware MUST log low-battery protection as blocked/pending and acceptance MUST remain non-green for that item.

## 6. User-Facing Requirements

- [ ] `UX-001` Pairing flow MUST be achievable from standard phone Bluetooth settings without custom app support.
- [ ] `UX-002` Device identity MUST remain consistent across reboots unless explicitly reconfigured.
- [ ] `UX-003` During normal operation, user actions needed to switch between music and calls MUST be limited to phone-native behavior (play/pause/call answer/hang up).
- [ ] `UX-004` If pairing/bond state is invalid or corrupt, the device MUST expose a recoverable path (manual re-pairing) and clear diagnostic logs.

## 7. Non-Functional Requirements

### 7.1 Reliability

- [ ] `NFR-REL-001` System MUST complete a 30-minute continuous music session without crash, watchdog reset, or fatal error.
- [ ] `NFR-REL-002` System MUST complete a 15-minute continuous call session without crash, watchdog reset, or fatal error.
- [ ] `NFR-REL-003` System MUST handle at least 20 music<->call handovers in one runtime session without requiring reboot.

### 7.2 Latency and Responsiveness

- [ ] `NFR-PERF-001` Mode transition from call activation signal to valid call audio routing MUST complete within 2 seconds in nominal RF conditions.
- [ ] `NFR-PERF-002` Mode transition from call end signal to restored music audio (if music is active) MUST complete within 2 seconds.
- [ ] `NFR-PERF-003` Disconnect detection to silent `IDLE` state MUST complete within 3 seconds.

### 7.3 Audio Quality Baseline

- [ ] `NFR-AUD-001` Music output MUST be free of sustained stutter/dropout under stable RF conditions for the 30-minute reliability test.
- [ ] `NFR-AUD-002` Call audio path MUST be intelligible bidirectionally under stable RF conditions.
- [ ] `NFR-AUD-003` Audible artifacts during handover SHOULD be brief and non-disruptive (target <500 ms).

### 7.4 Resource and Stability

- [ ] `NFR-RES-001` Runtime memory usage MUST remain bounded such that no progressive memory leak causes reset within a 2-hour mixed-use test.
- [ ] `NFR-RES-002` Repeated connect/disconnect cycles (minimum 30 cycles) MUST NOT degrade ability to pair/connect.

## 8. Required Behavioral Edge Cases

- [ ] `EC-001` Incoming call while music is playing: call audio takes priority and music is suppressed.
- [ ] `EC-002` Call ends while media session remains active: music resumes without manual reconnect.
- [ ] `EC-003` Call ends and media is inactive: endpoint returns to silent `IDLE`.
- [ ] `EC-004` Source disconnects during call: endpoint exits call mode and returns to `IDLE` safely.
- [ ] `EC-005` Source disconnects during music: endpoint exits music mode and returns to `IDLE` safely.
- [ ] `EC-006` Rapid alternating events (play/pause/call start/call end in quick succession): endpoint maintains a valid mode and does not deadlock.
- [ ] `EC-007` Volume change commands during mode transition: no crash; resulting output level remains within safe bounds.
- [ ] `EC-008` Reboot while bonded: endpoint remains pairable and can reconnect after startup.
- [ ] `EC-009` Re-pair attempt from a new source while bonded to another: behavior is deterministic and documented (reject or replace bond policy).

## 9. Compliance Checklist for Autonomous Rebuild

All items below MUST be true before declaring v1 complete:

- [ ] `ACC-001` Product can be paired from a phone over Bluetooth settings with no custom tooling.
- [ ] `ACC-002` Music streaming works end-to-end with audible output for 30 continuous minutes.
- [ ] `ACC-003` Call audio is full-duplex for 15 continuous minutes.
- [ ] `ACC-004` At least 20 call/music handovers succeed with no reboot requirement.
- [ ] `ACC-005` Post-reboot reconnect succeeds for a bonded source device in at least 4/5 attempts.
- [ ] `ACC-006` Disconnect recovery returns system to `IDLE` and reconnect-ready state.
- [ ] `ACC-007` Serial logs clearly show startup status, connection changes, and mode transitions.
- [ ] `ACC-008` No unresolved fatal errors occur during a 2-hour mixed-use test (music, calls, disconnect/reconnect).
- [ ] `ACC-009` All edge cases in Section 8 are manually validated and recorded as pass/fail.
- [ ] `ACC-010` Validation build and test run are completed using only the approved BOM in Section 4.4 (no additional hardware dependencies).
- [ ] `ACC-011` Validation records include explicit HFP preflight result, audio output path confirmation against PAM8403 analog input, and low-battery/jumper caveat status.

## 10. Test Matrix Requirements (Minimum)

The acceptance process MUST include the following matrix:

- [ ] `TM-001` At least one iPhone model and one non-iPhone source device.
- [ ] `TM-002` At least two power-cycle reconnect tests per source device.
- [ ] `TM-003` At least one long-session test per source type:
  - music-only,
  - call-only,
  - mixed transitions.
- [ ] `TM-004` Validation logs retained for each run with timestamp, source device, scenario, and pass/fail outcomes.
- [ ] `TM-005` Hardware audit record confirms each test run used only Section 4.4 BOM parts.

## 11. Explicit Non-Requirements (To Prevent Scope Creep)

- No requirement for multi-node intercom networking.
- No requirement for advanced DSP enhancement stack.
- No requirement for companion mobile app.
- No requirement for cloud connectivity or telemetry backend.
- No requirement for specific internal firmware architecture or code organization.
