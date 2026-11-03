# Building on a Raspberry Pi 3 Model B Plus (1 GB RAM)

`docs/RaspberryPi.md` assumes a Pi 4 or 5. This one is a straight record of
what had to change to get openFrameworks and Gaugemancy built on a **Pi 3B+**
with **905 MB of RAM** (`free -h`'s view of 1 GB) running Raspberry Pi OS
Trixie (Desktop image, not Lite) - done 2026-11-03, over SSH, following
`docs/RaspberryPi.md`'s own steps until they didn't work.

## 1. `scripts/linux/debian/install_dependencies.sh` fails outright

```
E: Unable to locate package libgconf-2-4
error installing dependencies, there could be an error with your internet connection
```

`libgconf-2-4` is a long-deprecated GNOME config library that no longer
exists in Debian trixie's repos at all - it's not a networking problem, the
package is simply gone. Because it's bundled into one big `apt-get install`
line with ~40 other packages, apt refuses the *entire* line over this one
unknown name, so nothing in that line gets installed, not just the missing
one.

**Fix:** run the same line yourself with `sudo`, with `libgconf-2-4` removed.
Copy the package list straight out of the script (everything in the
`apt-get install -y \ ... \` block right before the `exit_code=$?` check) -
on this Pi, that was:

```bash
sudo apt-get install -y \
  freeglut3-dev libasound2-dev libxmu-dev libxxf86vm-dev g++ \
  libgl1-mesa-dev libglu1-mesa-dev libraw1394-dev libudev-dev libdrm-dev \
  libglew-dev libopenal-dev libsndfile-dev libfreeimage-dev libcairo2-dev \
  libfreetype6-dev libssl-dev libpulse-dev libusb-1.0-0-dev \
  libgtk-3-dev libopencv-dev \
  libegl1-mesa-dev libgles1 libgles2-mesa-dev \
  libassimp-dev librtaudio-dev libglfw3-dev \
  liburiparser-dev libcurl4-openssl-dev libpugixml-dev libpoco-dev \
  brotli liblapack-dev libblas-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
  x11proto-dev libwayland-dev wayland-protocols libxkbcommon-dev libxkbcommon-x11-dev \
  libdecor-0-dev libglvnd-dev mesa-utils ccache
```

Then continue with the script's own next step by hand (gstreamer, detecting
version 1.0 since `libgstreamer1.0-dev` exists on trixie):

```bash
sudo apt-get install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-libav gstreamer1.0-pulseaudio gstreamer1.0-x \
  gstreamer1.0-plugins-bad gstreamer1.0-alsa gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

`./download_libs.sh` (the next step) ran fine unmodified.

## 2. `-j4` is too much; even `-j2` can take the Pi down

OF core (`make -j2 Release` in `libs/openFrameworksCompiled/project`)
compiled cleanly with `-j2` - slow (tens of minutes), swap climbed to
~300 MB at times, but it finished with 0 errors and recovered memory between
files.

Building Gaugemancy itself (`make -j2 Release` in the app folder, which
also compiles `ofxPDSP`/`ofxAudioFile`/`ofxGPIO`/`ofxMidi`/`ofxGui`/`ofxOsc`
for the first time) **took the Pi down**: it stopped accepting new SSH
connections entirely (`Connection timed out during banner exchange`) while
still answering `ping` - i.e. the kernel/network stack was alive but
userspace was too starved to fork a new sshd session. It needed a physical
power-cycle to recover; a stuck interactive SSH session gave no further
output either. Likely made worse by Raspberry Pi OS's own background
update checker (PackageKit/`gui-updater`) - `apt`'s history log showed a
large automatic update (Firefox, Chromium, Qt6, GDAL, ~250 packages) that
had just finished minutes earlier, and this is the Desktop image, which
runs that checker by default; it may have still been doing background work
(metadata refresh, etc.) that doesn't show up as a completed transaction.

**Fix: use `-j1` for the Gaugemancy/addons build specifically**, even though
`-j2` was fine for OF core. After the power-cycle, resuming with
`make -j1 Release` picked up from `ccache` (already enabled by OF's build
system by default) for everything already compiled before the crash, and
completed the rest file-by-file without incident, memory recovering between
each one.

```bash
cd ~/openFrameworks/apps/myApps/Gaugemancy
make -j1 Release
```

## 3. If -j1 still isn't enough: real swap and the desktop GUI

Even at `-j1`, this Pi's "swap" is **zram** (RAM-backed compressed swap) by
default - it doesn't add real headroom, since the compressed pages still
live in RAM. The kernel OOM-killer killed `cc1plus` (the compiler itself,
~250 MB RSS) mid-build with zram showing as "mostly free". Two fixes, both
safe to do over SSH without losing access (SSH is its own service,
independent of either of these):

**Add a real disk swapfile** alongside zram (2 GB was plenty, 20 GB was free
on the SD card):

```bash
sudo fallocate -l 2G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab   # survive reboots
```

**Disable the desktop GUI entirely** if programming always happens on a
laptop anyway (per the project's own ground rules) - frees real memory the
desktop session was holding (compositor, panel, the GUI update checker),
takes effect immediately with no reboot, and doesn't touch SSH:

```bash
sudo systemctl set-default multi-user.target   # boots to console from now on
sudo systemctl isolate multi-user.target        # stop the desktop right now
```

Console auto-login (if `raspi-config` had it set up, check with
`cat /etc/systemd/system/getty@tty1.service.d/*.conf`) survives this
unchanged - it's a separate `agetty --autologin` config, not tied to the
graphical session. Revert with
`sudo systemctl set-default graphical.target` (+ isolate or reboot).

With both of these, the same `-j1` build that had been getting OOM-killed
completed cleanly.

## 4. Missing `-ljack` at link time

```
/usr/bin/ld: .../ofxMidi/libs/rtmidi/RtMidi.o: undefined reference to symbol 'jack_port_name'
/usr/bin/ld: /lib/aarch64-linux-gnu/libjack.so.0: error adding symbols: DSO missing from command line
```

`ofxMidi`'s bundled `rtmidi` is compiled with `__UNIX_JACK__` (a default OF
compile flag on Linux), but on this Pi `libjack` wasn't installed - in fact
apt's history log showed the very first automatic system update had
*removed* `libjack-jackd2-0`, which is almost certainly what broke this (it
was presumably present, as a transitive dependency of something else,
before that update ran).

`sudo apt-get install -y libjack-jackd2-dev` fixes the *missing library*
half of it, but **not** the actual link error: `ofxMidi`'s
`addon_config.mk` already says `ADDON_PKG_CONFIG_LIBRARIES = alsa jack`,
and `pkg-config --libs jack` correctly returns `-ljack` once the package is
installed, but that never makes it onto this project's actual link command
- the addon pkg-config resolution mechanism doesn't pick it up here, for a
reason not tracked down. (If you go looking: `config.project.mk` in
`libs/openFrameworksCompiled/project/makefileCommon/` is **not** a build
cache safe to delete - it's a file tracked in openFrameworks' own git
history; deleting it just breaks the build differently. That was a dead
end, not the fix.)

The actual fix is in Gaugemancy's own `config.make`:

```make
PROJECT_LDFLAGS += -ljack
```

## 5. Watching a long build without blocking the terminal

Over SSH, background it and tail the log rather than waiting on a foreground
command (useful given how long this takes on this hardware):

```bash
cd ~/openFrameworks/apps/myApps/Gaugemancy
nohup make -j1 Release > ~/gaugemancy_build.log 2>&1 &
disown
# check in on it:
tail -f ~/gaugemancy_build.log      # Ctrl-C just stops watching, doesn't stop the build
grep -c 'error:' ~/gaugemancy_build.log
pgrep -fl 'make -j1'                # empty once it's done
free -h                             # watch for available dropping toward 0
```

## 6. Moving the SD card to the sensor hardware: what `install.sh` misses

Section 1-5 above were all done on a Pi 3B+ with no sensor circuit attached,
to get a clean build. Moving that SD card to the real sensor-equipped unit
(a *different* Pi 3B) and running `bin/Gaugemancy --selftest` there surfaced
four more real gaps, found 2026-11-04:

**SPI never actually enabled, despite `install.sh` having been run.**
`gauge`'s group memberships (`spi`, `i2c`, `gpio`, ...) were already correct,
but `/dev/spidev0.0` didn't exist at all and `/boot/firmware/config.txt` still
had `#dtparam=spi=on` / `#dtparam=i2c_arm=on` commented out. `install.sh`'s
`raspi-config nonint do_spi 0` / `do_i2c 0` step had evidently never actually
run on this SD card (only its group-setup step had, from a different occasion)
- it just hadn't mattered before, since nothing was wired up to test with.
**Fix:** re-run `scripts/pi/install.sh` on the real hardware, then
`sudo reboot` - a `dtparam` change needs a reboot to create the device nodes,
simply restarting the app isn't enough. After that, the I2C bus scan and all
6 MCP3008 pressure-sensor channels passed `--selftest` cleanly.

**Audio fell back to the wrong device.** `profile.json`'s `audio.pi` override
(see `docs/RaspberryPi.md`'s "Two devices, one file") had been added to the
settings schema but never actually filled in with a real value for any
profile - `enabled: false` everywhere. On the Pi, that leaves the base
`audio.deviceId` (a macOS-specific number) in effect, which doesn't exist on
Linux, so RtAudio silently fell back to its own idea of "default" (the
onboard `bcm2835 Headphones`, not the instrument's USB sound card). **Fix:**
`bin/Gaugemancy --list-audio` on the Pi to get the card's exact name (here,
"Sound Blaster Play! 3"), then set `audio.pi.enabled: true` and
`audio.pi.deviceName` to (part of) that name, in every profile - it's the
same physical card regardless of which performer's profile is active.

**Volume resets to the USB DAC's power-on default every time Gaugemancy opens
its audio stream** - *including* right after you've just set and
`alsactl store`'d it. The system-wide `alsa-restore.service` runs too early
in boot to restore a USB card's state anyway (it races the card's own
enumeration and loses), but the real culprit is worse: opening the Sound
Blaster's ALSA PCM stream itself resets its hardware gain, so even an
`ExecStartPre` mixer command in the systemd service gets overwritten the
moment the app's `ExecStart` opens the device a moment later. **Fix:** set it
in `ExecStartPost` instead, with a few seconds' delay so it runs *after* that
reset, not before it - see `scripts/pi/gaugemancy.service`. Address the card
by name (`amixer -c S3 ...`, its ALSA card id), not by number, since USB card
numbering can shift across boots.

**`hardware.shutdownOnQuit` (long-press button -> power off) did nothing.**
`src/ofApp.cpp`'s shutdown path runs `sudo -n systemctl poweroff`, which
needs passwordless sudo - the code's own comment calls this "the default on
Raspberry Pi OS", but that's only true for the stock `pi` user's
`010_pi-nopasswd` sudoers entry, not for a custom user like `gauge`, whose
sudo is fully password-protected (`sudo -l` just shows `(ALL : ALL) ALL`).
It silently "worked" in manual testing only because of a cached sudo
timestamp from an earlier interactive `ssh -t` session - the systemd service
has no tty and no such cache, so it failed every time in practice. Confirmed
with `sudo -K` (drop the cached ticket) then `sudo -n -l` before trusting any
test of this. **Fix:** `scripts/pi/install.sh` now installs a minimal
`/etc/sudoers.d/gaugemancy-poweroff` granting
`NOPASSWD: /usr/bin/systemctl poweroff` and nothing else, validated with
`visudo -cf` before it's put in place (see `scripts/pi/gaugemancy-poweroff.sudoers`).
The triple-click relay toggle (speaker on/off) needs no sudo at all and
always worked - don't let that fool you into thinking poweroff should too.

## Summary for next time

- Expect the whole thing (deps + OF core + Gaugemancy + addons, first build)
  to take well over an hour on a Pi 3B+.
- Fix `install_dependencies.sh` by hand (drop `libgconf-2-4`) before running
  it, or it fails on the first line.
- `-j2` is fine for OF core; drop to `-j1` for the app/addons build, or it
  can take the whole machine down, not just slow it down.
- If it does hang (SSH banner exchange times out, ping still works): it's
  not coming back on its own on this hardware - power-cycle it, then resume
  with `-j1` and let `ccache` pick up what already compiled.
- If `-j1` *still* gets OOM-killed: add a real disk swapfile (zram alone
  isn't real headroom) and, if the Pi only ever gets programmed from a
  laptop anyway, disable the desktop GUI (`multi-user.target`) - both safe
  over SSH, no reboot needed, don't touch SSH access either way.
- The link will fail on `undefined reference to jack_port_name` unless
  `libjack-jackd2-dev` is installed *and* `PROJECT_LDFLAGS += -ljack` is
  added to `config.make` - the addon's own `pkg-config`-based LDFLAGS
  resolution doesn't add it automatically here.
- This was the actual end-to-end result on 2026-11-03: `bin/Gaugemancy`
  built and ran on this Pi 3B+ (no sensor circuit attached to it) -
  `--selftest` passed GPIO/LEDs/button, correctly reported SPI/I2C as
  unconnected, and found the onboard audio device.
- Moving the SD card to real sensor hardware doesn't mean you're done: re-run
  `install.sh` and reboot (SPI/I2C `dtparam`s need it), fill in each
  profile's real `audio.pi` device name once you know it, and don't trust a
  manual `sudo -n ...` test of the poweroff path without `sudo -K` first -
  a cached ssh timestamp will lie to you about what the headless service can
  actually do.
