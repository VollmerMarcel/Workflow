#include "carwear/output/run_writer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "carwear/core/date.h"
#include "carwear/output/csv.h"

namespace carwear {
namespace {

toml::date to_toml_date(const Date& date) {
    return toml::date{static_cast<int>(date.year()),
                      static_cast<unsigned>(date.month()),
                      static_cast<unsigned>(date.day())};
}

toml::array tread_array(const std::array<double, 4>& values) {
    toml::array array;
    for (double value : values) {
        array.push_back(value);
    }
    return array;
}

toml::table event_table(const Event& event) {
    toml::table table;
    table.insert_or_assign("day", event.day);
    table.insert_or_assign("kind", std::string{to_string(event.kind)});
    table.insert_or_assign("item", event.item);
    table.insert_or_assign("cost_euros", event.cost_euros);
    return table;
}

toml::table month_table(const MonthSnapshot& snapshot) {
    toml::table table;
    table.insert_or_assign("month", snapshot.month);
    table.insert_or_assign("start_day", snapshot.start_day);
    table.insert_or_assign("end_day", snapshot.end_day);
    table.insert_or_assign("cost_euros", snapshot.cost_euros);
    table.insert_or_assign("cumulative_cost_euros",
                           snapshot.cumulative_cost_euros);
    table.insert_or_assign("fuel_litres", snapshot.fuel_litres);
    table.insert_or_assign("failures", snapshot.failures);
    table.insert_or_assign("front_pad_millimetres",
                           snapshot.front_pad_millimetres);
    table.insert_or_assign("rear_pad_millimetres",
                           snapshot.rear_pad_millimetres);
    table.insert_or_assign("front_disc_millimetres",
                           snapshot.front_disc_millimetres);
    table.insert_or_assign("rear_disc_millimetres",
                           snapshot.rear_disc_millimetres);
    table.insert_or_assign("tire_tread_millimetres",
                           tread_array(snapshot.tire_tread_millimetres));
    table.insert_or_assign("battery_state_of_health",
                           snapshot.battery_state_of_health);
    table.insert_or_assign("exhaust_corrosion_index",
                           snapshot.exhaust_corrosion_index);
    table.insert_or_assign("paint_damage_index", snapshot.paint_damage_index);
    table.insert_or_assign("odometer_kilometres",
                           snapshot.odometer_kilometres);
    return table;
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

}  // namespace

toml::table run_to_toml(const RunResult& result) {
    toml::table root;
    root.insert_or_assign("schema_version", "1.0");

    toml::table run;
    run.insert_or_assign("case_name", result.case_name);
    run.insert_or_assign("start_date", to_toml_date(result.start_date));
    run.insert_or_assign("horizon_days", result.horizon_days);
    run.insert_or_assign("run_seed",
                         static_cast<std::int64_t>(result.run_seed));
    run.insert_or_assign("frailty", result.frailty);
    run.insert_or_assign(
        "paint_repair_included_in_total_cost_of_ownership",
        result.include_paint_repair_in_total_cost_of_ownership);

    const RunTotals& totals = result.totals;
    toml::table totals_table;
    totals_table.insert_or_assign("distance_kilometres",
                                  totals.distance_kilometres);
    totals_table.insert_or_assign("fuel_litres", totals.fuel_litres);
    totals_table.insert_or_assign(
        "fuel_consumption_litres_per_100_kilometres",
        totals.distance_kilometres > 0.0
            ? totals.fuel_litres * 100.0 / totals.distance_kilometres
            : 0.0);
    totals_table.insert_or_assign("total_cost_of_ownership_euros",
                                  totals.total_cost_of_ownership_euros);
    totals_table.insert_or_assign("depreciation_euros",
                                  totals.depreciation_euros);
    totals_table.insert_or_assign("failure_count", totals.failure_count);
    totals_table.insert_or_assign("downtime_days", totals.downtime_days);
    run.insert_or_assign("totals", std::move(totals_table));

    toml::table breakdown;
    breakdown.insert_or_assign("fuel_euros", totals.fuel_cost_euros);
    breakdown.insert_or_assign("scheduled_maintenance_euros",
                               totals.scheduled_maintenance_euros);
    breakdown.insert_or_assign("wear_replacement_euros",
                               totals.wear_replacement_euros);
    breakdown.insert_or_assign("unplanned_repair_euros",
                               totals.unplanned_repair_euros);
    breakdown.insert_or_assign("consequential_euros",
                               totals.consequential_euros);
    breakdown.insert_or_assign("paint_repair_euros",
                               totals.paint_repair_euros);
    run.insert_or_assign("cost_breakdown", std::move(breakdown));

    const SimulationState& state = result.final_state;
    toml::table final_state;
    final_state.insert_or_assign("engine_wear_equivalent_kilometres",
                                 state.engine_wear_equivalent_kilometres);
    final_state.insert_or_assign("oil_degradation", state.oil_degradation);
    final_state.insert_or_assign("oil_level_fraction",
                                 state.oil_level_fraction);
    final_state.insert_or_assign("front_pad_millimetres",
                                 state.front_pad_millimetres);
    final_state.insert_or_assign("rear_pad_millimetres",
                                 state.rear_pad_millimetres);
    final_state.insert_or_assign("front_disc_millimetres",
                                 state.front_disc_millimetres);
    final_state.insert_or_assign("rear_disc_millimetres",
                                 state.rear_disc_millimetres);
    final_state.insert_or_assign(
        "tire_tread_millimetres",
        tread_array(state.tire_sets[static_cast<std::size_t>(
                                        state.active_tire_set)]
                        .tread_millimetres));
    final_state.insert_or_assign("tire_pressure_bar",
                                 tread_array(state.tire_pressure_bar));
    final_state.insert_or_assign("battery_state_of_health",
                                 result.terminal_battery_state_of_health);
    final_state.insert_or_assign("suspension_wear_index",
                                 state.suspension_wear_index);
    final_state.insert_or_assign("clutch_wear_index",
                                 state.clutch_wear_index);
    final_state.insert_or_assign("exhaust_corrosion_index",
                                 state.exhaust_corrosion_index);
    final_state.insert_or_assign("catalyst_degradation_index",
                                 state.catalyst_degradation_index);
    final_state.insert_or_assign("cooling_wear_index",
                                 state.cooling_wear_index);
    final_state.insert_or_assign("paint_damage_index",
                                 state.paint_damage_index);
    final_state.insert_or_assign("windshield_chip_count",
                                 state.windshield_chip_count);
    final_state.insert_or_assign("rust_started", state.rust_started);
    final_state.insert_or_assign("odometer_kilometres",
                                 state.odometer_kilometres);
    run.insert_or_assign("final_state", std::move(final_state));

    toml::array events;
    events.reserve(result.events.size());
    for (const Event& event : result.events) {
        events.push_back(event_table(event));
    }
    if (!events.empty()) {
        run.insert_or_assign("events", std::move(events));
    }

    toml::array monthly;
    monthly.reserve(result.monthly.size());
    for (const MonthSnapshot& snapshot : result.monthly) {
        monthly.push_back(month_table(snapshot));
    }
    if (!monthly.empty()) {
        run.insert_or_assign("monthly", std::move(monthly));
    }

    root.insert_or_assign("run", std::move(run));
    return root;
}

std::string run_to_toml_string(const RunResult& result) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << run_to_toml(result);
    return stream.str();
}

bool write_run_outputs(const Config& config,
                       const std::filesystem::path& directory,
                       const RunResult& result, std::string& error) {
    std::error_code code;
    std::filesystem::create_directories(directory, code);
    if (code) {
        error = "cannot create output directory " + directory.string() + ": " +
                code.message();
        return false;
    }

    if (!write_text_file(directory / "run.toml", run_to_toml_string(result),
                         error)) {
        return false;
    }

    if (config.output.write_run_tables) {
        {
            std::vector<std::vector<std::string>> rows;
            rows.reserve(result.events.size());
            for (const Event& event : result.events) {
                rows.push_back({std::to_string(event.day),
                                std::string{to_string(event.kind)}, event.item,
                                format_number(event.cost_euros)});
            }
            std::ostringstream csv;
            csv.imbue(std::locale::classic());
            write_csv(csv, {"day", "kind", "item", "cost_euros"}, rows);
            if (!write_text_file(directory / "run_events.csv", csv.str(),
                                 error)) {
                return false;
            }
        }
        {
            std::vector<std::vector<std::string>> rows;
            rows.reserve(result.monthly.size());
            for (const MonthSnapshot& snapshot : result.monthly) {
                rows.push_back(
                    {std::to_string(snapshot.month),
                     std::to_string(snapshot.start_day),
                     std::to_string(snapshot.end_day),
                     format_number(snapshot.cost_euros),
                     format_number(snapshot.cumulative_cost_euros),
                     format_number(snapshot.fuel_litres),
                     std::to_string(snapshot.failures),
                     format_number(snapshot.front_pad_millimetres),
                     format_number(snapshot.rear_pad_millimetres),
                     format_number(snapshot.front_disc_millimetres),
                     format_number(snapshot.rear_disc_millimetres),
                     format_number(snapshot.tire_tread_millimetres[0]),
                     format_number(snapshot.tire_tread_millimetres[1]),
                     format_number(snapshot.tire_tread_millimetres[2]),
                     format_number(snapshot.tire_tread_millimetres[3]),
                     format_number(snapshot.battery_state_of_health),
                     format_number(snapshot.exhaust_corrosion_index),
                     format_number(snapshot.paint_damage_index),
                     format_number(snapshot.odometer_kilometres)});
            }
            std::ostringstream csv;
            csv.imbue(std::locale::classic());
            write_csv(csv,
                      {"month",
                       "start_day",
                       "end_day",
                       "cost_euros",
                       "cumulative_cost_euros",
                       "fuel_litres",
                       "failures",
                       "front_pad_millimetres",
                       "rear_pad_millimetres",
                       "front_disc_millimetres",
                       "rear_disc_millimetres",
                       "tire_tread_front_left_millimetres",
                       "tire_tread_front_right_millimetres",
                       "tire_tread_rear_left_millimetres",
                       "tire_tread_rear_right_millimetres",
                       "battery_state_of_health",
                       "exhaust_corrosion_index",
                       "paint_damage_index",
                       "odometer_kilometres"},
                      rows);
            if (!write_text_file(directory / "run_monthly.csv", csv.str(),
                                 error)) {
                return false;
            }
        }
    }

    if (config.output.write_trips) {
        std::vector<std::vector<std::string>> rows;
        rows.reserve(result.trips.size());
        for (const TripRecord& trip : result.trips) {
            rows.push_back({std::to_string(trip.day),
                            std::string{to_string(trip.purpose)},
                            std::string{to_string(trip.route_class)},
                            format_number(trip.distance_kilometres),
                            format_number(trip.mean_speed_kph),
                            trip.cold_start ? "true" : "false",
                            format_number(trip.fuel_litres),
                            format_number(trip.ambient_temperature_celsius),
                            trip.road_wet ? "true" : "false",
                            trip.snowy ? "true" : "false"});
        }
        std::ostringstream csv;
        csv.imbue(std::locale::classic());
        write_csv(csv,
                  {"day", "purpose", "route_class", "distance_kilometres",
                   "mean_speed_kph", "cold_start", "fuel_litres",
                   "ambient_temperature_celsius", "road_wet", "snowy"},
                  rows);
        if (!write_text_file(directory / "run_trips.csv", csv.str(), error)) {
            return false;
        }
    }

    return true;
}

}  // namespace carwear
