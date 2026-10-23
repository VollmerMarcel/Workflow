#include "carwear/harness/statistics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string_view>

#include "carwear/core/math.h"

namespace carwear {
namespace {

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() &&
           text.substr(0, prefix.size()) == prefix;
}

bool is_tire_event(std::string_view item) {
    return starts_with(item, "tires");
}

bool replacement_event(const std::string& id, const Event& event) {
    if (event.kind == EventKind::failure) {
        return false;
    }
    if (id == "front_brake_pads") {
        return event.item == "front_brake_pads";
    }
    if (id == "rear_brake_pads") {
        return event.item == "rear_brake_pads";
    }
    if (id == "front_brake_discs") {
        return event.item == "front_brake_discs";
    }
    if (id == "rear_brake_discs") {
        return event.item == "rear_brake_discs";
    }
    if (id == "tires") {
        return is_tire_event(event.item);
    }
    if (id == "suspension") {
        return event.item == "suspension";
    }
    if (id == "clutch_or_transmission") {
        return event.item == "clutch_or_transmission";
    }
    if (id == "exhaust") {
        return event.item == "exhaust" ||
               event.item == "exhaust_emissions_failure";
    }
    if (id == "catalyst") {
        return event.item == "catalyst";
    }
    if (id == "cooling_system") {
        return event.item == "cooling_system";
    }
    if (id == "paint") {
        return event.kind == EventKind::paint_repair;
    }
    return false;
}

bool failure_event(const std::string& id, const Event& event) {
    if (event.kind != EventKind::failure) {
        return false;
    }
    if (id == "engine") {
        return starts_with(event.item, "engine_");
    }
    if (id == "battery") {
        return event.item == "battery";
    }
    if (id == "suspension") {
        return event.item == "suspension";
    }
    if (id == "clutch_or_transmission") {
        return event.item == "clutch_or_transmission";
    }
    if (id == "exhaust") {
        return event.item == "exhaust";
    }
    if (id == "cooling_system") {
        return event.item == "cooling_system";
    }
    if (id == "windshield") {
        return event.item == "windshield";
    }
    return false;
}

double part_state(const std::string& id, const RunRecord& record) {
    if (id == "engine") {
        return record.engine_wear_equivalent_kilometres;
    }
    if (id == "front_brake_pads") {
        return record.front_pad_millimetres;
    }
    if (id == "rear_brake_pads") {
        return record.rear_pad_millimetres;
    }
    if (id == "front_brake_discs") {
        return record.front_disc_millimetres;
    }
    if (id == "rear_brake_discs") {
        return record.rear_disc_millimetres;
    }
    if (id == "tires") {
        return *std::min_element(record.tire_tread_millimetres.begin(),
                                 record.tire_tread_millimetres.end());
    }
    if (id == "battery") {
        return record.battery_state_of_health;
    }
    if (id == "suspension") {
        return record.suspension_wear_index;
    }
    if (id == "clutch_or_transmission") {
        return record.clutch_wear_index;
    }
    if (id == "exhaust") {
        return record.exhaust_corrosion_index;
    }
    if (id == "catalyst") {
        return record.catalyst_degradation_index;
    }
    if (id == "cooling_system") {
        return record.cooling_wear_index;
    }
    if (id == "paint") {
        return record.paint_damage_index;
    }
    if (id == "windshield") {
        return static_cast<double>(record.windshield_chip_count);
    }
    return 0.0;
}

struct PartDefinition {
    const char* id;
    const char* label;
    const char* unit;
    bool replaceable;
    bool failure_modelled;
};

const std::array<PartDefinition, 14> kParts{{
    {"engine", "Engine", "equivalent kilometres", false, true},
    {"front_brake_pads", "Front brake pads", "millimetres", true, false},
    {"rear_brake_pads", "Rear brake pads", "millimetres", true, false},
    {"front_brake_discs", "Front brake discs", "millimetres", true, false},
    {"rear_brake_discs", "Rear brake discs", "millimetres", true, false},
    {"tires", "Tires", "millimetres", true, false},
    {"battery", "Battery", "state of health", true, true},
    {"suspension", "Suspension", "index", true, true},
    {"clutch_or_transmission", "Clutch or transmission", "index", true, true},
    {"exhaust", "Exhaust", "index", true, true},
    {"catalyst", "Catalyst", "index", true, false},
    {"cooling_system", "Cooling system", "index", true, true},
    {"paint", "Paint", "index", false, false},
    {"windshield", "Windshield", "chips", true, true},
}};

}  // namespace

