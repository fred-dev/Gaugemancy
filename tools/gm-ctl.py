#!/usr/bin/env python3
"""
gm-ctl: control and watch a running Gaugemancy over the network (OSC/UDP).
Works from a laptop, or on the Pi itself inside an SSH session. Python
standard library only.

    tools/gm-ctl.py [--host 192.168.1.20] [--port 1234] <command> [args]

Commands
    status                     print mode, preset, sensor source and levels
    watch                      print live sensor levels (needs telemetry or
                               repeats `status`), Ctrl-C to stop
    preset <n> | next          select preset n (1-based) or the next one
    skip                       skip the narration
    calibrate                  re-measure resting values and "level"
    reload                     reload settings from disk
    source hardware|sim|replay:<file.csv>
    squeeze <sensor 1-6> <level 0-1>     simulated squeeze (sim source)
    ramp [speed] [mask]        simulated squeeze ramp (mask: bit per sensor)
    tilt <x deg> <y deg>       simulated accelerometer tilt (sim source)
    record <file.csv> | record stop
    quit                       stop the app

Replies to `status`/`watch` arrive on --listen (default 1235, the profile's
osc.remotePort).
"""

import argparse
import math
import socket
import struct
import sys
import time


# ------------------------------------------------------------------ OSC codec

def _pad(data):
    return data + b"\0" * (4 - len(data) % 4)


def encode(address, *args):
    tags = ","
    payload = b""
    for arg in args:
        if isinstance(arg, bool):
            arg = int(arg)
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


def _read_string(data, offset):
    end = data.index(b"\0", offset)
    text = data[offset:end].decode(errors="replace")
    return text, (end + 4) & ~3


def decode(data):
    address, offset = _read_string(data, 0)
    tags, offset = _read_string(data, offset)
    values = []
    for tag in tags[1:]:
        if tag == "i":
            values.append(struct.unpack(">i", data[offset:offset + 4])[0])
            offset += 4
        elif tag == "h":
            values.append(struct.unpack(">q", data[offset:offset + 8])[0])
            offset += 8
        elif tag == "f":
            values.append(round(struct.unpack(">f", data[offset:offset + 4])[0], 3))
            offset += 4
        elif tag == "d":
            values.append(round(struct.unpack(">d", data[offset:offset + 8])[0], 3))
            offset += 8
        elif tag in "sS":
            text, offset = _read_string(data, offset)
            values.append(text)
    return address, values


# -------------------------------------------------------------------- commands

def send(args, address, *values):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(encode(address, *values), (args.host, args.port))


def listen(port, seconds):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", port))
    s.settimeout(seconds)
    return s


def print_message(address, values):
    if address == "/gm/sensors":
        bars = " ".join(("%d:%s" % (i + 1, "#" * int(v * 10))).ljust(13) for i, v in enumerate(values[:6]))
        print("%s  total %.2f" % (bars, values[6] if len(values) > 6 else 0.0))
    elif address == "/gm/state":
        print("mode %s | preset %s | sensors %s | skipped audio buffers %s" % tuple(values[:4]))
    else:
        print(address, values)


def status(args):
    sock = listen(args.listen, 1.5)
    send(args, "/gm/status")
    got = 0
    try:
        while got < 2:
            data, _ = sock.recvfrom(4096)
            print_message(*decode(data))
            got += 1
    except socket.timeout:
        if got == 0:
            print("no reply: is Gaugemancy running with osc.control on port %d, and is port %d free here?"
                  % (args.port, args.listen))
            return 1
    return 0


def watch(args):
    sock = listen(args.listen, 0.5)
    print("watching (Ctrl-C to stop)")
    try:
        while True:
            send(args, "/gm/status")
            try:
                data, _ = sock.recvfrom(4096)
                address, values = decode(data)
                if address == "/gm/sensors":
                    print_message(address, values)
            except socket.timeout:
                pass
            time.sleep(0.1)
    except KeyboardInterrupt:
        return 0


def tilt_to_accel(x_deg, y_deg):
    """Accelerometer reading (g) for a device tilted by x/y degrees from +z up."""
    x, y = math.radians(x_deg), math.radians(y_deg)
    ax, ay, az = math.sin(x), math.sin(y) * math.cos(x), math.cos(x) * math.cos(y)
    norm = math.sqrt(ax * ax + ay * ay + az * az)
    return ax / norm, ay / norm, az / norm


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1234, help="Gaugemancy osc.controlPort")
    parser.add_argument("--listen", type=int, default=1235, help="local port for replies (osc.remotePort)")
    parser.add_argument("command")
    parser.add_argument("args", nargs="*")
    args = parser.parse_args()
    c, a = args.command, args.args

    if c == "status":
        return status(args)
    if c == "watch":
        return watch(args)
    if c == "preset":
        if a and a[0] == "next":
            send(args, "/gm/preset/next")
        else:
            send(args, "/gm/preset", int(a[0]))
    elif c == "next":
        send(args, "/gm/preset/next")
    elif c == "skip":
        send(args, "/gm/narration/skip")
    elif c == "calibrate":
        send(args, "/gm/calibrate")
    elif c == "reload":
        send(args, "/gm/reload")
    elif c == "source":
        send(args, "/gm/sensors/source", a[0])
    elif c == "squeeze":
        send(args, "/gm/sim/sensor", int(a[0]), float(a[1]))
    elif c == "ramp":
        send(args, "/gm/sim/ramp", float(a[0]) if a else 0.5, int(a[1]) if len(a) > 1 else 63)
    elif c == "tilt":
        send(args, "/gm/sim/accel", *tilt_to_accel(float(a[0]), float(a[1]) if len(a) > 1 else 0.0))
    elif c == "record":
        send(args, "/gm/record", "" if not a or a[0] == "stop" else a[0])
    elif c == "quit":
        send(args, "/gm/quit")
    else:
        parser.print_help()
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
