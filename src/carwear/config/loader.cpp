#include "carwear/config/loader.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string_view>

#include "carwear/core/math.h"

namespace carwear {
namespace {

using Monthly = std::array<double, 12>;

class SectionReader {
public:
    SectionReader(const toml::table& table, std::string section, Diagnostics& diagnostics)
        : table_(table), section_(std::move(section)), diagnostics_(diagnostics) {}

    bool has(std::string_view key) const { return table_[key].node() != nullptr; }

    toml::node_view<const toml::node> view(std::string_view key) const { return table_[key]; }

    void get(std::string_view key, double& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        if (auto value = node.value<double>()) {
            target = *value;
        } else if (auto integer = node.value<std::int64_t>()) {
            target = static_cast<double>(*integer);
        } else {
            type_error(key, "a number");
        }
    }

    void get(std::string_view key, std::int64_t& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        if (auto value = node.value<std::int64_t>()) {
            target = *value;
        } else if (auto value = node.value<double>()) {
            if (std::floor(*value) == *value &&
                *value >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
                *value <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                target = static_cast<std::int64_t>(*value);
            } else {
                type_error(key, "an integer");
            }
        } else {
            type_error(key, "an integer");
        }
    }

    void get(std::string_view key, int& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        if (auto value = node.value<std::int64_t>()) {
            if (*value < std::numeric_limits<int>::min() ||
                *value > std::numeric_limits<int>::max()) {
                error(key, "is outside the supported integer range");
            } else {
                target = static_cast<int>(*value);
            }
        } else if (auto value = node.value<double>()) {
            if (std::floor(*value) == *value && *value >= std::numeric_limits<int>::min() &&
                *value <= std::numeric_limits<int>::max()) {
                target = static_cast<int>(*value);
            } else {
                type_error(key, "an integer");
            }
        } else {
            type_error(key, "an integer");
        }
    }

    void get(std::string_view key, bool& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        if (auto value = node.value<bool>()) {
            target = *value;
        } else {
            type_error(key, "a boolean");
        }
    }

    void get(std::string_view key, std::string& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        if (auto value = node.value<std::string>()) {
            target = std::move(*value);
        } else {
            type_error(key, "a string");
        }
    }

    bool get_date(std::string_view key, Date& target) {
        auto node = table_[key];
        if (!node) {
            return false;
        }
        mark(key);
        if (auto value = node.value<toml::date>()) {
            const Date candidate{
                std::chrono::year{value->year},
                std::chrono::month{static_cast<unsigned>(value->month)},
                std::chrono::day{static_cast<unsigned>(value->day)}};
            if (!candidate.ok()) {
                error(key, "is not a valid calendar date");
            } else {
                target = candidate;
            }
        } else {
            type_error(key, "a TOML date (YYYY-MM-DD)");
        }
        return true;
    }

    void get_monthly(std::string_view key, Monthly& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        auto* array = node.as_array();
        if (array == nullptr) {
            type_error(key, "an array of 12 numbers");
            return;
        }
        if (array->size() != 12) {
            error(key, std::format("must contain exactly 12 values, found {}", array->size()));
            return;
        }
        bool ok = true;
        for (std::size_t i = 0; i < 12; ++i) {
            if (!read_number((*array)[i], target[i])) {
                error(key, std::format("entry {} is not a number", i + 1));
                ok = false;
            }
        }
        if (!ok) {
            return;
        }
    }

    void get_array4(std::string_view key, std::array<double, 4>& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        auto* array = node.as_array();
        if (array == nullptr) {
            type_error(key, "an array of 4 numbers");
            return;
        }
        if (array->size() != 4) {
            error(key, std::format("must contain exactly 4 values, found {}", array->size()));
            return;
        }
        for (std::size_t i = 0; i < 4; ++i) {
            if (!read_number((*array)[i], target[i])) {
                error(key, std::format("entry {} is not a number", i + 1));
            }
        }
    }

    void get_route(std::string_view key, RouteClassConfig& target) {
        auto node = table_[key];
        if (!node) {
            return;
        }
        mark(key);
        auto* table = node.as_table();
        if (table == nullptr) {
            type_error(key, "a table");
            return;
        }
        SectionReader reader(*table, section_ + "." + std::string(key), diagnostics_);
        reader.get("mean_speed_kph", target.mean_speed_kph);
        reader.get("stop_density_per_kilometre", target.stop_density_per_kilometre);
        reader.get("lateral_acceleration_metres_per_second_squared",
                   target.lateral_acceleration_metres_per_second_squared);
        reader.get("idle_fraction", target.idle_fraction);
        reader.get("brake_onset_speed_fraction", target.brake_onset_speed_fraction);
        reader.get("positive_acceleration_metres_per_second_squared",
                   target.positive_acceleration_metres_per_second_squared);
        reader.finish();
    }

    void get_transmission(std::string_view key, Transmission& target) {
        std::string text;
        auto node = table_[key];
        if (!node) {
            return;
        }
        get(key, text);
        if (text == "manual") {
            target = Transmission::manual;
        } else if (text == "automatic") {
            target = Transmission::automatic;
        } else {
            error(key, std::format("must be \"manual\" or \"automatic\", found \"{}\"", text));
        }
    }

    void get_drivetrain(std::string_view key, Drivetrain& target) {
        std::string text;
        auto node = table_[key];
        if (!node) {
            return;
        }
        get(key, text);
        if (text == "front") {
            target = Drivetrain::front;
        } else if (text == "rear") {
            target = Drivetrain::rear;
        } else if (text == "all") {
            target = Drivetrain::all;
        } else {
            error(key, std::format("must be \"front\", \"rear\" or \"all\", found \"{}\"", text));
        }
    }

    void get_parking(std::string_view key, Parking& target) {
        std::string text;
        auto node = table_[key];
        if (!node) {
            return;
        }
        get(key, text);
        if (text == "street") {
            target = Parking::street;
        } else if (text == "carport") {
            target = Parking::carport;
        } else if (text == "garage") {
            target = Parking::garage;
        } else {
            error(key, std::format("must be \"street\", \"carport\" or \"garage\", found \"{}\"",
                                   text));
        }
    }

    void finish(std::initializer_list<std::string_view> allowed_extra = {}) {
        std::set<std::string, std::less<>> allowed;
        for (auto key : allowed_extra) {
            allowed.emplace(key);
        }
        for (const auto& [key, node] : table_) {
            (void)node;
            const std::string name{key.str()};
            if (seen_.contains(name) || allowed.contains(name)) {
                continue;
            }
            diagnostics_.warn(std::format("unknown key '{}.{}' ignored", section_, name));
        }
    }

    void error(std::string_view key, std::string_view message) {
        diagnostics_.error(std::format("{}.{} {}", section_, key, message));
    }

private:
    static bool read_number(const toml::node& node, double& out) {
        if (auto value = node.value<double>()) {
            out = *value;
            return true;
        }
        if (auto value = node.value<std::int64_t>()) {
            out = static_cast<double>(*value);
            return true;
        }
        return false;
    }

    void mark(std::string_view key) { seen_.emplace(key); }

    void type_error(std::string_view key, std::string_view expected) {
        error(key, std::format("must be {}", expected));
    }

    const toml::table& table_;
    std::string section_;
    Diagnostics& diagnostics_;
    std::set<std::string, std::less<>> seen_;
};

const toml::table* sub_table(const toml::table& root, std::string_view name,
                             Diagnostics& diagnostics, const std::string& source) {
    auto node = root[name];
    if (!node) {
        return nullptr;
    }
    auto* table = node.as_table();
    if (table == nullptr) {
        diagnostics.error(std::format("{}: section [{}] must be a table", source, name));
        return nullptr;
    }
    return table;
}

Monthly preset_temperature_maritime() {
    return {2.0, 3.0, 6.0, 10.0, 14.0, 17.0, 19.0, 19.0, 16.0, 11.0, 6.0, 3.0};
}

void load_case(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "case", diagnostics, config.source_path);
    if (table == nullptr) {
        diagnostics.error("missing required section [case]");
        return;
    }
    SectionReader reader(*table, "case", diagnostics);
    reader.get("name", config.case_config.name);
    config.case_config.start_date_specified =
        reader.get_date("start_date", config.case_config.start_date);
    reader.get("horizon_days", config.case_config.horizon_days);
    reader.finish();
    if (!config.case_config.start_date_specified) {
        diagnostics.error("case.start_date is required (TOML date, e.g. 2026-10-01)");
    }
}

void load_run(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "run", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "run", diagnostics);
    std::int64_t seed = 0;
    if (reader.has("run_seed")) {
        reader.get("run_seed", seed);
        if (seed < 0) {
            reader.error("run_seed", "must be non-negative");
        } else {
            config.run.run_seed = static_cast<std::uint64_t>(seed);
        }
    }
    reader.finish();
}

void load_monte_carlo(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "monte_carlo", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "monte_carlo", diagnostics);
    reader.get("number_of_runs", config.monte_carlo.number_of_runs);
    std::int64_t seed = 0;
    if (reader.has("master_seed")) {
        reader.get("master_seed", seed);
        if (seed < 0) {
            reader.error("master_seed", "must be non-negative");
        } else {
            config.monte_carlo.master_seed = static_cast<std::uint64_t>(seed);
        }
    }
    reader.get("sample_epistemic_uncertainty",
               config.monte_carlo.sample_epistemic_uncertainty);
    reader.get("runs_per_sample", config.monte_carlo.runs_per_sample);
    reader.finish();
}

