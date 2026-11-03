# Gaugemancy on the Raspberry Pi

The instrument runs headless on a Raspberry Pi (Pi 4 or 5) with Raspberry Pi OS
**Trixie, 64 bit, with the Raspberry Pi Desktop**, and openFrameworks built from
the `main` branch. Everything below is done over SSH from a Mac or Linux machine.

**On a Pi 3B+ (1 GB RAM) instead of a 4/5**, see
`docs/RaspberryPi3BPlus.md` first - `install_dependencies.sh` fails outright
on current Trixie repos, and `make -j4` here can take the whole machine down,
not just run slowly.

## 1. openFrameworks and addons

```bash
cd ~
git clone --depth 1 https://github.com/openframeworks/openFrameworks.git
cd openFrameworks/scripts/linux
sudo ./debian/install_dependencies.sh
./download_libs.sh
cd ../../libs/openFrameworksCompiled/project && make -j4 Release

cd ~/openFrameworks/addons
git clone -b ai-refactor https://github.com/fred-dev/ofxPDSP.git
git clone -b ai-refactor https://github.com/fred-dev/ofxAudioFile.git
git clone -b ai-refactor https://github.com/fred-dev/ofxGPIO.git
git clone https://github.com/danomatika/ofxMidi.git
cd ~/openFrameworks/apps/myApps
git clone -b ai-refactor https://github.com/fred-dev/Gaugemancy.git
```

## 2. Pi setup (once)

```bash
cd ~/openFrameworks/apps/myApps/Gaugemancy
scripts/pi/install.sh        # SPI, I2C, groups, systemd user service
sudo reboot                  # groups apply after a new login
```

## 3. Build and check

```bash
cd ~/openFrameworks/apps/myApps/Gaugemancy
make -j4 Release
make -C tests                          # unit tests
python3 tools/make_test_data.py        # Demo profiles, test audio, recordings
bin/Gaugemancy --selftest              # GPIO, ADC, I2C scan, accelerometer, files, audio
bin/Gaugemancy --benchmark 20          # CPU per preset of the active profile
bin/Gaugemancy --profile Demo --benchmark 20
```

`--selftest` blinks the LEDs, reads every sensor for half a second (squeeze the
balls while it runs to see the range), scans the whole I2C bus (every address
that answers, not just the configured one - useful if the accelerometer isn't
showing up where expected), checks every sample file and lists the audio
devices. `--benchmark` renders each preset offline with every ball being
squeezed; keep the average well under 50% and the worst block under 80%.

The ADC and button can also be checked without the app:
`make -C ../../../addons/ofxGPIO/tools/gpio-probe` then
`gpio-probe adc /dev/spidev0.0 6`, `gpio-probe watch 19 pullup activelow`.

## 4. Settings on a USB stick

The instrument reads `/media/Data/gaugemancy.json` if it exists, otherwise the
app's `bin/data`. To use a stick labelled `DATA`, add to `/etc/fstab`:

```
LABEL=DATA  /media/Data  vfat  defaults,nofail,uid=1000,gid=1000,umask=022  0  0
```

then `sudo mkdir -p /media/Data && sudo mount -a`. Copy the `bin/data` folder
contents (gaugemancy.json, the performer folders with `audio/`) onto the stick.
Old XML settings convert with `python3 tools/convert_xml_settings.py /media/Data`.

## 5. Audio device

Trixie routes audio through PipeWire. Run `bin/Gaugemancy --list-audio` and put
part of the device name in `profile.json` (`"audio": {"deviceName": "USB"}`):
names survive reboots and replugging, numeric ids do not. For the lowest
latency on a dedicated USB interface, `"api": "alsa"` with the `hw:` device
bypasses PipeWire (the device is then exclusive to Gaugemancy).

**Two devices, one file.** `profile.json`'s `audio` block is used as-is on a
laptop. A separate `audio.pi` block (`enabled`, `api`, `deviceName`,
`deviceId`) overrides it instead whenever the binary is actually running on
Pi hardware (`Instrument::hardwareExpected()` - Linux + arm/aarch64), so the
same profile file needs no editing when copied between a laptop and the
instrument's USB sound card. Set it either by hand or from the programming
GUI's "Audio device on the Pi" panel:

```json
"audio": {
  "deviceName": "MacBook Pro Speakers",
  "pi": { "enabled": true, "api": "alsa", "deviceName": "USB", "deviceId": -1 }
}
```

## 6. Running

```bash
systemctl --user start gaugemancy      # also starts at boot after install.sh
journalctl --user -u gaugemancy -f     # live log
systemctl --user stop gaugemancy       # stop it before running by hand:
bin/Gaugemancy --headless --sensors replay:bin/data/recordings/each_ball.csv
```

