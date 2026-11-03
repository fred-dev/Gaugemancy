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

## 3. Watching a long build without blocking the terminal

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
