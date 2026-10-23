#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "carwear/config/config.h"
#include "carwear/harness/statistics.h"

namespace carwear {

toml::table aggregate_to_toml(const Aggregate& aggregate);
std::string aggregate_to_toml_string(const Aggregate& aggregate);

// Writes aggregate.toml into `directory`, and runs.csv when `write_runs_csv`
// is true.
bool write_aggregate_outputs(const std::filesystem::path& directory,
                             const Aggregate& aggregate,
                             const std::vector<RunRecord>& records,
                             bool write_runs_csv, std::string& error);

}  // namespace carwear
