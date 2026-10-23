#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "carwear/harness/statistics.h"
#include "carwear/model/simulation.h"
#include "carwear/model/state.h"
#include "test_support.h"

namespace carwear {
namespace {

Config test_config() {
    const ConfigLoadResult loaded = test::load_case();
    return *loaded.config;
}

TEST(StatisticsTest, PercentileInterpolatesLinearly) {
    const std::vector<double> sorted{1.0, 2.0, 3.0, 4.0, 5.0};
    EXPECT_DOUBLE_EQ(percentile_sorted(sorted, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(percentile_sorted(sorted, 0.5), 3.0);
    EXPECT_DOUBLE_EQ(percentile_sorted(sorted, 1.0), 5.0);
    EXPECT_NEAR(percentile_sorted(sorted, 0.25), 2.0, 1e-12);
}

TEST(StatisticsTest, SummarizeReportsMeanAndTail) {
    const DistributionStats stats =
        summarize({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0});
    EXPECT_NEAR(stats.mean, 5.5, 1e-12);
    EXPECT_NEAR(stats.p50, 5.5, 1e-12);
    EXPECT_NEAR(stats.p90, 9.1, 1e-9);
    EXPECT_NEAR(stats.p95, 9.55, 1e-9);
    EXPECT_NEAR(stats.tail_average_95, 10.0, 1e-12);
}

TEST(StatisticsTest, EmptySummaryIsZero) {
    const DistributionStats stats = summarize({});
    EXPECT_EQ(stats.mean, 0.0);
    EXPECT_EQ(stats.p95, 0.0);
}

TEST(StatisticsTest, CorrelationDetectsLinearRelationship) {
    const std::vector<double> x{1, 2, 3, 4, 5};
    const std::vector<double> y{2, 4, 6, 8, 10};
    const std::vector<double> z{10, 8, 6, 4, 2};
    EXPECT_NEAR(pearson_correlation(x, y), 1.0, 1e-12);
    EXPECT_NEAR(pearson_correlation(x, z), -1.0, 1e-12);
    EXPECT_EQ(pearson_correlation(x, {1, 1, 1, 1, 1}), 0.0);
}

TEST(StatisticsTest, RunRecordMirrorsRunResult) {
    const Config config = test_config();
    ResolvedInputs inputs = resolve_inputs(config, 0, false);
    Config short_config = config;
    short_config.case_config.horizon_days = 30;
    const RunResult result = run_model(short_config, inputs, 5);
    const RunRecord record = make_run_record(3, result);

    EXPECT_EQ(record.run_index, 3);
    EXPECT_EQ(record.run_seed, 5u);
    EXPECT_DOUBLE_EQ(record.distance_kilometres,
                     result.totals.distance_kilometres);
    EXPECT_DOUBLE_EQ(record.total_cost_of_ownership_euros,
                     result.totals.total_cost_of_ownership_euros);
    EXPECT_EQ(record.events.size(), result.events.size());
    double monthly_sum = 0.0;
    for (double value : record.monthly_cost_by_calendar_month) {
        monthly_sum += value;
    }
    EXPECT_NEAR(monthly_sum, result.totals.total_cost_of_ownership_euros +
                                 result.totals.paint_repair_euros,
                1e-6);
}

TEST(StatisticsTest, AggregateSummarizesRecords) {
    const Config config = test_config();
    std::vector<RunRecord> records;
    for (int index = 0; index < 2; ++index) {
        RunRecord record;
        record.run_index = index;
        record.run_seed = static_cast<std::uint64_t>(index);
        record.distance_kilometres = 10000.0 + static_cast<double>(index);
        record.fuel_litres = 600.0;
        record.total_cost_of_ownership_euros = 2000.0 + 100.0 * index;
        record.fuel_cost_euros = 1000.0;
        record.failure_count = index;
        records.push_back(record);
    }

    const Aggregate aggregate = aggregate_records(config, records);
    EXPECT_EQ(aggregate.number_of_runs, 2);
    EXPECT_NEAR(aggregate.total_cost_of_ownership_euros.mean, 2050.0, 1e-12);
    EXPECT_NEAR(aggregate.distance_kilometres.p50, 10000.5, 1e-12);
    EXPECT_EQ(aggregate.part_prognoses.size(), 14u);
    EXPECT_EQ(aggregate.failures.size(), 7u);
}

TEST(StatisticsTest, PartPrognosisCountsReplacementsAndFailures) {
    const Config config = test_config();

    RunRecord replaced;
    replaced.run_index = 0;
    replaced.front_pad_millimetres = 3.0;
    Event pad_event;
    pad_event.day = 10;
    pad_event.kind = EventKind::wear_replacement;
    pad_event.item = "front_brake_pads";
    pad_event.cost_euros = 200.0;
    replaced.events.push_back(pad_event);

    RunRecord failed;
    failed.run_index = 1;
    failed.front_pad_millimetres = 7.0;
    Event battery_event;
    battery_event.day = 20;
    battery_event.kind = EventKind::failure;
    battery_event.item = "battery";
    battery_event.cost_euros = 150.0;
    failed.events.push_back(battery_event);

    const Aggregate aggregate =
        aggregate_records(config, {replaced, failed});

    const auto pad = std::find_if(
        aggregate.part_prognoses.begin(), aggregate.part_prognoses.end(),
        [](const PartPrognosis& prognosis) {
            return prognosis.id == "front_brake_pads";
        });
    ASSERT_NE(pad, aggregate.part_prognoses.end());
    EXPECT_NEAR(pad->replacement_probability, 0.5, 1e-12);
    EXPECT_NEAR(pad->expected_cost_euros, 100.0, 1e-12);
    EXPECT_NEAR(pad->state_at_horizon.p50, 5.0, 1e-12);

    const auto battery = std::find_if(
        aggregate.part_prognoses.begin(), aggregate.part_prognoses.end(),
        [](const PartPrognosis& prognosis) {
            return prognosis.id == "battery";
        });
    ASSERT_NE(battery, aggregate.part_prognoses.end());
    EXPECT_NEAR(battery->failure_probability, 0.5, 1e-12);
    EXPECT_NEAR(battery->replacement_probability, 0.5, 1e-12);
    EXPECT_NEAR(battery->expected_cost_euros, 75.0, 1e-12);
}

TEST(StatisticsTest, BudgetProbabilityIsComputed) {
    Config config = test_config();
    config.economics.budget_euros = 2050.0;

    std::vector<RunRecord> records(4);
    records[0].total_cost_of_ownership_euros = 1000.0;
    records[1].total_cost_of_ownership_euros = 2000.0;
    records[2].total_cost_of_ownership_euros = 3000.0;
    records[3].total_cost_of_ownership_euros = 4000.0;

    const Aggregate aggregate = aggregate_records(config, records);
    EXPECT_TRUE(aggregate.budget_defined);
    EXPECT_NEAR(aggregate.probability_exceeding_budget, 0.5, 1e-12);
}

}  // namespace
}  // namespace carwear
