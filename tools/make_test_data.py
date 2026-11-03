#!/usr/bin/env python3
"""
Create test data for Gaugemancy: demo profiles with synthetic audio and
sensor recordings, so the instrument can be tested anywhere (laptop, or the
Raspberry Pi over SSH) without the real samples or touching the balls.

    python3 tools/make_test_data.py                 # into bin/data
    python3 tools/make_test_data.py /media/Data     # onto the USB stick

Creates (existing files are overwritten, other profiles are not touched):
    <data>/Demo/                 multi mode, narration, 4 presets
        audio/tone_1..6.wav      six different synthetic textures (10 s)
        audio/narration.wav      30 s "narration" (pitched blips)
        presets/preset_1.json    one effect per slot, sensor mappings
        presets/preset_2.json    STRESS: every effect on every slot
        presets/preset_3.json    LIGHT: no slot effects, one master reverb
        presets/preset_4.json    tilt + total pressure mappings
    <data>/DemoAccumulated/      accumulated mode (one slot, playhead)
    <data>/recordings/*.csv      sensor recordings for --sensors replay:<file>

Only the Python standard library is used.
"""

import json
import math
import os
import random
import struct
import sys
import wave

RATE = 44100
REST = 200          # raw ADC value of a ball at rest
FULL = 530          # raw value at full squeeze (profile sensors.maxRaw)


# ------------------------------------------------------------------- audio

def write_wav(path, samples_lr):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for left, right in samples_lr:
            frames += struct.pack("<hh", int(max(-1.0, min(1.0, left)) * 32767),
                                  int(max(-1.0, min(1.0, right)) * 32767))
        w.writeframes(bytes(frames))


def texture(kind, seconds=10.0):
    """Six distinct, easy to recognise textures."""
    rng = random.Random(kind)
    n = int(seconds * RATE)
    out = []
    phase = [0.0] * 4
    noise = 0.0
    for i in range(n):
        t = i / RATE
        pos = i / n
        if kind == 1:    # rising chord
            f = 110.0 * (1 + pos)
            s = sum(math.sin(2 * math.pi * f * m * t) for m in (1, 1.25, 1.5)) / 3
        elif kind == 2:  # filtered noise swell
            noise = 0.97 * noise + 0.03 * rng.uniform(-1, 1)
            s = noise * 4 * math.sin(math.pi * pos)
        elif kind == 3:  # bell hits every 0.5 s
            local = t % 0.5
            s = math.sin(2 * math.pi * 660 * t) * math.exp(-local * 8) + 0.5 * math.sin(2 * math.pi * 1650 * t) * math.exp(-local * 14)
        elif kind == 4:  # slow sweep
            phase[0] += 2 * math.pi * (80 + 1200 * pos * pos) / RATE
            s = math.sin(phase[0])
        elif kind == 5:  # rhythmic clicks + bass
            s = 0.6 * math.sin(2 * math.pi * 55 * t) + (0.8 if (i % 5512) < 40 else 0.0)
        else:            # detuned pad
            s = sum(math.sin(2 * math.pi * (220 + d) * t) for d in (-1.3, 0, 1.7)) / 3
        out.append((0.5 * s, 0.5 * s * (0.8 + 0.2 * math.sin(2 * math.pi * 0.3 * t))))
    return out


def narration(seconds=30.0):
    """Pitched blips in 'syllables' so scrubbing is audible."""
    n = int(seconds * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        word = int(t * 3)
        f = 180 + (word * 37) % 220
        env = math.sin(math.pi * ((t * 3) % 1.0)) ** 2
        s = 0.5 * env * (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * 2 * f * t))
        out.append((s, s))
    return out


# ----------------------------------------------------------------- settings

def p(value, lo, hi, source="none"):
    return {"value": value, "min": lo, "max": hi, "source": source}


def grain(sensor, **overrides):
    g = {
        "length": p(400, 80, 900, sensor),
        "density": p(0.8, 0.3, 1.0),
        "distanceJitter": p(20, 0, 200),
        "pitchJitter": p(0, 0, 2),
        "pitch": p(0, -7, 7),
        "spread": p(0.02, 0, 0.2),
        "volume": p(0.6, 0.0, 0.9, sensor),
        "position": p(0.5, 0.0, 1.0, sensor),
        "direction": p(1, -1, 1),
    }
    g.update(overrides)
    return g


def effect(kind, **params):
    return {"type": kind, "enabled": True, "params": params}


def compressor():
    return effect("compressor", gain=p(1.0, 0, 10), threshold=p(-12, -50, 0), ratio=p(4, 1, 40),
                  attack=p(10, 1, 60), release=p(80, 10, 250), knee=p(3, 0, 10))


