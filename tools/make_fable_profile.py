#!/usr/bin/env python3
"""Create the "fable" showcase profile: 10 presets that show the synth
sources (analog oscillator, FM operator, simple oscillator, noise), the
granular engine, the effects, tilt and shake mappings, and the preset
crossfade.

    python3 tools/make_fable_profile.py            # writes bin/data/fable

Granular presets use the shared samples in bin/data/audio (BIRDS, HARD, PAD,
RING, six of each); they are copied into bin/data/fable/audio. Every slot is
gated by its own ball: ball N plays slot N.
"""

import json
import shutil
import sys
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "bin" / "data"
PROFILE = DATA / "fable"
FADE_MS = 3000

# ---------------------------------------------------------------- parameters

def fixed(value):
    return {"value": value, "min": value, "max": value, "source": "none"}

def mod(low, high, source):
    """Moves from `low` (source at 0) to `high` (source at 1)."""
    return {"value": low, "min": low, "max": high, "source": source}

def ball(n):
    return "sensor%d" % n

GRAIN_DEFAULTS = {"length": 500.0, "density": 0.9, "distanceJitter": 20.0, "pitchJitter": 0.0, "pitch": 0.0,
                  "spread": 0.0, "volume": 0.5, "position": 0.5, "direction": 1.0}

EFFECT_DEFAULTS = {
    "bitcrush": {"bits": 16.0},
    "decimator": {"rate": 44100.0},
    "filter": {"cutoff": 20000.0, "resonance": 0.0},
    "chorus": {"speed": 0.5, "depth": 0.0, "delay": 0.0},
    "delay": {"mix": 0.3, "time": 250.0, "feedback": 0.3, "damping": 0.5},
    "reverb": {"mix": 0.3, "time": 3.0, "density": 0.5, "damping": 0.5, "hiCut": 5000.0, "modFreq": 0.2,
               "modAmount": 0.0},
    "compressor": {"gain": 1.0, "threshold": 0.0, "ratio": 4.0, "attack": 10.0, "release": 50.0, "knee": 0.0},
    "gain": {"gain": 1.0},
}

SYNTH_DEFAULTS = {
    "va_osc": {"waveform": 0.0, "volume": 0.8, "pitch": 57.0, "pulseWidth": 0.5},
    "fm_operator": {"volume": 0.8, "pitch": 57.0, "ratio": 1.0, "fmAmount": 0.0, "feedback": 0.0},
    "lfo_osc": {"waveform": 0.0, "volume": 0.8, "freq": 220.0},
    "noise": {"volume": 0.8, "pitch": -100.0, "decimation": 151.0, "bits": 12.0},
}

def with_defaults(defaults, overrides):
    """Every parameter of `defaults`; numbers in `overrides` become fixed."""
    unknown = set(overrides) - set(defaults)
    if unknown:
        sys.exit("unknown parameter(s): %s" % ", ".join(sorted(unknown)))
    params = {}
    for name, default in defaults.items():
        value = overrides.get(name, default)
        params[name] = value if isinstance(value, dict) else fixed(float(value))
    return params

def effect(kind, **params):
    return {"type": kind, "enabled": True, "params": with_defaults(EFFECT_DEFAULTS[kind], params)}

# --------------------------------------------------------------------- slots

def grain_slot(n, file, effects=(), window="hann", voices=8, **grain):
    return {"enabled": True, "sourceType": "granular", "file": file, "window": window, "gate": ball(n),
            "voices": voices, "grain": with_defaults(GRAIN_DEFAULTS, grain), "effects": list(effects)}

def synth_slot(n, kind, effects=(), **synth):
    return {"enabled": True, "sourceType": kind, "file": "", "window": "hann", "gate": ball(n), "voices": 4,
            "grain": with_defaults(GRAIN_DEFAULTS, {}), "synth": with_defaults(SYNTH_DEFAULTS[kind], synth),
            "effects": list(effects)}

def preset(name, slots, master_effects=(), gain=1.0):
    return {"version": 1, "name": name, "playheadSpeed": 0.012, "slots": slots,
            "master": {"gain": gain, "effects": list(master_effects)}}

def limiter():
    """Keeps six voices squeezed together from clipping."""
    return effect("compressor", gain=1.0, threshold=-12.0, ratio=8.0, attack=5.0, release=120.0, knee=6.0)

# ------------------------------------------------------------------- presets

