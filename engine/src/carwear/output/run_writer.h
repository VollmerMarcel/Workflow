#pragma once

#include <filesystem>
#include <string>

#include <toml++/toml.hpp>

#include "carwear/config/config.h"
#include "carwear/model/types.h"

namespace carwear {

// One run, serialized according to the output contract.
toml::table run_to_toml(const RunResult& result);
std::string run_to_toml_string(const RunResult& result);

// Writes run.toml, run_events.csv, run_monthly.csv and, when requested,
// run_trips.csv into `directory`. Returns false and sets `error` on failure.
bool write_run_outputs(const Config& config,
                       const std::filesystem::path& directory,
                       const RunResult& result, std::string& error);

}  // namespace carwear
