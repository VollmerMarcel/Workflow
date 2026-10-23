#include "carwear/output/aggregate_writer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "carwear/output/csv.h"

namespace carwear {
namespace {

toml::table distribution_table(const DistributionStats& stats,
                               bool include_tail) {
    toml::table table;
    table.insert_or_assign("mean", stats.mean);
    table.insert_or_assign("p10", stats.p10);
    table.insert_or_assign("p50", stats.p50);
    table.insert_or_assign("p90", stats.p90);
    if (include_tail) {
        table.insert_or_assign("p95", stats.p95);
        table.insert_or_assign("tail_average_95", stats.tail_average_95);
    }
    return table;
}

void add_distribution(toml::table& parent, std::string_view key,
                      const DistributionStats& stats, bool include_tail) {
    parent.insert_or_assign(key, distribution_table(stats, include_tail));
}

bool write_text_file(const std::filesystem::path& path, const std::string& text,
                     std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "cannot open " + path.string() + " for writing";
        return false;
    }
    stream << text;
    if (!stream) {
        error = "failed while writing " + path.string();
        return false;
    }
    return true;
}

const char* const kMonthNames[12] = {
    "january", "february", "march",     "april",   "may",      "june",
    "july",    "august",   "september", "october", "november", "december"};

}  // namespace

toml::table aggregate_to_toml(const Aggregate& aggregate) {
    toml::table root;
    root.insert_or_assign("schema_version", "1.0");

    toml::table table;
    table.insert_or_assign("number_of_runs", aggregate.number_of_runs);
    table.insert_or_assign("master_seed",
                           static_cast<std::int64_t>(aggregate.master_seed));

    add_distribution(table, "distance_kilometres",
                     aggregate.distance_kilometres, false);
    add_distribution(table, "fuel_litres", aggregate.fuel_litres, false);
    add_distribution(table, "fuel_consumption_litres_per_100_kilometres",
                     aggregate.fuel_consumption_litres_per_100_kilometres, false);
    add_distribution(table, "total_cost_of_ownership_euros",
                     aggregate.total_cost_of_ownership_euros, true);
    add_distribution(table, "fuel_euros", aggregate.fuel_cost_euros, true);
    add_distribution(table, "scheduled_maintenance_euros",
                     aggregate.scheduled_maintenance_euros, true);
    add_distribution(table, "wear_replacement_euros",
                     aggregate.wear_replacement_euros, true);
    add_distribution(table, "unplanned_repair_euros",
                     aggregate.unplanned_repair_euros, true);
    add_distribution(table, "consequential_euros",
                     aggregate.consequential_euros, true);
    add_distribution(table, "paint_repair_euros", aggregate.paint_repair_euros,
                     true);
    add_distribution(table, "depreciation_euros", aggregate.depreciation_euros,
                     true);
    add_distribution(table, "failure_count", aggregate.failure_count, false);
    add_distribution(table, "downtime_days", aggregate.downtime_days, false);

    toml::table monthly;
    for (std::size_t month = 0; month < 12; ++month) {
        monthly.insert_or_assign(
            kMonthNames[month],
            aggregate.monthly_cost_mean_by_calendar_month[month]);
    }
    table.insert_or_assign("monthly_cost_mean_euros", std::move(monthly));

    toml::table budget;
    budget.insert_or_assign("defined", aggregate.budget_defined);
    if (aggregate.budget_defined) {
        budget.insert_or_assign("euros", aggregate.budget_euros);
    }
    budget.insert_or_assign("probability_exceeding",
                            aggregate.probability_exceeding_budget);
    table.insert_or_assign("budget", std::move(budget));

    toml::table variance;
    variance.insert_or_assign("available", aggregate.variance.available);
    variance.insert_or_assign("aleatory_fraction",
                              aggregate.variance.aleatory_fraction);
    variance.insert_or_assign("epistemic_fraction",
                              aggregate.variance.epistemic_fraction);
    variance.insert_or_assign("samples", aggregate.variance.samples);
    variance.insert_or_assign("runs_per_sample",
                              aggregate.variance.runs_per_sample);
    table.insert_or_assign("variance_decomposition", std::move(variance));

    toml::array prognoses;
    prognoses.reserve(aggregate.part_prognoses.size());
    for (const PartPrognosis& prognosis : aggregate.part_prognoses) {
        toml::table entry;
        entry.insert_or_assign("id", prognosis.id);
        entry.insert_or_assign("label", prognosis.label);
        entry.insert_or_assign("unit", prognosis.unit);
        entry.insert_or_assign("replaceable", prognosis.replaceable);
        entry.insert_or_assign("failure_modelled", prognosis.failure_modelled);
        entry.insert_or_assign("replacement_probability",
                               prognosis.replacement_probability);
        entry.insert_or_assign("failure_probability",
                               prognosis.failure_probability);
        entry.insert_or_assign("expected_cost_euros",
                               prognosis.expected_cost_euros);
        toml::table state;
        state.insert_or_assign("mean", prognosis.state_at_horizon.mean);
        state.insert_or_assign("p10", prognosis.state_at_horizon.p10);
        state.insert_or_assign("p50", prognosis.state_at_horizon.p50);
        state.insert_or_assign("p90", prognosis.state_at_horizon.p90);
        entry.insert_or_assign("state_at_horizon", std::move(state));
        prognoses.push_back(std::move(entry));
    }
    table.insert_or_assign("part_prognoses", std::move(prognoses));

    toml::array failures;
    failures.reserve(aggregate.failures.size());
    for (const FailureSummary& failure : aggregate.failures) {
        toml::table entry;
        entry.insert_or_assign("id", failure.id);
        entry.insert_or_assign("probability_at_least_one",
                               failure.probability_at_least_one);
        entry.insert_or_assign("expected_count", failure.expected_count);
        failures.push_back(std::move(entry));
    }
    table.insert_or_assign("failures", std::move(failures));

    toml::array sensitivity;
    sensitivity.reserve(aggregate.sensitivity.size());
    for (const SensitivityEntry& entry_value : aggregate.sensitivity) {
        toml::table entry;
        entry.insert_or_assign("input", entry_value.input);
        entry.insert_or_assign("correlation", entry_value.correlation);
        sensitivity.push_back(std::move(entry));
    }
    table.insert_or_assign("sensitivity", std::move(sensitivity));

    root.insert_or_assign("aggregate", std::move(table));
    return root;
}

