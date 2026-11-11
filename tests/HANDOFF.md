# Handoff: code review, optimisation, then smooth preset changes

Start a new Claude Code session on the Mac with this file. It replaces the
long history of the cloud session that did the rewrite. Read `tests/CLAUDE.md`
too (status, layout, rules, verified/unverified list), then follow the phases
below in order. Keep `tests/CLAUDE.md` updated after every milestone.

## Where everything is (Fred's Mac)

| What | Path |
|---|---|
| App repo (fred-dev/Gaugemancy) | `/Users/fredrodrigues/Library/CloudStorage/Dropbox/Code/OF_GITT/openFrameworks/apps/Ali moini/Gaugemancy` |
| Addons | `/Users/fredrodrigues/Library/CloudStorage/Dropbox/Code/OF_GITT/openFrameworks/addons` |
| openFrameworks root | `/Users/fredrodrigues/Library/CloudStorage/Dropbox/Code/OF_GITT/openFrameworks` |

Branches: `ai-refactor` in fred-dev/Gaugemancy, fred-dev/ofxPDSP,
fred-dev/ofxAudioFile and fred-dev/ofxGPIO; upstream `danomatika/ofxMidi`
(no fork needed). First step: confirm each addon folder is on that branch and
up to date (`git -C <addon> status -sb`, `git pull`).

