#include <gtest/gtest.h>

#include <string>

#include "carwear/config/loader.h"
#include "carwear/config/serialize.h"
#include "test_support.h"

namespace carwear {
namespace {

TEST(ConfigTest, MinimalCaseLoadsWithDefaults) {
    const ConfigLoadResult loaded = test::load_case();
    ASSERT_TRUE(loaded.ok());
    const Config& config = *loaded.config;
    EXPECT_EQ(config.case_config.name, "unit-test");
    EXPECT_EQ(config.case_config.horizon_days, 365);
    EXPECT_EQ(config.vehicle.mass_kilograms, 1450.0);
    EXPECT_EQ(config.monte_carlo.number_of_runs, 20000);
    EXPECT_TRUE(config.economics.budget_euros.has_value() == false);
}

TEST(ConfigTest, RunSeedIsParsed) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[run]
run_seed = 42
)toml");
    ASSERT_TRUE(loaded.ok());
    ASSERT_TRUE(loaded.config->run.run_seed.has_value());
    EXPECT_EQ(*loaded.config->run.run_seed, 42u);
}

TEST(ConfigTest, UnknownKeyProducesWarningButStillLoads) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[vehicle]
mass_kilogram = 1400
)toml");
    ASSERT_TRUE(loaded.ok());
    bool found_warning = false;
    for (const Diagnostic& item : loaded.diagnostics.items()) {
        if (item.severity == Severity::warning &&
            item.message.find("mass_kilogram") != std::string::npos) {
            found_warning = true;
        }
    }
    EXPECT_TRUE(found_warning);
}

TEST(ConfigTest, MissingStartDateIsAnError) {
    const ConfigLoadResult loaded = parse_config(R"toml(
[case]
name = "no-date"
)toml",
                                                 "no-date");
    EXPECT_FALSE(loaded.ok());
    EXPECT_TRUE(loaded.diagnostics.has_errors());
}

TEST(ConfigTest, InvalidHorizonIsAnError) {
    const ConfigLoadResult loaded = parse_config(R"toml(
[case]
name = "bad-horizon"
start_date = 2026-01-01
horizon_days = 0
)toml",
                                                 "bad-horizon");
    EXPECT_FALSE(loaded.ok());
    EXPECT_TRUE(loaded.diagnostics.has_errors());
}

TEST(ConfigTest, InvalidProbabilityIsAnError) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[user]
style_aggressiveness = 1.5
)toml");
    EXPECT_FALSE(loaded.ok());
}

TEST(ConfigTest, NegativeMassIsAnError) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[vehicle]
mass_kilograms = -1.0
)toml");
    EXPECT_FALSE(loaded.ok());
}

TEST(ConfigTest, UnknownClimateZoneIsAnError) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[climate]
zone = "tropical"
)toml");
    EXPECT_FALSE(loaded.ok());
}

TEST(ConfigTest, NordicPresetEnablesWinterTires) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[climate]
zone = "nordic"
)toml");
    ASSERT_TRUE(loaded.ok());
    EXPECT_TRUE(loaded.config->climate.winter_tires);
    EXPECT_TRUE(loaded.config->user.winter_tires);
}

TEST(ConfigTest, UserWinterTireSettingOverridesPreset) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[climate]
zone = "nordic"

[user]
winter_tires = false
)toml");
    ASSERT_TRUE(loaded.ok());
    EXPECT_FALSE(loaded.config->user.winter_tires);
}

TEST(ConfigTest, ClimateOverridesPresetValues) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[climate]
zone = "nordic"
monthly_temperature_mean_celsius = [-10, -9, -5, 0, 5, 10, 12, 11, 6, 1, -4, -8]
)toml");
    ASSERT_TRUE(loaded.ok());
    EXPECT_DOUBLE_EQ(
        loaded.config->climate.monthly_temperature_mean_celsius[0], -10.0);
    EXPECT_DOUBLE_EQ(
        loaded.config->climate.monthly_temperature_mean_celsius[6], 12.0);
}

TEST(ConfigTest, ResolvedConfigEchoContainsSchemaAndValues) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[climate]
zone = "alpine"

[user]
commute_distance_kilometres = 12.5
)toml");
    ASSERT_TRUE(loaded.ok());
    const std::string text = config_to_toml_string(*loaded.config);
    EXPECT_NE(text.find("schema_version"), std::string::npos);
    EXPECT_NE(text.find("commute_distance_kilometres"), std::string::npos);
    EXPECT_NE(text.find("alpine"), std::string::npos);
}

TEST(ConfigTest, ReplacementIndicesAreValidated) {
    const ConfigLoadResult loaded = test::load_case(R"toml(
[model]
suspension_replacement_wear_index = 1.5
)toml");
    EXPECT_FALSE(loaded.ok());
}

}  // namespace
}  // namespace carwear