ONE_EFFECT_EACH = [
    [effect("filter", cutoff=p(2000, 300, 12000, "sensor1"), resonance=p(0.3, 0, 1))],
    [effect("bitcrush", bits=p(16, 16, 4, "sensor2"))],
    [effect("delay", mix=p(0.3, 0, 0.6, "sensor3"), time=p(300, 1, 2000), feedback=p(0.4, 0, 0.95), damping=p(0.5, 0, 1))],
    [effect("chorus", speed=p(0.5, 0, 100), depth=p(5, 0, 30), delay=p(10, 0, 200))],
    [effect("decimator", rate=p(44100, 44100, 4000, "sensor5"))],
    [effect("reverb", mix=p(0.3, 0, 0.8, "sensor6"), time=p(3, 0.1, 20), density=p(0.5, 0, 1), damping=p(0.5, 0, 1),
            hiCut=p(6000, 200, 20000), modFreq=p(0.2, 0, 20), modAmount=p(0.1, 0, 1))],
]


def every_effect():
    return [
        effect("bitcrush", bits=p(12, 1, 16)),
        effect("decimator", rate=p(22050, 50, 44100)),
        effect("chorus", speed=p(0.5, 0, 100), depth=p(5, 0, 30), delay=p(10, 0, 200)),
        effect("filter", cutoff=p(4000, 20, 20000), resonance=p(0.2, 0, 1)),
        effect("delay", mix=p(0.2, 0, 1), time=p(250, 1, 2000), feedback=p(0.3, 0, 0.95), damping=p(0.5, 0, 1)),
        effect("reverb", mix=p(0.2, 0, 1), time=p(2, 0.1, 20), density=p(0.5, 0, 1), damping=p(0.5, 0, 1),
               hiCut=p(6000, 200, 20000), modFreq=p(0.2, 0, 20), modAmount=p(0.1, 0, 1)),
        compressor(),
    ]


def slot(index, effects, **grain_overrides):
    sensor = "sensor%d" % (index + 1)
    return {"enabled": True, "file": "tone_%d.wav" % (index + 1), "window": "hann", "gate": "auto",
            "voices": 8, "grain": grain(sensor, **grain_overrides), "effects": effects}


def preset(name, slots, master=None, speed=0.03):
    return {"version": 1, "name": name, "playheadSpeed": speed, "slots": slots,
            "master": {"gain": 1.0, "effects": master or []}}


def profile(name, mode, presets, narration_enabled):
    return {
        "version": 1, "name": name, "logLevel": "notice", "playMode": mode,
        "controlRateHz": 100, "presetSwitchMinIntervalMs": 2000,
        "presets": ["presets/preset_%d.json" % (i + 1) for i in range(presets)],
        "sensors": {"maxRaw": FULL, "activeThreshold": 0.035, "curve": "linear", "calibrationFrames": 16,
                    "accumulationDenominator": 3.0, "smoothingMs": 20.0},
        "motion": {"enabled": True, "zeroAtStartup": True, "upAxis": "+z", "rangeDegrees": 45,
                   "deadzoneDegrees": 3, "smoothingMs": 80, "shakeSensitivity": 0.5, "shakeReleaseMs": 300,
                   "calibrationFrames": 20},
        "audio": {"sampleRate": 44100, "bufferSize": 512, "numBuffers": 3, "api": "default",
                  "deviceName": "", "deviceId": -1, "masterGain": 0.8},
        "gestures": {"hitToChangePreset": True, "hitThreshold": 0.25, "troughThreshold": 0.08, "maxPeakMs": 300,
                     "maxTroughMs": 300, "requiredHits": 4, "minSensorsPerHit": 2},
        "hardware": {"spiDevice": "/dev/spidev0.0", "adcChannels": [0, 1, 2, 3, 4, 5], "buttonPin": 19,
                     "buttonActiveLow": True, "blueLedPin": 5, "redLedPin": 6, "relayPin": 13, "relayPulseMs": 400,
                     "clickTimeoutMs": 240, "longPressQuits": False, "shutdownOnQuit": False,
                     "accelerometer": True, "i2cBus": "/dev/i2c-1", "accelAddress": 24},
        "osc": {"telemetry": False, "remoteHost": "127.0.0.1", "remotePort": 1235, "control": True,
                "controlPort": 1234, "telemetryRateHz": 20},
        "narration": {
            "enabled": narration_enabled, "file": "narration.wav", "volume": 1.0, "startWithSensor": True,
            "autoStartDelayMs": 5000, "glitchThreshold": 0.1, "glitchStrand": 0.02, "window": "hann",
            "grain": grain("none"), "effects": [compressor()]},
    }


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print("wrote", path)


# --------------------------------------------------------------- recordings

