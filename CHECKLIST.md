# ESP32 Intercom v1 Checklist (Living)

Status summary: hardware-verified items remain `PENDING-DEVICE` until validated on real BOM hardware.
Canonical FR/HW/ACC status mapping is tracked in `SPEC_CONFORMANCE.md`.

Status legend:
- `GREEN`: implemented with direct evidence in repo (and host-test where applicable)
- `PENDING-DEVICE`: requires physical BOM execution, runtime-duration test, or measured on-device evidence
- `BLOCKED-TOOLCHAIN`: cannot be enabled/verified due to missing ESP32 profile/header/toolchain capability
- `GAP`: repo-side requirement mismatch still requiring implementation/doc fix

## Preflight (Must Run Before Device Acceptance)

| ID | Status | Evidence / Notes |
|---|---|---|
| PF-HFP-001 | GREEN | `ESP32-intercom.ino` uses compile-time guard for `esp_hf_client_api.h` and explicitly reports blocked status when unavailable. |
| PF-HFP-002 | PENDING-DEVICE | Boot log on target must explicitly show `HFP-TOOLCHAIN` status (`ok` or `blocked`) and be saved with test artifact. |
| PF-BOM-001 | PENDING-DEVICE | Verify PAM8403 analog input wiring: ESP32 DAC pin (GPIO25) -> PAM8403 input, shared ground, and correct amp supply rail. |
| PF-BOM-002 | PENDING-DEVICE | Verify slide switch wiring uses SPDT center-common pin to GPIO or rail as expected by chosen function. |
| PF-BOM-003 | PENDING-DEVICE | Verify tactile switch pin pairing (diagonal continuity pair) before wiring to GPIO; avoid same-side short assumptions. |
| PF-BOM-004 | PENDING-DEVICE | Verify common-cathode dual-color LED wiring uses separate current-limiting resistor per color channel. |
| PF-BOM-005 | PENDING-DEVICE | Verify LiPo/USB/jumper/power routing so amplifier and ESP32 rails are valid and no back-power condition exists. |
| PF-BOM-006 | PENDING-DEVICE | Verify speaker safety setup: 8Ω 0.5W speaker load, conservative initial volume, and no sustained overdrive behavior. |
| PF-BOM-007 | PENDING-DEVICE | Verify PAM8403 output wiring safety: one speaker per channel, no speaker terminal tied to GND, and no L/R output tying. |
| PF-BOM-008 | PENDING-DEVICE | Verify measured/estimated output stays within safe envelope (~<=2.0 Vrms equivalent per 8Ω speaker channel, or stricter). |

## FR (Functional Requirements)

