# Gaugemancy: handoff notes for Claude (read this first)

This file is the running status of the refactor so a new session (local on
Mac/Windows/Linux, or on the Raspberry Pi over SSH) can pick up where the last
one stopped. **Update it after every major milestone.**

Last updated: 2026-11-03 (cloud session, after motion, 3D view, Pi scripts and docs).

## Ground rules from the owner (Fred)

- Work on the `ai-refactor` branch in every repo; commit and push there.
- Never modify openFrameworks core or the standard OF addons (ofxGui, ofxOsc...).
- Addon changes go into Fred's forks only. If another addon needs changing,
  **ask Fred to fork it first**, then make the change in the fork.
- Each addon must build on every platform from its own `addon_config.mk`
  (projects need no extra flags). Keep existing upstream flags; only add flags
  that are real optimisations the project generator understands.
- Target: Raspberry Pi OS Trixie (64 bit, Raspberry Pi Desktop) + openFrameworks
  `main` from git. The Pi runs headless; Mac/Windows/Linux run the programming GUI.
- Code style: small classes, short functions, clear comments, safe on all platforms.
- User-facing text: no em dashes.

## Repositories (all on branch `ai-refactor`)

| Repo | Role | State |
|---|---|---|
| fred-dev/Gaugemancy | the app | rewritten, builds and runs on OF main (Linux x86 tested) |
| fred-dev/ofxPDSP | DSP | builds on OF main; `lockPatching()`, `renderOffline()`, denormal flush |
| fred-dev/ofxGPIO | Pi GPIO/SPI/I2C | rewritten on GPIO chardev v2 + spidev + i2c-dev; legacy API kept |
| fred-dev/ofxAudioFile | sample decoding | memory bugs fixed, decoders updated; recommended for `master` too |
| danomatika/ofxMidi | dependency of ofxPDSP | upstream works on OF main, no fork needed |

Addons in `addons.make` (one file for every platform): ofxGui ofxOsc
ofxAudioFile ofxMidi ofxPDSP ofxGPIO. ofxGPIO compiles to stubs off Linux.

## Layout of the app

```
src/core/      openFrameworks-free logic (unit tested): settings + JSON,
               sensor processing, curves, gestures, mode state machine,
               simulator, CSV recording, options
src/audio/     Effect + Effects.cpp (8 effects), EffectChain, GrainSlot,
               NarrationVoice, AudioEngine (owns pdsp::Engine)
src/hardware/  SensorSource (MCP3008/LIS3DH, replay, simulation), ControlPanel
src/app/       Instrument (runtime shared by Pi and desktop), RemoteControl
               (OSC), Diagnostics (--selftest/--benchmark), FramePacer
src/gui/       ProgrammingGui, GuiBuilder (controls bound to settings), Views,
               DeviceView (3D instrument: squeeze balls, drag to tilt)
tools/         convert_xml_settings.py, make_test_data.py, gm-ctl.py (OSC)
scripts/       pi/install.sh + pi/gaugemancy.service (systemd user), remote.sh (SSH)
docs/          RaspberryPi.md (setup, wiring, service, SSH workflow)
tests/         unit tests (no OF needed), this file

Motion: core/Smoother.h (time-constant smoothing + SmoothingBank, one per
modulation source), core/MotionProcessor (level = startup orientation or a
fixed up axis; outputs tiltX, tiltY, tilt, shake as modulation sources).
Sensor ball numbering in the 3D view: 1 top, 2 bottom, 3 side A upper,
4 side B lower, 5 side C upper, 6 side D lower (ADC channels in profile).
bin/data/      gaugemancy.json, one folder per performer profile, recordings/
```

Settings are JSON: `bin/data/gaugemancy.json` (active profile),
`<Profile>/profile.json`, `<Profile>/presets/preset_N.json`, audio in
`<Profile>/audio/`. On the Pi the data root is `/media/Data` (USB stick) when
`/media/Data/gaugemancy.json` exists. The old XML files are still there; all
five performers were converted with `tools/convert_xml_settings.py`.

## Build and test

