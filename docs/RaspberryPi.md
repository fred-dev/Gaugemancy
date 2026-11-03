# Gaugemancy on the Raspberry Pi

The instrument runs headless on a Raspberry Pi (Pi 4 or 5) with Raspberry Pi OS
**Trixie, 64 bit, with the Raspberry Pi Desktop**, and openFrameworks built from
the `main` branch. Everything below is done over SSH from a Mac or Linux machine.

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
bin/Gaugemancy --selftest              # GPIO, ADC, accelerometer, files, audio
bin/Gaugemancy --benchmark 20          # CPU per preset of the active profile
bin/Gaugemancy --profile Demo --benchmark 20
```

`--selftest` blinks the LEDs, reads every sensor for half a second (squeeze the
balls while it runs to see the range), checks every sample file and lists the
audio devices. `--benchmark` renders each preset offline with every ball being
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

## Hardware wiring (BCM numbering, configurable in profile.json `hardware`)

| Part | Connection |
|---|---|
| MCP3008 ADC | SPI0 CE0 (`/dev/spidev0.0`), sensors on channels 0-5 |
| LIS3DH accelerometer | I2C bus 1 (`/dev/i2c-1`), address 0x18 |
| Button | GPIO 19 to ground (internal pull-up, active low) |
| Blue LED | GPIO 5 |
| Red LED | GPIO 6 |
| Speaker relay | GPIO 13 (pulsed for 400 ms to toggle) |

## Button

| Action | Effect |
|---|---|
| double click | narration: skip to the instrument; otherwise next preset |
| triple click | toggle the speaker (relay pulse) |
| hold 3 s | recalibrate: balls at rest and "level" (keep still) |
| hold 6 s | quit (if `hardware.longPressQuits`), and power off if `shutdownOnQuit` |

LEDs: preset 1 blue, 2 blue+red, 3 blue blinking, 4 blue+red blinking;
narration red.
