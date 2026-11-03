# Base authoring patch - read this before touching anything here

`authoring.pd` now has the sensor-bridge chain built and saved: the
Pd-MCP-Server dynamic-patching receiver + `workspace` subpatch (unchanged),
plus `[iemnet/udpreceive 9000]` -> `[osc/unpackOSC]` -> `[osc/routeOSC ...]`
-> thirteen `[s sensorN]`/`[s total]`/`[s tiltX]`/etc. sends, sitting as flat
objects on the **main canvas** (not inside `workspace`, and not wrapped in a
`[pd sensor-bridge]` subpatch box either - that was tried and skipped as
unnecessary: `send`/`receive` are global in Pd regardless of nesting, so the
wrapper was cosmetic only). If you want it encapsulated later for easier
copy-paste into preset patches, select all 16 bridge objects on the main
canvas and Edit > Encapsulate.

Read `../../docs/PureData.md` first for the full design; this file is just
the concrete task record.

## Real gaps found building this (2026-11-03 session)

Two assumptions in `docs/PureData.md` turned out wrong on this Mac's actual
Pd 0.57-0 install - fix these first on any fresh machine before assuming the
MCP tools will do anything:

- **The `osc` abstraction library (`unpackOSC`/`routeOSC`/`packOSC`) does
  NOT ship with vanilla Pd 0.57-0** - checked `Pd-0.57-0.app`'s own `extra/`
  folder, it's not there. Without it, `authoring.pd`'s own receiver chain
  fails to instantiate at load (`osc/unpackOSC ... couldn't create`), which
  silently breaks *all* MCP tool calls - `create_object` etc. still report
  `"status": "success"` because that only means the Python server sent a UDP
  packet, not that Pd did anything with it. Fix: Help > Find externals,
  search `routeOSC` (searching by object name found it when the library's
  own name didn't), install, restart Pd.
- **iemnet's `[udpreceive <port>]` has no runtime `listen` method** - it
  starts listening immediately from its creation argument. The "listen 5000"
  message box inherited from Pd-MCP-Server's `example_patch.pd` is vestigial
  for this iemnet version and logs `udpreceive: no method for 'listen'` on
  click; harmless, the socket is already bound (confirmed with
  `lsof -nP -iUDP:5000`) before you ever click it.
- **Dynamic-patching messages to `pd-workspace` only land if the `workspace`
  subpatch window is already open.** If you send `create_object` calls while
  `workspace` has never been opened, Pd drops them with no error - nothing
  gets created and no exception is raised anywhere. Open `workspace` first,
  *then* start building.
- **`create_object`/`connect_objects` can only target the `workspace`
  subpatch** - the receiver's dynamic-patching target (`s pd-workspace`) is
  hardcoded, there's no way to address the top-level canvas or any other
  subpatch directly through the MCP tools. To get a finished graph onto the
  main canvas: build it in `workspace`, then Select All, Cut, switch to the
  main canvas window, Paste - a manual Pd step every time.

## Building an actual instrument patch

Once the bridge is confirmed working, describe to Claude what you want a
preset to sound like and which sensors should drive what - it builds into
the `workspace` subpatch using the same `[r sensorN]` etc. names, plus
whatever generators/samplers/effects the patch needs, ending in `[dac~]`.
Listen, iterate, then save just that patch (ideally the bridge subpatch +
the instrument graph, nothing from the MCP receiver machinery) as
`../../bin/data/<Profile>/patches/<name>.pd` - see `docs/PureData.md` for
why the receiver machinery itself should not ship in deployed presets.

## Next task, for whoever picks this up

The bridge is built and saved but **not yet tested with real OSC traffic** -
nothing has sent to port 9000 yet, so it's unconfirmed that
`/gm/sensor1 <v>` etc. actually makes `[r sensor1]` fire. In order:

1. There's a known stray empty object box on the main canvas (around
   `x=204, y=119`, near the "port 5000" message box) - harmless (nothing
   connects to it) but worth deleting next time the file is open for
   editing, then File > Save again.
2. Smoke-test the bridge before building Transport B: with `authoring.pd`
   open and `workspace` open (so you can see `[r sensorN]` etc. - actually
   they're on the main canvas now, so just watch the main canvas), send a
   one-off UDP packet to `127.0.0.1:9000` at address `/gm/sensor1` with a
   float arg (`oscsend 127.0.0.1 9000 /gm/sensor1 f 0.5`, or Python
   `python-osc`, or `gm-ctl.py` if it grows OSC-forward support first) and
   confirm something downstream reacts - easiest is temporarily wiring
   `[r sensor1]` to a `[print]` via the MCP tools (in `workspace`, then
   delete after) rather than hand-editing the saved file.
3. Build Transport B (see `docs/PureData.md`'s "Transport A/B" section) so
   Gaugemancy's own sensor pipeline can drive this for real, instead of
   synthetic test packets.
4. Then start on `PdEngine` (Transport A, the embedded/production path) -
   `docs/PureData.md`'s "Not built yet" list has the full order.

Read `docs/PureData.md`'s "Gaps found actually building the sensor bridge"
section first if anything here seems inconsistent with what you see live in
Pd - that section has the concrete fixes (missing `osc`/`iemnet` externals,
`workspace` window must be open before dynamic-patching into it, MCP tools
can only create into `workspace` so moving to the main canvas is a manual
cut/paste) discovered the hard way in this session.
