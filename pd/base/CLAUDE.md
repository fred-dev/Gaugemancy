# Base authoring patch - read this before touching anything here

`authoring.pd` is currently a plain copy of Pd-MCP-Server's
`example_patch.pd` (the dynamic-patching receiver + a `workspace` subpatch).
It is **not yet** the Gaugemancy base patch - it's missing the sensor-bridge
subpatch that every real preset patch needs. Read `../../docs/PureData.md`
first for the full design; this file is just the concrete task.

## Task: add the sensor bridge

Do this with a real Pd session open and the Pd-MCP-Server MCP tools loaded -
don't hand-edit the `.pd` text. Connection indices in Pd's file format are
positional (by order of `#X obj`/`#X msg` lines, zero-based) and easy to get
subtly wrong by hand; building it live and then saving confirms it's right.

1. Open `authoring.pd` in Pd, click "listen 5000".
2. Ask Claude to build, inside the top-level canvas (not the `workspace`
   subpatch - that one's for whatever instrument patch comes later), a
   `[pd sensor-bridge]` subpatch containing:
   - `[iemnet/udpreceive 9000]` -> `[osc/unpackOSC]` -> `[osc/routeOSC ...]`
     with the address list from `docs/PureData.md`'s "sensor question"
     section (`/gm/sensor1` through `/gm/sensor6`, `/gm/total`, `/gm/tiltX`,
     `/gm/tiltY`, `/gm/tilt`, `/gm/shake`, `/gm/button`, `/gm/preset`).
   - One `[s sensorN]` / `[s total]` / `[s tiltX]` / etc. per matched
     `routeOSC` outlet, in the same order as the address list.
   - Requires the `iemnet` external (Deken) - if it's not installed, the
     object will show as broken (dashed border); install it first.
3. Test it: once Transport B (the OSC-forward mode in Gaugemancy) exists,
   run it pointed at `127.0.0.1:9000` and confirm each `[r sensorN]` fires
   when you squeeze the corresponding simulated ball. Until Transport B
   exists, you can fake it by hand with `sendosc`/`oscsend` or a one-off
   script sending to the same addresses.
4. Save (File > Save As) over `authoring.pd` once it works, then update this
   file's first paragraph so it stops saying "not yet".

## Building an actual instrument patch

Once the bridge is confirmed working, describe to Claude what you want a
preset to sound like and which sensors should drive what - it builds into
the `workspace` subpatch using the same `[r sensorN]` etc. names, plus
whatever generators/samplers/effects the patch needs, ending in `[dac~]`.
Listen, iterate, then save just that patch (ideally the bridge subpatch +
the instrument graph, nothing from the MCP receiver machinery) as
`../../bin/data/<Profile>/patches/<name>.pd` - see `docs/PureData.md` for
why the receiver machinery itself should not ship in deployed presets.
