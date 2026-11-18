#!/usr/bin/env python3
"""
crossfade_stress: put a RUNNING Gaugemancy through hard preset changes and
report whether the crossfade held up. Meant for the Raspberry Pi (run it on
the Pi itself over SSH, or from a laptop with --host), with the real sound
device playing. Python standard library only.

    tools/crossfade_stress.py                       # on the Pi, app running
    tools/crossfade_stress.py --host gaugemancy.local
    tools/crossfade_stress.py --quick               # shorter run
    tools/crossfade_stress.py --pid $(pgrep -x Gaugemancy)   # also report CPU and memory

What it does (the sensor input is switched to the simulator for the test and
back to hardware afterwards, so the balls do not need to be touched):

  1. walk       every preset to the next, all balls squeezed
  2. heavy      back and forth between the two presets with the most effects
  3. interrupt  ask for a new preset in the middle of a fade (must queue)
  4. burst      five requests within a second (the last one must win)
  5. random     random jumps while the balls pump up and down
  6. idle       changes with no ball pressed (must stay silent)

What it checks, from the app's own status over OSC (/gm/status):
  * the app keeps answering (a stall of the control loop shows as a late reply)
  * "skipped audio buffers" does not rise more than a few per change
  * output never drops to silence while balls are held (gap check)
  * every fade ends, and the final preset is the one asked for
  * with --pid: process CPU while fading (percent of one core)
  * with --pid: memory does not keep growing from one change to the next
    (measured from the end of the first walk, when every preset has been
    built once, to the end of the test; a leak shows as MB per change)

Exit code 0 = all checks passed.
"""

import argparse
import json
import os
import random
import socket
import struct
import sys
import time


# ------------------------------------------------------------------ OSC codec

def _pad(data):
    return data + b"\0" * (4 - len(data) % 4)


def encode(address, *args):
    tags, payload = ",", b""
    for arg in args:
        if isinstance(arg, int):
            tags += "i"
            payload += struct.pack(">i", arg)
        elif isinstance(arg, float):
            tags += "f"
            payload += struct.pack(">f", arg)
        else:
            tags += "s"
            payload += _pad(str(arg).encode())
    return _pad(address.encode()) + _pad(tags.encode()) + payload


def _string(data, offset):
    end = data.index(b"\0", offset)
    return data[offset:end].decode(errors="replace"), (end + 4) & ~3


def decode(data):
    address, offset = _string(data, 0)
    tags, offset = _string(data, offset)
    values = []
    for tag in tags[1:]:
        if tag == "i":
            values.append(struct.unpack(">i", data[offset:offset + 4])[0])
            offset += 4
        elif tag == "h":
            values.append(struct.unpack(">q", data[offset:offset + 8])[0])
            offset += 8
        elif tag == "f":
            values.append(struct.unpack(">f", data[offset:offset + 4])[0])
            offset += 4
        elif tag in "sS":
            text, offset = _string(data, offset)
            values.append(text)
    return address, values


# ----------------------------------------------------------------- the client

class Instrument:
    """Talks to a running Gaugemancy and keeps the numbers the checks need."""

    def __init__(self, host, port, listen, pid):
        self.target = (host, port)
        self.out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", listen))
        self.pid = pid
        self.state = None          # last /gm/state: dict
        self.worst_reply_ms = 0.0
        self.missed_replies = 0

    def send(self, address, *args):
        self.out.sendto(encode(address, *args), self.target)

    def poll(self, timeout=0.5):
        """Ask for the status and wait for the reply. Returns the state or None."""
        self.sock.settimeout(0.0)
        try:
            while True:                      # drop replies that came late
                self.sock.recvfrom(4096)
        except (BlockingIOError, socket.timeout):
            pass
        asked = time.time()
        self.send("/gm/status")
        self.sock.settimeout(timeout)
        try:
            while True:
                address, v = decode(self.sock.recvfrom(4096)[0])
                if address == "/gm/state" and len(v) >= 6:
                    self.state = {"mode": v[0], "preset": v[1], "source": v[2], "skipped": v[3],
                                  "level": abs(v[4]), "fading": bool(v[5])}
                    self.worst_reply_ms = max(self.worst_reply_ms, (time.time() - asked) * 1000.0)
                    return self.state
        except socket.timeout:
            self.missed_replies += 1
            return None

    def cpu_seconds(self):
        """CPU time used by the app so far (only when --pid is given, on Linux)."""
        if not self.pid:
            return None
        try:
            with open("/proc/%d/stat" % self.pid) as f:
                fields = f.read().rsplit(")", 1)[1].split()
            return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
        except (OSError, IndexError, ValueError):
            return None

    def rss_mb(self):
        """Resident memory of the app in MB (only when --pid is given, on Linux)."""
        if not self.pid:
            return None
        try:
            with open("/proc/%d/status" % self.pid) as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        return int(line.split()[1]) / 1024.0
        except (OSError, IndexError, ValueError):
            pass
        return None

    def squeeze_all(self, level):
        for ball in range(1, 7):
            self.send("/gm/sim/sensor", ball, float(level))


