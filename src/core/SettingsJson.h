#pragma once

// -----------------------------------------------------------------------------
// JSON conversion for the settings structs.
//
// Reading is forgiving: a missing key keeps its default, a value of the wrong
// type or out of range is replaced/clamped, and each problem is appended to
// `warnings` with its JSON path (e.g. "slots[2].grain.pitch.max: clamped").
// This keeps the instrument running with a half-edited settings file instead of
// refusing to start in the middle of a show.
//
// Uses nlohmann::json (bundled with openFrameworks as ofJson).
// -----------------------------------------------------------------------------

#include "Settings.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace gm {

using Json = nlohmann::ordered_json;
using Warnings = std::vector<std::string>;

Json toJson(const ModulatedParam & param);
Json toJson(const GrainSettings & grain);
Json toJson(const EffectSettings & effect);
Json toJson(const EffectChainSettings & chain);
Json toJson(const SlotSettings & slot);
Json toJson(const PresetSettings & preset);
Json toJson(const NarrationSettings & narration);
Json toJson(const ProfileSettings & profile);
Json toJson(const RootSettings & root);

ModulatedParam modulatedParamFromJson(const Json & json, const ParamSpec & spec, const std::string & path, Warnings & warnings);
GrainSettings grainFromJson(const Json & json, const std::string & path, Warnings & warnings);
EffectSettings effectFromJson(const Json & json, const std::string & path, Warnings & warnings);
EffectChainSettings effectChainFromJson(const Json & json, const std::string & path, Warnings & warnings);
SlotSettings slotFromJson(const Json & json, const std::string & path, Warnings & warnings);
PresetSettings presetFromJson(const Json & json, Warnings & warnings);
NarrationSettings narrationFromJson(const Json & json, const std::string & path, Warnings & warnings);
ProfileSettings profileFromJson(const Json & json, Warnings & warnings);
RootSettings rootFromJson(const Json & json, Warnings & warnings);

} // namespace gm
