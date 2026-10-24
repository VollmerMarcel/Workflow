#pragma once

#include <optional>
#include <string>

#include "carwear/config/config.h"
#include "carwear/core/diagnostics.h"

namespace carwear {

struct ConfigLoadResult {
    std::optional<Config> config;
    Diagnostics diagnostics;

    bool ok() const noexcept { return config.has_value() && !diagnostics.has_errors(); }
};

// Load and validate a TOML configuration file. Parse errors, unknown keys and
// invalid values are reported through the diagnostics list.
ConfigLoadResult load_config(const std::string& path);
ConfigLoadResult parse_config(const std::string& text, const std::string& source_name);

// Validate an already parsed configuration. Returns false when errors were
// added to the diagnostics.
bool validate_config(const Config& config, Diagnostics& diagnostics);

// Climate presets, exposed for listing from the command line.
bool is_known_climate_zone(const std::string& name);
std::vector<std::string> known_climate_zones();
ClimateConfig climate_preset(const std::string& name);

}  // namespace carwear