def glass_choir():
    """FM bells on a D minor 9 chord. Harder squeeze = brighter. Tilt opens the hall."""
    notes = [50, 57, 62, 65, 69, 76]
    ratios = [2.0, 3.0, 3.5, 2.0, 5.0, 7.0]
    slots = [synth_slot(i + 1, "fm_operator", volume=mod(0.0, 0.42, ball(i + 1)), pitch=notes[i], ratio=ratios[i],
                        fmAmount=mod(0.1, 2.2, ball(i + 1)), feedback=0.05) for i in range(6)]
    return preset("Glass Choir", slots, [
        effect("chorus", speed=0.3, depth=4.0, delay=18.0),
        effect("reverb", mix=mod(0.25, 0.7, "tilt"), time=9.0, density=0.7, damping=0.3, hiCut=7000.0, modFreq=0.3,
               modAmount=0.4),
        limiter()])

def bird_cloud():
    """Short grains of birdsong. Squeeze scans the file, shaking scatters the pitch."""
    slots = [grain_slot(i + 1, "BIRDS_%d.wav" % (i + 1), window="gaussian", voices=10,
                        length=mod(60.0, 260.0, ball(i + 1)), density=0.85, distanceJitter=120.0,
                        pitchJitter=mod(0.0, 9.0, "shake"), pitch=[0, 0, 5, -5, 7, 12][i], spread=0.08,
                        volume=mod(0.0, 0.8, ball(i + 1)), position=mod(0.05, 0.95, ball(i + 1)))
             for i in range(6)]
    return preset("Bird Cloud", slots, [
        effect("delay", mix=0.3, time=mod(180.0, 620.0, "tiltX"), feedback=0.5, damping=0.4),
        effect("reverb", mix=0.3, time=4.0, density=0.6, damping=0.4, hiCut=9000.0),
        limiter()], gain=2.5)

def analog_fifths():
    """Saw and pulse oscillators in stacked fifths, each behind its own ladder filter."""
    notes = [38, 45, 50, 57, 62, 69]
    slots = []
    for i in range(6):
        n = i + 1
        pulse = i % 2 == 1
        slots.append(synth_slot(n, "va_osc", waveform=1.0 if pulse else 0.0, volume=mod(0.0, 0.4, ball(n)),
                                pitch=notes[i], pulseWidth=mod(0.15, 0.85, "tiltY"),
                                effects=[effect("filter", cutoff=mod(180.0, 5200.0, ball(n)),
                                                resonance=mod(0.1, 0.65, "tilt"))]))
    return preset("Analog Fifths", slots, [
        effect("chorus", speed=0.6, depth=6.0, delay=22.0),
        effect("reverb", mix=0.22, time=3.5, density=0.5, damping=0.5, hiCut=6000.0),
        limiter()])

def rust_engine():
    """Noise and square waves ground down by decimation. Dirty, percussive, no pitch centre."""
    slots = []
    for i in range(3):
        n = i + 1
        slots.append(synth_slot(n, "noise", volume=mod(0.0, 0.5, ball(n)), pitch=[-40.0, 10.0, 60.0][i],
                                decimation=mod(20.0 + 25 * i, 120.0, ball(n)), bits=mod(3.0, 10.0, "total"),
                                effects=[effect("filter", cutoff=mod(300.0, 9000.0, ball(n)), resonance=0.5)]))
    for i in range(3):
        n = i + 4
        slots.append(synth_slot(n, "lfo_osc", waveform=3.0, volume=mod(0.0, 0.35, ball(n)),
                                freq=mod([32.0, 48.0, 64.0][i], [96.0, 190.0, 400.0][i], ball(n)),
                                effects=[effect("bitcrush", bits=mod(12.0, 3.0, "shake")),
                                         effect("decimator", rate=mod(9000.0, 900.0, ball(n)))]))
    return preset("Rust Engine", slots, [
        effect("delay", mix=0.2, time=95.0, feedback=mod(0.2, 0.85, "tilt"), damping=0.7),
        limiter()])

def frozen_pads():
    """Long, slow grains of the pad samples, half of them backwards, in a very long hall."""
    slots = [grain_slot(i + 1, "PAD_%d.wav" % (i + 1), window="tukey", voices=8, length=mod(900.0, 2400.0, ball(i + 1)),
                        density=0.95, distanceJitter=300.0, pitchJitter=0.15, pitch=[0, -12, 7, 0, -5, 12][i],
                        spread=mod(0.02, 0.3, "tilt"), volume=mod(0.0, 0.7, ball(i + 1)),
                        position=mod(0.1, 0.9, "tiltX"), direction=-1.0 if i % 2 else 1.0)
             for i in range(6)]
    return preset("Frozen Pads", slots, [
        effect("filter", cutoff=mod(900.0, 12000.0, "total"), resonance=0.15),
        effect("reverb", mix=0.55, time=14.0, density=0.8, damping=0.25, hiCut=6000.0, modFreq=0.15, modAmount=0.6),
        limiter()], gain=4.0)

