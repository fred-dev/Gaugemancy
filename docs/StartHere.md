# Gaugemancy setup - start here

**If you're the person who just received this file:** forward it to Claude
(Claude Code, or Claude Desktop's Code tab) and ask it to follow the
instructions below. You don't need git, Xcode, or anything else installed
first - Claude will check what's already on your machine and tell you
exactly what to install, if anything, before it needs it.

**If you're Claude reading this:** you're helping a collaborator of Fred's
set up the Gaugemancy hardware instrument on a machine that may have none of
the usual developer tools yet - several physical units (each a Raspberry Pi
+ sensor hardware + a USB stick of settings/audio) plus a laptop programming
app. Work through the steps below in order. Check in with the person at
every point that needs their input, a decision, or something installed -
don't assume, don't guess at credentials, and don't do anything destructive
(reformatting a drive, overwriting an SD card that already holds a unit's
real settings, etc.) without them explicitly confirming first.

## 0. What this is

Gaugemancy is an openFrameworks app: a laptop "programming" GUI plus a
headless build that runs on each Raspberry Pi unit. The full technical
plan for this handoff - phase by phase, including open questions for Fred -
already lives in the project's own repo, at `docs/CollaboratorHandoff.md`,
written for exactly this situation. This file only gets you from a blank
machine to having that file open; once you've cloned the repo (step 2),
read it in full before doing anything else. It is the source of truth.

## 1. Check what's already on this machine

Run both, and tell the person what's missing before going further:

```
git --version
python3 --version
```

- If `git --version` fails, or macOS pops up a dialog asking to install
  "the command line developer tools," let that dialog run. It installs
  git *and* the compiler toolchain needed later in one step - no Apple ID
  or App Store needed for this part.
- `python3` already ships with macOS; there's nothing to install for it,
  and none of this project's scripts need any extra packages (standard
  library only - no `pip install` ever needed).

## 2. Clone the repo

```
cd ~
git clone https://github.com/fred-dev/Gaugemancy.git
cd Gaugemancy
```

Check with Fred which branch is current before this step if it isn't
obvious - the work may still be on `gui-rewrite` rather than `main`:

```
git clone -b gui-rewrite https://github.com/fred-dev/Gaugemancy.git
```

## 3. Read the real plan, then follow it

```
cat docs/CollaboratorHandoff.md
```

In short, once you've read it in full:

- **Do the laptop app first (its Phase D)** - it doesn't need any Pi unit
  present, so there's no reason to wait on hardware. It needs full Xcode
  from the App Store (an Apple ID and a sizeable download) for the actual
  app build - `docs/CollaboratorHandoff.md` has the exact clone/addon/build
  commands. If a full Xcode install is impractical right now (slow
  connection, no Apple ID handy), ask the person whether to try just the
  command line developer tools from step 1 first and add full Xcode later
  only if something specifically needs it.
- **Then, per physical unit (its Phases A-C)**: the person flashes that
  unit's SD card with Raspberry Pi Imager themselves (the gear-icon
  settings: hostname, SSH, username, password - `docs/CollaboratorHandoff.md`
  Phase A lists exactly which fields to set and why each one matters, e.g.
  a unique hostname per unit). Once they give you that unit's hostname and
  the credentials they chose, SSH in and follow `docs/RaspberryPi.md` for
  the actual Pi-side setup, then `docs/CollaboratorHandoff.md` Phase C for
  migrating that unit's USB stick.

## 4. Open questions - ask, don't guess

`docs/CollaboratorHandoff.md`'s own "Open questions" section lists what's
still unresolved as of this handoff: the exact Raspberry Pi OS image to
use, the final unit count, whether any unit is an older Pi 3B+ (needs
`docs/RaspberryPi3BPlus.md`'s extra care), the standard SSH username/
password to set via the Imager, which git branch is current, and where
each unit's USB stick currently is. Get these from the person you're
helping, or from Fred directly if they don't know, before moving past
Phase B/C for any given unit.
