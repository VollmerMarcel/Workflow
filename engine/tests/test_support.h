#pragma once

#include <string>
#include <string_view>

#include "carwear/config/loader.h"

namespace carwear::test {

inline constexpr std::string_view kMinimalCase = R"toml(
[case]
name = "unit-test"
start_date = 2026-01-01
horizon_days = 365
)toml";

inline ConfigLoadResult load_case(std::string_view extra = {}) {
    std::string text{kMinimalCase};
    text.append(extra);
    return parse_config(text, "unit-test");
}

}  // namespace carwear::test
