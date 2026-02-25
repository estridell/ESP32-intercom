# ESP32 Intercom v1 Checklist (Living)

Status summary: GREEN 47 | YELLOW 0 | PENDING-DEVICE 26 | RED 0

Status legend:
- `GREEN`: implemented with direct evidence in repo (and host-test where applicable); may still note pending on-device validation
- `YELLOW`: partially implemented or ambiguous evidence; not used for the current v1 implementation posture
- `RED`: currently blocked or missing
- `PENDING-DEVICE`: requires physical BOM execution, runtime-duration test, or measured on-device evidence

## FR (Functional Requirements)

| ID | Status | Evidence / Notes |
|---|---|---|
| FR-BOOT-001 | GREEN | `state_machine.h` initializes to `IDLE`; `setup()` sets mode `IDLE`. |
| FR-BOOT-002 | GREEN | `initBluetoothOrHalt()` calls discoverable/connectable during boot and tolerates already-initialized BT states; pending on-device validation. |
| FR-PAIR-001 | GREEN | Classic BT pairing flow enabled with GAP pin config; pending on-device validation. |
| FR-PAIR-002 | GREEN | Uses default stack bonding persistence; pending on-device validation across reboot. |
| FR-PAIR-003 | GREEN | Bonding kept by default; reconnect behavior implemented, pending on-device validation against phone/stack behavior. |
| FR-PAIR-004 | GREEN | Discoverable/connectable restored on disconnect + periodic refresh in loop; pending on-device validation. |
| FR-MUSIC-001 | GREEN | A2DP sink init and callbacks implemented in `esp32_intercom.ino`; pending on-device validation. |
| FR-MUSIC-002 | GREEN | A2DP PCM callback streams to I2S with gain control; pending on-device validation. |
| FR-MUSIC-003 | GREEN | AVRCP absolute-volume clamp path implemented plus RN volume capability/interim/changed responses (guarded for core compatibility); pending on-device validation. |
| FR-MUSIC-004 | GREEN | A2DP audio stop/suspend clears `music_active` -> `IDLE` via arbiter; pending on-device validation. |
| FR-CALL-001 | GREEN | HFP client callback/data path implemented with compile-time availability guard; pending on-device validation. |
| FR-CALL-002 | GREEN | HFP incoming audio -> I2S output path implemented; pending on-device validation. |
| FR-CALL-003 | GREEN | ADC mic sampling -> HFP outgoing PCM implemented; pending on-device validation. |
| FR-CALL-004 | GREEN | Call mode enters/exits by event-driven arbitration; pending on-device validation. |
| FR-MODE-001 | GREEN | Exactly `IDLE/MUSIC/CALL` in shared `state_machine.h`. |
| FR-MODE-002 | GREEN | `resolveMode()` always prioritizes call over music; host-tested. |
| FR-MODE-003 | GREEN | On call end returns to music-if-active else idle; host-tested. |
| FR-MODE-004 | GREEN | Deterministic shared arbiter with host-tested transitions; runtime inputs now use derived `source_connected = (A2DP || HFP)` to avoid false mode drops; pending on-device validation. |
| FR-FMT-001 | GREEN | Music output config set to 44.1 kHz stereo I2S path; pending on-device validation. |
| FR-FMT-002 | GREEN | Call output switches I2S to 16 kHz; HFP mic uplink is mono PCM from ADC; pending on-device validation. |
| FR-FMT-003 | GREEN | Automatic mode-triggered sample-rate switching in transition handler; pending on-device validation. |
| FR-FMT-004 | GREEN | Call exit transitions reapply music format; pending on-device validation. |
| FR-REC-001 | GREEN | Link-state tracking split across A2DP/HFP with derived source connectivity; disconnect-to-`IDLE` timing (<3s) pending on-device validation. |
| FR-REC-002 | GREEN | Reconnect-ready mode kept by discoverable/connectable refresh policy and disconnect callbacks; pending on-device validation. |
| FR-REC-003 | GREEN | Recoverable warnings/disconnect reasons logged and discoverable restore attempted; pending on-device validation under repeated failures. |
| FR-OBS-001 | GREEN | Startup/connect/disconnect/mode transition logs implemented. |
| FR-OBS-002 | GREEN | Transition direction strings (`IDLE->MUSIC`, etc.) emitted. |
| FR-OBS-003 | GREEN | Recoverable `WARN` logging without halt implemented. |
| FR-OBS-004 | GREEN | Stable `FATAL-xxx` startup codes + halt path implemented. |
| FR-OBS-005 | GREEN | 10s heartbeat (`HB ...`) implemented in loop. |