class Watch:
    """Samples the status for a while and summarises it."""

    def __init__(self, inst):
        self.inst = inst
        self.levels = []
        self.saw_fade = False
        self.start_skipped = inst.state["skipped"] if inst.state else 0
        self.start_cpu = inst.cpu_seconds()
        self.start_time = time.time()

    def sample(self, seconds, step=0.03, pump=None):
        end = time.time() + seconds
        while time.time() < end:
            if pump:
                pump(time.time())
            s = self.inst.poll()
            if s:
                self.levels.append(s["level"])
                self.saw_fade |= s["fading"]
            time.sleep(step)

    def wait_fade_end(self, limit):
        end = time.time() + limit
        while time.time() < end:
            s = self.inst.poll()
            if s:
                self.levels.append(s["level"])
                self.saw_fade |= s["fading"]
                if not s["fading"]:
                    return True
            time.sleep(0.03)
        return False

    def result(self):
        s = self.inst.state or {}
        elapsed = max(time.time() - self.start_time, 1e-6)
        cpu = self.inst.cpu_seconds()
        return {
            "skipped": s.get("skipped", 0) - self.start_skipped,
            # The meter is one sample per reply, so judge loudness by the peak
            # over short windows, not by single readings.
            "quietest": min(max(self.levels[i:i + 8]) for i in range(0, max(len(self.levels) - 7, 1))) if self.levels else 0.0,
            "loudest": max(self.levels) if self.levels else 0.0,
            "cpu": None if cpu is None or self.start_cpu is None else 100.0 * (cpu - self.start_cpu) / elapsed,
            "preset": s.get("preset"),
            "saw_fade": self.saw_fade,
        }


# ------------------------------------------------------------------ scenarios

def effect_count(preset_path):
    try:
        with open(preset_path) as f:
            p = json.load(f)
    except (OSError, ValueError):
        return 0
    count = len(p.get("master", {}).get("effects", []))
    for slot in p.get("slots", []):
        count += len([e for e in slot.get("effects", []) if e.get("enabled", True)])
    return count


def heaviest_two(data, profile, presets):
    """1-based numbers of the two presets with the most effects (needs --data)."""
    if not data or not profile:
        return 1, min(2, presets)
    try:
        with open(os.path.join(data, profile, "profile.json")) as f:
            files = json.load(f).get("presets", [])
    except (OSError, ValueError):
        return 1, min(2, presets)
    ranked = sorted(range(len(files)), key=lambda i: -effect_count(os.path.join(data, profile, files[i])))
    return (ranked[0] + 1, ranked[1] + 1) if len(ranked) >= 2 else (1, min(2, presets))


