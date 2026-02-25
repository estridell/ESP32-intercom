# SPEC Conformance Audit

Audit date: 2026-02-25  
Scope: `PROJECT_SPEC.md` Section 5 Functional Requirements (`FR-*`), Section 4.4 BOM constraints (`HW-*`), and Section 9 Acceptance (`ACC-*`).

Status legend:
- `GREEN`: repo implementation/docs satisfy the requirement with direct local evidence.
- `PENDING-DEVICE`: requires physical BOM validation and/or runtime measurement on target hardware.
- `BLOCKED-TOOLCHAIN`: cannot be enabled/verified due to missing ESP32 toolchain/profile/header support.
- `GAP`: repo-side mismatch still needing code/doc changes.

Local sanity checks run:
- `./scripts/run_host_tests.sh` -> PASS (`state_machine_test: PASS (28 assertions)`).
- `arduino-cli` compile check -> `BLOCKED-TOOLCHAIN` locally (`arduino-cli: command not found`).

Repo-side fixes applied during this audit:
- Added this file (`SPEC_CONFORMANCE.md`) for explicit FR/HW/ACC conformance tracking.
- Updated `CHECKLIST.md` HW coverage (`HW-009` meaning corrected; `HW-010` added).
- Updated `TEST_LOG_TEMPLATE.md` with explicit `ACC-011` preflight fields.
- Updated `README.md` artifact list to include this conformance report.

## FR Conformance

| ID | Status | Evidence / Rationale |
|---|---|---|
| FR-BOOT-001 | GREEN | `state_machine.h` defaults to `IDLE`; `setup()` sets active mode to `IDLE`. |
| FR-BOOT-002 | PENDING-DEVICE | Discoverable/connectable is enabled in boot path (`setDiscoverableConnectable()`), but 10s timing must be measured on device. |
| FR-PAIR-001 | PENDING-DEVICE | GAP pairing path is enabled; phone pairing must be validated on target hardware. |
| FR-PAIR-002 | PENDING-DEVICE | Bond persistence relies on stack/NVS behavior; requires power-cycle validation. |
| FR-PAIR-003 | PENDING-DEVICE | Reconnect behavior depends on source device + RF runtime behavior. |
| FR-PAIR-004 | PENDING-DEVICE | Reconnect-fallback discoverability is implemented, but manual reconnection behavior needs device proof. |
| FR-MUSIC-001 | PENDING-DEVICE | A2DP sink init/callbacks implemented; end-to-end stream validation pending device. |
| FR-MUSIC-002 | PENDING-DEVICE | PCM rendering to analog DAC path implemented; continuity/audibility needs device validation. |
| FR-MUSIC-003 | PENDING-DEVICE | AVRCP absolute-volume handling and clamp exist; perceived monotonic loudness needs hardware validation. |
| FR-MUSIC-004 | PENDING-DEVICE | Stop/suspend handling and IDLE arbitration implemented; silent return behavior must be validated on device. |
| FR-CALL-001 | PENDING-DEVICE | HFP client path exists behind compile guard; call function validation needs device/toolchain support. |
| FR-CALL-002 | PENDING-DEVICE | HFP incoming audio callback routes to output path; audible call validation pending device. |
| FR-CALL-003 | PENDING-DEVICE | ADC mic sampling to HFP outgoing callback implemented; intelligibility pending device. |
| FR-CALL-004 | PENDING-DEVICE | Event-driven call entry/exit path exists; no-repair/no-reboot runtime proof pending device. |
| FR-MODE-001 | GREEN | Active modes are exactly `IDLE`, `MUSIC`, `CALL` in shared state machine. |
| FR-MODE-002 | GREEN | `resolveMode()` enforces call-over-music priority; host-tested. |
| FR-MODE-003 | GREEN | Call end returns to music-if-active else idle; host-tested. |
| FR-MODE-004 | GREEN | Automatic deterministic arbitration via `ModeArbiter`; host-tested with rapid events. |
| FR-FMT-001 | PENDING-DEVICE | Music stream handling path supports 44.1k stereo source input processing; hardware output verification pending. |
| FR-FMT-002 | PENDING-DEVICE | Call stream handling path supports 16k mono voice processing; hardware output verification pending. |
| FR-FMT-003 | GREEN | Mode transitions auto-update target output rate in transition handler. |
| FR-FMT-004 | GREEN | Call exit path restores music target rate automatically. |
| FR-REC-001 | PENDING-DEVICE | Disconnect-to-IDLE behavior exists, but 3s timing needs measured runtime evidence. |
| FR-REC-002 | PENDING-DEVICE | Reconnect-ready discoverable policy implemented; runtime verification pending device. |
| FR-REC-003 | PENDING-DEVICE | Reconnect-ready logging paths exist; repeated-failure behavior needs live test evidence. |
| FR-OBS-001 | GREEN | Startup/connect/disconnect/mode logs implemented in firmware callbacks and setup. |
| FR-OBS-002 | GREEN | Transition direction strings (`IDLE->MUSIC`, etc.) are emitted. |
| FR-OBS-003 | GREEN | Recoverable errors are logged (`WARN ...`) without halting. |
| FR-OBS-004 | GREEN | Fatal startup failures emit stable `FATAL-xxx` codes. |
| FR-OBS-005 | GREEN | Periodic heartbeat log every 10 seconds is implemented. |
| FR-BLOCK-001 | GREEN | Boot/runtime logs explicitly report HFP toolchain state (`ok` vs blocked). |
| FR-BLOCK-002 | GREEN | Missing HFP header path explicitly blocks call support and logs `BLOCKED HFP-TOOLCHAIN`. |
| FR-BLOCK-003 | GREEN | Boot/runtime logs explicitly report output path status (`analog-ok` or blocked). |
| FR-BLOCK-004 | GREEN | Repo docs/checklist avoid claiming blocked capabilities as passed. |
| FR-PWR-001 | GREEN | Battery/USB/jumper caveats documented in `README.md` and checklist preflight. |
| FR-PWR-002 | PENDING-DEVICE | Low-battery guard logic exists behind VBAT ADC wiring; threshold behavior needs bench validation. |
| FR-PWR-003 | GREEN | No-VBAT-wiring path logs explicit blocked low-battery status. |