Notes for this machine (see the macOS section of `tests/CLAUDE.md`):
- The Xcode project was regenerated and the macOS build verified on
  2026-11-03. Build in Xcode or with `xcodebuild ... CODE_SIGNING_ALLOWED=NO`
  (Dropbox xattrs break Xcode's automatic signing).
- Dropbox sometimes makes Xcode skip a relink: after building, check that
  `bin/Gaugemancy.app`'s binary mtime changed; build again if not.
- The path contains spaces (`Ali moini`); OF makefiles can break on that, so
  prefer Xcode on the Mac. Do not move the repo without asking Fred.

The instrument is a **Raspberry Pi 3B+** (4x Cortex-A53, 1 GB RAM), see
`docs/RaspberryPi3BPlus.md`: build with `-j1` plus a swapfile, desktop
disabled. It is far weaker than the x86 numbers in CLAUDE.md suggest, so every
optimisation below matters, and CPU and memory must be measured on the 3B+.

## Ground rules (from Fred, unchanged)

- Work and commit on `ai-refactor` in each repo; push when a step is done.
- Never modify openFrameworks core or the standard OF addons. Changes to other
  addons go into Fred's forks only; if a new addon needs changing, ask Fred to
  fork it first.
- Each addon builds on every platform from its own `addon_config.mk`.
- Simple code: small classes, short functions, clear comments.
- Target hardware: Raspberry Pi 3B+ (Trixie 64 bit, headless); Mac, Windows
  and Linux are the programming tool. Optimisations must help the Pi.
- No em dashes in user-facing text.

## Phase 1: evaluate the code, optimise, refactor

Goal: an honest review of the current code (written quickly in one long cloud
session), then measured improvements. Measure before and after every
performance change with `bin/Gaugemancy --benchmark` (and on the Pi when
possible). Report findings to Fred before large refactors.

### 1.1 Baseline before changing anything

- Pull all four repos, build on the Mac, run `make -C tests` (OF_ROOT default
  `../../../..` matches this layout) and `python3 tools/make_test_data.py`.
- Record `bin/Gaugemancy --profile Demo --benchmark 20` and the same for each
  real performer profile, on the Mac AND on the Pi 3B+ (`scripts/remote.sh`
  or SSH). Also record the Pi's idle CPU of the running service and its
  memory (RSS) with a heavy preset loaded. These are the before numbers.

### 1.2 Review checklist

Go through every file in `src/` and the forks' changed files. Look for:
correctness bugs, thread safety (main thread vs audio thread), memory
ownership and lifetime, error handling, readability, duplicated code, and
comments that no longer match the code. Known candidates to check first:

1. `AudioEngine::loadPreset()` allocates and builds effect chains while
   holding the audio lock, so the output is silent for that time. Build
   outside the lock and only connect under it (this also prepares Phase 2).
2. `Instrument::update()` builds a `Telemetry` (with strings) every frame even
   when telemetry is off; build it only when it will be sent.
3. `GrainSlot::update()` writes all nine grain controls every frame even when
   unchanged; measure whether skipping unchanged values helps.
4. Check that a slot whose gate is closed really costs ~nothing (pdsp `Amp`
   skips its input when its gain is a control-rate 0; confirm that smoothed
   `ValueControl`s settle to control-rate output). Add a benchmark case with
   all gates closed and one with one slot open.
5. `pdsp::SampleBuffer::load()` copies ofxAudioFile's buffer into its own:
   check for a second full copy of every sample (peak memory on the Pi) and
   fix in the ofxPDSP fork if so.
6. GUI per-frame costs: `DeviceView` creates an `ofMaterial` per ball per frame
   and a backdrop mesh per frame; `std::function` meters per slot per frame;
   sphere resolution. Cache what can be cached. The GUI does not run on the
   Pi, so keep this low priority.
7. `FramePacer` replaced `ofSetFrameRate` (OF's limiter busy-waits). Confirm
   the control loop holds its rate and stays under 1% CPU on the Pi 3B+.
   Consider a lower default `controlRateHz` (e.g. 60) if the 3B+ needs it.
8. Effect costs on the Pi 3B+: per-slot reverb is the heaviest unit. Consider
   recommending master-chain reverb in the docs and in the GUI cost labels,
   and lower default grain voices. Check the 3B+ audio buffer size needed for
   no dropouts (`audio.pi` block) and memory use of six long samples.
9. Unit tests: add coverage where the review finds gaps (EffectChain matching,
   GrainSlot gate logic, settings round trips for new fields).

Deliver: a short findings list (fixed / proposed / rejected with reason),
commits per topic, benchmark numbers before/after, CLAUDE.md updated.

## Phase 2: smooth preset changes (crossfade with tails)

Today a preset switch takes the audio lock, mutes, swaps samples and rebuilds
the chains; the output is silent while the lock is held and reverb/delay tails
are cut. Replace it with a two-deck crossfade.

Design agreed with Fred:

1. Move everything that belongs to a preset (the 6 slots + master effect
   chain) into a `PresetGraph` class. `AudioEngine` keeps two: the playing one
   and the incoming one, each feeding its own fade gain before the shared
   master volume. Narration stays outside both.
2. Build the incoming graph WITHOUT the lock: decode samples, construct units,
   patch them together. Safe because pdsp only processes units reachable from
   the outputs, and units created while audio runs prepare themselves in their
   constructor (pdsp `Preparable::dynamicConstruction`). Never touch a node of
   the live graph during this step.
3. Connect the new graph with its fade gain at 0 under the lock (pointer
   operations only, no buffer lost).
4. Fade on the control thread: equal-power (old = cos, new = sin of the
   progress), lightly smoothed `ValueControl`s, no lock. Length from a new
   profile setting, e.g. `presetFadeMs` (default about 1500 ms), editable in
   the GUI and saved in JSON (with tests).
5. Keep the old graph connected until its output stays below a threshold for
   a moment (natural reverb/delay tails), then disconnect it under a brief
   lock and destroy it after releasing the lock.
6. A preset request during a fade: finish (or jump to the end of) the current
   fade, then start the new one. A queue of one is enough.
7. The GUI and sensor mappings act on the incoming preset during a fade.
8. Later optimisation, not first: when a slot keeps the same sample and the
   same effect order, glide its parameter values instead of building a second
   copy.

Watch: CPU roughly doubles during the fade (both graphs run; closed gates stay
cheap), and memory briefly holds both presets' samples. Add a benchmark mode
that renders a preset change and reports the worst block during the fade, and
an offline test that renders across a switch and checks for no silent gap and
no click (sample-to-sample jump threshold).

## Phase 3: later ideas (only when Fred asks)

From the synthesis discussion, in suggested order:
1. Ball voice allocator: a squeeze becomes a note (scale/mapping, velocity from
   squeeze speed, pressure as continuous control): a 6-voice pressure synth.
2. Generalise a slot to a "source" type: grain, sampler, subtractive, FM,
   wavetable, sharing effect chains, mappings and GUI.
3. Wavefolder/waveshaper unit (optionally 2x oversampled).
4. Modal resonator bank (8 to 16 tuned band-pass resonators struck by squeezes).
5. Karplus-Strong / waveguide string units.
6. Vocoder module (narration as the modulator).
7. Spectral freeze/blur (heaviest; benchmark on the Pi first).
New DSP units go in the ofxPDSP fork following its `Unit` pattern; ofxSIMDFloats
is not needed (ofxPDSP bundles its own copy).

## Open questions for Fred (still unanswered)

- Physical sensor numbering and ADC channels (assumed: sensor 1 top, 2 bottom,
  3..6 around the sides upper/lower/upper/lower on ADC channels 0..5).
- How the LIS3DH board is mounted (only matters for the absolute "up" mode).
