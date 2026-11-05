# Handing Gaugemancy units + the laptop app to a collaborator

**Status: planning only. Nothing below has been built yet** - no scripts,
no website, no doc updates to the other files it references. This is the
saved plan for that work, written so it survives between sessions and can be
hand carried (with the repo) to whoever picks it up next, human or Claude.

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

Runbook for that Claude session (to write up properly later, but the shape
is):
1. Confirm reachability: `ssh <user>@<hostname>.local` (fall back to the
   router's DHCP client list for an IP if `.local` doesn't resolve).
2. Follow `docs/RaspberryPi.md` section 1 to clone openFrameworks + addons
   and section 2 to run `scripts/pi/install.sh` + reboot - no need to
   duplicate those steps here, just point at them.
3. Build and verify per section 3: `make -j4 Release`, `make -C tests`,
   `bin/Gaugemancy --selftest`, `--benchmark 20`.
4. Mount and migrate that unit's USB stick - see Phase C.
5. Enable the systemd service (`scripts/pi/install.sh` already does this)
   and confirm `--selftest` passes with the real sensor hardware attached,
   not just offline.
6. Repeat for the next unit's hostname.

## Phase C - USB stick settings migration (once per stick)

`tools/convert_xml_settings.py` already does the XML → JSON conversion and
deliberately leaves the originals in place ("nothing is deleted" - see its
own docstring). `docs/RaspberryPi.md` section 4 already has the exact
`/etc/fstab` line for auto-mounting a stick labelled `DATA` at
`/media/Data` on boot - that part needs no new work, just executing it per
unit.

The one gap: nothing currently *archives* the old XML afterward, so a
migrated stick ends up with old and new files sitting side by side
indefinitely. Plan for a small new script, run once per stick, likely
`tools/archive_old_xml_settings.py` (sits next to the converter):
1. Run (or confirm has already been run) `convert_xml_settings.py`.
2. Move the original XML files (`username.xml` + each performer's
   `*_appSettings.xml` / `*_fileSettings.xml` / `*_preset_N.xml` /
   `*_effectSettings_preset_N.xml` / `*_effectParameterSettings_preset_N.xml`
   / `*_narr.xml`) into a timestamped folder, e.g. `_archive_xml/<date>/`,
   rather than deleting them - a safety net, not a permanent mess.
3. Make it safe to run more than once (new timestamp each time, never
   clobbers a previous archive).

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