## UX (User-Facing Requirements)

| ID | Status | Evidence / Notes |
|---|---|---|
| UX-001 | GREEN | Intended standard BT settings pairing path is implemented; pending on-device validation on iOS/Android. |
| UX-002 | GREEN | Stable build-time device name constant plus explicit hands-free/audio CoD setup (guarded by available core APIs); pending on-device validation across reboot. |
| UX-003 | GREEN | Phone-native play/pause/call events drive arbitration automatically; pending on-device validation. |
| UX-004 | GREEN | Recoverable path documented + boot-hold bond clear policy + logs implemented; pending on-device validation. |

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
| EC-007 | GREEN | Volume clamp + ramp + transition-safe gain updates implemented; pending on-device validation under stress. |
| EC-008 | GREEN | Bond persistence path implemented via default stack; pending on-device reboot/reconnect validation. |
| EC-009 | GREEN | Deterministic policy implemented: keep latest auth bond, clear older bonds; pending on-device validation. |

## ACC (Acceptance)

| ID | Status | Evidence / Notes |
|---|---|---|
| ACC-001 | PENDING-DEVICE | Requires live phone pairing demo and log evidence. |
| ACC-002 | PENDING-DEVICE | Requires 30-minute music end-to-end run. |
| ACC-003 | PENDING-DEVICE | Requires 15-minute full-duplex call run. |
| ACC-004 | PENDING-DEVICE | Requires >=20 successful handovers without reboot. |
| ACC-005 | PENDING-DEVICE | Requires 4/5 post-reboot reconnect pass metric capture. |
| ACC-006 | PENDING-DEVICE | Requires disconnect recovery run and idle/reconnect-ready confirmation. |
| ACC-007 | PENDING-DEVICE | Logging scaffolding is implemented; acceptance requires captured live session logs. |
| ACC-008 | PENDING-DEVICE | Requires 2-hour mixed-use validation with no unresolved fatal errors. |
| ACC-009 | PENDING-DEVICE | Requires manual execution + records for all Section 8 edge cases. |
| ACC-010 | PENDING-DEVICE | Requires per-run hardware audit that only approved BOM was used. |

## TM (Test Matrix)

| ID | Status | Evidence / Notes |
|---|---|---|
| TM-001 | PENDING-DEVICE | Needs at least one iPhone + one non-iPhone source test. |
| TM-002 | PENDING-DEVICE | Needs two power-cycle reconnect tests per source device. |
| TM-003 | PENDING-DEVICE | Needs long-session runs (music-only/call-only/mixed) per source type. |
| TM-004 | GREEN | Procedure documented in `README.md` and `TEST_LOG_TEMPLATE.md`; pending on-device execution artifacts. |
| TM-005 | PENDING-DEVICE | `TEST_LOG_TEMPLATE.md` includes BOM audit fields; completed hardware audit records still pending per run. |

## HW (BOM Constraints)

| ID | Status | Evidence / Notes |
|---|---|---|
| HW-001 | PENDING-DEVICE | Firmware/docs remain BOM-locked and test log template includes BOM audit checklist; pending bench build and per-run audit evidence. |
| HW-002 | GREEN | No extra hardware dependencies introduced in code/docs. |
| HW-003 | GREEN | Single-speaker operation supported as baseline; second speaker optional; pending on-device validation. |
| HW-004 | GREEN | Optional controls/indicators limited to available switches + dual-color LEDs. |
