#!/bin/bash
# -----------------------------------------------------------------------------
# Move the Raspberry Pi to the `fable` branches and rebuild. Run ON the Pi:
#
#   cd ~/openFrameworks/apps/myApps/Gaugemancy && scripts/pi/update_to_fable.sh
#
# Branches after this: Gaugemancy `fable`, ofxPDSP `fable` (the app needs it),
# ofxGPIO and ofxAudioFile `ai-refactor` (unchanged), ofxMidi and ofxImGui as
# they are. Local changes are never thrown away: the script stops if a
# repository has uncommitted work. JOBS=2 scripts/pi/update_to_fable.sh uses
# two compile jobs (the default of 1 is the safe choice on a Pi 3B+).
# -----------------------------------------------------------------------------
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ADDONS="$(cd "$APP_DIR/../../../addons" && pwd)"
JOBS="${JOBS:-1}"

step() { printf '\n== %s\n' "$*"; }

switch() { # switch <dir> <branch>
	local dir="$1" branch="$2"
	if [[ ! -d "$dir/.git" ]]; then
		echo "MISSING: $dir"
		return 1
	fi
	if [[ -n "$(git -C "$dir" status --porcelain --untracked-files=no)" ]]; then
		echo "STOP: $dir has uncommitted changes. Commit or stash them, then run this again."
		git -C "$dir" status --short --untracked-files=no | head -10
		return 1
	fi
	git -C "$dir" fetch --quiet origin "$branch"
	git -C "$dir" checkout --quiet -B "$branch" "origin/$branch"
	echo "$(basename "$dir"): $branch @ $(git -C "$dir" log --oneline -1)"
}

step "Switch branches"
switch "$APP_DIR" fable
switch "$ADDONS/ofxPDSP" fable
switch "$ADDONS/ofxGPIO" ai-refactor
switch "$ADDONS/ofxAudioFile" ai-refactor
for addon in ofxMidi ofxImGui; do
	if [[ -d "$ADDONS/$addon" ]]; then
		echo "$addon: $(git -C "$ADDONS/$addon" log --oneline -1)"
	else
		echo "MISSING: $ADDONS/$addon (see docs/RaspberryPi.md)"
		exit 1
	fi
done

step "Unit tests"
make -C "$APP_DIR/tests"

step "Build (make -j$JOBS Release; slow on a Pi 3B+)"
systemctl --user stop gaugemancy 2>/dev/null || true
make -C "$APP_DIR" -j"$JOBS" Release

step "Done. Next: scripts/pi/crossfade_test.sh"
