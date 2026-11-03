#include "SettingsStore.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace gm {

// ------------------------------------------------------------------ paths --

namespace {
std::string join(const std::string & a, const std::string & b) {
	return (fs::path(a) / fs::path(b)).lexically_normal().string();
}
} // namespace

std::string SettingsStore::rootFilePath() const {
	return join(dataRoot_, "gaugemancy.json");
}

std::string SettingsStore::profileDir(const std::string & profile) const {
	return join(dataRoot_, profile);
}

std::string SettingsStore::profileFilePath(const std::string & profile) const {
	return join(profileDir(profile), "profile.json");
}

std::string SettingsStore::presetFilePath(const std::string & profile, const std::string & presetFile) const {
	return isAbsolutePath(presetFile) ? presetFile : join(profileDir(profile), presetFile);
}

std::string SettingsStore::audioDir(const std::string & profile) const {
	return join(profileDir(profile), "audio");
}

std::string SettingsStore::audioPath(const std::string & profile, const std::string & file) const {
	if (file.empty()) {
		return "";
	}
	return isAbsolutePath(file) ? file : join(audioDir(profile), file);
}

bool SettingsStore::isAbsolutePath(const std::string & path) {
	return fs::path(path).is_absolute();
}

// ------------------------------------------------------------- file i/o ----

bool SettingsStore::readJsonFile(const std::string & path, Json & out, std::string & error) {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		error = "cannot open " + path;
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	// Comments are allowed so settings files can carry notes for the performers.
	out = Json::parse(buffer.str(), nullptr, false, true);
	if (out.is_discarded()) {
		error = path + " is not valid JSON";
		return false;
	}
	return true;
}

bool SettingsStore::writeJsonFileAtomic(const std::string & path, const Json & json, std::string & error) {
	std::error_code ec;
	fs::path target(path);
	if (target.has_parent_path()) {
		fs::create_directories(target.parent_path(), ec);
	}
	std::string temp = path + ".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) {
			error = "cannot write " + temp;
			return false;
		}
		out << json.dump(2) << "\n";
		out.flush();
		if (!out) {
			error = "write failed for " + temp;
			return false;
		}
	}
	fs::rename(temp, target, ec);
	if (ec) {
		error = "cannot replace " + path + ": " + ec.message();
		fs::remove(temp, ec);
		return false;
	}
	return true;
}

// -------------------------------------------------------------- loading ----

RootSettings SettingsStore::loadRoot(Warnings & warnings) const {
	Json json;
	std::string error;
	if (!readJsonFile(rootFilePath(), json, error)) {
		warnings.push_back(error + " (using profile 'Default')");
		return RootSettings {};
	}
	return rootFromJson(json, warnings);
}

LoadedProfile SettingsStore::loadProfile(const std::string & profile) const {
	LoadedProfile loaded;
	loaded.profile.name = profile;

	Json json;
	std::string error;
	if (readJsonFile(profileFilePath(profile), json, error)) {
		loaded.profileFileFound = true;
		Warnings w;
		loaded.profile = profileFromJson(json, w);
		for (auto & message : w) loaded.warnings.push_back("profile.json " + message);
		loaded.profile.name = profile; // folder name wins over the name field
	} else {
		loaded.warnings.push_back(error + " (using defaults)");
	}

	for (const std::string & presetFile : loaded.profile.presetFiles) {
		Json presetJson;
		std::string presetError;
		std::string path = presetFilePath(profile, presetFile);
		if (readJsonFile(path, presetJson, presetError)) {
			Warnings w;
			loaded.presets.push_back(presetFromJson(presetJson, w));
			for (auto & message : w) loaded.warnings.push_back(presetFile + " " + message);
		} else {
			loaded.warnings.push_back(presetError + " (using an empty preset)");
			loaded.presets.emplace_back();
		}
	}
	return loaded;
}

LoadedProfile SettingsStore::loadActiveProfile() const {
	Warnings warnings;
	RootSettings root = loadRoot(warnings);
	LoadedProfile loaded = loadProfile(root.activeProfile);
	loaded.warnings.insert(loaded.warnings.begin(), warnings.begin(), warnings.end());
	return loaded;
}

std::vector<std::string> SettingsStore::listProfiles() const {
	std::vector<std::string> names;
	std::error_code ec;
	for (const auto & entry : fs::directory_iterator(dataRoot_, ec)) {
		if (entry.is_directory(ec) && fs::exists(entry.path() / "profile.json", ec)) {
			names.push_back(entry.path().filename().string());
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

// --------------------------------------------------------------- saving ----

bool SettingsStore::saveRoot(const RootSettings & root, std::string & error) const {
	return writeJsonFileAtomic(rootFilePath(), toJson(root), error);
}

bool SettingsStore::saveProfile(const ProfileSettings & profile, std::string & error) const {
	return writeJsonFileAtomic(profileFilePath(profile.name), toJson(profile), error);
}

bool SettingsStore::savePreset(const ProfileSettings & profile, int presetIndex, const PresetSettings & preset, std::string & error) const {
	if (presetIndex < 0 || presetIndex >= static_cast<int>(profile.presetFiles.size())) {
		error = "preset index " + std::to_string(presetIndex) + " has no file in the profile";
		return false;
	}
	return writeJsonFileAtomic(presetFilePath(profile.name, profile.presetFiles[presetIndex]), toJson(preset), error);
}

bool SettingsStore::saveAll(const ProfileSettings & profile, const std::vector<PresetSettings> & presets, std::string & error) const {
	if (!saveProfile(profile, error)) {
		return false;
	}
	for (size_t i = 0; i < presets.size() && i < profile.presetFiles.size(); ++i) {
		if (!savePreset(profile, static_cast<int>(i), presets[i], error)) {
			return false;
		}
	}
	return true;
}

bool SettingsStore::createProfile(const std::string & profile, std::string & error) const {
	std::error_code ec;
	if (fs::exists(profileFilePath(profile), ec)) {
		error = "profile '" + profile + "' already exists";
		return false;
	}
	fs::create_directories(audioDir(profile), ec);
	ProfileSettings settings;
	settings.name = profile;
	std::vector<PresetSettings> presets(settings.presetFiles.size());
	return saveAll(settings, presets, error);
}

} // namespace gm