```bash
# unit tests (any OS with a C++17 compiler; needs OF's bundled nlohmann json)
make -C tests                      # OF_ROOT defaults to ../../../..
make -C tests SANITIZE=1           # ASan/UBSan

# app (Linux / Pi), project inside openFrameworks/apps/myApps/Gaugemancy
make -j4 Release
python3 tools/make_test_data.py    # Demo profiles + WAVs + recordings
bin/Gaugemancy --help
bin/Gaugemancy --profile Demo --benchmark 10        # offline CPU report
bin/Gaugemancy --selftest                           # hardware/files/audio check
bin/Gaugemancy --headless --profile Demo --sensors replay:bin/data/recordings/each_ball.csv
bin/Gaugemancy --gui --profile Demo --sensors sim   # programming window
```

Other addon tests: `make -C ../../../addons/ofxGPIO/tests` (and `stubs`),
`make -C ../../../addons/ofxAudioFile/tests` (needs ffmpeg),
`make -C ../../../addons/ofxGPIO/tools/gpio-probe`.

Cloud-session tricks: no sound card or GPIO in the container, so use
`--no-audio`; GUI screenshots with `Xvfb :99 &` + `DISPLAY=:99 ... &` +
`import -window root shot.png`.

## Verified so far (cloud, Ubuntu 24.04 x86-64, gcc 13, OF main 2026-11-02)

- All 4 Gaugemancy-related forks build on OF main; ofxPDSP example links.
- 47 core unit tests pass (also under ASan/UBSan); ofxGPIO and ofxAudioFile tests pass.
- 3D view renders (Xvfb screenshot), squeeze/tilt via gm-ctl.py over OSC works.
- Benchmark (x86, 512 buffer): Demo light preset ~2% of a core, stress preset
  (42 effects) ~9%.
- Headless loop: <1% CPU after FramePacer (was ~25% with ofSetFrameRate).
- GUI renders under Xvfb; replay drives modes correctly.

## NOT verified yet (needs a local session)

- [ ] Raspberry Pi (Trixie, 64 bit): build, `--selftest` with real hardware
      (button pin 19 pull-up active-low, LEDs 5/6, relay 13, MCP3008 on
      spidev0.0, LIS3DH 0x18), `--benchmark` numbers, real audio device by name.
- [ ] macOS build (Xcode project is from the old code and must be regenerated
      with the project generator; the Makefile build should work).
- [ ] Windows build (Visual Studio via project generator, or msys2).
- [ ] Listening tests: effect chains, narration pause/resume, glitch.

## Done since the first handoff

- Motion processing and smoothing bank, tilt/shake modulation sources.
- 3D instrument view (DeviceView) with squeeze and tilt simulation.
- gm-ctl.py, Pi install script + systemd user service, remote.sh, docs, README.
- pm2 `ecosystem.config.js` removed (replaced by systemd).

## Next steps (in order)

1. Local platform testing (see "NOT verified yet"); fix what breaks.
2. Regenerate the Xcode and Visual Studio projects with the project generator.
3. Confirm the physical sensor numbering and ADC channels against the real
   instrument; adjust `hardware.adcChannels` defaults and the DeviceView table.
4. Confirm the LIS3DH axis orientation on the board (absolute "up" axis).
5. Listening pass on converted performer presets (delay/reverb are now inserts
   with a wet mix rather than parallel sends; levels may need a trim).

## Bugs found in the original code (for the record)

- Combinatorial effect patching: delay/decimator paths skipped units, the
  delay/reverb "send" Amps were mono and shared by L and R.
- Effect units allocated twice (populateEffectVectors called twice) and leaked
  on every preset switch; DSP repatched from the main thread while audio ran.
- `applyDynamicValuesToParameters` copied four vectors and a string ~300 times
  per frame; verbose log strings built every frame even when not printed.
- ofSetFrameRate busy-wait (see FramePacer) and blocking `ofSleepMillis(400)`
  relay pulses in the loop.
- LIS3DH never powered up (CTRL_REG1 not written) and its scale was wrong.
- `exponentialEaseOut`/`quarticEaseOut` divided by zero (unused).
- Narration "pause" kept the sampler running, so the glitch drifted.
- `setupButton()` set the direction on the blue LED instead of the button.
- `loadRoutine` wrote file names to `fileNamesSet[presetIndex]` (off by one).
- sysfs GPIO: broken on current kernels (offset numbering, Pi 5 chip).
