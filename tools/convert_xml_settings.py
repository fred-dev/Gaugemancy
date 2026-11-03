#!/usr/bin/env python3
"""
Convert the old Gaugemancy XML settings to the new JSON settings.

Old layout (one folder per performer, e.g. Fred/):
    username.xml                                  active performer
    Fred/Fred_appSettings.xml                     instrument settings
    Fred/Fred_fileSettings.xml                    sample file per slot/preset
    Fred/Fred_preset_N.xml                        grain parameters (ofxGui panel)
    Fred/Fred_effectSettings_preset_N.xml         which effects each slot has
    Fred/Fred_effectParameterSettings_preset_N.xml  effect parameters
    Fred/Fred_narr.xml                            narration grain settings

New layout (written next to the old files, nothing is deleted):
    gaugemancy.json
    Fred/profile.json
    Fred/presets/preset_N.json

Usage:
    python3 tools/convert_xml_settings.py bin/data                # all performers
    python3 tools/convert_xml_settings.py /media/Data --only Fred
    python3 tools/convert_xml_settings.py bin/data --dry-run      # print, write nothing

Notes on behaviour changes (also printed when converting):
  * The old effect order was fixed: bitcrush, decimator, chorus, filter,
    delay (send), reverb (send), compressor. That order is kept. Delay and
    reverb were parallel sends; they are now inserts with a wet "mix".
  * TIME_ADVANCE_INTERVAL_N was "playhead per frame"; it is converted to
    "playhead per second" assuming the old loop ran at 60 frames per second.
  * Values outside the new legal ranges are clamped (and reported).
"""

import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

NUM_SLOTS = 6
NUM_PRESETS = 4
ASSUMED_OLD_FPS = 60.0

WINDOWS = ["rectangular", "triangular", "hann", "hamming", "blackman",
           "blackmanHarris", "sine", "welch", "gaussian", "tukey"]
CURVES = ["linear", "sqrt", "quartic", "exponential"]
LOG_LEVELS = {0: "verbose", 1: "error", 2: "notice", 3: "silent"}

# Legal ranges: (default, lowest, highest). Must match src/core.
GRAIN_SPECS = {
    "length": (500.0, 10.0, 3000.0),
    "density": (0.9, 0.1, 1.0),
    "distanceJitter": (20.0, 0.0, 1000.0),
    "pitchJitter": (0.0, -200.0, 200.0),
    "pitch": (0.0, -20.0, 20.0),
    "spread": (0.0, 0.0, 1.0),
    "volume": (0.5, 0.0, 1.0),
    "position": (0.5, 0.0, 1.0),
    "direction": (1.0, -1.0, 1.0),
}

EFFECT_SPECS = {
    "bitcrush": {"bits": (16.0, 1.0, 16.0)},
    "decimator": {"rate": (44100.0, 50.0, 44100.0)},
    "filter": {"cutoff": (20000.0, 20.0, 20000.0), "resonance": (0.0, 0.0, 1.0)},
    "chorus": {"speed": (0.5, 0.0, 100.0), "depth": (0.0, 0.0, 30.0), "delay": (0.0, 0.0, 200.0)},
    "delay": {"mix": (0.3, 0.0, 1.0), "time": (250.0, 1.0, 2000.0),
              "feedback": (0.3, 0.0, 0.95), "damping": (0.5, 0.0, 1.0)},
    "reverb": {"mix": (0.3, 0.0, 1.0), "time": (3.0, 0.1, 20.0), "density": (0.5, 0.0, 1.0),
               "damping": (0.5, 0.0, 1.0), "hiCut": (5000.0, 200.0, 20000.0),
               "modFreq": (0.2, 0.0, 20.0), "modAmount": (0.0, 0.0, 1.0)},
    "compressor": {"gain": (1.0, 0.0, 10.0), "threshold": (0.0, -50.0, 0.0), "ratio": (4.0, 1.0, 40.0),
                   "attack": (10.0, 1.0, 60.0), "release": (50.0, 10.0, 250.0), "knee": (0.0, 0.0, 10.0)},
}

