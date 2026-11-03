#pragma once

// -----------------------------------------------------------------------------
// Diagnostics: command line tools for testing over SSH. Each prints a report
// to stdout and returns a process exit code (0 = all good).
//
//   --selftest     settings, sample files, GPIO, ADC, accelerometer, audio
//   --benchmark    offline rendering of every preset, CPU load per preset
//   --list-audio   audio output devices for the configured API
// -----------------------------------------------------------------------------

#include "../core/Options.h"

#include <string>

namespace gm {

int runSelfTest(const Options & options, const std::string & dataRoot);
int runBenchmark(const Options & options, const std::string & dataRoot);
int runListAudio(const Options & options, const std::string & dataRoot);

} // namespace gm