From the Mac/Linux machine:

```bash
scripts/remote.sh pi@gaugemancy.local build       # sync this checkout, build
scripts/remote.sh pi@gaugemancy.local selftest
scripts/remote.sh pi@gaugemancy.local log
tools/gm-ctl.py --host gaugemancy.local status    # OSC: state + sensor levels
tools/gm-ctl.py --host gaugemancy.local watch     # live levels
tools/gm-ctl.py --host gaugemancy.local next      # next preset
tools/gm-ctl.py --host gaugemancy.local record /tmp/take1.csv   # record a performance
```

## 7. Moving the SD card to the instrument (sensor circuit) hardware

If the app was built on a Pi that isn't wired to the sensor circuit (e.g.
built remotely, SD card moved to the real instrument after), run
`bin/Gaugemancy --selftest` first thing once it's on the real hardware and
read it top to bottom - it covers everything below in one pass:

- **GPIO**: lists `/dev/gpiochip*`, requests the button/LED/relay pins,
  blinks blue/red/both, prints whether the button currently reads
  pressed/released. If this section fails, nothing else (ADC, I2C) will
  work either - fix it first.
- **Pressure sensors (MCP3008, SPI)**: reads all 6 channels for ~0.5s.
  Squeeze each ball while it runs - min/max per sensor should move. A
  channel stuck at 0 reads as disconnected; stuck near 1023 as
  saturated/shorted; a resting value above `sensors.maxRaw` or noisy at
  rest are both flagged too.
- **I2C bus scan**: tries every address 0x03-0x77, not just the one in
  `hardware.accelAddress` - if the accelerometer has been flaky, check this
  first. Nothing responding at all usually means I2C isn't enabled
  (`sudo raspi-config nonint do_i2c 0`, then reboot) or it's a wiring/power
  problem, not a software one. LIS3DH is normally at 0x18 (SA0 low) or 0x19
  (SA0 high) - if the scan finds a device at the *other* one, update
  `hardware.accelAddress` (decimal: 24 or 25) rather than rewiring.
- **Accelerometer (LIS3DH)**: opens the configured address specifically and
  reads one x/y/z sample in g. This can still fail even when the bus scan
  found *something* at that address, if it's a different chip or the LIS3DH
  needs its power-up register written (`CTRL_REG1`) before it'll respond -
  see `docs/../tests/CLAUDE.md`'s "Bugs found in the original code" if this
  looks familiar.
- **Audio output devices**: run `--list-audio` separately to get the exact
  name of the instrument's USB sound card, then see "Two devices, one file"
  above to set `audio.pi.deviceName` so this profile keeps working when
  copied back to a laptop.

## Hardware wiring (BCM numbering, configurable in profile.json `hardware`)

| Part | Connection |
|---|---|
| MCP3008 ADC | SPI0 CE0 (`/dev/spidev0.0`), sensors on channels 0-5 |
| LIS3DH accelerometer | I2C bus 1 (`/dev/i2c-1`), address 0x18 |
| Button | GPIO 19 to ground (internal pull-up, active low) |
| Blue LED | GPIO 5 |
| Red LED | GPIO 6 |
| Speaker relay | GPIO 13 (pulsed for 400 ms to toggle) |

The speaker relay also pulses automatically: once at the very end of startup
(after settings, hardware, sensors, audio and OSC are all ready - not before,
so a slow audio device open can't leave the speaker on with nothing playing
yet) and once during shutdown, before the LEDs are turned off. Both are a
blocking pulse (`ControlPanel::pulseRelayBlocking`), unlike the main loop's
button-triggered pulse - acceptable since each only runs once, outside the
per-frame update. Since the relay is a plain toggle switch with no readable
state, this relies on it actually being off when the instrument starts (same
assumption the original pre-refactor app made).

## Button

| Action | Effect |
|---|---|
| double click | narration: skip to the instrument; otherwise next preset |
| triple click | toggle the speaker (relay pulse) |
| hold 3 s | recalibrate: balls at rest and "level" (keep still) |
| hold 6 s | quit (if `hardware.longPressQuits`), and power off if `shutdownOnQuit` |

`shutdownOnQuit` runs `sudo -n systemctl poweroff` (`src/ofApp.cpp`), which
needs the passwordless sudo grant `scripts/pi/install.sh` sets up in
`/etc/sudoers.d/gaugemancy-poweroff` - see `docs/RaspberryPi3BPlus.md` if
it's not working after moving to a new SD card or user account.

LEDs: preset 1 blue, 2 blue+red, 3 blue blinking, 4 blue+red blinking;
narration red.