def metal_rain():
    """Tiny grains of struck metal. Squeeze for density, total pressure feeds the echo."""
    slots = [grain_slot(i + 1, "HARD_%d.wav" % (i + 1), window="triangular", voices=12,
                        length=mod(110.0, 22.0, ball(i + 1)), density=mod(0.2, 1.0, ball(i + 1)),
                        distanceJitter=mod(700.0, 40.0, ball(i + 1)), pitchJitter=3.0,
                        pitch=[-12, -7, 0, 0, 7, 12][i], spread=0.5, volume=mod(0.0, 0.85, ball(i + 1)),
                        position=mod(0.0, 0.6, "tiltY"),
                        effects=[effect("decimator", rate=mod(44100.0, 6000.0, "shake"))])
             for i in range(6)]
    return preset("Metal Rain", slots, [
        effect("delay", mix=0.35, time=330.0, feedback=mod(0.2, 0.9, "total"), damping=0.3),
        effect("reverb", mix=0.2, time=2.2, density=0.4, damping=0.6, hiCut=10000.0),
        limiter()], gain=3.0)

def sine_swarm():
    """Six raw oscillators that glide upward as you squeeze. Tilt morphs the waveform."""
    low = [110.0, 138.6, 164.8, 220.0, 277.2, 329.6]
    slots = [synth_slot(i + 1, "lfo_osc", waveform=mod(0.0, 3.0, "tiltX"), volume=mod(0.0, 0.3, ball(i + 1)),
                        freq=mod(low[i], low[i] * 4.0, ball(i + 1)),
                        effects=[effect("filter", cutoff=mod(2500.0, 9000.0, ball(i + 1)), resonance=0.2)])
             for i in range(6)]
    return preset("Sine Swarm", slots, [
        effect("chorus", speed=mod(0.2, 6.0, "shake"), depth=8.0, delay=25.0),
        effect("delay", mix=0.3, time=410.0, feedback=0.55, damping=0.5),
        effect("reverb", mix=0.3, time=5.0, density=0.6, damping=0.4, hiCut=8000.0),
        limiter()])

def bell_garden():
    """Ringing samples on three balls, inharmonic FM bells on the other three."""
    slots = []
    for i in range(3):
        n = i + 1
        slots.append(grain_slot(n, "RING_%d.wav" % (i * 2 + 1), window="gaussian", voices=8, length=420.0,
                                density=mod(0.3, 0.95, ball(n)), distanceJitter=200.0, pitchJitter=0.05,
                                pitch=[0, 7, 12][i], spread=0.2, volume=mod(0.0, 0.75, ball(n)),
                                position=mod(0.0, 0.5, ball(n))))
    for i in range(3):
        n = i + 4
        slots.append(synth_slot(n, "fm_operator", volume=mod(0.0, 0.3, ball(n)), pitch=[74, 81, 86][i],
                                ratio=[1.41, 2.76, 5.4][i], fmAmount=mod(0.6, 3.2, ball(n)),
                                feedback=mod(0.0, 0.5, "shake")))
    return preset("Bell Garden", slots, [
        effect("delay", mix=0.25, time=520.0, feedback=0.45, damping=0.5),
        effect("reverb", mix=mod(0.3, 0.65, "tilt"), time=7.0, density=0.7, damping=0.3, hiCut=9000.0, modFreq=0.4,
               modAmount=0.3),
        limiter()])

