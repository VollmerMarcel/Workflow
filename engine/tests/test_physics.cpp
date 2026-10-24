#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "carwear/config/config.h"
#include "carwear/model/physics.h"
#include "test_support.h"

namespace carwear {
namespace {

Config test_config() {
    const ConfigLoadResult loaded = test::load_case();
    return *loaded.config;
}

Drive make_drive() {
    Drive drive;
    drive.purpose = TripPurpose::commute;
    drive.route_class = RouteClass::mixed;
    drive.distance_kilometres = 20.0;
    drive.mean_speed_kph = 50.0;
    drive.speed_variation_kph = 8.0;
    drive.stop_density_per_kilometre = 0.3;
    drive.brake_onset_speed_fraction = 0.7;
    drive.lateral_acceleration = 1.0;
    drive.positive_acceleration = 1.5;
    drive.idle_fraction = 0.15;
    drive.cold_start = true;
    drive.short_trip = false;
    drive.start_minute = 8 * 60;
    drive.duration_hours =
        drive.distance_kilometres / drive.mean_speed_kph;
    return drive;
}

TEST(PhysicsTest, ColdStartWearDecreasesWithTemperatureAndRespectsBounds) {
    const Config config = test_config();
    const double warm = cold_start_equivalent_distance(config, 20.0);
    const double cold = cold_start_equivalent_distance(config, -10.0);
    EXPECT_NEAR(warm, config.model.cold_start_wear_base_kilometres, 1e-9);
    EXPECT_GT(cold, warm);
    EXPECT_LE(cold, config.model.cold_start_wear_maximum_kilometres);
    EXPECT_GE(cold, config.model.cold_start_wear_minimum_kilometres);
}

TEST(PhysicsTest, FuelConsumptionIsPositiveAndAirConditioningCostsFuel) {
    Config config = test_config();
    SimulationState state;
    state.tire_pressure_bar.fill(2.3);
    state.front_pad_wear_factor = 1.0;
    state.rear_pad_wear_factor = 1.0;
    state.front_disc_wear_factor = 1.0;
    state.rear_disc_wear_factor = 1.0;

    Weather weather;
    weather.temperature_celsius = 15.0;
    const double baseline = fuel_litres(config, make_drive(), state, weather);
    EXPECT_GT(baseline, 0.0);

    weather.temperature_celsius = 30.0;
    config.user.air_conditioning = false;
    const double without_ac = fuel_litres(config, make_drive(), state, weather);
    config.user.air_conditioning = true;
    const double with_ac = fuel_litres(config, make_drive(), state, weather);
    EXPECT_GT(with_ac, without_ac);
}

TEST(PhysicsTest, BatteryHealthFallsWithAgeHeatAndCycles) {
    const Config config = test_config();
    SimulationState state;
    const double fresh = battery_state_of_health(config, state);

    state.battery_age_years = 4.0;
    const double aged = battery_state_of_health(config, state);
    EXPECT_LT(aged, fresh);

    state.battery_degree_days_above_25 = 1000.0;
    state.battery_start_count = 5000.0;
    const double degraded = battery_state_of_health(config, state);
    EXPECT_LT(degraded, aged);
    EXPECT_GE(degraded, 0.0);
}

TEST(PhysicsTest, RequiredHealthRisesInColdWeather) {
    const Config config = test_config();
    const double warm = required_battery_health(config, 20.0);
    const double cold = required_battery_health(config, -10.0);
    EXPECT_GT(cold, warm);
    EXPECT_LE(cold, 0.95);
}

TEST(PhysicsTest, NoStartProbabilityFallsWithHealth) {
    const Config config = test_config();
    const double required = required_battery_health(config, -10.0);
    const double healthy =
        battery_no_start_probability(config, required, 0.95);
    const double weak = battery_no_start_probability(config, required, 0.35);
    EXPECT_LT(healthy, weak);
    EXPECT_GE(healthy, 0.0);
    EXPECT_LE(weak, 1.0);
}

TEST(PhysicsTest, TirePressureFallsWithTemperature) {
    EXPECT_LT(effective_tire_pressure(2.3, -10.0),
              effective_tire_pressure(2.3, 30.0));
}

TEST(PhysicsTest, TireCornerSharesSumToOne) {
    const Config config = test_config();
    const auto shares = tire_corner_shares(config);
    EXPECT_NEAR(shares[0] + shares[1] + shares[2] + shares[3], 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(shares[0], shares[1]);
    EXPECT_DOUBLE_EQ(shares[2], shares[3]);
}

TEST(PhysicsTest, FailureHazardIncreasesWithWearAndFrailty) {
    const Config config = test_config();
    const double unworn = daily_failure_probability(
        config, 0.01, 0.0, 5.0, 1.0);
    const double worn = daily_failure_probability(
        config, 0.01, 0.8, 5.0, 1.0);
    const double frail = daily_failure_probability(
        config, 0.01, 0.8, 5.0, 2.0);
    EXPECT_GT(worn, unworn);
    EXPECT_GT(frail, worn);
    EXPECT_GE(unworn, 0.0);
    EXPECT_LE(frail, 1.0);
}

TEST(PhysicsTest, ReplacementTriggerIncreasesWithAdherence) {
    EXPECT_LT(replacement_trigger(1.6, 1.0, 0.0),
              replacement_trigger(1.6, 1.0, 1.0));
}

TEST(PhysicsTest, InstalledCostScalesWithQualityAndPriceIndex) {
    const double cheap = installed_cost(100.0, 0.0, 1.0);
    const double good = installed_cost(100.0, 1.0, 1.0);
    const double expensive_region = installed_cost(100.0, 1.0, 2.0);
    EXPECT_NEAR(cheap, 75.0, 1e-9);
    EXPECT_NEAR(good, 125.0, 1e-9);
    EXPECT_NEAR(expensive_region, 250.0, 1e-9);
}

TEST(PhysicsTest, MonthlyWeightMean) {
    const std::array<double, 12> weights{1, 1, 1, 1, 1, 1,
                                         1, 1, 1, 1, 1, 1};
    EXPECT_DOUBLE_EQ(monthly_weight_mean(weights), 1.0);
}

}  // namespace
}  // namespace carwear