# Old effect group name -> (new type, {old param base name -> new param id}).
# Old param bases are matched after lower-casing and removing non-letters.
OLD_EFFECTS = [
    ("BIT_CRUSH", "Bitcrusher", "bitcrush", {"bits": "bits"}),
    ("DECIMATOR", "Decimator", "decimator", {"rate": "rate"}),
    ("CHORUS", "Chorus", "chorus", {"speed": "speed", "depth": "depth", "delay": "delay"}),
    ("FILTER", "Filter", "filter", {"freq": "cutoff", "reso": "resonance"}),
    ("DELAY", "Delay", "delay", {"send": "mix", "time": "time", "feedback": "feedback", "damping": "damping"}),
    ("REVERB", "Reverb", "reverb", {"send": "mix", "time": "time", "density": "density", "damping": "damping",
                                    "hicut": "hiCut", "modfreq": "modFreq", "modamount": "modAmount"}),
]
COMPRESSOR_PARAMS = {"ingain": "gain", "threshold": "threshold", "ratio": "ratio",
                     "attackms": "attack", "releasems": "release", "knee": "knee"}


class Log:
    def __init__(self):
        self.lines = []

    def note(self, text):
        self.lines.append(text)
        print("  " + text)


# ----------------------------------------------------------------- xml helpers