## HW Conformance

| ID | Status | Evidence / Rationale |
|---|---|---|
| HW-001 | PENDING-DEVICE | Repo is BOM-scoped, but physical build/test evidence with approved BOM is still required. |
| HW-002 | GREEN | Repo does not introduce extra sensors/radios/displays/co-processors. |
| HW-003 | PENDING-DEVICE | Single-speaker operation is design-compatible; physical operation confirmation pending device. |
| HW-004 | PENDING-DEVICE | Firmware optional IO maps to listed switch/LED classes; wiring audit still required on bench. |
| HW-005 | PENDING-DEVICE | Firmware targets ESP32 analog DAC -> PAM8403 input path; end-to-end wiring/audio validation pending. |
| HW-006 | GREEN | HFP header/toolchain precondition is explicitly handled and logged as blocked when unavailable. |
| HW-007 | PENDING-DEVICE | Required SPDT/tactile/CC-LED wiring assumptions are documented; physical continuity validation pending. |
| HW-008 | PENDING-DEVICE | Battery/jumper caveats are documented; mandatory physical power-path test still pending. |
| HW-009 | PENDING-DEVICE | VBAT-sensing branch requires bench validation when wired; no-sensing build is explicitly logged as blocked/pending. |
| HW-010 | PENDING-DEVICE | PAM8403 BTL output wiring safety requires physical wiring confirmation on the assembled build. |

## ACC Conformance

| ID | Status | Evidence / Rationale |
|---|---|---|
| ACC-001 | PENDING-DEVICE | Requires live phone pairing demonstration. |
| ACC-002 | PENDING-DEVICE | Requires 30-minute continuous music run on hardware. |
| ACC-003 | PENDING-DEVICE | Requires 15-minute full-duplex call run, or explicit HFP toolchain block record. |
| ACC-004 | PENDING-DEVICE | Requires >=20 call/music handovers without reboot on device. |
| ACC-005 | PENDING-DEVICE | Requires measured 4/5 post-reboot reconnect success. |
| ACC-006 | PENDING-DEVICE | Requires disconnect recovery to IDLE + reconnect-ready confirmation. |
| ACC-007 | PENDING-DEVICE | Logging framework exists; acceptance requires captured runtime logs. |
| ACC-008 | PENDING-DEVICE | Requires 2-hour mixed-use stability run with no unresolved fatal errors. |
| ACC-009 | PENDING-DEVICE | Requires manual edge-case validation records for Section 8 scenarios. |
| ACC-010 | PENDING-DEVICE | Requires per-run hardware audit proving approved BOM-only usage. |
| ACC-011 | PENDING-DEVICE | Requires explicit preflight records (HFP/toolchain, analog output path, low-battery/jumper status). |

## Remaining Non-Repo Blockers

- Physical BOM bench validation has not been executed in this audit environment.
- Local ESP32 toolchain is unavailable (`arduino-cli` missing), so target compile/flash validation is blocked here.
