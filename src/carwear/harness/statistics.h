#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "carwear/config/config.h"
#include "carwear/model/types.h"

namespace carwear {

struct DistributionStats {
    double mean = 0.0;
    double p10 = 0.0;
    double p50 = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double tail_average_95 = 0.0;
};

// Linear interpolation between order statistics, as specified in the
// whitepaper. The input vector is copied and sorted.
DistributionStats summarize(std::vector<double> values);

// Linear-interpolation percentile of an already sorted vector.
double percentile_sorted(const std::vector<double>& sorted, double quantile);

double pearson_correlation(const std::vector<double>& x,
                           const std::vector<double>& y);

// Compact per-run record. It keeps everything the aggregate and the CSV writer
// need, but not the trip table, which is far larger and only requested for
// single-run output.
struct RunRecord {
    int run_index = 0;
    std::uint64_t run_seed = 0;

    double distance_kilometres = 0.0;
    double fuel_litres = 0.0;
    double fuel_consumption_litres_per_100_kilometres = 0.0;

    double engine_wear_equivalent_kilometres = 0.0;
    double oil_degradation = 0.0;
    double oil_level_fraction = 0.0;
    double front_pad_millimetres = 0.0;
    double rear_pad_millimetres = 0.0;
    double front_disc_millimetres = 0.0;
    double rear_disc_millimetres = 0.0;
    std::array<double, 4> tire_tread_millimetres{};
    double battery_state_of_health = 0.0;
    double suspension_wear_index = 0.0;
    double clutch_wear_index = 0.0;
    double exhaust_corrosion_index = 0.0;
    double catalyst_degradation_index = 0.0;
    double cooling_wear_index = 0.0;
    double paint_damage_index = 0.0;
    int windshield_chip_count = 0;
    bool rust_started = false;

    double fuel_cost_euros = 0.0;
    double scheduled_maintenance_euros = 0.0;
    double wear_replacement_euros = 0.0;
    double unplanned_repair_euros = 0.0;
    double consequential_euros = 0.0;
    double paint_repair_euros = 0.0;
    double total_cost_of_ownership_euros = 0.0;
    double depreciation_euros = 0.0;
    int failure_count = 0;
    double downtime_days = 0.0;

    // Resolved inputs, retained for the sensitivity diagnostic.
    double frailty = 1.0;
    double initial_engine_wear_equivalent_kilometres = 0.0;
    double initial_front_pad_millimetres = 0.0;
    double initial_rear_pad_millimetres = 0.0;
    double initial_battery_state_of_health = 0.0;
    double initial_tire_tread_millimetres = 0.0;
    double initial_paint_damage_index = 0.0;

    std::vector<Event> events;
    std::array<double, 12> monthly_cost_by_calendar_month{};
};

RunRecord make_run_record(int run_index, const RunResult& result);

struct PartPrognosis {
    std::string id;
    std::string label;
    std::string unit;
    bool replaceable = true;
    bool failure_modelled = false;
    double replacement_probability = 0.0;
    double failure_probability = 0.0;
    double expected_cost_euros = 0.0;
    DistributionStats state_at_horizon;
};

struct FailureSummary {
    std::string id;
    double probability_at_least_one = 0.0;
    double expected_count = 0.0;
};

struct SensitivityEntry {
    std::string input;
    double correlation = 0.0;
};

struct VarianceDecomposition {
    bool available = false;
    double aleatory_fraction = 0.0;
    double epistemic_fraction = 0.0;
    int samples = 0;
    int runs_per_sample = 1;
};

struct Aggregate {
    int number_of_runs = 0;
    std::uint64_t master_seed = 0;

    DistributionStats distance_kilometres;
    DistributionStats fuel_litres;
    DistributionStats fuel_consumption_litres_per_100_kilometres;
    DistributionStats total_cost_of_ownership_euros;
    DistributionStats fuel_cost_euros;
    DistributionStats scheduled_maintenance_euros;
    DistributionStats wear_replacement_euros;
    DistributionStats unplanned_repair_euros;
    DistributionStats consequential_euros;
    DistributionStats paint_repair_euros;
    DistributionStats depreciation_euros;
    DistributionStats failure_count;
    DistributionStats downtime_days;

    bool budget_defined = false;
    double budget_euros = 0.0;
    double probability_exceeding_budget = 0.0;

    std::array<double, 12> monthly_cost_mean_by_calendar_month{};

    std::vector<PartPrognosis> part_prognoses;
    std::vector<FailureSummary> failures;
    std::vector<SensitivityEntry> sensitivity;
    VarianceDecomposition variance;
};

Aggregate aggregate_records(const Config& config,
                            const std::vector<RunRecord>& records,
                            const VarianceDecomposition& variance = {});

}  // namespace carwear
