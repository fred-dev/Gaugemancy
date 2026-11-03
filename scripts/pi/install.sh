#!/bin/bash
# -----------------------------------------------------------------------------
# One-time setup of a Raspberry Pi (Raspberry Pi OS Trixie, 64 bit, Desktop)
# to run Gaugemancy. Run it ON the Pi from the app folder:
#
#   cd ~/openFrameworks/apps/myApps/Gaugemancy
#   scripts/pi/install.sh            # interfaces, groups, service
#   scripts/pi/install.sh --no-service
#
# Safe to run again. It does NOT edit /etc/fstab (see docs/RaspberryPi.md for
# mounting the USB stick) and does NOT build openFrameworks.
# -----------------------------------------------------------------------------
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALL_SERVICE=1
[[ "${1:-}" == "--no-service" ]] && INSTALL_SERVICE=0

step() { printf '\n== %s\n' "$*"; }

step "Enable SPI (ADC) and I2C (accelerometer)"
if command -v raspi-config >/dev/null; then
	sudo raspi-config nonint do_spi 0
	sudo raspi-config nonint do_i2c 0
else
	echo "raspi-config not found: enable dtparam=spi=on and dtparam=i2c_arm=on in /boot/firmware/config.txt"
fi

step "Give $USER access to GPIO, SPI, I2C and audio without sudo"
for group in gpio spi i2c audio; do
	if getent group "$group" >/dev/null; then
		sudo usermod -aG "$group" "$USER"
	fi
done
echo "(log out and back in, or reboot, for new groups to apply)"

step "Check the build tools and the Gaugemancy addons"
OF_ROOT="$(cd "$APP_DIR/../../.." && pwd)"
for addon in ofxPDSP ofxAudioFile ofxMidi ofxGPIO; do
	if [[ -d "$OF_ROOT/addons/$addon" ]]; then
		echo "  ok      $addon ($(git -C "$OF_ROOT/addons/$addon" branch --show-current 2>/dev/null || echo '?'))"
	else
		echo "  MISSING $addon  -> see docs/RaspberryPi.md"
	fi
done

if [[ $INSTALL_SERVICE -eq 1 ]]; then
	step "Install the systemd user service"
	mkdir -p "$HOME/.config/systemd/user"
	sed "s#@APP_DIR@#$APP_DIR#g" "$APP_DIR/scripts/pi/gaugemancy.service" > "$HOME/.config/systemd/user/gaugemancy.service"
	systemctl --user daemon-reload
	systemctl --user enable gaugemancy.service
	# Start the user session (and the instrument) at boot even without a login.
	sudo loginctl enable-linger "$USER"
	echo "installed; start now with: systemctl --user start gaugemancy"
fi

step "Done. Next: build (make -j4 Release), then bin/Gaugemancy --selftest"