void load_output(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "output", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "output", diagnostics);
    reader.get("directory", config.output.directory);
    reader.get("write_run_tables", config.output.write_run_tables);
    reader.get("write_runs_csv", config.output.write_runs_csv);
    reader.get("write_trips", config.output.write_trips);
    reader.finish();
}

void load_vehicle(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "vehicle", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "vehicle", diagnostics);
    auto& vehicle = config.vehicle;
    reader.get("mass_kilograms", vehicle.mass_kilograms);
    reader.get("drag_area_square_metres", vehicle.drag_area_square_metres);
    reader.get("engine_displacement_litres", vehicle.engine_displacement_litres);
    reader.get("turbocharged", vehicle.turbocharged);
    reader.get_transmission("transmission", vehicle.transmission);
    reader.get_drivetrain("drivetrain", vehicle.drivetrain);
    reader.get("odometer_kilometres", vehicle.odometer_kilometres);
    reader.get("vehicle_age_years", vehicle.vehicle_age_years);
    reader.get("oil_capacity_litres", vehicle.oil_capacity_litres);
    reader.get("front_pad_new_millimetres", vehicle.front_pad_new_millimetres);
    reader.get("rear_pad_new_millimetres", vehicle.rear_pad_new_millimetres);
    reader.get("front_disc_new_millimetres", vehicle.front_disc_new_millimetres);
    reader.get("rear_disc_new_millimetres", vehicle.rear_disc_new_millimetres);
    reader.get("tire_new_tread_millimetres", vehicle.tire_new_tread_millimetres);
    reader.get("tire_pressure_specification_bar",
               vehicle.tire_pressure_specification_bar);
    reader.finish();
}

void load_model(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "model", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "model", diagnostics);
    auto& model = config.model;

    reader.get("cold_start_wear_base_kilometres", model.cold_start_wear_base_kilometres);
    reader.get("cold_start_wear_temperature_scale_celsius",
               model.cold_start_wear_temperature_scale_celsius);
    reader.get("cold_start_wear_minimum_kilometres",
               model.cold_start_wear_minimum_kilometres);
    reader.get("cold_start_wear_maximum_kilometres",
               model.cold_start_wear_maximum_kilometres);
    reader.get("cold_start_threshold_minutes", model.cold_start_threshold_minutes);
    reader.get("short_trip_distance_kilometres", model.short_trip_distance_kilometres);
    reader.get("warm_up_idle_factor", model.warm_up_idle_factor);
    reader.get("engine_load_wear_sensitivity", model.engine_load_wear_sensitivity);
    reader.get("engine_design_life_kilometres", model.engine_design_life_kilometres);
    reader.get("oil_degradation_per_kilometre", model.oil_degradation_per_kilometre);
    reader.get("oil_degradation_per_cold_start", model.oil_degradation_per_cold_start);
    reader.get("oil_degradation_per_short_trip", model.oil_degradation_per_short_trip);
    reader.get("oil_degradation_per_day", model.oil_degradation_per_day);
    reader.get("oil_wear_sensitivity", model.oil_wear_sensitivity);
    reader.get("oil_wear_exponent", model.oil_wear_exponent);
    reader.get("oil_level_critical_fraction", model.oil_level_critical_fraction);
    reader.get("oil_level_starvation_wear_factor", model.oil_level_starvation_wear_factor);
    reader.get("oil_starvation_hazard_multiplier",
               model.oil_starvation_hazard_multiplier);
    reader.get("base_oil_consumption_litres_per_1000_kilometres",
               model.base_oil_consumption_litres_per_1000_kilometres);
    reader.get("wear_oil_consumption_growth", model.wear_oil_consumption_growth);
    reader.get("turbo_wear_per_cycle", model.turbo_wear_per_cycle);
    reader.get("turbo_heat_sensitivity", model.turbo_heat_sensitivity);
    reader.get("turbo_hot_shutdown_wear", model.turbo_hot_shutdown_wear);
    reader.get("turbo_hot_shutdown_load_threshold",
               model.turbo_hot_shutdown_load_threshold);

    reader.get("front_brake_energy_share", model.front_brake_energy_share);
    reader.get("pad_wear_millimetres_per_megajoule",
               model.pad_wear_millimetres_per_megajoule);
    reader.get("disc_wear_ratio", model.disc_wear_ratio);
    reader.get("brake_temperature_rise_per_megajoule_per_hour",
               model.brake_temperature_rise_per_megajoule_per_hour);
    reader.get("brake_temperature_wear_start_celsius",
               model.brake_temperature_wear_start_celsius);
    reader.get("brake_temperature_wear_factor", model.brake_temperature_wear_factor);
    reader.get("brake_corrosion_per_salt_day_millimetres",
               model.brake_corrosion_per_salt_day_millimetres);
    reader.get("pad_compound_factor", model.pad_compound_factor);

    reader.get("tire_wear_millimetres_per_kilometre",
               model.tire_wear_millimetres_per_kilometre);
    reader.get("tire_load_wear_exponent", model.tire_load_wear_exponent);
    reader.get("tire_pressure_wear_exponent", model.tire_pressure_wear_exponent);
    reader.get("tire_lateral_wear_sensitivity", model.tire_lateral_wear_sensitivity);
    reader.get("tire_temperature_wear_sensitivity",
               model.tire_temperature_wear_sensitivity);
    reader.get("tire_pressure_leak_per_month_bar", model.tire_pressure_leak_per_month_bar);
    reader.get("tire_age_limit_years", model.tire_age_limit_years);
    reader.get("winter_tire_wear_factor", model.winter_tire_wear_factor);
    reader.get("reference_mass_kilograms", model.reference_mass_kilograms);
    reader.get("reference_pressure_bar", model.reference_pressure_bar);
    reader.get("wet_surface_wear_increase", model.wet_surface_wear_increase);
    reader.get("gravel_surface_wear_increase", model.gravel_surface_wear_increase);
    reader.get("tire_tread_wear_limit_millimetres",
               model.tire_tread_wear_limit_millimetres);
    reader.get("front_axle_tire_share_front_wheel_drive",
               model.front_axle_tire_share_front_wheel_drive);
    reader.get("front_axle_tire_share_rear_wheel_drive",
               model.front_axle_tire_share_rear_wheel_drive);
    reader.get("front_axle_tire_share_all_wheel_drive",
               model.front_axle_tire_share_all_wheel_drive);

    reader.get("battery_age_factor", model.battery_age_factor);
    reader.get("battery_heat_damage_per_degree_day",
               model.battery_heat_damage_per_degree_day);
    reader.get("battery_cycle_damage_per_start", model.battery_cycle_damage_per_start);
    reader.get("battery_deep_discharge_damage", model.battery_deep_discharge_damage);
    reader.get("battery_deep_discharge_trip_distance_kilometres",
               model.battery_deep_discharge_trip_distance_kilometres);
    reader.get("battery_required_health_base", model.battery_required_health_base);
    reader.get("battery_required_health_per_degree_below_20",
               model.battery_required_health_per_degree_below_20);
    reader.get("battery_failure_steepness", model.battery_failure_steepness);

    reader.get("suspension_wear_per_kilometre", model.suspension_wear_per_kilometre);
    reader.get("road_roughness_sensitivity", model.road_roughness_sensitivity);
    reader.get("cornering_wear_sensitivity", model.cornering_wear_sensitivity);
    reader.get("suspension_replacement_wear_index",
               model.suspension_replacement_wear_index);
    reader.get("clutch_wear_per_start_manual", model.clutch_wear_per_start_manual);
    reader.get("clutch_wear_per_start_automatic", model.clutch_wear_per_start_automatic);
    reader.get("clutch_style_sensitivity", model.clutch_style_sensitivity);
    reader.get("clutch_replacement_wear_index", model.clutch_replacement_wear_index);

    reader.get("exhaust_corrosion_per_day", model.exhaust_corrosion_per_day);
    reader.get("short_trip_corrosion_factor", model.short_trip_corrosion_factor);
    reader.get("exhaust_salt_corrosion_per_day", model.exhaust_salt_corrosion_per_day);
    reader.get("exhaust_humidity_corrosion_per_day",
               model.exhaust_humidity_corrosion_per_day);
    reader.get("catalyst_degradation_per_kilometre",
               model.catalyst_degradation_per_kilometre);
    reader.get("catalyst_oil_poisoning_factor", model.catalyst_oil_poisoning_factor);
    reader.get("exhaust_emissions_threshold", model.exhaust_emissions_threshold);
    reader.get("catalyst_emissions_threshold", model.catalyst_emissions_threshold);
    reader.get("exhaust_replacement_wear_index",
               model.exhaust_replacement_wear_index);
    reader.get("cooling_replacement_wear_index",
               model.cooling_replacement_wear_index);
    reader.get("cooling_wear_per_day", model.cooling_wear_per_day);
    reader.get("cooling_wear_per_thermal_cycle", model.cooling_wear_per_thermal_cycle);

    reader.get("uv_damage_per_index_day", model.uv_damage_per_index_day);
    reader.get("salt_damage_per_day", model.salt_damage_per_day);
    reader.get("grit_damage_per_kilometre", model.grit_damage_per_kilometre);
    reader.get("stone_chip_rate_per_highway_kilometre",
               model.stone_chip_rate_per_highway_kilometre);
    reader.get("stone_chip_damage", model.stone_chip_damage);
    reader.get("hail_damage", model.hail_damage);
    reader.get("parking_ding_rate_per_day", model.parking_ding_rate_per_day);
    reader.get("wash_protection_per_wash_per_month",
               model.wash_protection_per_wash_per_month);
    reader.get("rust_threshold", model.rust_threshold);
    reader.get("rust_growth_per_wet_salt_day", model.rust_growth_per_wet_salt_day);
    reader.get("paint_repair_threshold", model.paint_repair_threshold);
    reader.get("paint_repair_reset_index", model.paint_repair_reset_index);

    reader.get("windshield_chip_rate_per_highway_kilometre",
               model.windshield_chip_rate_per_highway_kilometre);
    reader.get("windshield_crack_probability_per_chip_per_cold_day",
               model.windshield_crack_probability_per_chip_per_cold_day);
    reader.get("windshield_crack_temperature_threshold_celsius",
               model.windshield_crack_temperature_threshold_celsius);
    reader.get("windshield_speed_reference_kph", model.windshield_speed_reference_kph);
    reader.get("windshield_gravel_factor_per_share",
               model.windshield_gravel_factor_per_share);

    reader.get("indicated_efficiency", model.indicated_efficiency);
    reader.get("driveline_efficiency", model.driveline_efficiency);
    reader.get("friction_mean_effective_pressure_bar",
               model.friction_mean_effective_pressure_bar);
    reader.get("accessory_power_kilowatts", model.accessory_power_kilowatts);
    reader.get("air_conditioning_power_kilowatts",
               model.air_conditioning_power_kilowatts);
    reader.get("air_conditioning_temperature_threshold_celsius",
               model.air_conditioning_temperature_threshold_celsius);
    reader.get("gasoline_energy_density_megajoules_per_litre",
               model.gasoline_energy_density_megajoules_per_litre);
    reader.get("cold_start_extra_fuel_litres", model.cold_start_extra_fuel_litres);
    reader.get("cold_start_fuel_temperature_sensitivity",
               model.cold_start_fuel_temperature_sensitivity);
    reader.get("air_density_kilograms_per_cubic_metre",
               model.air_density_kilograms_per_cubic_metre);
    reader.get("gravity_metres_per_second_squared",
               model.gravity_metres_per_second_squared);
    reader.get("idle_engine_speed_rpm", model.idle_engine_speed_rpm);
    reader.get("engine_speed_per_kilometre_per_hour",
               model.engine_speed_per_kilometre_per_hour);
    reader.get("base_rolling_resistance", model.base_rolling_resistance);
    reader.get("wet_rolling_resistance_increase",
               model.wet_rolling_resistance_increase);
    reader.get("pressure_rolling_resistance_sensitivity",
               model.pressure_rolling_resistance_sensitivity);

    reader.get("speed_style_sensitivity", model.speed_style_sensitivity);
    reader.get("stop_style_sensitivity", model.stop_style_sensitivity);
    reader.get("lateral_style_sensitivity", model.lateral_style_sensitivity);
    reader.get("brake_onset_style_sensitivity", model.brake_onset_style_sensitivity);
    reader.get("acceleration_style_sensitivity", model.acceleration_style_sensitivity);
    reader.finish();
}

