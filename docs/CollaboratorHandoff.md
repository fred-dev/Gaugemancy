# Handing Gaugemancy units + the laptop app to a collaborator

**Status: Phases B and C have now actually been run once, on a real unit
(2026-11-06)** - those two sections below are a confirmed procedure, not
just a plan, and include the real lessons from doing it (low-RAM build
limits, `.local` resolution failing, the exact shutdown behaviour). Phases
A, D, E and F are still planning only - no SD-card guide, no laptop doc, no
website. This file is meant to survive between sessions and be hand
carried (with the repo) to whoever picks it up next, human or Claude.

## The situation

Fred is giving a collaborator several physical Gaugemancy units (each a
Raspberry Pi + sensor hardware + a USB stick holding that performer's
settings and audio) plus the laptop programming app. Goal: the collaborator
does the smallest possible number of manual steps themselves - flashing SD
cards with Raspberry Pi Imager - and then everything else (Pi-side setup,
settings migration, laptop build) is done by a Claude Code session working
over SSH and on the collaborator's laptop, following a clear runbook.

Existing docs already cover most of the Pi-side mechanics in detail:
`docs/RaspberryPi.md` (standard setup, Pi 4/5) and `docs/RaspberryPi3BPlus.md`
(workarounds for the weaker 3B+). This plan is the layer *above* those - the
bits that are new for a multi-unit handoff to someone who isn't Fred: SD
card imaging with SSH pre-enabled, the laptop-side build (not covered
anywhere yet), settings archiving, and a walkthrough for actually using the
app. It should end up pointing at those docs rather than repeating them.

## Phase A - SD card imaging (collaborator does this, by hand, per unit)

Raspberry Pi Imager's gear-icon "OS Customisation" dialog can set hostname,
enable SSH with a password, and set the username/password, all before
writing the card - no post-boot keyboard/monitor needed.

Steps to write up (with screenshots, later):
1. Raspberry Pi Imager → choose OS → the *exact* image Fred's own units run
   (confirm the precise entry: Raspberry Pi OS, 64-bit, **with Desktop**,
   Trixie - need the exact release date/URL from Fred so the collaborator
   picks the identical build, not just "whatever is current" when they do
   this).
2. Choose storage → the SD card.
3. Click the gear icon (OS Customisation) before writing:
   - **General tab**: hostname - must be **unique per unit** (e.g.
     `gaugemancy-1`, `gaugemancy-2`, ...) since several `.local` units on
     the same network would otherwise collide; username + password (decide
     on a standard pair so the handoff doc can state fixed credentials
     rather than "whatever you typed").
   - **Services tab**: enable SSH, "Use password authentication".
   - Locale/timezone/keyboard as appropriate.
4. Write, eject, insert into the Pi, power on. Repeat per unit, changing only
   the hostname each time.

Open question for Fred: exact OS image identifier/date, final unit count,
and whether any unit is a Pi 3B+ (needs the separate doc's workarounds -
`install_dependencies.sh` fails outright on current Trixie repos there, and
`make -j4` can take the whole machine down).

## Phase B - handoff to a Claude session over SSH (once per unit)

What the collaborator gives Claude: each unit's hostname (or IP, if `.local`
mDNS resolution is unreliable on their network) and the SSH username/password
set in Phase A.

Runbook for that Claude session (confirmed against a real unit 2026-11-06):
1. Confirm reachability: `ssh <user>@<hostname>.local`. In practice
   `.local` mDNS resolution from a Mac can just fail outright (it did:
   `ping: cannot resolve gaugemancy.local`) even with the Pi up and SSH
   reachable - don't take that as "the Pi is off." Fall back to `arp -a`
   on the Mac and look for a Raspberry Pi Foundation MAC prefix (`b8:27:eb`,
   `dc:a6:32`, `e4:5f:01`, `d8:3a:dd`...) to find its IP directly, or the
   router's DHCP client list.
2. **If updating an already-running unit** (not a fresh SD card): stop the
   app *before* touching anything - `systemctl --user stop gaugemancy`.
   This is a real `SIGTERM`-triggered clean exit (`src/main.cpp` handles
   it), not a kill, and `Instrument::shutdown()` unconditionally pulses the
   speaker relay off as part of it - that is "the full shutoff routine,"
   turning off the speaker without powering off the Pi. Confirm the old
   PID is actually gone (`systemctl --user status gaugemancy`,
   `ps -p <pid>`) before moving on.
