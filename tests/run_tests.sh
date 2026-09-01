#!/usr/bin/env bash
# Builds and runs the pure-logic unit tests. Deliberately has no dependency on
# openFrameworks/ofxPDSP/GPIO -- it only compiles the handful of src/*.cpp
# files that are plain C++ with no OF/hardware includes, so this runs in
# seconds without a display, audio device, or Pi hardware attached.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$DIR/../src"
BUILD_DIR="$DIR/build"
mkdir -p "$BUILD_DIR"

# Pure-logic sources under src/ with no openFrameworks/ofxPDSP dependency --
# add to this list as more logic gets extracted out of ofApp.
PURE_LOGIC_SOURCES=(
  "$SRC_DIR/HitGestureDetector.cpp"
  "$SRC_DIR/ButtonClickClassifier.cpp"
  "$SRC_DIR/AppSettings.cpp"
)

# nlohmann::json is vendored inside openFrameworks' own libs/ -- reuse it
# rather than vendoring a second copy just for the pure-logic tests.
OF_JSON_INCLUDE="$SRC_DIR/../../../../libs/json/include"

TEST_SOURCES=("$DIR"/test_*.cpp)

echo "Building tests..."
clang++ -std=c++17 -Wall -Wextra \
  -I "$DIR/thirdparty" -I "$SRC_DIR" -I "$OF_JSON_INCLUDE" \
  "${PURE_LOGIC_SOURCES[@]}" "${TEST_SOURCES[@]}" \
  -o "$BUILD_DIR/run_tests"

echo "Running tests..."
cd "$DIR/.." # so tests reading bin/data/... resolve regardless of caller's cwd
"$BUILD_DIR/run_tests" "$@"