void load_maintenance(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "maintenance", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "maintenance", diagnostics);
    auto& maintenance = config.maintenance;
    reader.get("oil_change_interval_kilometres",
               maintenance.oil_change_interval_kilometres);
    reader.get("oil_change_interval_months", maintenance.oil_change_interval_months);
    reader.get("inspection_interval_months", maintenance.inspection_interval_months);
    reader.get("brake_fluid_interval_months", maintenance.brake_fluid_interval_months);
    reader.get("coolant_interval_months", maintenance.coolant_interval_months);
    reader.get("spark_plug_interval_kilometres",
               maintenance.spark_plug_interval_kilometres);
    reader.get("air_filter_interval_kilometres",
               maintenance.air_filter_interval_kilometres);
    reader.get("cabin_filter_interval_kilometres",
               maintenance.cabin_filter_interval_kilometres);
    reader.get("wiper_blade_interval_months", maintenance.wiper_blade_interval_months);
    reader.get("tire_rotation_interval_kilometres",
               maintenance.tire_rotation_interval_kilometres);
    reader.get("seasonal_tire_swap_spring_month",
               maintenance.seasonal_tire_swap_spring_month);
    reader.get("seasonal_tire_swap_autumn_month",
               maintenance.seasonal_tire_swap_autumn_month);
    reader.get("emissions_inspection_month", maintenance.emissions_inspection_month);
    reader.get("max_deferral_days", maintenance.max_deferral_days);
    reader.get("max_deferral_kilometres", maintenance.max_deferral_kilometres);
    reader.get("inspection_detection_base_probability",
               maintenance.inspection_detection_base_probability);
    reader.get("front_pad_wear_limit_millimetres",
               maintenance.front_pad_wear_limit_millimetres);
    reader.get("rear_pad_wear_limit_millimetres",
               maintenance.rear_pad_wear_limit_millimetres);
    reader.get("front_disc_wear_limit_millimetres",
               maintenance.front_disc_wear_limit_millimetres);
    reader.get("rear_disc_wear_limit_millimetres",
               maintenance.rear_disc_wear_limit_millimetres);
    reader.get("front_pad_replacement_margin_millimetres",
               maintenance.front_pad_replacement_margin_millimetres);
    reader.get("rear_pad_replacement_margin_millimetres",
               maintenance.rear_pad_replacement_margin_millimetres);
    reader.get("front_disc_replacement_margin_millimetres",
               maintenance.front_disc_replacement_margin_millimetres);
    reader.get("rear_disc_replacement_margin_millimetres",
               maintenance.rear_disc_replacement_margin_millimetres);
    reader.get("tire_replacement_margin_millimetres",
               maintenance.tire_replacement_margin_millimetres);
    reader.finish();
}

void load_reliability(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "reliability", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "reliability", diagnostics);
    auto& reliability = config.reliability;
    reader.get("reliability_prior_multiplier", reliability.reliability_prior_multiplier);
    reader.get("frailty_sigma", reliability.frailty_sigma);
    reader.get("engine_annual_failure_rate", reliability.engine_annual_failure_rate);
    reader.get("engine_wear_hazard_multiplier",
               reliability.engine_wear_hazard_multiplier);
    reader.get("suspension_annual_failure_rate",
               reliability.suspension_annual_failure_rate);
    reader.get("suspension_wear_hazard_multiplier",
               reliability.suspension_wear_hazard_multiplier);
    reader.get("clutch_annual_failure_rate", reliability.clutch_annual_failure_rate);
    reader.get("clutch_wear_hazard_multiplier", reliability.clutch_wear_hazard_multiplier);
    reader.get("exhaust_annual_failure_rate", reliability.exhaust_annual_failure_rate);
    reader.get("exhaust_wear_hazard_multiplier",
               reliability.exhaust_wear_hazard_multiplier);
    reader.get("cooling_annual_failure_rate", reliability.cooling_annual_failure_rate);
    reader.get("cooling_wear_hazard_multiplier",
               reliability.cooling_wear_hazard_multiplier);
    reader.get("wear_hazard_exponent", reliability.wear_hazard_exponent);
    reader.get("failure_cost_lognormal_sigma",
               reliability.failure_cost_lognormal_sigma);
    reader.get("engine_mode_weight_rings", reliability.engine_mode_weight_rings);
    reader.get("engine_mode_weight_bearings", reliability.engine_mode_weight_bearings);
    reader.get("engine_mode_weight_turbocharger",
               reliability.engine_mode_weight_turbocharger);
    reader.get("engine_mode_weight_seals", reliability.engine_mode_weight_seals);
    reader.get("engine_mode_weight_ignition", reliability.engine_mode_weight_ignition);
    reader.get("engine_mode_weight_timing", reliability.engine_mode_weight_timing);
    reader.finish();
}

void load_costs(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "costs", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "costs", diagnostics);
    auto& costs = config.costs;
    reader.get("engine_oil_and_filter_euros", costs.engine_oil_and_filter_euros);
    reader.get("inspection_euros", costs.inspection_euros);
    reader.get("brake_fluid_euros", costs.brake_fluid_euros);
    reader.get("coolant_euros", costs.coolant_euros);
    reader.get("spark_plugs_euros", costs.spark_plugs_euros);
    reader.get("air_filter_euros", costs.air_filter_euros);
    reader.get("cabin_filter_euros", costs.cabin_filter_euros);
    reader.get("wiper_blades_euros", costs.wiper_blades_euros);
    reader.get("tire_rotation_euros", costs.tire_rotation_euros);
    reader.get("seasonal_tire_swap_euros", costs.seasonal_tire_swap_euros);
    reader.get("front_brake_pads_euros", costs.front_brake_pads_euros);
    reader.get("rear_brake_pads_euros", costs.rear_brake_pads_euros);
    reader.get("front_brake_discs_euros", costs.front_brake_discs_euros);
    reader.get("rear_brake_discs_euros", costs.rear_brake_discs_euros);
    reader.get("tire_per_corner_euros", costs.tire_per_corner_euros);
    reader.get("battery_euros", costs.battery_euros);
    reader.get("suspension_euros", costs.suspension_euros);
    reader.get("clutch_or_transmission_euros", costs.clutch_or_transmission_euros);
    reader.get("exhaust_euros", costs.exhaust_euros);
    reader.get("catalyst_euros", costs.catalyst_euros);
    reader.get("cooling_euros", costs.cooling_euros);
    reader.get("windshield_euros", costs.windshield_euros);
    reader.get("paint_repair_euros", costs.paint_repair_euros);
    reader.get("engine_repair_rings_euros", costs.engine_repair_rings_euros);
    reader.get("engine_repair_bearings_euros", costs.engine_repair_bearings_euros);
    reader.get("engine_repair_turbocharger_euros",
               costs.engine_repair_turbocharger_euros);
    reader.get("engine_repair_seals_euros", costs.engine_repair_seals_euros);
    reader.get("engine_repair_ignition_euros", costs.engine_repair_ignition_euros);
    reader.get("engine_repair_timing_euros", costs.engine_repair_timing_euros);
    reader.finish();
}

