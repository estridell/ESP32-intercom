#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="/tmp/esp32_intercom_host_tests"
TEST_BIN="$BUILD_DIR/state_machine_test"

mkdir -p "$BUILD_DIR"

g++ -std=c++17 -Wall -Wextra -pedantic \
  "$ROOT_DIR/tests/state_machine_test.cpp" \
  -o "$TEST_BIN"

"$TEST_BIN"