3. Follow `docs/RaspberryPi.md` section 1 to clone openFrameworks + addons
   and section 2 to run `scripts/pi/install.sh` + reboot, for a *fresh*
   unit. For an existing checkout being updated instead: `git fetch origin`
   in the app folder, then check out whatever branch is current (confirm
   with Fred or against this laptop's own `git rev-parse HEAD` - don't
   assume `main`, this project did substantial work on `gui-rewrite` before
   it merged). `ofxImGui` is a newer addon (added for the GUI rewrite) that
   an older checkout's `openFrameworks/addons/` won't have yet -
   `git clone -b develop https://github.com/jvcleave/ofxImGui.git` there;
   the other addons (`ofxPDSP`/`ofxAudioFile`/`ofxGPIO`/`ofxMidi`) are
   normally already present and already on their correct tracked branch -
   verify with `git status -sb` in each rather than assuming, but don't
   re-clone them if they're already clean and up to date.
4. **Check `free -h` before building.** `docs/RaspberryPi.md` assumes a Pi
   4/5 and says `make -j4 Release`; a Pi 3B+ (905 MB RAM) cannot take that -
   `docs/RaspberryPi3BPlus.md` documents it taking the whole Pi down hard
   enough to need a physical power-cycle, even at `-j2`, for this exact
   app+addons build. If `free -h` shows well under ~2 GB total, use
   `make -j1 Release` for the Gaugemancy/addons build specifically (OF core
   alone, if it needs rebuilding too, tolerates `-j2`), confirm a real
   swapfile exists already (`swapon --show` / `cat /proc/swaps` - `zram`
   alone is not real headroom, see that doc for adding one) and that the
   desktop GUI is disabled (`systemctl get-default` should say
   `multi-user.target`, not `graphical.target`) if programming only ever
   happens from a laptop anyway. Run the build in the background
   (`nohup make -j1 Release > ~/gaugemancy_build.log 2>&1 & disown`) and
   poll it - watching `free -h` and tailing the log - rather than blocking
   on it in the foreground; it can take a long time on this hardware and a
   blocked foreground SSH command gives no warning before a hang.
5. Build and verify: `make -C tests`, `bin/Gaugemancy --selftest`,
   `--benchmark 20`.
6. Mount and migrate that unit's USB stick - see Phase C.
7. Enable the systemd service (`scripts/pi/install.sh` already does this
   for a fresh unit) and confirm `--selftest` passes with the real sensor
   hardware attached, not just offline.
8. Repeat for the next unit's hostname.

## Phase C - USB stick settings migration (once per stick)

**Done for real once already** (2026-11-06, the first unit, stick labelled
`EMI_GAUGE`), so this is no longer speculative - it's the actual procedure,
run over SSH on the Pi itself (the stick was already mounted at
`/media/Data` per `docs/RaspberryPi.md` section 4's fstab line - that part
needed no new work). Repeat exactly this per stick:

1. **Archive first, convert second.** Copy (never move) every performer's
   raw XML - `username.xml` at the top plus each performer folder's
   `*_appSettings.xml` / `*_fileSettings.xml` / `*_preset_N.xml` /
   `*_effectSettings_preset_N.xml` / `*_effectParameterSettings_preset_N.xml`
   / `*_narr.xml` - into `/media/Data/_archive_xml/`, mirroring the
   performer-folder structure. This is a pure backup; it must not disturb
   the originals, because step 2 reads those same original paths and
   writes its JSON output *next to them* (nothing in the real performer
   folders gets deleted or moved - `convert_xml_settings.py`'s own
   docstring already says so, this just adds an extra, separate copy).
2. **Convert**: `python3 tools/convert_xml_settings.py /media/Data` - no
   extra Python packages needed, this (and every script in `tools/`) is
   standard-library only, so whatever `python3` is already on the machine
   works as-is. Check its output for anything beyond the "values clamped"
   kind of notice it documents; stop and ask Fred about anything else.
3. **Record each performer's modification date before moving on** - the
   latest `mtime` among that performer's *original* XML files, e.g. a
   one-line note per performer in `/media/Data/_archive_xml/README.md`.
   This matters because the same performer's data likely exists, slightly
   diverged, on more than one unit's stick (edited live on whichever unit
   they last played) - comparing these recorded dates across units later is
   how to find the actually-newest version of each performer's presets
   without guessing. Treat *this* stick's converted copy as the current
   "final" one for now, but don't discard the date - a later unit's stick
   may turn out to hold a newer edit of the same performer.
4. **Bring it into the git repo.** Copy each performer's resulting folder
   (old XML *and* new JSON side by side, exactly as the stick has them) and
   their sample audio into this repo's `bin/data/<performer>/`, plus the
   `_archive_xml/` copy, then commit and push - GitHub is meant to become
   the single converging point across every unit's stick over time, not
   any one stick. Check each audio file's size before adding it: GitHub
   hard-rejects anything over 100 MB per file, and a repo heavy with many
   large WAVs is worth flagging to Fred rather than just pushing - say so
   rather than silently committing something huge.