void load_economics(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "economics", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "economics", diagnostics);
    auto& economics = config.economics;
    reader.get("fuel_price_euros_per_litre", economics.fuel_price_euros_per_litre);
    reader.get("regional_price_index", economics.regional_price_index);
    reader.get("towing_cost_euros", economics.towing_cost_euros);
    reader.get("downtime_value_euros_per_day", economics.downtime_value_euros_per_day);
    reader.get("downtime_days_per_failure", economics.downtime_days_per_failure);
    reader.get("depreciation_euros_per_kilometre",
               economics.depreciation_euros_per_kilometre);
    reader.get("depreciation_euros_per_year", economics.depreciation_euros_per_year);
    reader.get("include_paint_repair_in_total_cost_of_ownership",
               economics.include_paint_repair_in_total_cost_of_ownership);
    if (reader.has("budget_euros")) {
        double budget = 0.0;
        reader.get("budget_euros", budget);
        economics.budget_euros = budget;
    }
    reader.finish();
}

void load_route_classes(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "route_classes", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "route_classes", diagnostics);
    reader.get_route("city", config.route_classes.city);
    reader.get_route("rural", config.route_classes.rural);
    reader.get_route("mixed", config.route_classes.mixed);
    reader.get_route("highway", config.route_classes.highway);
    reader.finish();
}

void load_user(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "user", diagnostics, config.source_path);
    auto& user = config.user;
    if (table == nullptr) {
        user.winter_tires = config.climate.winter_tires;
        return;
    }
    SectionReader reader(*table, "user", diagnostics);
    reader.get("commute_distance_kilometres", user.commute_distance_kilometres);
    reader.get("commute_distance_coefficient_of_variation",
               user.commute_distance_coefficient_of_variation);
    reader.get("commute_days_per_week", user.commute_days_per_week);
    reader.get("errand_trips_per_week", user.errand_trips_per_week);
    reader.get("errand_distance_kilometres", user.errand_distance_kilometres);
    reader.get("errand_distance_coefficient_of_variation",
               user.errand_distance_coefficient_of_variation);
    reader.get("weekend_trips_per_week", user.weekend_trips_per_week);
    reader.get("weekend_distance_kilometres", user.weekend_distance_kilometres);
    reader.get("weekend_distance_coefficient_of_variation",
               user.weekend_distance_coefficient_of_variation);
    reader.get("long_trips_per_year", user.long_trips_per_year);
    reader.get("long_trip_distance_kilometres", user.long_trip_distance_kilometres);
    reader.get("long_trip_distance_coefficient_of_variation",
               user.long_trip_distance_coefficient_of_variation);
    reader.get_monthly("long_trip_month_weights", user.long_trip_month_weights);
    reader.get("style_aggressiveness", user.style_aggressiveness);
    reader.get("maintenance_adherence", user.maintenance_adherence);
    reader.get_parking("parking", user.parking);
    reader.get("part_quality", user.part_quality);
    reader.get("washing_per_month", user.washing_per_month);
    if (reader.has("winter_tires")) {
        reader.get("winter_tires", user.winter_tires);
    } else {
        user.winter_tires = config.climate.winter_tires;
    }
    reader.get("air_conditioning", user.air_conditioning);
    reader.finish();
}

void load_climate(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    const auto* table = sub_table(root, "climate", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "climate", diagnostics);

    std::string zone = config.climate.zone;
    if (reader.has("zone")) {
        reader.get("zone", zone);
    }
    if (!is_known_climate_zone(zone) && zone != "custom") {
        reader.error("zone", std::format(
                                  "unknown climate zone \"{}\" (known: {}, or \"custom\")",
                                  zone, [] {
                                      std::string list;
                                      for (const auto& name : known_climate_zones()) {
                                          if (!list.empty()) {
                                              list += ", ";
                                          }
                                          list += name;
                                      }
                                      return list;
                                  }()));
    }
    config.climate = climate_preset(zone);
    config.climate.zone = zone;

    auto& climate = config.climate;
    reader.get_monthly("monthly_temperature_mean_celsius",
                       climate.monthly_temperature_mean_celsius);
    reader.get_monthly("monthly_temperature_deviation_celsius",
                       climate.monthly_temperature_deviation_celsius);
    reader.get_monthly("monthly_precipitation_probability",
                       climate.monthly_precipitation_probability);
    reader.get_monthly("monthly_uv_index", climate.monthly_uv_index);
    reader.get("temperature_autocorrelation", climate.temperature_autocorrelation);
    reader.get("precipitation_persistence", climate.precipitation_persistence);
    reader.get("snow_temperature_threshold_celsius",
               climate.snow_temperature_threshold_celsius);
    reader.get("frost_temperature_threshold_celsius",
               climate.frost_temperature_threshold_celsius);
    reader.get("salt_probability_on_frost_day", climate.salt_probability_on_frost_day);
    reader.get("annual_hail_days", climate.annual_hail_days);
    reader.get("road_roughness_index", climate.road_roughness_index);
    reader.get("traffic_density_index", climate.traffic_density_index);
    reader.get("humidity_index", climate.humidity_index);
    reader.get("gravel_road_share", climate.gravel_road_share);
    reader.get("uv_cloud_reduction", climate.uv_cloud_reduction);
    reader.get("rain_demand_reduction", climate.rain_demand_reduction);
    reader.get("snow_demand_reduction", climate.snow_demand_reduction);
    reader.get("rain_speed_factor", climate.rain_speed_factor);
    reader.get("snow_speed_factor", climate.snow_speed_factor);
    reader.get("rain_stop_factor", climate.rain_stop_factor);
    reader.get("snow_stop_factor", climate.snow_stop_factor);
    reader.get("winter_tires", climate.winter_tires);
    reader.finish();

    if (zone == "custom") {
        if (!table->contains("monthly_temperature_mean_celsius") ||
            !table->contains("monthly_temperature_deviation_celsius") ||
            !table->contains("monthly_precipitation_probability") ||
            !table->contains("monthly_uv_index")) {
            diagnostics.error(
                "climate.zone = \"custom\" requires all four monthly arrays: "
                "monthly_temperature_mean_celsius, monthly_temperature_deviation_celsius, "
                "monthly_precipitation_probability, monthly_uv_index");
        }
    }
}

