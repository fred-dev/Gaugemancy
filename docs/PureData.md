# Gaugemancy on Pure Data (`ai-refactor-pd` branch)

This is a second, parallel line of Gaugemancy. The `ai-refactor` branch plays
granular synthesis on fixed sample sets through `ofxPDSP`. This branch
(`ai-refactor-pd`) keeps the same hardware (buttons, LEDs, relay, pressure
sensors, accelerometer) and the same button/OSC preset-switching, but the
sound is a Pure Data patch instead of C++ DSP. No narration here; a preset
may or may not use audio files, that is entirely up to the patch.

Status as of 2026-11-03: branches created (`fred-dev/Gaugemancy` and
`fred-dev/ofxPd`, both `ai-refactor-pd`), architecture decided below, nothing
built yet. See "What's actually built so far" at the end before assuming any
of this runs.

## Repositories

| Repo | Role | Branch |
|---|---|---|
| fred-dev/Gaugemancy | the app | `ai-refactor-pd` |
| fred-dev/ofxPd | Pd embedding (fork of danomatika/ofxPd, unmodified so far) | `ai-refactor-pd` |
| nikmaniatis/Pd-MCP-Server (Fred's clone, local only so far) | lets Claude build/edit a Pd patch live via MCP | n/a - not a Gaugemancy dependency, a laptop tool |

`addons.make` for this branch: `ofxGui ofxOsc ofxPd ofxGPIO`. No `ofxPDSP`,
`ofxAudioFile`, or `ofxMidi` - this branch doesn't use any of them.

## Install (laptop, for authoring patches)

Versions actually present on this Mac when this was written, not just
"should work":

- **Pure Data (vanilla) 0.57-0** - `/Applications/Pd-0.57-0.app`. The
  MCP tool's receiver patch needs the `osc` abstraction library
  (`osc/unpackOSC`, `osc/routeOSC`) and the **iemnet** external
  (`iemnet/udpreceive`) for the actual UDP socket - **neither ships with this
  vanilla build** (checked the app bundle's `extra/` folder: not there).
  Install both from Pd's own Deken plugin (Help > Find externals): search
  "iemnet" for the external, and search "routeOSC" for the abstraction
  library (searching by object name works better than guessing the library's
  listed name).
- **uv 0.10.12**, **Python 3.14.7** available; Pd-MCP-Server's
  `pyproject.toml` pins `requires-python >= 3.12` and its own `.python-version`
  says `3.12` - let `uv` manage that venv rather than assuming the system
  Python.
- Pd-MCP-Server's actual Python deps (from its `pyproject.toml`, not its
  README, which mentions `fastmcp` but the lockfile doesn't): `mcp[cli]>=1.4.1`,
  `python-osc>=1.9.3`, `jsonschema>=4.23.0`.

## Where to get the MCP server

`https://github.com/nikmaniatis/Pd-MCP-Server` - Fred has a local clone at
`~/Dropbox/Code/Pd-MCP-Server` (not a Gaugemancy submodule, lives on its own).
Install per its own README (`uv` + the `claude_desktop_config.json` block
pointing `--directory` at that clone). Once added, Claude gets tools
`create_object`, `connect_objects`, `set_param`, `start_dsp`/`stop_dsp`,
`delete_object`, `disconnect_objects`, `save_patch`, `load_patch` against
whatever OSC port Pd is listening on (default 5000, feedback 5001) - these
build into an **already-open, real Pd window** via Pd's own dynamic-patching
messages, so you watch the patch get built live.

**Gaps found in it while planning this** (fix before relying on them):
- `delete_object`/`disconnect_objects` exist as MCP tools but
  `pd/base/authoring.pd`'s receiver only handles `/pd/create`, `/pd/connect`,
  `/pd/dsp`, `/pd/clear` - deleting/disconnecting a single object currently
  does nothing; `clear_workspace` (wipe everything) is the only way back.
- `save_patch`/`load_patch` send `/pd/save` and `/pd/load`, but the patch
  doesn't route those either - saving today means doing it by hand in Pd
  (File > Save As), which is fine as a workflow but means the "ask Claude
  to save it" tool doesn't actually do anything yet.
- Signal objects created with a trailing `0` argument (e.g. `+~ 0`) get a
  float inlet instead of a signal inlet on Pd 0.55+; the server already
  strips those automatically.

**Gaps found actually building the sensor bridge** (2026-11-03, see
`pd/base/CLAUDE.md` for the full record):
- The claim below that the `osc` library "ships with vanilla since around
  0.52" is **wrong** for this Mac's Pd 0.57-0 - it's not in the app bundle's
  `extra/` folder. Without it, `authoring.pd`'s own receiver fails to load,
  which breaks every MCP tool call silently (`create_object` still reports
  `"status": "success"` - that only means a UDP packet was sent, not that Pd
  did anything). Install via Deken, searching the object name (`routeOSC`)
  rather than a library name.
- `create_object`/`connect_objects` can only ever create into the
  `workspace` subpatch (the receiver's dynamic-patching target, `s
  pd-workspace`, is hardcoded) - there is no way to address the top-level
  canvas or any other subpatch through the MCP tools. Getting a finished
  graph onto the main canvas is a manual cut-from-workspace,
  paste-onto-main-canvas step every time.
- That dynamic-patching target only works while the `workspace` window is
  already open - `create_object` calls sent before it's ever been opened are
  dropped with no error and nothing is created.

## The sensor question: two transports, one patch

The same patch must work two ways: embedded in the OF app (production, on
the Pi or the Mac) and inside a full standalone Pd (laptop authoring, so the
MCP tool can drive a real Pd GUI). The fix is to make every patch depend on
a fixed set of `receive` names and never care how they got their values:

```
r sensor1   r sensor2   r sensor3   r sensor4   r sensor5   r sensor6
r total     r tiltX     r tiltY     r tilt      r shake
r button    (bang, or a symbol naming the event - TBD when it's built)
r preset    (float: 1-based preset index, sent on every switch)
```

These are exactly the `ModSource` ids already used everywhere else in the
app (`src/core/ControlInputs.cpp`'s `kSourceNames`), so a patch author who
already knows Gaugemancy's existing JSON (`"source": "sensor1"`, etc.) is
reading a name they've seen before.

**Transport A - embedded (production).** A new `PdEngine` (replacing
`AudioEngine` on this branch) owns an `ofxPd` instance, loads the profile's
`.pd` file, and every control-rate tick calls `pd.sendFloat("sensor1", v)`
etc. directly - `ofxPd`'s send targets a Pd `receive` object in-process, no
network involved, lowest latency. This is what runs on the Pi.

**Transport B - OSC (authoring).** Gaugemancy can also run with the `PdEngine`
replaced by a thin OSC sender - same `Instrument`, same sensors, same 3D
`DeviceView` squeeze simulator, same button/gesture code, just forwarding
every value out over `ofxOsc` instead of into libpd:
`/gm/sensor1 <v>` ... `/gm/total <v>`, `/gm/tiltX <v>` ... `/gm/button <name>`,
`/gm/preset <n>`, to a host:port you choose. This is the "separate, so the
full PD can be used" path - you run real Gaugemancy on the laptop (with
`--sensors sim` and the GUI, exactly as already built), it talks OSC to a
real standalone Pd with the patch open, and the MCP tool can shape that same
patch live while you squeeze simulated balls and listen.

**The bridge, so one patch file serves both.** Every patch includes a small
always-present subpatch:

```
[iemnet/udpreceive 9000] -> [osc/unpackOSC] ->
  [osc/routeOSC /gm/sensor1 /gm/sensor2 /gm/sensor3 /gm/sensor4 /gm/sensor5
                /gm/sensor6 /gm/total /gm/tiltX /gm/tiltY /gm/tilt /gm/shake
                /gm/button /gm/preset]
    -> one [s sensor1] / [s sensor2] / ... per matched outlet
```

Port 9000 is arbitrary but distinct from the MCP command channel (5000/5001)
so the two don't collide. When the patch is embedded via `ofxPd`, this
subpatch just sits there unused - `pd.sendFloat` hits the `[r sensorN]`
objects directly, bypassing the network layer entirely, so there's no
"embedded mode" special-casing anywhere in the patch itself. One file, two
contexts, zero patch changes between them.

**Still to decide once this is actually built:** whether `button`/`preset`
need richer payloads than a single float/symbol (e.g. which button, press vs.
release, double vs. triple click) - start minimal and extend the `routeOSC`
address list and the embedded `sendFloat`/`sendSymbol` calls together as
real patches need more.

## Presets as patches

Mirrors the existing `bin/data/<Profile>/presets/preset_N.json` convention:

- `bin/data/<Profile>/patches/<name>.pd` - a self-contained patch (the
  sensor-bridge subpatch above, plus whatever signal chain the MCP session
  built, plus a `[dac~]`). A profile's preset list can point different
  presets at different `.pd` files, **or** at the same file with a different
  initial parameter-set (a small JSON of receive-name -> value, sent right
  after `pd.openPatch()`) - both patterns the same mechanism, just like audio
  presets already support "different samples" or "different settings on the
  same slots".
- No `NarrationSettings` on this branch - drop narration wiring from
  `Instrument` rather than carry dead fields.

## Authoring workflow

1. On the laptop: open `pd/base/authoring.pd` in real Pd, click "listen 5000"
   as the patch instructs.
2. Run Gaugemancy itself on the laptop in OSC-forward mode (once Transport B
   exists) pointed at the sensor-bridge's port, with `--sensors sim --gui` so
   you get the 3D squeeze simulator for free.
3. Tell Claude (with the Pd-MCP-Server tools loaded) what you want the patch
   to do with which sensors - it builds into the `workspace` subpatch while
   you watch and listen.
4. When it sounds right: File > Save As in Pd itself (the MCP `save_patch`
   tool doesn't work yet, see Gaps above) to
   `bin/data/<Profile>/patches/<name>.pd`.
5. On the Pi: the same file loads through the embedded `PdEngine` - no
   changes needed, per the bridge design above.

## What's actually built so far

Done: `fred-dev/Gaugemancy` and `fred-dev/ofxPd` both have an
`ai-refactor-pd` branch; `addons.make` updated; `pd/base/authoring.pd` has
the sensor-bridge chain (`iemnet/udpreceive 9000` -> `osc/unpackOSC` ->
`osc/routeOSC` -> thirteen sends) built live via the MCP tools and saved,
sitting as flat objects on the main canvas (not encapsulated into a `[pd
sensor-bridge]` box - tried, skipped as unnecessary since `send`/`receive`
are global in Pd regardless of nesting). See `pd/base/CLAUDE.md` for the
real-world gaps hit getting here (missing `osc` library on vanilla Pd
0.57-0, `workspace` window must be open before dynamic-patching into it,
etc.) - read that before doing this on a fresh machine.

Not built yet, in rough order:
1. `PdEngine` (`src/audio/` or a new `src/pd/`) wrapping `ofxPd`: load patch,
   `sendFloat` per control tick, DSP on/off, preset switch = load a different
   file or re-send a parameter set.
2. Transport B: an OSC-forward mode in `Instrument` that runs the full
   sensor/button pipeline without `PdEngine` and sends `/gm/...` instead -
   needed to actually test the bridge end-to-end (squeeze a ball, confirm
   `[r sensorN]` fires).
3. Profile/preset JSON schema for `patches` (list of `.pd` files +
   optional per-preset parameter sets), replacing `audio`/presets'
   slot-based schema on this branch.
4. Fixing `save_patch`/`load_patch`/`delete_object`/`disconnect_objects` in
   the MCP tool's receiver patch, if the manual-save workaround gets annoying.
