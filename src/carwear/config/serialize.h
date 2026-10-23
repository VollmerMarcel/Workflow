#pragma once

#include <string>

#include <toml++/toml.hpp>

#include "carwear/config/config.h"

namespace carwear {

// Build a TOML document that echoes every resolved configuration value,
// including defaults and climate-preset expansions.
toml::table config_to_toml(const Config& config);
std::string config_to_toml_string(const Config& config);

}  // namespace carwear
