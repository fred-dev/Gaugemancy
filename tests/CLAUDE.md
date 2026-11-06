# Gaugemancy: handoff notes for Claude (read this first)

This file is the running status of the refactor so a new session (local on
Mac/Windows/Linux, or on the Raspberry Pi over SSH) can pick up where the last
one stopped. **Update it after every major milestone.**

Last updated: 2026-11-03 (local Mac session: first macOS build/run).

## Ground rules from the owner (Fred)

- Work on `main` in fred-dev/Gaugemancy itself (as of 2026-11-06 - the old
  `gui-rewrite` branch was renamed to `main`; the pre-rewrite codebase that
  used to be `main` is kept as `legacy_version`, reference only). The three
  addon forks below keep their own, separate `ai-refactor` branch - that
  name is unrelated to Gaugemancy's branches, don't conflate the two.
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

## Repositories

| Repo | Branch | Role | State |
|---|---|---|---|
| fred-dev/Gaugemancy | `main` | the app | GUI rewrite (ofxImGui) merged in; builds and runs on OF main |
| fred-dev/ofxPDSP | `ai-refactor` | DSP | builds on OF main; `lockPatching()`, `renderOffline()`, denormal flush |
| fred-dev/ofxGPIO | `ai-refactor` | Pi GPIO/SPI/I2C | rewritten on GPIO chardev v2 + spidev + i2c-dev; legacy API kept |
| fred-dev/ofxAudioFile | `ai-refactor` | sample decoding | memory bugs fixed, decoders updated; recommended for `master` too |
| danomatika/ofxMidi | `master` | dependency of ofxPDSP | upstream works on OF main, no fork needed |
| jvcleave/ofxImGui | `develop` | the programming GUI | added for the GUI rewrite |

Addons in `addons.make` (one file for every platform): ofxAudioFile ofxGPIO
ofxImGui ofxMidi ofxOsc ofxPDSP. ofxGPIO compiles to stubs off Linux.

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
- [ ] Windows build (Visual Studio via project generator, or msys2).
- [ ] Listening tests: effect chains, narration pause/resume, glitch.

## macOS, verified 2026-11-03 (Xcode 17F42, local, x86_64 build)