| ID | Status | Evidence / Notes |
|---|---|---|
| FR-BOOT-001 | GREEN | `state_machine.h` initializes to `IDLE`; `setup()` sets mode `IDLE`. |
| FR-BOOT-002 | PENDING-DEVICE | `initBluetoothOrHalt()` enables discoverable/connectable; timing and runtime confirmation require hardware. |
| FR-PAIR-001 | PENDING-DEVICE | Classic BT pairing flow enabled with GAP pin config; must be verified on phone(s). |
| FR-PAIR-002 | PENDING-DEVICE | Default stack bonding persistence path exists; reboot persistence requires hardware run. |
| FR-PAIR-003 | PENDING-DEVICE | Reconnect behavior implemented; phone/stack behavior must be measured on device. |
| FR-PAIR-004 | PENDING-DEVICE | Discoverable/connectable restore path implemented; manual reconnection behavior pending hardware validation. |
| FR-MUSIC-001 | PENDING-DEVICE | A2DP sink init and callbacks implemented in `ESP32-intercom.ino`; end-to-end proof pending device. |
| FR-MUSIC-002 | PENDING-DEVICE | A2DP PCM now routes to analog DAC output backend (PAM8403-compatible direction); audible validation pending device. |
| FR-MUSIC-003 | PENDING-DEVICE | AVRCP absolute-volume clamp path implemented; monotonic loudness must be validated on hardware. |
| FR-MUSIC-004 | PENDING-DEVICE | A2DP stop/suspend clears `music_active` -> `IDLE`; runtime proof pending device. |
| FR-CALL-001 | PENDING-DEVICE | HFP client path present with explicit toolchain block handling; full-duplex validation pending device. |
| FR-CALL-002 | PENDING-DEVICE | HFP incoming audio routes to analog output backend; audibility validation pending device. |
| FR-CALL-003 | PENDING-DEVICE | ADC mic sampling -> HFP outgoing PCM implemented; call intelligibility pending device. |
| FR-CALL-004 | PENDING-DEVICE | Call mode enters/exits by event-driven arbitration; runtime validation pending device. |
| FR-MODE-001 | GREEN | Exactly `IDLE/MUSIC/CALL` in shared `state_machine.h`. |
| FR-MODE-002 | GREEN | `resolveMode()` always prioritizes call over music; host-tested. |
| FR-MODE-003 | GREEN | On call end returns to music-if-active else idle; host-tested. |
| FR-MODE-004 | GREEN | Deterministic shared arbiter with host-tested transitions and derived `source_connected = (A2DP || HFP)`. |
| FR-FMT-001 | PENDING-DEVICE | Music-mode source compatibility target retained (44.1k input); rendered through analog DAC backend with downsample strategy; must be validated audibly. |
| FR-FMT-002 | PENDING-DEVICE | Call-mode source compatibility target retained (16k mono input); rendered through analog DAC backend; must be validated audibly. |
| FR-FMT-003 | GREEN | Mode-triggered output-rate target updates are automatic in transition handler. |
| FR-FMT-004 | GREEN | Call exit transitions restore music-rate target automatically. |
| FR-REC-001 | PENDING-DEVICE | Disconnect-to-`IDLE` target requires measured runtime validation. |
| FR-REC-002 | PENDING-DEVICE | Reconnect-ready policy is implemented; needs device validation. |
| FR-REC-003 | PENDING-DEVICE | Recoverable warnings and discoverable restore are logged; repeated-failure behavior pending device tests. |
| FR-OBS-001 | GREEN | Startup/connect/disconnect/mode transition logs implemented. |
| FR-OBS-002 | GREEN | Transition direction strings (`IDLE->MUSIC`, etc.) emitted. |
| FR-OBS-003 | GREEN | Recoverable `WARN` logging without halt implemented. |
| FR-OBS-004 | GREEN | Stable `FATAL-xxx` startup codes + halt path implemented. |
| FR-OBS-005 | GREEN | 10s heartbeat (`HB ...`) implemented in loop and now includes output/HFP/low-battery status fields. |
| FR-BLOCK-001 | GREEN | Boot/runtime logs explicitly report HFP toolchain blocked/ok state. |
| FR-BLOCK-002 | GREEN | Missing HFP headers force blocked status and disable call-path claim. |
| FR-BLOCK-003 | GREEN | Boot/runtime logs explicitly report analog output path status (`analog-ok` or `blocked`). |
| FR-BLOCK-004 | GREEN | Checklist policy keeps blocked capabilities out of pass claims. |
| FR-PWR-001 | GREEN | Power/jumper caveats captured in `PROJECT_SPEC.md` and `README.md`. |
| FR-PWR-002 | PENDING-DEVICE | Low-battery protection behavior is implemented behind configurable VBAT ADC wiring; threshold behavior needs bench validation. |
| FR-PWR-003 | GREEN | If VBAT ADC wiring is absent, firmware logs explicit low-battery blocked status. |

## UX (User-Facing Requirements)

| ID | Status | Evidence / Notes |
|---|---|---|
| UX-001 | PENDING-DEVICE | Standard Bluetooth pairing path intended; must be validated on iOS/Android. |
| UX-002 | PENDING-DEVICE | Stable build-time name and CoD setup implemented; reboot consistency pending device validation. |
| UX-003 | PENDING-DEVICE | Phone-native play/pause/call events drive arbitration automatically; pending device validation. |
| UX-004 | PENDING-DEVICE | Recoverable path documented + boot-hold bond clear policy + logs implemented; pending device validation. |

## NFR (Non-Functional Requirements)

| ID | Status | Evidence / Notes |
|---|---|---|
| NFR-REL-001 | PENDING-DEVICE | Requires 30-minute continuous music soak test on hardware. |
| NFR-REL-002 | PENDING-DEVICE | Requires 15-minute continuous call soak test on hardware. |
| NFR-REL-003 | PENDING-DEVICE | Requires >=20 handovers in one runtime session. |
| NFR-PERF-001 | PENDING-DEVICE | Requires measured call activation to routed audio latency. |
| NFR-PERF-002 | PENDING-DEVICE | Requires measured call-end to music-restore latency. |
| NFR-PERF-003 | PENDING-DEVICE | Requires measured disconnect-to-idle latency. |
| NFR-AUD-001 | PENDING-DEVICE | Requires dropout/stutter assessment under stable RF. |
| NFR-AUD-002 | PENDING-DEVICE | Requires bidirectional call intelligibility validation. |
| NFR-AUD-003 | PENDING-DEVICE | Requires audible artifact duration measurement at handover. |
| NFR-RES-001 | PENDING-DEVICE | Requires 2-hour mixed-use stability/no-leak observation. |
| NFR-RES-002 | PENDING-DEVICE | Requires >=30 connect/disconnect cycle robustness run. |

