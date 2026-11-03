#pragma once

// -----------------------------------------------------------------------------
// SettingsStore: loads and saves profiles and presets as JSON files and turns
// relative file names into full paths.
//
//   SettingsStore store("/media/Data");
//   LoadedProfile loaded = store.loadActiveProfile();
//   for (auto & w : loaded.warnings) log(w);
//   std::string wav = store.audioPath(loaded.profile.name, "BIRDS_1.wav");
//
// Saving writes to "<file>.tmp" and renames it over the original, so a power
// cut during a save leaves either the old or the new file, never half of one.
// -----------------------------------------------------------------------------

#include "SettingsJson.h"

#include <string>
#include <vector>

namespace gm {

struct LoadedProfile {
	ProfileSettings profile;
	std::vector<PresetSettings> presets; ///< one per profile.presetFiles entry
	Warnings warnings;                   ///< everything odd found while loading
	bool profileFileFound = false;
};

class SettingsStore {
public:
	SettingsStore() = default;
	explicit SettingsStore(std::string dataRoot) : dataRoot_(std::move(dataRoot)) {}

	void setDataRoot(const std::string & dataRoot) { dataRoot_ = dataRoot; }
	const std::string & dataRoot() const { return dataRoot_; }

	// ---- paths --------------------------------------------------------------
	std::string rootFilePath() const;                              ///< <root>/gaugemancy.json
	std::string profileDir(const std::string & profile) const;     ///< <root>/<profile>
	std::string profileFilePath(const std::string & profile) const;///< .../profile.json
	std::string presetFilePath(const std::string & profile, const std::string & presetFile) const;
	std::string audioDir(const std::string & profile) const;       ///< .../audio
	/// Full path of a sample. Absolute paths are returned unchanged.
	std::string audioPath(const std::string & profile, const std::string & file) const;

	// ---- loading ------------------------------------------------------------
	RootSettings loadRoot(Warnings & warnings) const;
	/// Loads the profile and all its presets. Missing files give defaults
	/// plus a warning; it never throws.
	LoadedProfile loadProfile(const std::string & profile) const;
	LoadedProfile loadActiveProfile() const;

	/// Folders under the data root that contain a profile.json.
	std::vector<std::string> listProfiles() const;

	// ---- saving -------------------------------------------------------------
	bool saveRoot(const RootSettings & root, std::string & error) const;
	bool saveProfile(const ProfileSettings & profile, std::string & error) const;
	bool savePreset(const ProfileSettings & profile, int presetIndex, const PresetSettings & preset, std::string & error) const;
	/// Profile plus every preset.
	bool saveAll(const ProfileSettings & profile, const std::vector<PresetSettings> & presets, std::string & error) const;

	/// Create a new profile folder with defaults (does not overwrite).
	bool createProfile(const std::string & profile, std::string & error) const;

	// ---- helpers (public for tests and tools) -------------------------------
	static bool readJsonFile(const std::string & path, Json & out, std::string & error);
	static bool writeJsonFileAtomic(const std::string & path, const Json & json, std::string & error);
	static bool isAbsolutePath(const std::string & path);

private:
	std::string dataRoot_ = ".";
};

} // namespace gm