def broken_radio():
    """Birds and static through bit crushers: a mix of every source type in one preset."""
    slots = [
        synth_slot(1, "noise", volume=mod(0.0, 0.35, ball(1)), pitch=30.0, decimation=mod(140.0, 40.0, ball(1)),
                   bits=6.0, effects=[effect("filter", cutoff=mod(600.0, 6000.0, "tiltX"), resonance=0.6)]),
        grain_slot(2, "BIRDS_2.wav", length=90.0, density=0.7, distanceJitter=400.0, pitch=-7.0, spread=0.4,
                   volume=mod(0.0, 0.8, ball(2)), position=mod(0.0, 1.0, ball(2)),
                   effects=[effect("bitcrush", bits=mod(10.0, 3.0, ball(2)))]),
        synth_slot(3, "va_osc", waveform=1.0, volume=mod(0.0, 0.3, ball(3)), pitch=mod(41.0, 53.0, ball(3)),
                   pulseWidth=mod(0.08, 0.5, "shake"), effects=[effect("decimator", rate=mod(12000.0, 1500.0, "tilt"))]),
        grain_slot(4, "HARD_4.wav", window="triangular", length=45.0, density=mod(0.2, 0.9, ball(4)),
                   distanceJitter=500.0, pitchJitter=6.0, spread=0.6, volume=mod(0.0, 0.8, ball(4)), position=0.3,
                   effects=[effect("bitcrush", bits=5.0)]),
        synth_slot(5, "fm_operator", volume=mod(0.0, 0.3, ball(5)), pitch=45.0, ratio=mod(0.5, 9.0, ball(5)),
                   fmAmount=3.0, feedback=0.7),
        synth_slot(6, "lfo_osc", waveform=2.0, volume=mod(0.0, 0.25, ball(6)), freq=mod(40.0, 2400.0, ball(6)),
                   effects=[effect("decimator", rate=4000.0)]),
    ]
    return preset("Broken Radio", slots, [
        effect("filter", cutoff=mod(1200.0, 14000.0, "total"), resonance=0.3),
        effect("delay", mix=0.2, time=mod(30.0, 240.0, "tiltY"), feedback=0.6, damping=0.6),
        limiter()], gain=1.6)

def tilt_organ():
    """A soft organ: sine and triangle drawbars. Tilting bends the whole chord and swells the hall."""
    notes = [36, 48, 55, 60, 64, 67]
    slots = [synth_slot(i + 1, "va_osc", waveform=3.0 if i < 3 else 2.0, volume=mod(0.0, 0.38, ball(i + 1)),
                        pitch=mod(notes[i] - 1.0, notes[i] + 1.0, "tiltX"))
             for i in range(6)]
    return preset("Tilt Organ", slots, [
        effect("chorus", speed=mod(0.4, 5.5, "tilt"), depth=5.0, delay=14.0),
        effect("filter", cutoff=mod(1500.0, 9000.0, "total"), resonance=0.1),
        effect("reverb", mix=mod(0.2, 0.6, "tiltY"), time=6.0, density=0.7, damping=0.35, hiCut=5000.0),
        limiter()])

PRESETS = [glass_choir, bird_cloud, analog_fifths, rust_engine, frozen_pads, metal_rain, sine_swarm, bell_garden,
           broken_radio, tilt_organ]

# --------------------------------------------------------------------- write

def profile(preset_count):
    return {
        "version": 1, "name": "fable", "logLevel": "notice", "playMode": "multi", "controlRateHz": 100,
        "presetSwitchMinIntervalMs": 1000, "presetFadeMs": FADE_MS,
        "presets": ["presets/preset_%d.json" % (i + 1) for i in range(preset_count)],
        "audio": {"sampleRate": 44100, "bufferSize": 512, "numBuffers": 3, "masterGain": 0.8},
        "narration": {"enabled": False},
    }

def copy_samples(presets):
    files = sorted({s["file"] for p in presets for s in p["slots"] if s["file"]})
    target = PROFILE / "audio"
    target.mkdir(parents=True, exist_ok=True)
    missing = []
    for name in files:
        source = DATA / "audio" / name
        if source.exists():
            shutil.copyfile(source, target / name)
        elif not (target / name).exists():
            missing.append(name)
    return files, missing

def main():
    presets = [make() for make in PRESETS]
    (PROFILE / "presets").mkdir(parents=True, exist_ok=True)
    for i, p in enumerate(presets):
        (PROFILE / "presets" / ("preset_%d.json" % (i + 1))).write_text(json.dumps(p, indent=2) + "\n")
    (PROFILE / "profile.json").write_text(json.dumps(profile(len(presets)), indent=2) + "\n")
    files, missing = copy_samples(presets)
    print("wrote %s: %d presets, %d sample(s), crossfade %d ms" % (PROFILE, len(presets), len(files), FADE_MS))
    for i, make in enumerate(PRESETS):
        print("  %2d %-14s %s" % (i + 1, presets[i]["name"], make.__doc__.strip()))
    if missing:
        print("missing samples (put them in %s): %s" % (DATA / "audio", ", ".join(missing)))
    print("Try:  bin/Gaugemancy --gui --profile fable --sensors sim")

if __name__ == "__main__":
    main()