- All three forked addons (`ofxAudioFile`, `ofxPDSP`, `ofxGPIO`) switched to
  `fred-dev`'s `ai-refactor` branch in `openFrameworks/addons/`. ofxGPIO's
  rewrite builds clean off Linux (`--selftest` correctly reports "not a Linux
  build" and skips GPIO/ADC/accel checks instead of failing to compile).
- Old `Gaugemancy.xcodeproj` deleted and regenerated with
  `projectGenerator/projectGenerator -o<ofPath> -posx <projectPath>`
  (downloaded via `scripts/osx/download_projectGenerator.sh`); picks up all 6
  addons from `addons.make` automatically.
- Build succeeds (`xcodebuild ... -scheme "Gaugemancy Debug" build`) and the
  app launches and runs `--selftest`/`--help`; settings and the "Fred" profile
  load (its sample `.wav`s aren't in git, so `--selftest` reports those
  missing — expected, not a build problem).
- **Gotcha: building inside this Dropbox-synced folder, Xcode's automatic
  "Sign to Run Locally" codesign step fails** ("resource fork, Finder
  information, or similar detritus not allowed") because Dropbox tags files
  in the built `.app` with `com.dropbox.attrs`/`com.apple.FinderInfo` xattrs.
  The project's own `OF_CODESIGN` script-based signing is already off; this is
  Xcode's separate automatic signing. Workaround: build with
  `CODE_SIGNING_ALLOWED=NO` (command line), or in Xcode set the target's
  Signing & Capabilities to "None" for local runs.
- **Real bug found and fixed: no audio output at all, on any device, in any
  mode.** `pdsp::Engine::setup()` (in `ofxPDSP/src/ofx/Engine.cpp`) treated
  `outputID`/`inputID` as an *index* into `outputStream.getDeviceList()`
  (`devices[outputID]`), but `AudioEngine::startDevice()` correctly passes a
  real `ofSoundDevice::deviceID` — on CoreAudio that is an arbitrary
  OS-assigned AudioObjectID (we saw 132, not 0..12), not a small index. Two
  symptoms from the one bug: (1) no explicit device match -> `outputID`
  stayed at its old default of `0` -> `devices[0]` is just "whatever happens
  to enumerate first", which on this Mac was an input-only/unusable device,
  so `RtApiCore::probeDeviceOpen` rejected the channel count and the stream
  silently never started (pdsp logged "engine started" regardless — it
  doesn't check). (2) picking a device by name/id that resolves to a real
  deviceID like 132 indexed 132 slots into a ~13-entry vector -> an
  out-of-bounds `operator[]` -> `abort()` under libc++ hardening. Fixed by
  matching on `ofSoundDevice::deviceID` (falling back to the device actually
  flagged `isDefaultOutput`/`isDefaultInput`) instead of treating the ID as
  an index; default `outputID`/`inputID` changed from `0` to `-1` ("no
  explicit choice"). Verified headless with both the empty-device-name
  default path and an explicit `"MacBook Pro Speakers"` match: both now open
  cleanly with no RtAudio errors and no crash. This fix lives in the
  `fred-dev/ofxPDSP` fork and needs a commit/push there, separate from the
  Gaugemancy repo.
- Also fixed: clicking/dragging a slot's waveform in the GUI ("audition") set
  `SlotOverrides.overridePosition`/`overrideGate` but not volume, so a slot
  whose volume is sensor-modulated (every slot, in all 5 real performer
  presets) stayed silent during audition unless its physical sensor was
  pressed at the same time. Added `overrideVolume`/`volume` to
  `SlotOverrides` (`src/audio/GrainSlot.h`); `Instrument::setAudition` now
  forces full volume while auditioning (`src/app/Instrument.cpp`).
- Narration "skip" (one performer starts with narration, others go straight
  to granular) is already fully preserved: `profile.narration.enabled` and
  `startWithSensor` are per-profile JSON fields, and
  `ModeController::start()` already enters `Mode::Granular` directly when
  `enabled` is false. No code change needed; confirmed by inspection.
- Verified end-to-end that the synthesis/effects pipeline itself was never
  the problem: `Diagnostics::runBenchmark` (which drives a real simulated
  squeeze via `SensorSimulator::RampSettings`) produces real non-zero audio
  (checked peak amplitude with temporary instrumentation, since removed) —
  confirming the bug was entirely in the output device stream, not the DSP.
- Narration skip now sticks across restarts. Added `NarrationSettings::skip`
  (separate from `enabled`): `enabled=false` means "this profile has no
  narration at all"; `enabled=true, skip=true` means "narration exists but
  is permanently skipped for this profile" (what some performers' original
  devices did — one had narration, the rest always went straight to
  granular). `ModeController::start()` checks both. `Instrument::skipNarration()`
  (hardware double-click / `/gm/narration/skip`) now sets `skip=true` and
  calls `store_.saveProfile()` immediately, since the hardware button has no
  other way to persist anything (no GUI "save" on a headless Pi). The GUI
  also has a `skip` toggle next to `enabled` in the Narration panel; that one
  follows the existing convention of needing an explicit save (F12). Verified
  headless over OSC (`gm-ctl.py skip`): mode flips to granular immediately,
  `profile.json` gets `"skip": true`, and a fresh relaunch starts straight in
  granular without re-entering "wait for narration".
- Gotcha hit while testing this: an `xcodebuild build` can report
  `BUILD SUCCEEDED` without actually relinking the binary (observed: the
  binary's mtime/size did not change across two separate builds after
  editing `.cpp`/`.h` files, even though the `.o` wasn't recompiled either) —
  in this Dropbox-synced folder, Xcode's build system occasionally seems to
  miss that a source file changed. A `clean` can't delete `bin/` here either
  ("not created by the build system"). If a change does not seem to take
  effect, check whether the binary's mtime actually moved after the build;
  rerunning `build` again (no clean needed) was enough to get a real relink.

## Done since the first handoff

- Motion processing and smoothing bank, tilt/shake modulation sources.
- 3D instrument view (DeviceView) with squeeze and tilt simulation.
- gm-ctl.py, Pi install script + systemd user service, remote.sh, docs, README.
- pm2 `ecosystem.config.js` removed (replaced by systemd).

## Next steps (in order)

1. Local platform testing (see "NOT verified yet"); fix what breaks.
2. Regenerate the Visual Studio project with the project generator (Xcode done).
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
