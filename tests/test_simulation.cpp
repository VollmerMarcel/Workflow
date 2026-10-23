#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>

#include "carwear/config/config.h"
#include "carwear/model/simulation.h"
#include "carwear/model/state.h"
#include "test_support.h"

namespace carwear {
namespace {

Config test_config() {
    const ConfigLoadResult loaded = test::load_case();
    return *loaded.config;
}

RunResult run(const Config& config, std::uint64_t seed) {
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    return run_model(config, inputs, seed);
}

TEST(SimulationTest, SameSeedProducesIdenticalRun) {
    const Config config = test_config();
    const RunResult first = run(config, 12345);
    const RunResult second = run(config, 12345);

    EXPECT_DOUBLE_EQ(first.totals.distance_kilometres,
                     second.totals.distance_kilometres);
    EXPECT_DOUBLE_EQ(first.totals.fuel_litres, second.totals.fuel_litres);
    EXPECT_DOUBLE_EQ(first.totals.total_cost_of_ownership_euros,
                     second.totals.total_cost_of_ownership_euros);
    ASSERT_EQ(first.events.size(), second.events.size());
    for (std::size_t i = 0; i < first.events.size(); ++i) {
        EXPECT_EQ(first.events[i].day, second.events[i].day);
        EXPECT_EQ(first.events[i].item, second.events[i].item);
        EXPECT_DOUBLE_EQ(first.events[i].cost_euros,
                         second.events[i].cost_euros);
    }
}

TEST(SimulationTest, DifferentSeedsProduceDifferentRuns) {
    const Config config = test_config();
    const RunResult first = run(config, 1);
    const RunResult second = run(config, 2);
    EXPECT_NE(first.totals.distance_kilometres,
              second.totals.distance_kilometres);
}

TEST(SimulationTest, AnnualRunHasTwelveMonthlySnapshots) {
    const Config config = test_config();
    const RunResult result = run(config, 7);
    ASSERT_EQ(result.monthly.size(), 12u);
    EXPECT_EQ(result.monthly.front().month, 1);
    EXPECT_EQ(result.monthly.back().month, 12);
    EXPECT_EQ(result.monthly.back().end_day, 365);
}

TEST(SimulationTest, ShortHorizonHasOneSnapshot) {
    Config config = test_config();
    config.case_config.horizon_days = 30;
    const RunResult result = run(config, 7);
    ASSERT_EQ(result.monthly.size(), 1u);
    EXPECT_EQ(result.monthly.front().start_day, 1);
    EXPECT_EQ(result.monthly.front().end_day, 30);
}

TEST(SimulationTest, TotalsAreConsistentWithMonthlyAndCategories) {
    const Config config = test_config();
    const RunResult result = run(config, 99);
    const RunTotals& totals = result.totals;

    EXPECT_GT(totals.distance_kilometres, 0.0);
    EXPECT_GT(totals.fuel_litres, 0.0);
    EXPECT_NEAR(totals.fuel_cost_euros,
                totals.fuel_litres *
                    config.economics.fuel_price_euros_per_litre,
                1e-6);

    const double category_sum =
        totals.fuel_cost_euros + totals.scheduled_maintenance_euros +
        totals.wear_replacement_euros + totals.unplanned_repair_euros +
        totals.consequential_euros + totals.paint_repair_euros;
    EXPECT_NEAR(result.monthly.back().cumulative_cost_euros, category_sum,
                1e-6);
    EXPECT_NEAR(totals.total_cost_of_ownership_euros,
                category_sum - totals.paint_repair_euros, 1e-6);

    EXPECT_NEAR(result.final_state.odometer_kilometres -
                    config.vehicle.odometer_kilometres,
                totals.distance_kilometres, 1e-9);
}

TEST(SimulationTest, EventCostsMatchCostCategories) {
    const Config config = test_config();
    const RunResult result = run(config, 2026);

    double scheduled_events = 0.0;
    double wear_events = 0.0;
    double failure_events = 0.0;
    double paint_events = 0.0;
    int previous_day = 0;
    for (const Event& event : result.events) {
        EXPECT_GE(event.day, 1);
        EXPECT_LE(event.day, result.horizon_days);
        EXPECT_GE(event.day, previous_day);
        previous_day = event.day;
        EXPECT_GE(event.cost_euros, 0.0);

        switch (event.kind) {
            case EventKind::scheduled_maintenance:
            case EventKind::inspection:
            case EventKind::tire_swap:
                scheduled_events += event.cost_euros;
                break;
            case EventKind::wear_replacement:
                wear_events += event.cost_euros;
                break;
            case EventKind::failure:
                failure_events += event.cost_euros;
                break;
            case EventKind::paint_repair:
                paint_events += event.cost_euros;
                break;
        }
    }

    EXPECT_NEAR(scheduled_events, result.totals.scheduled_maintenance_euros,
                1e-6);
    EXPECT_NEAR(wear_events, result.totals.wear_replacement_euros, 1e-6);
    EXPECT_NEAR(failure_events, result.totals.unplanned_repair_euros, 1e-6);
    EXPECT_NEAR(paint_events, result.totals.paint_repair_euros, 1e-6);
    EXPECT_EQ(result.totals.failure_count,
              static_cast<int>(
                  std::count_if(result.events.begin(), result.events.end(),
                                [](const Event& event) {
                                    return event.kind == EventKind::failure;
                                })));
}

TEST(SimulationTest, BatteryStartCountMatchesTripsWithoutReplacement) {
    Config config = test_config();
    config.case_config.horizon_days = 30;
    config.output.write_trips = true;
    const RunResult result = run(config, 11);
    ASSERT_GT(result.trips.size(), 0u);
    if (result.final_state.battery_replacement_count == 0) {
        EXPECT_EQ(result.final_state.battery_start_count,
                  static_cast<double>(result.trips.size()));
    }
}

TEST(SimulationTest, MonthlySnapshotsAreBounded) {
    const Config config = test_config();
    const RunResult result = run(config, 5);
    for (const MonthSnapshot& snapshot : result.monthly) {
        EXPECT_GE(snapshot.cost_euros, 0.0);
        EXPECT_GE(snapshot.front_pad_millimetres,
                  config.maintenance.front_pad_wear_limit_millimetres);
        EXPECT_LE(snapshot.front_pad_millimetres,
                  config.vehicle.front_pad_new_millimetres);
        EXPECT_GE(snapshot.battery_state_of_health, 0.0);
        EXPECT_LE(snapshot.battery_state_of_health, 1.0);
        for (double tread : snapshot.tire_tread_millimetres) {
            EXPECT_GE(tread, config.model.tire_tread_wear_limit_millimetres);
            EXPECT_LE(tread, config.vehicle.tire_new_tread_millimetres);
        }
    }
}

TEST(SimulationTest, WinterTireConfigurationRuns) {
    const ConfigLoadResult loaded = parse_config(R"toml(
[case]
name = "winter"
start_date = 2026-10-01
horizon_days = 365

[climate]
zone = "nordic"
)toml",
                                                 "winter");
    ASSERT_TRUE(loaded.ok());
    const RunResult result = run(*loaded.config, 3);
    EXPECT_GT(result.totals.distance_kilometres, 0.0);
    EXPECT_EQ(result.monthly.size(), 12u);
}

TEST(SimulationTest, CatalystDoesNotSaturateInOneYear) {
    const Config config = test_config();
    const RunResult result = run(config, 5);
    EXPECT_LT(result.final_state.catalyst_degradation_index, 0.9);
}

TEST(SimulationTest, ParkingTypeScalesDingDamage) {
    const ConfigLoadResult loaded = parse_config(R"toml(
[case]
name = "parking"
start_date = 2026-01-01
horizon_days = 90

[model]
parking_ding_rate_per_day = 1.0
parking_ding_multiplier_street = 1.0
parking_ding_multiplier_carport = 0.5
parking_ding_multiplier_garage = 0.2
stone_chip_rate_per_highway_kilometre = 0.0
uv_damage_per_index_day = 0.0
salt_damage_per_day = 0.0
grit_damage_per_kilometre = 0.0
hail_damage = 0.0

[user]
parking = "street"
)toml",
                                             "parking-street");
    ASSERT_TRUE(loaded.ok());
    Config street = *loaded.config;
    Config garage = street;
    garage.user.parking = Parking::garage;

