#!/bin/bash
# -----------------------------------------------------------------------------
# Work on the Raspberry Pi from a Mac or Linux machine over SSH.
#
#   scripts/remote.sh <user@pi-host> <command>
#
# Commands
#   sync        copy this checkout to the Pi (rsync, excludes builds and audio)
#   build       sync, then build on the Pi (make -j4 Release)
#   test        run the unit tests on the Pi
#   selftest    run bin/Gaugemancy --selftest on the Pi
#   benchmark   run bin/Gaugemancy --benchmark 20 on the Pi
#   restart     restart the systemd service
#   log         follow the service log (Ctrl-C to stop)
#   stop/start  stop or start the service (stop it before running by hand)
#   fable       switch the Pi to the fable branches and rebuild (no sync)
#   crossfade   run the crossfade test on the Pi (PROFILE=<name> to choose)
#   ssh         open a shell in the app folder
#
# Environment: GM_REMOTE_DIR (default ~/openFrameworks/apps/myApps/Gaugemancy)
# For OSC control from this machine use tools/gm-ctl.py --host <pi-host> ...
# -----------------------------------------------------------------------------
set -euo pipefail

HOST="${1:?usage: scripts/remote.sh <user@host> <command>}"
CMD="${2:?usage: scripts/remote.sh <user@host> <command>}"
DIR="${GM_REMOTE_DIR:-openFrameworks/apps/myApps/Gaugemancy}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"

run() { ssh -t "$HOST" "cd $DIR && $*"; }

case "$CMD" in
	sync)
		rsync -az --delete --exclude obj/ --exclude 'bin/Gaugemancy*' --exclude '*.wav' \
			--exclude tests/test_core --exclude .git/ "$HERE/" "$HOST:$DIR/"
		;;
	build) "$0" "$HOST" sync && run "make -j4 Release" ;;
	test) run "make -C tests" ;;
	selftest) run "bin/Gaugemancy --selftest" ;;
	benchmark) run "bin/Gaugemancy --benchmark 20" ;;
	restart) ssh "$HOST" systemctl --user restart gaugemancy ;;
	stop) ssh "$HOST" systemctl --user stop gaugemancy ;;
	start) ssh "$HOST" systemctl --user start gaugemancy ;;
	log) ssh -t "$HOST" journalctl --user -u gaugemancy -f ;;
	fable) run "scripts/pi/update_to_fable.sh" ;;
	crossfade) run "PROFILE=${PROFILE:-fable} scripts/pi/crossfade_test.sh" ;;
	ssh) ssh -t "$HOST" "cd $DIR && exec \$SHELL -l" ;;
	*) echo "unknown command '$CMD'"; exit 2 ;;
esac