void load_current_state(const toml::table& root, Config& config, Diagnostics& diagnostics) {
    // Priors derived from vehicle age and odometer, used when a key is absent.
    const auto& vehicle = config.vehicle;
    const auto& model = config.model;
    const auto& maintenance = config.maintenance;
    auto& state = config.current_state;

    const auto worn_fraction = [](double wear_limit, double new_value,
                                  double fraction) {
        return wear_limit + fraction * (new_value - wear_limit);
    };

    state.engine_wear_equivalent_kilometres =
        clamp(vehicle.odometer_kilometres, 0.0, model.engine_design_life_kilometres);
    state.front_pad_millimetres = worn_fraction(
        maintenance.front_pad_wear_limit_millimetres,
        vehicle.front_pad_new_millimetres, 0.75);
    state.rear_pad_millimetres = worn_fraction(
        maintenance.rear_pad_wear_limit_millimetres,
        vehicle.rear_pad_new_millimetres, 0.75);
    state.front_disc_millimetres = worn_fraction(
        maintenance.front_disc_wear_limit_millimetres,
        vehicle.front_disc_new_millimetres, 0.75);
    state.rear_disc_millimetres = worn_fraction(
        maintenance.rear_disc_wear_limit_millimetres,
        vehicle.rear_disc_new_millimetres, 0.75);
    state.tire_tread_millimetres = {5.0, 5.0, 5.0, 5.0};
    state.tire_pressure_bar = {vehicle.tire_pressure_specification_bar,
                               vehicle.tire_pressure_specification_bar,
                               vehicle.tire_pressure_specification_bar,
                               vehicle.tire_pressure_specification_bar};
    state.tire_age_years = clamp(0.5 * vehicle.vehicle_age_years, 0.0, 5.0);
    state.winter_tire_tread_millimetres = {vehicle.tire_new_tread_millimetres,
                                           vehicle.tire_new_tread_millimetres,
                                           vehicle.tire_new_tread_millimetres,
                                           vehicle.tire_new_tread_millimetres};
    state.winter_tire_age_years = state.tire_age_years;
    state.battery_age_years = clamp(0.5 * vehicle.vehicle_age_years, 0.0, 6.0);
    state.battery_state_of_health =
        clamp(1.0 - model.battery_age_factor * std::sqrt(state.battery_age_years), 0.0, 1.0);
    state.suspension_wear_index =
        clamp(vehicle.odometer_kilometres / 300000.0, 0.0, 0.8);
    state.clutch_wear_index = clamp(vehicle.odometer_kilometres / 300000.0, 0.0, 0.8);
    state.exhaust_corrosion_index = 0.20;
    state.catalyst_degradation_index = 0.15;
    state.cooling_wear_index = 0.20;
    state.paint_damage_index = 0.20;
    state.rust_index = 0.0;
    state.windshield_chip_count = 0;

    const auto* table = sub_table(root, "current_state", diagnostics, config.source_path);
    if (table == nullptr) {
        return;
    }
    SectionReader reader(*table, "current_state", diagnostics);
    reader.get("engine_wear_equivalent_kilometres",
               state.engine_wear_equivalent_kilometres);
    reader.get("oil_degradation", state.oil_degradation);
    reader.get("oil_level_fraction", state.oil_level_fraction);
    reader.get("turbo_wear_index", state.turbo_wear_index);
    reader.get("front_pad_thickness_millimetres", state.front_pad_millimetres);
    reader.get("rear_pad_thickness_millimetres", state.rear_pad_millimetres);
    reader.get("front_disc_thickness_millimetres", state.front_disc_millimetres);
    reader.get("rear_disc_thickness_millimetres", state.rear_disc_millimetres);
    reader.get_array4("tire_tread_millimetres", state.tire_tread_millimetres);
    reader.get_array4("tire_pressure_bar", state.tire_pressure_bar);
    reader.get("tire_age_years", state.tire_age_years);
    reader.get_array4("winter_tire_tread_millimetres",
                      state.winter_tire_tread_millimetres);
    reader.get("winter_tire_age_years", state.winter_tire_age_years);
    reader.get("battery_state_of_health", state.battery_state_of_health);
    reader.get("battery_age_years", state.battery_age_years);
    reader.get("suspension_wear_index", state.suspension_wear_index);
    reader.get("clutch_wear_index", state.clutch_wear_index);
    reader.get("exhaust_corrosion_index", state.exhaust_corrosion_index);
    reader.get("catalyst_degradation_index", state.catalyst_degradation_index);
    reader.get("cooling_wear_index", state.cooling_wear_index);
    reader.get("paint_damage_index", state.paint_damage_index);
    reader.get("rust_index", state.rust_index);
    reader.get("windshield_chip_count", state.windshield_chip_count);
    reader.get("coolant_age_months", state.coolant_age_months);
    reader.get("oil_change_age_months", state.oil_change_age_months);
    reader.get("inspection_age_months", state.inspection_age_months);
    reader.get("brake_fluid_age_months", state.brake_fluid_age_months);
    reader.get("spark_plug_age_kilometres", state.spark_plug_age_kilometres);
    reader.get("air_filter_age_kilometres", state.air_filter_age_kilometres);
    reader.get("cabin_filter_age_kilometres", state.cabin_filter_age_kilometres);
    reader.get("wiper_blade_age_months", state.wiper_blade_age_months);
    reader.get("tire_rotation_age_kilometres", state.tire_rotation_age_kilometres);
    reader.finish({"uncertainty"});

    const auto* uncertainty_table =
        sub_table(*table, "uncertainty", diagnostics, config.source_path);
    if (uncertainty_table != nullptr) {
        SectionReader uncertainty_reader(*uncertainty_table, "current_state.uncertainty",
                                         diagnostics);
        auto& uncertainty = config.uncertainty;
        uncertainty_reader.get("engine_wear_equivalent_kilometres",
                               uncertainty.engine_wear_equivalent_kilometres);
        uncertainty_reader.get("oil_degradation", uncertainty.oil_degradation);
        uncertainty_reader.get("oil_level_fraction", uncertainty.oil_level_fraction);
        uncertainty_reader.get("turbo_wear_index", uncertainty.turbo_wear_index);
        uncertainty_reader.get("front_pad_thickness_millimetres",
                               uncertainty.front_pad_millimetres);
        uncertainty_reader.get("rear_pad_thickness_millimetres",
                               uncertainty.rear_pad_millimetres);
        uncertainty_reader.get("front_disc_thickness_millimetres",
                               uncertainty.front_disc_millimetres);
        uncertainty_reader.get("rear_disc_thickness_millimetres",
                               uncertainty.rear_disc_millimetres);
        uncertainty_reader.get_array4("tire_tread_millimetres",
                                      uncertainty.tire_tread_millimetres);
        uncertainty_reader.get_array4("tire_pressure_bar",
                                      uncertainty.tire_pressure_bar);
        uncertainty_reader.get("tire_age_years", uncertainty.tire_age_years);
        uncertainty_reader.get("battery_state_of_health",
                               uncertainty.battery_state_of_health);
        uncertainty_reader.get("battery_age_years", uncertainty.battery_age_years);
        uncertainty_reader.get("suspension_wear_index",
                               uncertainty.suspension_wear_index);
        uncertainty_reader.get("clutch_wear_index", uncertainty.clutch_wear_index);
        uncertainty_reader.get("exhaust_corrosion_index",
                               uncertainty.exhaust_corrosion_index);
        uncertainty_reader.get("catalyst_degradation_index",
                               uncertainty.catalyst_degradation_index);
        uncertainty_reader.get("cooling_wear_index", uncertainty.cooling_wear_index);
        uncertainty_reader.get("paint_damage_index", uncertainty.paint_damage_index);
        uncertainty_reader.get("rust_index", uncertainty.rust_index);
        uncertainty_reader.get("windshield_chip_count", uncertainty.windshield_chip_count);
        uncertainty_reader.finish();
    }
}

std::string major_version(std::string_view version) {
    const auto dot = version.find('.');
    if (dot == std::string_view::npos) {
        return std::string(version);
    }
    return std::string(version.substr(0, dot));
}

}  // namespace