def read_xml(path):
    """Parse an ofxXmlSettings/ofxGui file. These often have several root
    elements and odd declarations, so they are wrapped in a <root> first."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = re.sub(r"<\?xml[^>]*\?>", "", text)
    try:
        return ET.fromstring("<root>" + text + "</root>")
    except ET.ParseError as error:
        raise ValueError("%s: %s" % (path, error))


def simplify(name):
    return re.sub(r"[^a-z]", "", name.lower())


def number(element, default=None):
    if element is None or element.text is None:
        return default
    try:
        return float(element.text.strip())
    except ValueError:
        return default


def find_child(element, simplified_name):
    for child in element:
        if simplify(child.tag) == simplified_name:
            return child
    return None


def clamp(value, low, high):
    return max(low, min(high, value))


def source_id(old_index):
    old_index = int(old_index or 0)
    return "sensor%d" % old_index if 1 <= old_index <= NUM_SLOTS else "none"


def param(spec, value, low=None, high=None, connect=0, log=None, where=""):
    default, lowest, highest = spec
    raw = {"value": value if value is not None else default,
           "min": low if low is not None else lowest,
           "max": high if high is not None else highest}
    out = {key: clamp(v, lowest, highest) for key, v in raw.items()}
    if log is not None and out != raw:
        log.note("%s: value outside %g..%g, clamped" % (where, lowest, highest))
    out["source"] = source_id(connect)
    return out


# ---------------------------------------------------------------- converters

def convert_app_settings(root, log):
    s = root.find("SETTINGS")
    if s is None:
        s = root

    def get(key, default):
        element = s.find(key)
        if element is None or element.text is None:
            return default
        text = element.text.strip()
        if isinstance(default, str):
            return text
        try:
            return type(default)(float(text))
        except ValueError:
            return default

    return {
        "max_sensor": get("MAX_SENSOR_VALUE", 520),
        "threshold": get("ACTIVE_THRESHOLD", 0.0025),
        "accumulated": get("ACCUMULATED_PRESSURE", 0) != 0,
        "denominator": get("ACCUMULATION_DENOMINATOR", 4.0),
        "click_ms": get("BUTTON_PRESS_MAX_WAIT", 220),
        "curve": get("EASING_SELECTOR", 0),
        "buffer": get("BUFFER_SIZE", 512),
        "buffers": get("NUMBER_OF_BUFFERS", 3),
        "device_id": get("AUDIO_DEVICE_ID", -1),
        "has_narration": get("HAS_NARRATION", 0) != 0,
        "narr_volume": get("NARRATION_VOLUME", 1.0),
        "narr_thresh": get("NARRATION_GLITCH_THRESH", 0.025),
        "narr_strand": get("NARRATION_GLITCH_STRAND", 0.0025),
        "narr_sensor": get("NARR_PLAY_WITH_SENSOR", 0) != 0,
        "hit": get("HIT_TO_CHANGE_PRESETS", 0) != 0,
        "hit_thresh": get("HIT_THRESHOLD", 0.085),
        "trough_thresh": get("TROUGH_THRESHOLD", 0.025),
        "max_trough": get("MAX_TROUGH_DURATION", 250),
        "max_peak": get("MAX_PEAK_DURATION", 80),
        "hits": get("REQUIRED_HITS", 8),
        "log": get("LOG_LEVEL", 1),
        "long_press_quits": get("SH_P", 0) != 0,
        "shutdown": get("D_SH", 0) != 0,
        "osc_local": get("LOCAL_OSC_PORT", 1234),
        "osc_remote_port": get("REMOTE_OSC_PORT", 1235),
        "osc_remote_ip": get("REMOTE_OSC_IP", "127.0.0.1"),
        "osc_debug": get("OSC_DEBUG", 0) != 0,
        "advance": [get("TIME_ADVANCE_INTERVAL_%d" % (i + 1), 0.0002) for i in range(NUM_PRESETS)],
    }


def convert_grain_slot(slot_element, slot_number, log, where):
    """One <Slot_N> group of an old preset file."""
    # simplify() drops digits, so "in_density_1" is matched as "indensity".
    groups = {simplify(child.tag): child for child in slot_element}

    def group_values(group_key, value_key, min_key, max_key, connect_key):
        g = groups.get(group_key)
        if g is None:
            return None, None, None, 0
        return (number(find_child(g, value_key)), number(find_child(g, min_key)),
                number(find_child(g, max_key)), number(find_child(g, connect_key), 0))

    grain = {}
    table = [
        ("length", "inlength", "inlength", "inlengthmin", "inlengthmax", "ilconnectto"),
        ("density", "density", "indensity", "indensitymin", "indensitymax", "idconnectto"),
        ("distanceJitter", "distancejitter", "distancejitter", "distancejittermin", "distancejittermax", "djconnectto"),
        ("pitchJitter", "pitchjitter", "pitchjitter", "pitchjittermin", "pitchjittermax", "pjconnectto"),
        ("pitch", "pitch", "pitch", "pitchmin", "pitchmax", "pitchconnectto"),
        ("spread", "spread", "spread", "spreadmin", "spreadmax", "sconnectto"),
        ("volume", "volume", "volume", "volumemin", "volumemax", "vconnectto"),
        ("position", "position", "playposition", "playpositionmin", "playpositionmax", "pconnectto"),
    ]
    for new_id, group_key, value_key, min_key, max_key, connect_key in table:
        value, low, high, connect = group_values(group_key, value_key, min_key, max_key, connect_key)
        grain[new_id] = param(GRAIN_SPECS[new_id], value, low, high, connect, log, "%s.%s" % (where, new_id))

    direction = groups.get("direction")
    grain["direction"] = param(GRAIN_SPECS["direction"],
                               number(find_child(direction, "grainforwards")) if direction is not None else None)

    window = 2
    window_group = groups.get("windowtypeslot")
    if window_group is not None:
        window = int(number(find_child(window_group, "windowtype"), 2))
    return grain, WINDOWS[clamp(window, 0, len(WINDOWS) - 1)]


def effect_params(old_group, new_type, mapping, log, where):
    """Convert one old effect group (e.g. <Reverb_slot_1>) to new params."""
    specs = EFFECT_SPECS[new_type]
    params = {}
    for old_base, new_id in mapping.items():
        value = low = high = None
        connect = 0
        for child in old_group:
            key = simplify(child.tag)
            if key == old_base:
                value = number(child)
            elif key == old_base + "min":
                low = number(child)
            elif key == old_base + "max":
                high = number(child)
            elif key == old_base + "connectto":
                connect = number(child, 0)
        params[new_id] = param(specs[new_id], value, low, high, connect, log, "%s.%s" % (where, new_id))
    return params


def default_params(new_type):
    return {pid: {"value": spec[0], "min": spec[1], "max": spec[2], "source": "none"}
            for pid, spec in EFFECT_SPECS[new_type].items()}


def convert_effects(enabled_root, params_root, slot_number, log, where):
    chain = []
    slot_flags = enabled_root.find("SLOTS/SLOT_%d" % slot_number) if enabled_root is not None else None
    slot_params = None
    if params_root is not None:
        slot_params = params_root.find("Effects_Slot_%d" % slot_number)

    for flag, group_prefix, new_type, mapping in OLD_EFFECTS:
        on = slot_flags is not None and number(slot_flags.find(flag), 0) != 0
        if not on:
            continue
        old_group = None
        if slot_params is not None:
            old_group = slot_params.find("%s_slot_%d" % (group_prefix, slot_number))
        params = (effect_params(old_group, new_type, mapping, log, "%s.%s" % (where, new_type))
                  if old_group is not None else default_params(new_type))
        chain.append({"type": new_type, "enabled": True, "params": params})

    # Every old slot ended in a compressor with input gain.
    comp_group = slot_params.find("Compressor_slot_%d" % slot_number) if slot_params is not None else None
    comp = (effect_params(comp_group, "compressor", COMPRESSOR_PARAMS, log, where + ".compressor")
            if comp_group is not None else default_params("compressor"))
    chain.append({"type": "compressor", "enabled": True, "params": comp})
    return chain


def convert_narration(narr_root, app, narration_file):
    grain = {key: {"value": spec[0], "min": spec[1], "max": spec[2], "source": "none"}
             for key, spec in GRAIN_SPECS.items()}
    window = "hann"
    if narr_root is not None:
        n = narr_root.find("Narration")
        if n is None:
            n = narr_root
        mapping = {"inlength": "length", "indensity": "density", "distancejitter": "distanceJitter",
                   "pitchjitter": "pitchJitter", "pitch": "pitch", "spread": "spread", "direction": "direction"}
        for child in n:
            key = simplify(child.tag)
            if key in mapping:
                new_id = mapping[key]
                default, lowest, highest = GRAIN_SPECS[new_id]
                value = number(child, default)
                grain[new_id]["value"] = clamp(value, lowest, highest)
            if key == "windowtype":
                window = WINDOWS[clamp(int(number(child, 2)), 0, len(WINDOWS) - 1)]
    return {
        "enabled": app["has_narration"],
        "file": narration_file,
        "volume": app["narr_volume"],
        "startWithSensor": app["narr_sensor"],
        "autoStartDelayMs": 5000,
        "glitchThreshold": app["narr_thresh"],
        "glitchStrand": app["narr_strand"],
        "window": window,
        "grain": grain,
        "effects": [{"type": "compressor", "enabled": True, "params": default_params("compressor")}],
    }


def find_file(folder, *names):
    for name in names:
        path = os.path.join(folder, name)
        if os.path.exists(path):
            return path
    return None


def convert_performer(root_dir, name, dry_run):
    folder = os.path.join(root_dir, name)
    log = Log()
    print("\n%s" % name)
    app_path = find_file(folder, "%s_appSettings.xml" % name)
    if app_path is None:
        log.note("no %s_appSettings.xml, skipped" % name)
        return False
    app = convert_app_settings(read_xml(app_path), log)

    files_path = find_file(folder, "%s_fileSettings.xml" % name)
    files_root = read_xml(files_path) if files_path else None
    paths = files_root.find("PATHS") if files_root is not None else None

    def sample(slot, preset):
        if paths is None:
            return ""
        element = paths.find("FILE_%d_%d" % (slot, preset))
        return element.text.strip() if element is not None and element.text else ""

    narration_file = ""
    if paths is not None:
        for tag in ("NARRATION_FILE", "NARR_1TION_FILE"):  # the second is a typo found in real files
            element = paths.find(tag)
            if element is not None and element.text:
                narration_file = element.text.strip()
                break

    preset_files = []
    presets = []
    for p in range(1, NUM_PRESETS + 1):
        grain_path = find_file(folder, "%s_preset_%d.xml" % (name, p))
        if grain_path is None:
            continue
        grain_root = read_xml(grain_path)
        enabled_path = find_file(folder, "%s_effectSettings_preset_%d.xml" % (name, p))
        params_path = find_file(folder, "%s_effectParameterSettings_preset_%d.xml" % (name, p))
        enabled_root = read_xml(enabled_path) if enabled_path else None
        params_root = read_xml(params_path) if params_path else None

        slots = []
        for s in range(1, NUM_SLOTS + 1):
            where = "preset %d slot %d" % (p, s)
            slot_element = grain_root.find("Slot_%d" % s)
            if slot_element is None:
                grain = {k: {"value": v[0], "min": v[1], "max": v[2], "source": "none"} for k, v in GRAIN_SPECS.items()}
                window = "hann"
            else:
                grain, window = convert_grain_slot(slot_element, s, log, where)
            file_name = sample(s, p)
            slots.append({
                "enabled": bool(file_name) and (not app["accumulated"] or s == 1),
                "file": file_name,
                "window": window,
                "gate": "auto",
                "voices": 8,
                "grain": grain,
                "effects": convert_effects(enabled_root, params_root, s, log, where),
            })
        presets.append({
            "version": 1,
            "name": "Preset %d" % p,
            "playheadSpeed": round(app["advance"][p - 1] * ASSUMED_OLD_FPS, 6),
            "slots": slots,
            "master": {"gain": 1.0, "effects": []},
        })
        preset_files.append("presets/preset_%d.json" % p)

    if not presets:
        log.note("no %s_preset_N.xml files, skipped" % name)
        return False

    narr_path = find_file(folder, "%s_narr.xml" % name)
    profile = {
        "version": 1,
        "name": name,
        "logLevel": LOG_LEVELS.get(int(app["log"]), "notice"),
        "playMode": "accumulated" if app["accumulated"] else "multi",
        "controlRateHz": 100,
        "presetSwitchMinIntervalMs": 4000,
        "presets": preset_files,
        "sensors": {
            "maxRaw": int(app["max_sensor"]),
            "activeThreshold": app["threshold"],
            "curve": CURVES[clamp(int(app["curve"]), 0, 3)],
            "calibrationFrames": 16,
            "accumulationDenominator": app["denominator"],
            "smoothingMs": 0.0,
        },
        "audio": {
            "sampleRate": 44100,
            "bufferSize": int(app["buffer"]),
            "numBuffers": int(app["buffers"]),
            "api": "default",
            "deviceName": "",
            "deviceId": int(app["device_id"]),
            "masterGain": 1.0,
        },
        "gestures": {
            "hitToChangePreset": app["hit"],
            "hitThreshold": app["hit_thresh"],
            "troughThreshold": app["trough_thresh"],
            "maxPeakMs": int(app["max_peak"]),
            "maxTroughMs": int(app["max_trough"]),
            "requiredHits": int(app["hits"]),
            "minSensorsPerHit": 2,
        },
        "hardware": {
            "spiDevice": "/dev/spidev0.0",
            "adcChannels": [0, 1, 2, 3, 4, 5],
            "buttonPin": 19,
            "buttonActiveLow": True,
            "blueLedPin": 5,
            "redLedPin": 6,
            "relayPin": 13,
            "relayPulseMs": 400,
            "clickTimeoutMs": int(app["click_ms"]),
            "longPressQuits": app["long_press_quits"],
            "shutdownOnQuit": app["shutdown"],
            "accelerometer": True,
            "i2cBus": "/dev/i2c-1",
            "accelAddress": 24,
        },
        "osc": {
            "telemetry": app["osc_debug"],
            "remoteHost": app["osc_remote_ip"],
            "remotePort": int(app["osc_remote_port"]),
            "control": True,
            "controlPort": int(app["osc_local"]),
            "telemetryRateHz": 20,
        },
        "narration": convert_narration(read_xml(narr_path) if narr_path else None, app, narration_file),
    }

    if app["device_id"] >= 0:
        log.note("audio.deviceId %d copied; better set audio.deviceName (ids change between boots)" % app["device_id"])
    log.note("effect order kept (bitcrush, decimator, chorus, filter, delay, reverb, compressor); "
             "delay/reverb sends became inserts with a wet mix")
    log.note("playhead speed = TIME_ADVANCE_INTERVAL x %g frames/s" % ASSUMED_OLD_FPS)

    outputs = [(os.path.join(folder, "profile.json"), profile)]
    for preset, file_name in zip(presets, preset_files):
        outputs.append((os.path.join(folder, file_name), preset))
    for path, data in outputs:
        if dry_run:
            print("  would write %s" % path)
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        print("  wrote %s" % path)
    return True


def active_user(root_dir):
    path = os.path.join(root_dir, "username.xml")
    if not os.path.exists(path):
        return None
    element = read_xml(path).find("NAMES/USERNAME")
    return element.text.strip() if element is not None and element.text else None


def main():
    parser = argparse.ArgumentParser(description="Convert old Gaugemancy XML settings to JSON.")
    parser.add_argument("data", help="data folder (contains username.xml and one folder per performer)")
    parser.add_argument("--only", help="convert just this performer")
    parser.add_argument("--dry-run", action="store_true", help="show what would be written")
    args = parser.parse_args()

    names = [args.only] if args.only else sorted(
        d for d in os.listdir(args.data)
        if os.path.isdir(os.path.join(args.data, d)) and
        os.path.exists(os.path.join(args.data, d, "%s_appSettings.xml" % d)))
    if not names:
        print("no performer folders with *_appSettings.xml found in %s" % args.data)
        return 1

    converted = [name for name in names if convert_performer(args.data, name, args.dry_run)]
    active = active_user(args.data) or (converted[0] if converted else "Default")
    root_json = {"version": 1, "activeProfile": active}
    root_path = os.path.join(args.data, "gaugemancy.json")
    if args.dry_run:
        print("\nwould write %s (activeProfile %s)" % (root_path, active))
    else:
        with open(root_path, "w", encoding="utf-8") as f:
            json.dump(root_json, f, indent=2)
            f.write("\n")
        print("\nwrote %s (activeProfile %s)" % (root_path, active))
    print("converted: %s" % ", ".join(converted))
    return 0


if __name__ == "__main__":
    sys.exit(main())
