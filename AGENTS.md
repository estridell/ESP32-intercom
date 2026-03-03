# Repository Guidelines

## Project Structure & Module Organization
- `ESP32-intercom.ino`: main firmware sketch for ESP32 bring-up, Bluetooth flow, and hardware pin usage.
- `state_machine.h`: header-only audio mode arbitration logic shared by firmware and host tests.
- `tests/state_machine_test.cpp`: host-side C++ regression tests for mode transitions and edge cases.
- `scripts/run_host_tests.sh`: local test runner used in CI-style checks.
- Documentation: `README.md` (build/test flow), `PROJECT_SPEC.md` (requirements), `CHECKLIST.md` and `TEST_LOG_TEMPLATE.md` (validation artifacts).

## Build, Test, and Development Commands
- `arduino-cli compile --fqbn esp32:esp32:esp32 .` builds the firmware sketch from repo root.
- `arduino-cli upload --fqbn esp32:esp32:esp32 -p <PORT> .` flashes firmware to hardware.
- `bash scripts/run_host_tests.sh` compiles and runs host regression tests with `g++ -std=c++17`.
- Optional setup (first time): `arduino-cli core install esp32:esp32@2.0.17`.

## Coding Style & Naming Conventions
- Use 2-space indentation and brace style matching existing `.ino`/`.h` files.
- Keep logic readable and deterministic; prefer small helpers over deeply nested conditionals.
- Naming patterns:
  - Types/enums: `PascalCase` (for example `ModeArbiter`, `AudioMode`).
  - Functions: `camelCase` (for example `resolveMode`, `applyInputs`).
  - Enum values/constants: `UPPER_SNAKE_CASE` (for example `IDLE`, `MUSIC`).
- Preserve header-only compatibility for shared state machine code unless refactoring is intentional.

## Testing Guidelines
- Add or update host tests in `tests/state_machine_test.cpp` when mode logic changes.
- Cover each transition and sanitization edge case (disconnect, call priority, rapid toggles).
- Run `bash scripts/run_host_tests.sh` before opening a PR.
- For hardware-facing changes, also record manual results using `TEST_LOG_TEMPLATE.md`.

## Commit & Pull Request Guidelines
- Follow concise, scoped commit subjects with a prefix seen in history: `docs:`, `chore:`, `spec:`, `audit:`.
- Write commits in imperative mood and keep each commit focused on one concern.
- PRs should include: purpose, affected files, test evidence (host test output and/or serial logs), and any checklist/spec updates.
- Link relevant issues and include wiring or behavior notes when hardware assumptions change.
