#!/bin/bash
# -----------------------------------------------------------------------------
# Crossfade test on the Raspberry Pi with the real sound device. Run ON the Pi:
#
#   scripts/pi/crossfade_test.sh                 # profile "fable" (10 presets)
#   PROFILE=Fred scripts/pi/crossfade_test.sh    # a performer's presets
#   QUICK=1 scripts/pi/crossfade_test.sh         # shorter
#
# 1. offline: bin/Gaugemancy --benchmark, whose "Preset changes" section gives
#    the time to build and free a preset and the audio CPU during a fade
# 2. live: starts the instrument headless with simulated sensors and runs
#    tools/crossfade_stress.py against it (listen to it: no gaps, no clicks)
# The systemd service is stopped for the test and started again afterwards.
# Everything is also written to crossfade_test.log in the app folder.
# -----------------------------------------------------------------------------
set -uo pipefail

APP_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
PROFILE="${PROFILE:-fable}"
DATA="${DATA:-$APP_DIR/bin/data}"
LOG="$APP_DIR/crossfade_test.log"
cd "$APP_DIR"

exec > >(tee "$LOG") 2>&1
echo "Crossfade test: profile $PROFILE, data $DATA, $(date)"
echo "$(tr -d '\0' < /proc/device-tree/model 2>/dev/null || uname -m), $(git log --oneline -1)"

WAS_ACTIVE=0
if systemctl --user is-active --quiet gaugemancy 2>/dev/null; then
	WAS_ACTIVE=1
	systemctl --user stop gaugemancy
fi

echo
echo "== 1. Offline benchmark (preset build/free times, CPU during a fade)"
bin/Gaugemancy --data "$DATA" --profile "$PROFILE" --benchmark "${BENCH_SECONDS:-8}" 2>/dev/null

echo
echo "== 2. Live test with the sound device"
bin/Gaugemancy --headless --data "$DATA" --profile "$PROFILE" --sensors sim > "$APP_DIR/crossfade_app.log" 2>&1 &
APP_PID=$!
sleep "${START_WAIT:-12}"
if ! kill -0 "$APP_PID" 2>/dev/null; then
	echo "the app did not start; see crossfade_app.log"
	tail -20 "$APP_DIR/crossfade_app.log"
	STATUS=2
else
	grep "gaugemancy" "$APP_DIR/crossfade_app.log" | head -12
	python3 tools/crossfade_stress.py --data "$DATA" --profile "$PROFILE" --pid "$APP_PID" ${QUICK:+--quick}
	STATUS=$?
	python3 tools/gm-ctl.py quit
	sleep 2
	kill "$APP_PID" 2>/dev/null
	echo
	echo "warnings from the app during the test:"
	grep -i "warning\|error\|underrun\|xrun\|NOT RUNNING" "$APP_DIR/crossfade_app.log" | grep -v "^ALSA lib" | tail -15 || true
fi

if [[ $WAS_ACTIVE -eq 1 ]]; then
	systemctl --user start gaugemancy
fi
echo
echo "log saved to $LOG"
exit "$STATUS"