def write_recording(path, frames):
    """frames: list of (time_ms, [6 normalised levels], (ax, ay, az))."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Gaugemancy sensor recording (raw ADC values). Replay: --sensors replay:%s\n" % os.path.basename(path))
        f.write("time_ms,s1,s2,s3,s4,s5,s6,ax,ay,az\n")
        for t, levels, accel in frames:
            raw = [int(REST + (FULL - REST) * max(0.0, min(1.0, v))) for v in levels]
            f.write("%d,%s,%.3f,%.3f,%.3f\n" % (t, ",".join(str(r) for r in raw), *accel))
    print("wrote", path)


def frames_for(duration_ms, levels_at, accel_at=lambda t: (0.0, 0.0, 1.0)):
    # 500 ms of rest first so the instrument calibrates on resting values.
    out = [(t, [0.0] * 6, (0.0, 0.0, 1.0)) for t in range(0, 500, 10)]
    for t in range(0, duration_ms, 10):
        out.append((500 + t, levels_at(t), accel_at(t)))
    return out


def squeeze(t, start, length):
    """A smooth squeeze: 0 -> 1 -> 0 over `length` ms starting at `start`."""
    if t < start or t > start + length:
        return 0.0
    return math.sin(math.pi * (t - start) / length) ** 2


def recordings(folder):
    write_recording(os.path.join(folder, "each_ball.csv"), frames_for(
        13000, lambda t: [squeeze(t, i * 2000, 1800) for i in range(6)]))
    write_recording(os.path.join(folder, "all_balls.csv"), frames_for(
        8000, lambda t: [squeeze(t, 0, 4000) * (0.6 + 0.07 * i) + squeeze(t, 4000, 4000) for i in range(6)]))
    write_recording(os.path.join(folder, "pump_gesture.csv"), frames_for(
        3000, lambda t: [1.0 if (t % 400) < 150 and t < 2000 and i < 3 else 0.0 for i in range(6)]))
    write_recording(os.path.join(folder, "narration_then_glitch.csv"), frames_for(
        12000, lambda t: [squeeze(t, 500, 600) + (0.8 if 5000 <= t < 8000 else 0.0), 0, 0, 0, 0, 0]))
    write_recording(os.path.join(folder, "tilt.csv"), frames_for(
        8000, lambda t: [0.5] * 6,
        lambda t: (math.sin(t / 1000.0), math.cos(t / 1300.0), 0.7)))
    write_recording(os.path.join(folder, "long_performance.csv"), frames_for(
        120000, lambda t: [max(0.0, math.sin(t / (900.0 + 170 * i) + i)) ** 3 for i in range(6)]))


# --------------------------------------------------------------------- main

def main():
    data = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "bin", "data")
    data = os.path.abspath(data)
    print("test data into", data)

    for name in ("Demo", "DemoAccumulated"):
        audio = os.path.join(data, name, "audio")
        for i in range(1, 7):
            write_wav(os.path.join(audio, "tone_%d.wav" % i), texture(i))
        write_wav(os.path.join(audio, "narration.wav"), narration())
        print("wrote audio in", audio)

    demo = os.path.join(data, "Demo")
    write_json(os.path.join(demo, "profile.json"), profile("Demo", "multi", 4, True))
    write_json(os.path.join(demo, "presets", "preset_1.json"),
               preset("One effect per slot", [slot(i, ONE_EFFECT_EACH[i] + [compressor()]) for i in range(6)]))
    write_json(os.path.join(demo, "presets", "preset_2.json"),
               preset("STRESS every effect", [slot(i, every_effect()) for i in range(6)]))
    write_json(os.path.join(demo, "presets", "preset_3.json"),
               preset("LIGHT master reverb", [slot(i, []) for i in range(6)], master=[
                   effect("reverb", mix=p(0.25, 0, 1), time=p(3, 0.1, 20), density=p(0.5, 0, 1),
                          damping=p(0.5, 0, 1), hiCut=p(6000, 200, 20000), modFreq=p(0.2, 0, 20),
                          modAmount=p(0.1, 0, 1)), compressor()]))
    write_json(os.path.join(demo, "presets", "preset_4.json"),
               preset("Tilt and total", [slot(i, [], pitch=p(0, -12, 12, "tiltX"), spread=p(0, 0, 0.3, "total"))
                                         for i in range(6)]))

    acc = os.path.join(data, "DemoAccumulated")
    write_json(os.path.join(acc, "profile.json"), profile("DemoAccumulated", "accumulated", 1, False))
    write_json(os.path.join(acc, "presets", "preset_1.json"),
               preset("Accumulated walk", [slot(0, [compressor()])] + [dict(slot(i, []), enabled=False) for i in range(1, 6)],
                      speed=0.05))

    recordings(os.path.join(data, "recordings"))

    root = os.path.join(data, "gaugemancy.json")
    if not os.path.exists(root):
        write_json(root, {"version": 1, "activeProfile": "Demo"})
    print("\nTry:  bin/Gaugemancy --profile Demo --sensors replay:%s" % os.path.join(data, "recordings", "each_ball.csv"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