    const RunResult street_run = run(street, 31);
    const RunResult garage_run = run(garage, 31);
    EXPECT_GT(street_run.final_state.paint_damage_index,
              garage_run.final_state.paint_damage_index);
}

TEST(SimulationTest, AnnualEmissionsInspectionChecksDegradedCatalyst) {
    const ConfigLoadResult loaded = parse_config(R"toml(
[case]
name = "emissions"
start_date = 2026-11-01
horizon_days = 365

[model]
catalyst_degradation_per_kilometre = 0.001
catalyst_emissions_threshold = 0.05
catalyst_oil_poisoning_per_litre = 0.0

[current_state]
catalyst_degradation_index = 0.0

[current_state.uncertainty]
catalyst_degradation_index = 0.0
)toml",
                                             "emissions");
    ASSERT_TRUE(loaded.ok());
    const RunResult result = run(*loaded.config, 5);

    const auto catalyst = std::find_if(
        result.events.begin(), result.events.end(), [](const Event& event) {
            return event.item == "catalyst" &&
                   event.kind == EventKind::wear_replacement;
        });
    ASSERT_NE(catalyst, result.events.end());
    EXPECT_GT(catalyst->day, 300);
    EXPECT_LE(catalyst->day, 365);
}

TEST(SimulationTest, HorizonAndSeedAreEchoed) {
    Config config = test_config();
    config.case_config.horizon_days = 45;
    const RunResult result = run(config, 777);
    EXPECT_EQ(result.horizon_days, 45);
    EXPECT_EQ(result.run_seed, 777u);
    EXPECT_EQ(result.case_name, "unit-test");
}

}  // namespace
}  // namespace carwear