double percentile_sorted(const std::vector<double>& sorted, double quantile) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double q = clamp(quantile, 0.0, 1.0);
    const double position = q * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) {
        return sorted[lower];
    }
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

DistributionStats summarize(std::vector<double> values) {
    DistributionStats stats;
    if (values.empty()) {
        return stats;
    }
    std::sort(values.begin(), values.end());
    stats.mean = std::accumulate(values.begin(), values.end(), 0.0) /
                 static_cast<double>(values.size());
    stats.p10 = percentile_sorted(values, 0.10);
    stats.p50 = percentile_sorted(values, 0.50);
    stats.p90 = percentile_sorted(values, 0.90);
    stats.p95 = percentile_sorted(values, 0.95);

    std::size_t tail_count = static_cast<std::size_t>(
        std::ceil(0.05 * static_cast<double>(values.size())));
    tail_count = std::max<std::size_t>(1, tail_count);
    tail_count = std::min(tail_count, values.size());
    const double tail_sum =
        std::accumulate(values.end() - static_cast<std::ptrdiff_t>(tail_count),
                        values.end(), 0.0);
    stats.tail_average_95 = tail_sum / static_cast<double>(tail_count);
    return stats;
}

double pearson_correlation(const std::vector<double>& x,
                           const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) {
        return 0.0;
    }
    const double n = static_cast<double>(x.size());
    const double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    const double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

    double covariance = 0.0;
    double variance_x = 0.0;
    double variance_y = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double dx = x[i] - mean_x;
        const double dy = y[i] - mean_y;
        covariance += dx * dy;
        variance_x += dx * dx;
        variance_y += dy * dy;
    }
    if (!(variance_x > 0.0) || !(variance_y > 0.0)) {
        return 0.0;
    }
    return covariance / std::sqrt(variance_x * variance_y);
}

RunRecord make_run_record(int run_index, const RunResult& result) {
    RunRecord record;
    record.run_index = run_index;
    record.run_seed = result.run_seed;

    record.distance_kilometres = result.totals.distance_kilometres;
    record.fuel_litres = result.totals.fuel_litres;
    record.fuel_consumption_litres_per_100_kilometres =
        safe_divide(result.totals.fuel_litres * 100.0,
                    result.totals.distance_kilometres);

    const SimulationState& state = result.final_state;
    record.engine_wear_equivalent_kilometres =
        state.engine_wear_equivalent_kilometres;
    record.oil_degradation = state.oil_degradation;
    record.oil_level_fraction = state.oil_level_fraction;
    record.front_pad_millimetres = state.front_pad_millimetres;
    record.rear_pad_millimetres = state.rear_pad_millimetres;
    record.front_disc_millimetres = state.front_disc_millimetres;
    record.rear_disc_millimetres = state.rear_disc_millimetres;
    record.tire_tread_millimetres =
        state.tire_sets[static_cast<std::size_t>(state.active_tire_set)]
            .tread_millimetres;
    record.battery_state_of_health = result.terminal_battery_state_of_health;
    record.suspension_wear_index = state.suspension_wear_index;
    record.clutch_wear_index = state.clutch_wear_index;
    record.exhaust_corrosion_index = state.exhaust_corrosion_index;
    record.catalyst_degradation_index = state.catalyst_degradation_index;
    record.cooling_wear_index = state.cooling_wear_index;
    record.paint_damage_index = state.paint_damage_index;
    record.windshield_chip_count = state.windshield_chip_count;
    record.rust_started = state.rust_started;

    record.fuel_cost_euros = result.totals.fuel_cost_euros;
    record.scheduled_maintenance_euros =
        result.totals.scheduled_maintenance_euros;
    record.wear_replacement_euros = result.totals.wear_replacement_euros;
    record.unplanned_repair_euros = result.totals.unplanned_repair_euros;
    record.consequential_euros = result.totals.consequential_euros;
    record.paint_repair_euros = result.totals.paint_repair_euros;
    record.total_cost_of_ownership_euros =
        result.totals.total_cost_of_ownership_euros;
    record.depreciation_euros = result.totals.depreciation_euros;
    record.failure_count = result.totals.failure_count;
    record.downtime_days = result.totals.downtime_days;

    record.frailty = result.frailty;
    record.initial_engine_wear_equivalent_kilometres =
        result.initial_state.engine_wear_equivalent_kilometres;
    record.initial_front_pad_millimetres =
        result.initial_state.front_pad_millimetres;
    record.initial_rear_pad_millimetres =
        result.initial_state.rear_pad_millimetres;
    record.initial_battery_state_of_health =
        result.initial_state.battery_state_of_health;
    record.initial_paint_damage_index = result.initial_state.paint_damage_index;
    double tread_sum = 0.0;
    for (double tread : result.initial_state.tire_tread_millimetres) {
        tread_sum += tread;
    }
    record.initial_tire_tread_millimetres = tread_sum / 4.0;

    record.events = result.events;
    for (const MonthSnapshot& snapshot : result.monthly) {
        if (snapshot.month >= 1 && snapshot.month <= 12) {
            record.monthly_cost_by_calendar_month[
                static_cast<std::size_t>(snapshot.month - 1)] +=
                snapshot.cost_euros;
        }
    }
    return record;
}

