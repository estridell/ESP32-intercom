# ESP32 Intercom v1 Build Plan

Date: 2026-02-25

## Phase Status

| Phase | Scope | Current Status | Next Actions |
|---|---|---|---|
| P0 | Core architecture and deterministic mode arbiter (`FR-MODE-*`) | DONE | Keep host tests green on each change set. |
| P1 | Firmware robustness hardening (`FR-REC-*`, `FR-MUSIC-003`, identity metadata) | IN PROGRESS | Run on-device smoke test after this refinement: pair, stream, call, disconnect/reconnect. |
| P2 | Device validation matrix (`TM-001..TM-005`, `ACC-001..ACC-011`) | PENDING-DEVICE | Execute full matrix and record each run with `TEST_LOG_TEMPLATE.md`. |
| P3 | v1 acceptance closeout | BLOCKED BY P2 | Convert remaining `PENDING-DEVICE` evidence to pass/fail records and finalize checklist. |

## Major FR Group Plan

| FR Group | Current Implementation Status | Evidence | Next Action |
|---|---|---|---|
| FR-BOOT / FR-PAIR | PARTIAL (firmware ready, device validation pending) | Boot to `IDLE`, discoverable/connectable flow, bond clear and single-bond policy in `ESP32-intercom.ino`. | Perform physical pairing/reboot reconnect runs and log outcomes. |
| FR-MUSIC | PARTIAL (runtime validation pending) | A2DP sink + analog DAC render path compatible with PAM8403 input, AVRCP absolute volume clamp and RN handshake support. | Validate 30-minute stream + monotonic loudness on device. |
| FR-CALL | PARTIAL (runtime validation pending) | HFP client guarded path, duplex callbacks, call-priority routing via shared arbiter. | Validate 15-minute full-duplex call intelligibility/stability. |
| FR-MODE | IMPLEMENTED | Shared `state_machine.h` with deterministic priority and host tests. | Keep regression test passing; validate >=20 handovers on hardware. |
| FR-FMT | PARTIAL (runtime validation pending) | Mode-driven output-rate target switch: music input 44.1k, call input 16k with analog output backend. | Measure call entry/exit transitions on device. |
| FR-REC | PARTIAL (runtime validation pending) | Separate A2DP/HFP link booleans with derived source connectivity + reconnect-ready behavior. | Time disconnect-to-idle and reconnection readiness under repeated drops. |
| FR-OBS | IMPLEMENTED IN FIRMWARE (session evidence pending) | Startup/mode/disconnect/heartbeat/warn/fatal logging in sketch. | Capture real run logs for acceptance records. |

## Acceptance Mapping (ACC-001..ACC-011)

| ACC ID | Mapped FR Groups | Current Status | Next Action |
|---|---|---|---|
| ACC-001 | FR-BOOT, FR-PAIR, UX-001 | PENDING-DEVICE | Pair from phone settings and retain serial/log evidence. |
| ACC-002 | FR-MUSIC, NFR-REL-001, NFR-AUD-001 | PENDING-DEVICE | 30-minute music run with dropout notes. |
| ACC-003 | FR-CALL, NFR-REL-002, NFR-AUD-002 | PENDING-DEVICE | 15-minute duplex call run with intelligibility notes. |
| ACC-004 | FR-MODE, NFR-REL-003 | PENDING-DEVICE | Execute and record >=20 call/music handovers. |
| ACC-005 | FR-PAIR, EC-008, TM-002 | PENDING-DEVICE | Run 5 reboot reconnect attempts and summarize pass rate. |
| ACC-006 | FR-REC, EC-004, EC-005 | PENDING-DEVICE | Trigger disconnects in music/call and confirm idle + reconnect-ready state. |
| ACC-007 | FR-OBS | YELLOW | Capture and archive live logs now that logging scaffolding is complete. |
| ACC-008 | NFR-RES-001, NFR-RES-002 | PENDING-DEVICE | 2-hour mixed-use stress run with reconnect cycles. |
| ACC-009 | EC-001..EC-009 | PENDING-DEVICE | Execute full edge-case matrix on hardware and record pass/fail. |
| ACC-010 | HW-001..HW-004, TM-005 | PENDING-DEVICE | Complete per-run BOM audit fields in test logs. |
| ACC-011 | FR-BLOCK, FR-PWR, HW-005..HW-009 | PENDING-DEVICE | Record HFP preflight result, analog-output wiring proof, and low-battery/jumper caveat status. |

## Immediate Execution Queue

1. Run quick bench smoke: boot, pair, stream, call, disconnect.
2. Execute TM matrix runs using `TEST_LOG_TEMPLATE.md` entries per run.
3. Update `CHECKLIST.md` from `PENDING-DEVICE` to `GREEN/RED` based on recorded evidence.