bool validate_config(const Config& config, Diagnostics& diagnostics) {
    const auto failed_before = diagnostics.has_errors();

    auto error = [&](std::string message) { diagnostics.error(std::move(message)); };
    auto positive = [&](double value, std::string_view key) {
        if (!(value > 0.0)) {
            error(std::format("{} must be positive, found {}", key, value));
        }
    };
    auto non_negative = [&](double value, std::string_view key) {
        if (!(value >= 0.0)) {
            error(std::format("{} must be non-negative, found {}", key, value));
        }
    };
    auto probability = [&](double value, std::string_view key) {
        if (!(value >= 0.0 && value <= 1.0)) {
            error(std::format("{} must be between 0 and 1, found {}", key, value));
        }
    };
    auto in_range = [&](double value, double lo, double hi, std::string_view key) {
        if (!(value >= lo && value <= hi)) {
            error(std::format("{} must be between {} and {}, found {}", key, lo, hi, value));
        }
    };

    if (!config.case_config.start_date_specified) {
        error("case.start_date is required");
    }
    if (config.case_config.horizon_days <= 0) {
        error(std::format("case.horizon_days must be positive, found {}",
                          config.case_config.horizon_days));
    }
    if (config.monte_carlo.number_of_runs <= 0) {
        error(std::format("monte_carlo.number_of_runs must be positive, found {}",
                          config.monte_carlo.number_of_runs));
    }
    if (config.monte_carlo.runs_per_sample <= 0) {
        error(std::format("monte_carlo.runs_per_sample must be positive, found {}",
                          config.monte_carlo.runs_per_sample));
    }

    const auto& vehicle = config.vehicle;
    positive(vehicle.mass_kilograms, "vehicle.mass_kilograms");
    positive(vehicle.drag_area_square_metres, "vehicle.drag_area_square_metres");
    positive(vehicle.engine_displacement_litres, "vehicle.engine_displacement_litres");
    positive(vehicle.oil_capacity_litres, "vehicle.oil_capacity_litres");
    non_negative(vehicle.odometer_kilometres, "vehicle.odometer_kilometres");
    non_negative(vehicle.vehicle_age_years, "vehicle.vehicle_age_years");
    positive(vehicle.tire_pressure_specification_bar,
             "vehicle.tire_pressure_specification_bar");

    const auto& maintenance = config.maintenance;
    if (vehicle.front_pad_new_millimetres <=
        maintenance.front_pad_wear_limit_millimetres) {
        error("vehicle.front_pad_new_millimetres must exceed the front pad wear limit");
    }
    if (vehicle.rear_pad_new_millimetres <= maintenance.rear_pad_wear_limit_millimetres) {
        error("vehicle.rear_pad_new_millimetres must exceed the rear pad wear limit");
    }
    if (vehicle.front_disc_new_millimetres <=
        maintenance.front_disc_wear_limit_millimetres) {
        error("vehicle.front_disc_new_millimetres must exceed the front disc wear limit");
    }
    if (vehicle.rear_disc_new_millimetres <=
        maintenance.rear_disc_wear_limit_millimetres) {
        error("vehicle.rear_disc_new_millimetres must exceed the rear disc wear limit");
    }
    if (vehicle.tire_new_tread_millimetres <=
        config.model.tire_tread_wear_limit_millimetres) {
        error("vehicle.tire_new_tread_millimetres must exceed the tire tread wear limit");
    }

    const auto& state = config.current_state;
    non_negative(state.engine_wear_equivalent_kilometres,
                 "current_state.engine_wear_equivalent_kilometres");
    probability(state.oil_degradation, "current_state.oil_degradation");
    probability(state.oil_level_fraction, "current_state.oil_level_fraction");
    probability(state.turbo_wear_index, "current_state.turbo_wear_index");
    in_range(state.front_pad_millimetres,
             maintenance.front_pad_wear_limit_millimetres,
             vehicle.front_pad_new_millimetres,
             "current_state.front_pad_thickness_millimetres");
    in_range(state.rear_pad_millimetres, maintenance.rear_pad_wear_limit_millimetres,
             vehicle.rear_pad_new_millimetres,
             "current_state.rear_pad_thickness_millimetres");
    in_range(state.front_disc_millimetres,
             maintenance.front_disc_wear_limit_millimetres,
             vehicle.front_disc_new_millimetres,
             "current_state.front_disc_thickness_millimetres");
    in_range(state.rear_disc_millimetres, maintenance.rear_disc_wear_limit_millimetres,
             vehicle.rear_disc_new_millimetres,
             "current_state.rear_disc_thickness_millimetres");
    for (std::size_t i = 0; i < 4; ++i) {
        in_range(state.tire_tread_millimetres[i],
                 config.model.tire_tread_wear_limit_millimetres,
                 vehicle.tire_new_tread_millimetres,
                 std::format("current_state.tire_tread_millimetres[{}]", i));
        in_range(state.tire_pressure_bar[i], 0.5, 5.0,
                 std::format("current_state.tire_pressure_bar[{}]", i));
        in_range(state.winter_tire_tread_millimetres[i],
                 config.model.tire_tread_wear_limit_millimetres,
                 vehicle.tire_new_tread_millimetres,
                 std::format("current_state.winter_tire_tread_millimetres[{}]", i));
    }
    probability(state.battery_state_of_health,
                "current_state.battery_state_of_health");
    probability(state.suspension_wear_index,
                "current_state.suspension_wear_index");
    probability(state.clutch_wear_index, "current_state.clutch_wear_index");
    probability(state.exhaust_corrosion_index,
                "current_state.exhaust_corrosion_index");
    probability(state.catalyst_degradation_index,
                "current_state.catalyst_degradation_index");
    probability(state.cooling_wear_index, "current_state.cooling_wear_index");
    probability(state.paint_damage_index, "current_state.paint_damage_index");
    probability(state.rust_index, "current_state.rust_index");
    if (state.windshield_chip_count < 0) {
        error("current_state.windshield_chip_count must be non-negative");
    }
    non_negative(state.coolant_age_months, "current_state.coolant_age_months");
    non_negative(state.oil_change_age_months, "current_state.oil_change_age_months");
    non_negative(state.inspection_age_months, "current_state.inspection_age_months");
    non_negative(state.brake_fluid_age_months,
                 "current_state.brake_fluid_age_months");
    non_negative(state.spark_plug_age_kilometres,
                 "current_state.spark_plug_age_kilometres");
    non_negative(state.air_filter_age_kilometres,
                 "current_state.air_filter_age_kilometres");
    non_negative(state.cabin_filter_age_kilometres,
                 "current_state.cabin_filter_age_kilometres");
    non_negative(state.wiper_blade_age_months,
                 "current_state.wiper_blade_age_months");
    non_negative(state.tire_rotation_age_kilometres,
                 "current_state.tire_rotation_age_kilometres");

    const auto& uncertainty = config.uncertainty;
    non_negative(uncertainty.engine_wear_equivalent_kilometres,
                 "current_state.uncertainty.engine_wear_equivalent_kilometres");
    non_negative(uncertainty.oil_degradation,
                 "current_state.uncertainty.oil_degradation");
    non_negative(uncertainty.oil_level_fraction,
                 "current_state.uncertainty.oil_level_fraction");
    non_negative(uncertainty.turbo_wear_index,
                 "current_state.uncertainty.turbo_wear_index");
    non_negative(uncertainty.front_pad_millimetres,
                 "current_state.uncertainty.front_pad_thickness_millimetres");
    non_negative(uncertainty.rear_pad_millimetres,
                 "current_state.uncertainty.rear_pad_thickness_millimetres");
    non_negative(uncertainty.front_disc_millimetres,
                 "current_state.uncertainty.front_disc_thickness_millimetres");
    non_negative(uncertainty.rear_disc_millimetres,
                 "current_state.uncertainty.rear_disc_thickness_millimetres");
    for (std::size_t i = 0; i < 4; ++i) {
        non_negative(uncertainty.tire_tread_millimetres[i],
                     std::format("current_state.uncertainty.tire_tread_millimetres[{}]",
                                 i));
        non_negative(uncertainty.tire_pressure_bar[i],
                     std::format("current_state.uncertainty.tire_pressure_bar[{}]", i));
    }
    non_negative(uncertainty.battery_state_of_health,
                 "current_state.uncertainty.battery_state_of_health");
    non_negative(uncertainty.suspension_wear_index,
                 "current_state.uncertainty.suspension_wear_index");
    non_negative(uncertainty.clutch_wear_index,
                 "current_state.uncertainty.clutch_wear_index");
    non_negative(uncertainty.exhaust_corrosion_index,
                 "current_state.uncertainty.exhaust_corrosion_index");
    non_negative(uncertainty.catalyst_degradation_index,
                 "current_state.uncertainty.catalyst_degradation_index");
    non_negative(uncertainty.cooling_wear_index,
                 "current_state.uncertainty.cooling_wear_index");
    non_negative(uncertainty.paint_damage_index,
                 "current_state.uncertainty.paint_damage_index");

    const auto& user = config.user;
    non_negative(user.commute_distance_kilometres,
                 "user.commute_distance_kilometres");
    non_negative(user.commute_distance_coefficient_of_variation,
                 "user.commute_distance_coefficient_of_variation");
    if (user.commute_days_per_week < 0 || user.commute_days_per_week > 7) {
        error("user.commute_days_per_week must be between 0 and 7");
    }
    non_negative(user.errand_trips_per_week, "user.errand_trips_per_week");
    non_negative(user.errand_distance_kilometres,
                 "user.errand_distance_kilometres");
    non_negative(user.weekend_trips_per_week, "user.weekend_trips_per_week");
    non_negative(user.weekend_distance_kilometres,
                 "user.weekend_distance_kilometres");
    non_negative(user.long_trips_per_year, "user.long_trips_per_year");
    non_negative(user.long_trip_distance_kilometres,
                 "user.long_trip_distance_kilometres");
    probability(user.style_aggressiveness, "user.style_aggressiveness");
    probability(user.maintenance_adherence, "user.maintenance_adherence");
    probability(user.part_quality, "user.part_quality");
    non_negative(user.washing_per_month, "user.washing_per_month");
    double month_weight_sum = 0.0;
    for (std::size_t i = 0; i < 12; ++i) {
        if (user.long_trip_month_weights[i] < 0.0) {
            error(std::format("user.long_trip_month_weights[{}] must be non-negative",
                              i));
        }
        month_weight_sum += user.long_trip_month_weights[i];
    }
    if (!(month_weight_sum > 0.0)) {
        error("user.long_trip_month_weights must not all be zero");
    }

    const auto& climate = config.climate;
    probability(climate.temperature_autocorrelation,
                "climate.temperature_autocorrelation");
    probability(climate.precipitation_persistence,
                "climate.precipitation_persistence");
    probability(climate.salt_probability_on_frost_day,
                "climate.salt_probability_on_frost_day");
    probability(climate.uv_cloud_reduction, "climate.uv_cloud_reduction");
    probability(climate.rain_demand_reduction,
                "climate.rain_demand_reduction");
    probability(climate.snow_demand_reduction,
                "climate.snow_demand_reduction");
    non_negative(climate.annual_hail_days, "climate.annual_hail_days");
    non_negative(climate.road_roughness_index, "climate.road_roughness_index");
    non_negative(climate.traffic_density_index, "climate.traffic_density_index");
    probability(climate.humidity_index, "climate.humidity_index");
    probability(climate.gravel_road_share, "climate.gravel_road_share");
    positive(climate.rain_speed_factor, "climate.rain_speed_factor");
    positive(climate.snow_speed_factor, "climate.snow_speed_factor");
    positive(climate.rain_stop_factor, "climate.rain_stop_factor");
    positive(climate.snow_stop_factor, "climate.snow_stop_factor");
    for (std::size_t i = 0; i < 12; ++i) {
        probability(climate.monthly_precipitation_probability[i],
                    std::format("climate.monthly_precipitation_probability[{}]", i));
        non_negative(climate.monthly_temperature_deviation_celsius[i],
                     std::format("climate.monthly_temperature_deviation_celsius[{}]", i));
        non_negative(climate.monthly_uv_index[i],
                     std::format("climate.monthly_uv_index[{}]", i));
        in_range(climate.monthly_temperature_mean_celsius[i], -60.0, 60.0,
                 std::format("climate.monthly_temperature_mean_celsius[{}]", i));
    }

    const auto& reliability = config.reliability;
    positive(reliability.wear_hazard_exponent,
             "reliability.wear_hazard_exponent");
    non_negative(reliability.reliability_prior_multiplier,
                 "reliability.reliability_prior_multiplier");
    non_negative(reliability.frailty_sigma, "reliability.frailty_sigma");
    non_negative(reliability.engine_annual_failure_rate,
                 "reliability.engine_annual_failure_rate");
    non_negative(reliability.suspension_annual_failure_rate,
                 "reliability.suspension_annual_failure_rate");
    non_negative(reliability.clutch_annual_failure_rate,
                 "reliability.clutch_annual_failure_rate");
    non_negative(reliability.exhaust_annual_failure_rate,
                 "reliability.exhaust_annual_failure_rate");
    non_negative(reliability.cooling_annual_failure_rate,
                 "reliability.cooling_annual_failure_rate");
    non_negative(reliability.failure_cost_lognormal_sigma,
                 "reliability.failure_cost_lognormal_sigma");

    const auto& model = config.model;
    positive(model.engine_design_life_kilometres,
             "model.engine_design_life_kilometres");
    positive(model.cold_start_wear_temperature_scale_celsius,
             "model.cold_start_wear_temperature_scale_celsius");
    positive(model.indicated_efficiency, "model.indicated_efficiency");
    positive(model.driveline_efficiency, "model.driveline_efficiency");
    probability(model.indicated_efficiency, "model.indicated_efficiency");
    probability(model.driveline_efficiency, "model.driveline_efficiency");
    probability(model.front_brake_energy_share,
                "model.front_brake_energy_share");
    probability(model.oil_level_critical_fraction,
                "model.oil_level_critical_fraction");
    probability(model.rust_threshold, "model.rust_threshold");
    probability(model.paint_repair_threshold, "model.paint_repair_threshold");
    probability(model.paint_repair_reset_index, "model.paint_repair_reset_index");
    positive(model.reference_mass_kilograms, "model.reference_mass_kilograms");
    positive(model.reference_pressure_bar, "model.reference_pressure_bar");
    positive(model.tire_tread_wear_limit_millimetres,
             "model.tire_tread_wear_limit_millimetres");
    positive(model.gasoline_energy_density_megajoules_per_litre,
             "model.gasoline_energy_density_megajoules_per_litre");
    positive(model.air_density_kilograms_per_cubic_metre,
             "model.air_density_kilograms_per_cubic_metre");
    positive(model.gravity_metres_per_second_squared,
             "model.gravity_metres_per_second_squared");
    non_negative(model.cold_start_wear_minimum_kilometres,
                 "model.cold_start_wear_minimum_kilometres");
    non_negative(model.cold_start_threshold_minutes,
                 "model.cold_start_threshold_minutes");
    positive(model.windshield_speed_reference_kph,
             "model.windshield_speed_reference_kph");

    const auto& maintenance_check = config.maintenance;
    positive(maintenance_check.oil_change_interval_kilometres,
             "maintenance.oil_change_interval_kilometres");
    positive(maintenance_check.oil_change_interval_months,
             "maintenance.oil_change_interval_months");
    positive(maintenance_check.inspection_interval_months,
             "maintenance.inspection_interval_months");
    positive(maintenance_check.brake_fluid_interval_months,
             "maintenance.brake_fluid_interval_months");
    positive(maintenance_check.coolant_interval_months,
             "maintenance.coolant_interval_months");
    positive(maintenance_check.spark_plug_interval_kilometres,
             "maintenance.spark_plug_interval_kilometres");
    positive(maintenance_check.air_filter_interval_kilometres,
             "maintenance.air_filter_interval_kilometres");
    positive(maintenance_check.cabin_filter_interval_kilometres,
             "maintenance.cabin_filter_interval_kilometres");
    positive(maintenance_check.wiper_blade_interval_months,
             "maintenance.wiper_blade_interval_months");
    positive(maintenance_check.tire_rotation_interval_kilometres,
             "maintenance.tire_rotation_interval_kilometres");
    non_negative(maintenance_check.max_deferral_days,
                 "maintenance.max_deferral_days");
    non_negative(maintenance_check.max_deferral_kilometres,
                 "maintenance.max_deferral_kilometres");
    probability(maintenance_check.inspection_detection_base_probability,
                "maintenance.inspection_detection_base_probability");
    if (maintenance_check.seasonal_tire_swap_spring_month < 1 ||
        maintenance_check.seasonal_tire_swap_spring_month > 12) {
        error("maintenance.seasonal_tire_swap_spring_month must be between 1 and 12");
    }
    if (maintenance_check.seasonal_tire_swap_autumn_month < 1 ||
        maintenance_check.seasonal_tire_swap_autumn_month > 12) {
        error("maintenance.seasonal_tire_swap_autumn_month must be between 1 and 12");
    }
    if (maintenance_check.emissions_inspection_month < 1 ||
        maintenance_check.emissions_inspection_month > 12) {
        error("maintenance.emissions_inspection_month must be between 1 and 12");
    }
    positive(maintenance_check.front_pad_wear_limit_millimetres,
             "maintenance.front_pad_wear_limit_millimetres");
    positive(maintenance_check.rear_pad_wear_limit_millimetres,
             "maintenance.rear_pad_wear_limit_millimetres");
    positive(maintenance_check.front_disc_wear_limit_millimetres,
             "maintenance.front_disc_wear_limit_millimetres");
    positive(maintenance_check.rear_disc_wear_limit_millimetres,
             "maintenance.rear_disc_wear_limit_millimetres");
    non_negative(maintenance_check.front_pad_replacement_margin_millimetres,
                 "maintenance.front_pad_replacement_margin_millimetres");
    non_negative(maintenance_check.rear_pad_replacement_margin_millimetres,
                 "maintenance.rear_pad_replacement_margin_millimetres");
    non_negative(maintenance_check.front_disc_replacement_margin_millimetres,
                 "maintenance.front_disc_replacement_margin_millimetres");
    non_negative(maintenance_check.rear_disc_replacement_margin_millimetres,
                 "maintenance.rear_disc_replacement_margin_millimetres");
    non_negative(maintenance_check.tire_replacement_margin_millimetres,
                 "maintenance.tire_replacement_margin_millimetres");

    in_range(model.suspension_replacement_wear_index, 0.0, 1.0,
             "model.suspension_replacement_wear_index");
    in_range(model.clutch_replacement_wear_index, 0.0, 1.0,
             "model.clutch_replacement_wear_index");
    in_range(model.exhaust_replacement_wear_index, 0.0, 1.0,
             "model.exhaust_replacement_wear_index");
    in_range(model.cooling_replacement_wear_index, 0.0, 1.0,
             "model.cooling_replacement_wear_index");
    in_range(model.exhaust_emissions_threshold, 0.0, 1.0,
             "model.exhaust_emissions_threshold");
    in_range(model.catalyst_emissions_threshold, 0.0, 1.0,
             "model.catalyst_emissions_threshold");

    const auto& costs = config.costs;
    const double cost_values[] = {
        costs.engine_oil_and_filter_euros,   costs.inspection_euros,
        costs.brake_fluid_euros,             costs.coolant_euros,
        costs.spark_plugs_euros,             costs.air_filter_euros,
        costs.cabin_filter_euros,            costs.wiper_blades_euros,
        costs.tire_rotation_euros,           costs.seasonal_tire_swap_euros,
        costs.front_brake_pads_euros,        costs.rear_brake_pads_euros,
        costs.front_brake_discs_euros,       costs.rear_brake_discs_euros,
        costs.tire_per_corner_euros,         costs.battery_euros,
        costs.suspension_euros,              costs.clutch_or_transmission_euros,
        costs.exhaust_euros,                 costs.catalyst_euros,
        costs.cooling_euros,                 costs.windshield_euros,
        costs.paint_repair_euros,            costs.engine_repair_rings_euros,
        costs.engine_repair_bearings_euros,  costs.engine_repair_turbocharger_euros,
        costs.engine_repair_seals_euros,     costs.engine_repair_ignition_euros,
        costs.engine_repair_timing_euros};
    for (double value : cost_values) {
        non_negative(value, "costs.*");
    }

    const auto& economics = config.economics;
    non_negative(economics.fuel_price_euros_per_litre,
                 "economics.fuel_price_euros_per_litre");
    positive(economics.regional_price_index, "economics.regional_price_index");
    non_negative(economics.towing_cost_euros, "economics.towing_cost_euros");
    non_negative(economics.downtime_value_euros_per_day,
                 "economics.downtime_value_euros_per_day");
    non_negative(economics.downtime_days_per_failure,
                 "economics.downtime_days_per_failure");
    non_negative(economics.depreciation_euros_per_kilometre,
                 "economics.depreciation_euros_per_kilometre");
    non_negative(economics.depreciation_euros_per_year,
                 "economics.depreciation_euros_per_year");

    const auto check_route = [&](const RouteClassConfig& route, std::string_view name) {
        positive(route.mean_speed_kph, name);
        non_negative(route.stop_density_per_kilometre, name);
        non_negative(route.lateral_acceleration_metres_per_second_squared, name);
        probability(route.idle_fraction, name);
        probability(route.brake_onset_speed_fraction, name);
    };
    check_route(config.route_classes.city, "route_classes.city");
    check_route(config.route_classes.rural, "route_classes.rural");
    check_route(config.route_classes.mixed, "route_classes.mixed");
    check_route(config.route_classes.highway, "route_classes.highway");

    return !diagnostics.has_errors() && !failed_before;
}