std::string aggregate_to_toml_string(const Aggregate& aggregate) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << aggregate_to_toml(aggregate);
    return stream.str();
}

bool write_aggregate_outputs(const std::filesystem::path& directory,
                             const Aggregate& aggregate,
                             const std::vector<RunRecord>& records,
                             bool write_runs_csv, std::string& error) {
    std::error_code code;
    std::filesystem::create_directories(directory, code);
    if (code) {
        error = "cannot create output directory " + directory.string() + ": " +
                code.message();
        return false;
    }

    if (!write_text_file(directory / "aggregate.toml",
                         aggregate_to_toml_string(aggregate), error)) {
        return false;
    }

    if (!write_runs_csv) {
        return true;
    }

    std::vector<std::vector<std::string>> rows;
    rows.reserve(records.size());
    for (const RunRecord& record : records) {
        rows.push_back(
            {std::to_string(record.run_index),
             std::to_string(record.run_seed),
             format_number(record.distance_kilometres),
             format_number(record.fuel_litres),
             format_number(
                 record.fuel_consumption_litres_per_100_kilometres),
             format_number(record.engine_wear_equivalent_kilometres),
             format_number(record.oil_degradation),
             format_number(record.oil_level_fraction),
             format_number(record.front_pad_millimetres),
             format_number(record.rear_pad_millimetres),
             format_number(record.front_disc_millimetres),
             format_number(record.rear_disc_millimetres),
             format_number(record.tire_tread_millimetres[0]),
             format_number(record.tire_tread_millimetres[1]),
             format_number(record.tire_tread_millimetres[2]),
             format_number(record.tire_tread_millimetres[3]),
             format_number(record.battery_state_of_health),
             format_number(record.suspension_wear_index),
             format_number(record.clutch_wear_index),
             format_number(record.exhaust_corrosion_index),
             format_number(record.catalyst_degradation_index),
             format_number(record.paint_damage_index),
             std::to_string(record.windshield_chip_count),
             record.rust_started ? "true" : "false",
             format_number(record.fuel_cost_euros),
             format_number(record.scheduled_maintenance_euros),
             format_number(record.wear_replacement_euros),
             format_number(record.unplanned_repair_euros),
             format_number(record.consequential_euros),
             format_number(record.total_cost_of_ownership_euros),
             std::to_string(record.failure_count),
             format_number(record.downtime_days),
             format_number(record.depreciation_euros)});
    }

    std::ostringstream csv;
    csv.imbue(std::locale::classic());
    write_csv(
        csv,
        {"run_index",
         "run_seed",
         "distance_kilometres",
         "fuel_litres",
         "fuel_consumption_litres_per_100_kilometres",
         "engine_wear_equivalent_kilometres",
         "oil_degradation",
         "oil_level_fraction",
         "front_pad_millimetres",
         "rear_pad_millimetres",
         "front_disc_millimetres",
         "rear_disc_millimetres",
         "tire_tread_front_left_millimetres",
         "tire_tread_front_right_millimetres",
         "tire_tread_rear_left_millimetres",
         "tire_tread_rear_right_millimetres",
         "battery_state_of_health",
         "suspension_wear_index",
         "clutch_wear_index",
         "exhaust_corrosion_index",
         "catalyst_degradation_index",
         "paint_damage_index",
         "windshield_chip_count",
         "rust_started",
         "fuel_cost_euros",
         "scheduled_maintenance_euros",
         "wear_replacement_euros",
         "unplanned_repair_euros",
         "consequential_euros",
         "total_cost_of_ownership_euros",
         "failure_count",
         "downtime_days",
         "depreciation_euros"},
        rows);

    if (!write_text_file(directory / "runs.csv", csv.str(), error)) {
        return false;
    }
    return true;
}

}  // namespace carwear