5. Only after all of this does the stick get a `gaugemancy.json` (if it
   doesn't have one yet) pointing `activeProfile` at whichever profile
   should actually run on that unit - don't set this before the migration
   above, or the app starts reading a data root that's still only
   half-converted.

## Phase D - laptop app build (collaborator's Mac)

Not documented anywhere yet - `docs/RaspberryPi.md` is Pi-only. New doc
needed (`docs/Laptop.md`, matching that doc's style) covering what this very
session has been doing all along to build and test Gaugemancy on macOS:
1. Install Xcode from the App Store; accept its license; install the Command
   Line Tools (`xcode-select --install` if Xcode's own first-run doesn't
   already prompt for them).
2. `git clone --depth 1 https://github.com/openframeworks/openFrameworks.git`
3. `cd openFrameworks/scripts/osx && ./download_libs.sh`
4. `cd ../../libs/openFrameworksCompiled/project && make -j4 Release`
   (builds OF core once; alternatively via the Xcode project if preferred).
5. Clone the addons `addons.make` lists, into `openFrameworks/addons/`:
   - `git clone -b ai-refactor https://github.com/fred-dev/ofxPDSP.git`
   - `git clone -b ai-refactor https://github.com/fred-dev/ofxAudioFile.git`
   - `git clone -b ai-refactor https://github.com/fred-dev/ofxGPIO.git`
     (compiles to stubs off Linux, but `addons.make` lists it for every
     platform, so it still needs to be present to build)
   - `git clone https://github.com/danomatika/ofxMidi.git`
   - `git clone -b develop https://github.com/jvcleave/ofxImGui.git` (added
     for the GUI rewrite - **not yet in `docs/RaspberryPi.md`'s addon list
     either**, flag to fix there too when this work happens)
   - `ofxOsc` ships with core openFrameworks already - no separate clone.
6. `cd openFrameworks/apps/myApps && git clone <branch-at-the-time> https://github.com/fred-dev/Gaugemancy.git`
   (confirm at handoff time whether `gui-rewrite` has merged to `main`).
7. `cd Gaugemancy && make -j4 Release` (same command used throughout this
   whole session) - or open `Gaugemancy.xcodeproj` and build the Release
   scheme in Xcode's UI if the collaborator prefers that.
8. Sanity check before anything is plugged in: `make -C tests`,
   `./bin/Gaugemancy.app/Contents/MacOS/Gaugemancy --list-audio`.

## Phase E - using the migrated settings on the laptop

The laptop app reads `bin/data` by default; `--data <path>` points it at a
mounted USB stick instead, or the performer folder can just be copied
locally. Reuses the exact same JSON from Phase C - no separate laptop-side
conversion.

## Phase F - interactive local walkthrough website (material gathered later)

A small, static, no-build-step site the collaborator can open locally
(`open index.html`, no server required - avoids depending on anything beyond
what macOS already has) to get oriented in the laptop app. Planned sections,
to flesh out once screenshots exist:
1. First launch - picking a profile, `--list-audio`, setting
   `audio.deviceName` so the right speaker/interface is used (see
   `docs/RaspberryPi.md`'s "Audio device" section for the laptop-vs-Pi split
   this mirrors).
2. Tour of the fixed panel layout: Transport / Simulation / Master output /
   Inspector, and the function-key shortcuts (F1-F6 slot select, F7
   Settings, F8 narration, F9 squeeze ramp, F10 message log, F11
   fullscreen, F12 save).
3. Editing a slot: granular vs. synth/generator sources, the free-run
   toggle, the effects chain editor.
4. Where profiles/presets live on disk and on the USB stick, and how
   saving works.
5. Troubleshooting basics: audio device not found, testing without sensor
   hardware (Simulation mode), where to find the message log.

Needs: screenshots/short recordings (gather later, per Fred). Build as plain
static HTML/CSS/JS under something like `docs/walkthrough/` so it ships with
the repo and needs no server.

## Open questions to settle with Fred before executing any of this

- Exact Raspberry Pi OS image identifier/release date to standardize on.
- Final unit count, and whether any is a Pi 3B+.
- Standard SSH username/password to set via the Imager.
- Which Gaugemancy branch to hand off (`main` vs `gui-rewrite`) by the time
  this actually happens.
- Where each unit's USB stick currently lives - already with the hardware,
  or does data need copying from Fred's laptop first.

## Deliverables when this is picked up

1. This doc, refined into a literal step-by-step runbook.
2. `docs/RaspberryPi.md`: add `ofxImGui` to its addon list.
3. New `docs/Laptop.md` for the macOS build (Phase D).
4. New `tools/archive_old_xml_settings.py` (Phase C).
5. SD-card-imaging mini-guide with screenshots (Phase A) - likely folds into
   the walkthrough site below rather than being a separate doc.
6. The interactive local walkthrough website (Phase F).