## EC (Edge Cases)

| ID | Status | Evidence / Notes |
|---|---|---|
| EC-001 | GREEN | Host assertion in `tests/state_machine_test.cpp` validates call-over-music priority. |
| EC-002 | GREEN | Host assertion validates resume-to-music after call end. |
| EC-003 | GREEN | Host assertion validates return-to-idle after call end with no media. |
| EC-004 | GREEN | Host assertion validates disconnect during call forces idle safely. |
| EC-005 | GREEN | Host assertion validates disconnect during music forces idle safely. |
| EC-006 | GREEN | Host rapid-event assertion sequence validates no invalid/deadlocked mode. |
| EC-007 | PENDING-DEVICE | Volume clamp + ramp + transition-safe gain updates implemented; stress validation pending on-device. |
| EC-008 | PENDING-DEVICE | Bond persistence path implemented via default stack; pending reboot/reconnect validation. |
| EC-009 | PENDING-DEVICE | Deterministic single-bond policy implemented; pending on-device validation. |

## ACC (Acceptance)

| ID | Status | Evidence / Notes |
|---|---|---|
| ACC-001 | PENDING-DEVICE | Requires live phone pairing demo and log evidence. |
| ACC-002 | PENDING-DEVICE | Requires 30-minute music end-to-end run. |
| ACC-003 | PENDING-DEVICE | Requires 15-minute full-duplex call run (or explicit HFP-toolchain blocked record). |
| ACC-004 | PENDING-DEVICE | Requires >=20 successful handovers without reboot. |
| ACC-005 | PENDING-DEVICE | Requires 4/5 post-reboot reconnect pass metric capture. |
| ACC-006 | PENDING-DEVICE | Requires disconnect recovery run and idle/reconnect-ready confirmation. |
| ACC-007 | PENDING-DEVICE | Logging scaffolding is implemented; acceptance requires captured live session logs. |
| ACC-008 | PENDING-DEVICE | Requires 2-hour mixed-use validation with no unresolved fatal errors. |
| ACC-009 | PENDING-DEVICE | Requires manual execution + records for all Section 8 edge cases. |
| ACC-010 | PENDING-DEVICE | Requires per-run hardware audit that only approved BOM was used. |
| ACC-011 | PENDING-DEVICE | Requires explicit preflight record of HFP availability, analog output wiring confirmation, and low-battery/jumper caveat status (fields now explicit in `TEST_LOG_TEMPLATE.md`). |

## TM (Test Matrix)

| ID | Status | Evidence / Notes |
|---|---|---|
| TM-001 | PENDING-DEVICE | Needs at least one iPhone + one non-iPhone source test. |
| TM-002 | PENDING-DEVICE | Needs two power-cycle reconnect tests per source device. |
| TM-003 | PENDING-DEVICE | Needs long-session runs (music-only/call-only/mixed) per source type. |
| TM-004 | GREEN | Procedure documented in `README.md` and `TEST_LOG_TEMPLATE.md`; pending on-device execution artifacts. |
| TM-005 | PENDING-DEVICE | `TEST_LOG_TEMPLATE.md` includes BOM and ACC-011 preflight fields; completed per-run audit records still pending. |

## HW (BOM Constraints)

| ID | Status | Evidence / Notes |
|---|---|---|
| HW-001 | PENDING-DEVICE | Firmware/docs remain BOM-locked; bench build and per-run audit evidence still required. |
| HW-002 | GREEN | No extra hardware dependencies introduced in code/docs. |
| HW-003 | PENDING-DEVICE | Single-speaker baseline kept; needs on-device confirmation for real loudness/thermal behavior. |
| HW-004 | PENDING-DEVICE | Optional controls/indicators limited to listed switch/LED parts; physical wiring audit pending. |
| HW-005 | PENDING-DEVICE | Firmware output backend now targets analog DAC path for PAM8403 compatibility; physical wiring/audio validation pending. |
| HW-006 | GREEN | HFP toolchain precondition + blocked-status handling implemented explicitly in firmware logs/checklist. |
| HW-007 | PENDING-DEVICE | SPDT/tactile/CC-LED wiring assumptions require bench continuity and assembly validation. |
| HW-010 | PENDING-DEVICE | PAM8403 BTL output wiring safety (no `L-/R-` to GND, no tied channels, one load/channel) requires physical wiring validation. |
| HW-008 | PENDING-DEVICE | Battery/jumper caveats require physical power-path validation. |
| HW-009 | PENDING-DEVICE | Low-battery safe-behavior branch requires bench validation when VBAT sensing is wired; no-sensing path is explicitly blocked in firmware logs. |