bool is_known_climate_zone(const std::string& name) {
    return name == "temperate-maritime" || name == "continental" || name == "alpine" ||
           name == "mediterranean" || name == "nordic" || name == "arid";
}

std::vector<std::string> known_climate_zones() {
    return {"temperate-maritime", "continental", "alpine",
            "mediterranean",       "nordic",     "arid"};
}

ClimateConfig climate_preset(const std::string& name) {
    ClimateConfig climate;
    if (name == "continental") {
        climate.monthly_temperature_mean_celsius = {-1.0, 1.0, 5.0, 10.0, 15.0, 18.0,
                                                    20.0, 19.0, 15.0, 10.0, 4.0, 0.0};
        climate.monthly_temperature_deviation_celsius = {4.0, 4.0, 4.0, 4.0, 4.0, 3.5,
                                                         3.0, 3.0, 3.5, 4.0, 4.0, 4.0};
        climate.monthly_precipitation_probability = {0.45, 0.40, 0.40, 0.40, 0.45, 0.45,
                                                     0.45, 0.45, 0.42, 0.42, 0.45, 0.48};
        climate.monthly_uv_index = {0.7, 1.5, 3.0, 4.5, 5.5, 6.0,
                                    6.2, 5.2, 3.5, 2.0, 1.0, 0.6};
        climate.precipitation_persistence = 0.60;
        climate.road_roughness_index = 0.35;
        climate.traffic_density_index = 0.40;
        climate.humidity_index = 0.55;
        climate.gravel_road_share = 0.05;
        climate.salt_probability_on_frost_day = 0.75;
        climate.annual_hail_days = 2.0;
        climate.winter_tires = true;
    } else if (name == "alpine") {
        climate.monthly_temperature_mean_celsius = {-3.0, -2.0, 2.0, 6.0, 11.0, 14.0,
                                                    16.0, 15.0, 11.0, 6.0, 1.0, -2.0};
        climate.monthly_temperature_deviation_celsius = {4.0, 4.0, 4.0, 4.0, 4.0, 3.5,
                                                         3.0, 3.0, 3.5, 4.0, 4.0, 4.0};
        climate.monthly_precipitation_probability = {0.50, 0.50, 0.50, 0.50, 0.55, 0.58,
                                                     0.55, 0.55, 0.50, 0.48, 0.50, 0.52};
        climate.monthly_uv_index = {1.0, 2.0, 3.5, 5.0, 6.0, 7.0,
                                    7.0, 6.0, 4.5, 2.5, 1.2, 0.8};
        climate.precipitation_persistence = 0.60;
        climate.road_roughness_index = 0.50;
        climate.traffic_density_index = 0.25;
        climate.humidity_index = 0.50;
        climate.gravel_road_share = 0.08;
        climate.salt_probability_on_frost_day = 0.90;
        climate.annual_hail_days = 4.0;
        climate.winter_tires = true;
    } else if (name == "mediterranean") {
        climate.monthly_temperature_mean_celsius = {9.0, 10.0, 12.0, 15.0, 19.0, 24.0,
                                                    27.0, 27.0, 23.0, 18.0, 13.0, 10.0};
        climate.monthly_temperature_deviation_celsius = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0,
                                                         3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
        climate.monthly_precipitation_probability = {0.35, 0.30, 0.28, 0.30, 0.20, 0.10,
                                                     0.05, 0.07, 0.20, 0.35, 0.40, 0.38};
        climate.monthly_uv_index = {2.0, 3.0, 4.5, 6.0, 7.0, 8.0,
                                    9.0, 8.0, 6.0, 4.0, 2.5, 1.8};
        climate.precipitation_persistence = 0.50;
        climate.road_roughness_index = 0.30;
        climate.traffic_density_index = 0.55;
        climate.humidity_index = 0.55;
        climate.gravel_road_share = 0.04;
        climate.salt_probability_on_frost_day = 0.20;
        climate.annual_hail_days = 1.0;
        climate.winter_tires = false;
    } else if (name == "nordic") {
        climate.monthly_temperature_mean_celsius = {-5.0, -5.0, -2.0, 3.0, 9.0, 14.0,
                                                    17.0, 15.0, 10.0, 4.0, -1.0, -4.0};
        climate.monthly_temperature_deviation_celsius = {4.0, 4.0, 3.5, 3.0, 3.0, 3.0,
                                                         2.5, 3.0, 3.0, 3.5, 4.0, 4.0};
        climate.monthly_precipitation_probability = {0.50, 0.42, 0.40, 0.40, 0.42, 0.45,
                                                     0.48, 0.50, 0.50, 0.52, 0.55, 0.52};
        climate.monthly_uv_index = {0.3, 0.8, 2.0, 3.5, 5.0, 6.0,
                                    5.8, 4.5, 2.8, 1.2, 0.5, 0.2};
        climate.precipitation_persistence = 0.60;
        climate.road_roughness_index = 0.40;
        climate.traffic_density_index = 0.20;
        climate.humidity_index = 0.60;
        climate.gravel_road_share = 0.12;
        climate.salt_probability_on_frost_day = 0.80;
        climate.annual_hail_days = 1.5;
        climate.winter_tires = true;
    } else if (name == "arid") {
        climate.monthly_temperature_mean_celsius = {13.0, 15.0, 18.0, 23.0, 28.0, 33.0,
                                                    35.0, 34.0, 30.0, 24.0, 17.0, 13.0};
        climate.monthly_temperature_deviation_celsius = {4.0, 4.0, 4.0, 4.0, 4.0, 4.0,
                                                         4.0, 4.0, 4.0, 4.0, 4.0, 4.0};
        climate.monthly_precipitation_probability = {0.15, 0.15, 0.15, 0.10, 0.07, 0.03,
                                                     0.10, 0.12, 0.10, 0.10, 0.12, 0.15};
        climate.monthly_uv_index = {3.0, 4.0, 5.5, 7.0, 8.0, 9.0,
                                    9.0, 8.5, 7.0, 5.5, 3.5, 2.8};
        climate.precipitation_persistence = 0.30;
        climate.road_roughness_index = 0.20;
        climate.traffic_density_index = 0.50;
        climate.humidity_index = 0.20;
        climate.gravel_road_share = 0.06;
        climate.salt_probability_on_frost_day = 0.0;
        climate.annual_hail_days = 2.0;
        climate.winter_tires = false;
    } else if (name == "custom") {
        climate.zone = "custom";
        climate.winter_tires = false;
    } else {
        // Maritime is the default preset.
        climate.monthly_temperature_mean_celsius = preset_temperature_maritime();
        climate.precipitation_persistence = 0.65;
        climate.road_roughness_index = 0.25;
        climate.traffic_density_index = 0.45;
        climate.humidity_index = 0.65;
        climate.gravel_road_share = 0.03;
        climate.salt_probability_on_frost_day = 0.60;
        climate.annual_hail_days = 1.0;
        climate.winter_tires = false;
    }
    return climate;
}