def run(inst, args):
    rows = []
    fade = args.fade_ms / 1000.0
    settle = fade + 1.5
    ok = True

    def change(name, target, hold=True, extra_requests=(), pump=None, expect=None):
        """One preset change; returns the row and whether it passed."""
        nonlocal ok
        inst.squeeze_all(0.8 if hold else 0.0)
        time.sleep(0.4)
        inst.poll()
        w = Watch(inst)
        inst.send("/gm/preset", target)
        for delay, other in extra_requests:
            w.sample(delay, pump=pump)
            inst.send("/gm/preset", other)
        w.sample(min(fade * 0.5, 1.0), pump=pump)
        ended = w.wait_fade_end(settle * (1 + len(extra_requests)) + 4.0)
        w.sample(0.4, pump=pump)
        r = w.result()
        want = expect if expect is not None else (extra_requests[-1][1] if extra_requests else target)
        problems = []
        if not ended:
            problems.append("fade did not end")
        if r["preset"] != want:
            problems.append("ended on preset %s, wanted %d" % (r["preset"], want))
        if r["skipped"] > args.max_skipped:
            problems.append("%d audio buffers skipped" % r["skipped"])
        if hold and not pump and r["quietest"] < args.min_level:
            problems.append("output dropped to %.4f (gap)" % r["quietest"])
        if not hold and r["loudest"] > 0.02:
            problems.append("sound with no ball pressed (%.3f)" % r["loudest"])
        ok &= not problems
        cpu = "" if r["cpu"] is None else "%5.1f%%" % r["cpu"]
        rows.append((name, "-> %d" % want, r["skipped"], "%.3f" % r["quietest"], cpu, "ok" if not problems else "; ".join(problems)))
        print("  %-22s %-6s skipped %-3d quietest %-6s %s %s" % rows[-1])

    presets = args.presets
    print("1. walk through every preset")
    for n in list(range(2, presets + 1)) + [1]:
        change("walk", n)

    mem_start, changes_start = inst.rss_mb(), len(rows)

    a, b = heaviest_two(args.data, args.profile, presets)
    print("2. heavy: presets %d <-> %d" % (a, b))
    for i in range(args.rounds):
        change("heavy", a if i % 2 == 0 else b)

    print("3. interrupt a fade (the second request must be queued)")
    for i in range(args.rounds):
        first, second = 1 + (i % presets), 1 + ((i + presets // 2) % presets)
        if first == second:
            second = 1 + (second % presets)
        change("interrupt", first, extra_requests=[(fade * 0.4, second)])

    print("4. burst of five requests (the last must win)")
    for i in range(max(args.rounds // 2, 1)):
        seq = [1 + ((i * 3 + k * 2) % presets) for k in range(5)]
        change("burst", seq[0], extra_requests=[(0.15, s) for s in seq[1:]])

    print("5. random jumps while the balls pump")
    rng = random.Random(7)

    def pump(now):
        for ball in range(1, 7):
            phase = (now * (0.6 + 0.17 * ball)) % 2.0
            inst.send("/gm/sim/sensor", ball, float(phase if phase < 1.0 else 2.0 - phase))

    for _ in range(args.rounds * 2):
        target = rng.randint(1, presets)
        current = (inst.state or {}).get("preset")
        if target == current:
            target = 1 + (target % presets)
        change("random", target, pump=pump)

    print("6. changes with no ball pressed (must be silent)")
    # Let the delay and reverb tails of the last scenario die away first.
    inst.squeeze_all(0.0)
    quiet_since = None
    deadline = time.time() + 20.0
    while time.time() < deadline:
        state = inst.poll()
        if state and state["level"] < 0.005:
            quiet_since = quiet_since or time.time()
            if time.time() - quiet_since > 1.0:
                break
        else:
            quiet_since = None
        time.sleep(0.05)
    for n in (2, 1):
        if n <= presets:
            change("idle", n, hold=False)

    inst.squeeze_all(0.0)

    mem_end = inst.rss_mb()
    changes = len(rows) - changes_start
    if mem_start is not None and mem_end is not None and changes > 0:
        per_change = (mem_end - mem_start) / changes
        leak = per_change > args.max_growth_mb
        print("memory: %.0f MB after the first walk, %.0f MB at the end, %d changes later: %+.2f MB per change -> %s"
              % (mem_start, mem_end, changes, per_change, "LEAK" if leak else "ok"))
        ok &= not leak
    return ok


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=1234, help="the app's osc.controlPort")
    p.add_argument("--listen", type=int, default=1235, help="local port for replies (osc.remotePort)")
    p.add_argument("--presets", type=int, default=0, help="number of presets (default: read from --data/--profile, else 4)")
    p.add_argument("--fade-ms", type=float, default=0, help="the profile's presetFadeMs (default: read from the profile, else 1500)")
    p.add_argument("--data", default="", help="data folder, to read the profile (e.g. bin/data or /media/Data)")
    p.add_argument("--profile", default="", help="profile name, to read the preset list and fade time")
    p.add_argument("--pid", type=int, default=0, help="process id of Gaugemancy on this machine, to report its CPU and memory")
    p.add_argument("--rounds", type=int, default=6)
    p.add_argument("--quick", action="store_true", help="two rounds per scenario")
    p.add_argument("--max-skipped", type=int, default=4, help="audio buffers a single change may skip")
    p.add_argument("--min-level", type=float, default=0.002, help="output below this while balls are held counts as a gap")
    p.add_argument("--max-growth-mb", type=float, default=0.5, help="memory growth per preset change that counts as a leak (needs --pid)")
    p.add_argument("--keep-source", action="store_true", help="do not switch back to hardware sensors at the end")
    args = p.parse_args()
    if args.quick:
        args.rounds = 2

    if args.data and args.profile:
        try:
            with open(os.path.join(args.data, args.profile, "profile.json")) as f:
                profile = json.load(f)
            args.presets = args.presets or len(profile.get("presets", []))
            args.fade_ms = args.fade_ms or float(profile.get("presetFadeMs", 1500))
        except (OSError, ValueError) as error:
            print("could not read the profile (%s); using defaults" % error)
    args.presets = args.presets or 4
    args.fade_ms = args.fade_ms or 1500.0

    inst = Instrument(args.host, args.port, args.listen, args.pid)
    if not inst.poll(1.5):
        print("no reply from Gaugemancy at %s:%d. Is it running, with osc.control on, and is local port %d free?"
              % (args.host, args.port, args.listen))
        return 2
    if inst.state["mode"] != "granular":
        # Skipping the narration is saved in the profile, so it is not done here.
        print("the instrument is in '%s' mode: the crossfade test needs the granular mode. Start it with a profile "
              "without narration (e.g. --profile fable) or skip the narration first." % inst.state["mode"])
        return 2
    print("connected: preset %(preset)s, sensors %(source)s, skipped buffers so far %(skipped)s" % inst.state)
    print("testing %d presets, fade %.0f ms, %d rounds\n" % (args.presets, args.fade_ms, args.rounds))

    original_source = inst.state["source"]
    inst.send("/gm/sensors/source", "sim")
    time.sleep(0.5)
    started = time.time()
    try:
        ok = run(inst, args)
    finally:
        inst.squeeze_all(0.0)
        if original_source == "hardware" and not args.keep_source:
            inst.send("/gm/sensors/source", "hardware")

    print("\n%.0f s, slowest status reply %.0f ms, missed replies %d"
          % (time.time() - started, inst.worst_reply_ms, inst.missed_replies))
    if inst.worst_reply_ms > 250:
        print("note: a reply slower than 250 ms means the control loop paused (loading or freeing a preset).")
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