Aggregate aggregate_records(const Config& config,
                            const std::vector<RunRecord>& records,
                            const VarianceDecomposition& variance) {
    Aggregate aggregate;
    aggregate.number_of_runs = static_cast<int>(records.size());
    aggregate.master_seed = config.monte_carlo.master_seed;
    aggregate.variance = variance;

    std::vector<double> distance;
    std::vector<double> fuel;
    std::vector<double> consumption;
    std::vector<double> tco;
    std::vector<double> fuel_cost;
    std::vector<double> scheduled;
    std::vector<double> wear;
    std::vector<double> unplanned;
    std::vector<double> consequential;
    std::vector<double> paint_cost;
    std::vector<double> depreciation;
    std::vector<double> failures;
    std::vector<double> downtime;

    distance.reserve(records.size());
    fuel.reserve(records.size());
    consumption.reserve(records.size());
    tco.reserve(records.size());
    fuel_cost.reserve(records.size());
    scheduled.reserve(records.size());
    wear.reserve(records.size());
    unplanned.reserve(records.size());
    consequential.reserve(records.size());
    paint_cost.reserve(records.size());
    depreciation.reserve(records.size());
    failures.reserve(records.size());
    downtime.reserve(records.size());

    for (const RunRecord& record : records) {
        distance.push_back(record.distance_kilometres);
        fuel.push_back(record.fuel_litres);
        consumption.push_back(record.fuel_consumption_litres_per_100_kilometres);
        tco.push_back(record.total_cost_of_ownership_euros);
        fuel_cost.push_back(record.fuel_cost_euros);
        scheduled.push_back(record.scheduled_maintenance_euros);
        wear.push_back(record.wear_replacement_euros);
        unplanned.push_back(record.unplanned_repair_euros);
        consequential.push_back(record.consequential_euros);
        paint_cost.push_back(record.paint_repair_euros);
        depreciation.push_back(record.depreciation_euros);
        failures.push_back(static_cast<double>(record.failure_count));
        downtime.push_back(record.downtime_days);
        for (std::size_t month = 0; month < 12; ++month) {
            aggregate.monthly_cost_mean_by_calendar_month[month] +=
                record.monthly_cost_by_calendar_month[month];
        }
    }

    if (!records.empty()) {
        const double n = static_cast<double>(records.size());
        for (double& month_sum : aggregate.monthly_cost_mean_by_calendar_month) {
            month_sum /= n;
        }
    }

    aggregate.distance_kilometres = summarize(distance);
    aggregate.fuel_litres = summarize(fuel);
    aggregate.fuel_consumption_litres_per_100_kilometres =
        summarize(consumption);
    aggregate.total_cost_of_ownership_euros = summarize(tco);
    aggregate.fuel_cost_euros = summarize(fuel_cost);
    aggregate.scheduled_maintenance_euros = summarize(scheduled);
    aggregate.wear_replacement_euros = summarize(wear);
    aggregate.unplanned_repair_euros = summarize(unplanned);
    aggregate.consequential_euros = summarize(consequential);
    aggregate.paint_repair_euros = summarize(paint_cost);
    aggregate.depreciation_euros = summarize(depreciation);
    aggregate.failure_count = summarize(failures);
    aggregate.downtime_days = summarize(downtime);

    if (config.economics.budget_euros.has_value()) {
        aggregate.budget_defined = true;
        aggregate.budget_euros = *config.economics.budget_euros;
        if (!records.empty()) {
            std::size_t exceeding = 0;
            for (double value : tco) {
                if (value > aggregate.budget_euros) {
                    ++exceeding;
                }
            }
            aggregate.probability_exceeding_budget =
                static_cast<double>(exceeding) / static_cast<double>(tco.size());
        }
    }

    for (const PartDefinition& part : kParts) {
        PartPrognosis prognosis;
        prognosis.id = part.id;
        prognosis.label = part.label;
        prognosis.unit = part.unit;
        prognosis.replaceable = part.replaceable;
        prognosis.failure_modelled = part.failure_modelled;

        std::vector<double> states;
        states.reserve(records.size());
        std::size_t replacement_runs = 0;
        std::size_t failure_runs = 0;
        double expected_cost = 0.0;

        for (const RunRecord& record : records) {
            states.push_back(part_state(prognosis.id, record));
            bool replaced = false;
            bool failed = false;
            for (const Event& event : record.events) {
                const bool is_replacement =
                    replacement_event(prognosis.id, event);
                const bool is_failure = failure_event(prognosis.id, event);
                if (is_replacement || is_failure) {
                    expected_cost += event.cost_euros;
                }
                replaced = replaced || is_replacement;
                failed = failed || is_failure;
            }
            if (replaced) {
                ++replacement_runs;
            }
            if (failed) {
                ++failure_runs;
            }
        }

        const double n = records.empty() ? 1.0 : static_cast<double>(records.size());
        prognosis.state_at_horizon = summarize(states);
        prognosis.replacement_probability =
            static_cast<double>(replacement_runs) / n;
        prognosis.failure_probability = static_cast<double>(failure_runs) / n;
        // A failed battery or windshield is replaced on the spot, so for those
        // two parts the replacement event is the failure event.
        if (prognosis.id == "battery" || prognosis.id == "windshield") {
            prognosis.replacement_probability = prognosis.failure_probability;
        }
        prognosis.expected_cost_euros = expected_cost / n;
        aggregate.part_prognoses.push_back(std::move(prognosis));
    }

    const std::array<std::string_view, 7> failure_ids{
        "engine",          "battery",         "suspension", "clutch_or_transmission",
        "exhaust",         "cooling_system",  "windshield"};
    for (std::string_view id : failure_ids) {
        FailureSummary summary;
        summary.id = std::string{id};
        std::size_t runs_with_failure = 0;
        std::size_t total_count = 0;
        for (const RunRecord& record : records) {
            bool failed = false;
            for (const Event& event : record.events) {
                if (event.kind == EventKind::failure &&
                    failure_event(summary.id, event)) {
                    failed = true;
                    ++total_count;
                }
            }
            if (failed) {
                ++runs_with_failure;
            }
        }
        const double n = records.empty() ? 1.0 : static_cast<double>(records.size());
        summary.probability_at_least_one =
            static_cast<double>(runs_with_failure) / n;
        summary.expected_count = static_cast<double>(total_count) / n;
        aggregate.failures.push_back(std::move(summary));
    }

    // A diagnostic ranking: the sampled inputs whose correlation with total
    // cost of ownership is largest in absolute value.
    const std::vector<std::pair<std::string_view, std::vector<double>>> inputs{
        {"frailty", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.frailty); return v; }()},
        {"initial_engine_wear", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_engine_wear_equivalent_kilometres); return v; }()},
        {"initial_front_pad_millimetres", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_front_pad_millimetres); return v; }()},
        {"initial_rear_pad_millimetres", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_rear_pad_millimetres); return v; }()},
        {"initial_battery_state_of_health", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_battery_state_of_health); return v; }()},
        {"initial_tire_tread_millimetres", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_tire_tread_millimetres); return v; }()},
        {"initial_paint_damage_index", [&] { std::vector<double> v; for (const auto& r : records) v.push_back(r.initial_paint_damage_index); return v; }()},
    };

    for (const auto& [name, values] : inputs) {
        const double correlation = pearson_correlation(values, tco);
        if (correlation != 0.0) {
            aggregate.sensitivity.push_back({std::string{name}, correlation});
        }
    }
    std::sort(aggregate.sensitivity.begin(), aggregate.sensitivity.end(),
              [](const SensitivityEntry& lhs, const SensitivityEntry& rhs) {
                  return std::abs(lhs.correlation) > std::abs(rhs.correlation);
              });

    return aggregate;
}

}  // namespace carwear