ConfigLoadResult parse_config(const std::string& text, const std::string& source_name) {
    ConfigLoadResult result;
    toml::table root;
    try {
        root = toml::parse(text, source_name);
    } catch (const toml::parse_error& error) {
        result.diagnostics.error(std::format(
            "{}:{}:{}: {}", source_name, error.source().begin.line,
            error.source().begin.column, error.description()));
        return result;
    }

    Config config;
    config.source_path = source_name;

    // Schema version.
    std::string schema_version = "1.0";
    if (auto node = root["schema_version"]) {
        if (auto value = node.value<std::string>()) {
            schema_version = *value;
        } else {
            result.diagnostics.error("schema_version must be a string");
        }
    }
    if (major_version(schema_version) != "1") {
        result.diagnostics.error(std::format(
            "unsupported schema_version \"{}\": this build reads schema major version 1",
            schema_version));
        return result;
    }

    // Unknown top-level keys.
    static const std::set<std::string, std::less<>> known_sections = {
        "schema_version", "case",       "run",        "monte_carlo", "output",
        "vehicle",        "current_state", "user",    "climate",     "maintenance",
        "reliability",    "model",      "costs",      "economics",   "route_classes"};
    for (const auto& [key, node] : root) {
        (void)node;
        const std::string name{key.str()};
        if (!known_sections.contains(name)) {
            result.diagnostics.warn(std::format(
                "{}: unknown top-level key '{}' ignored", source_name, name));
        }
    }

    // Order matters: current_state derives defaults from vehicle and model.
    load_case(root, config, result.diagnostics);
    load_run(root, config, result.diagnostics);
    load_monte_carlo(root, config, result.diagnostics);
    load_output(root, config, result.diagnostics);
    load_vehicle(root, config, result.diagnostics);
    load_model(root, config, result.diagnostics);
    load_maintenance(root, config, result.diagnostics);
    load_reliability(root, config, result.diagnostics);
    load_costs(root, config, result.diagnostics);
    load_economics(root, config, result.diagnostics);
    load_route_classes(root, config, result.diagnostics);
    load_climate(root, config, result.diagnostics);
    load_user(root, config, result.diagnostics);
    load_current_state(root, config, result.diagnostics);

    validate_config(config, result.diagnostics);

    if (!result.diagnostics.has_errors()) {
        result.config = std::move(config);
    }
    return result;
}

ConfigLoadResult load_config(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        ConfigLoadResult result;
        result.diagnostics.error(std::format("cannot open configuration file '{}'", path));
        return result;
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    return parse_config(contents.str(), path);
}

}  // namespace carwear
