#include "carwear/config/serialize.h"

#include <array>
#include <sstream>
#include <string_view>

#include "carwear/core/date.h"

namespace carwear {
namespace {

template <typename T>
void put(toml::table& table, std::string_view key, const T& value) {
    table.insert(key, value);
}

toml::array monthly_array(const std::array<double, 12>& values) {
    toml::array array;
    for (double value : values) {
        array.push_back(value);
    }
    return array;
}

toml::array corner_array(const std::array<double, 4>& values) {
    toml::array array;
    for (double value : values) {
        array.push_back(value);
    }
    return array;
}

toml::date to_toml_date(const Date& date) {
    return toml::date{static_cast<std::uint16_t>(static_cast<int>(date.year())),
                      static_cast<std::uint8_t>(static_cast<unsigned>(date.month())),
                      static_cast<std::uint8_t>(static_cast<unsigned>(date.day()))};
}

toml::table route_to_toml(const RouteClassConfig& route) {
    toml::table table;
    put(table, "mean_speed_kph", route.mean_speed_kph);
    put(table, "stop_density_per_kilometre", route.stop_density_per_kilometre);
    put(table, "lateral_acceleration_metres_per_second_squared",
        route.lateral_acceleration_metres_per_second_squared);
    put(table, "idle_fraction", route.idle_fraction);
    put(table, "brake_onset_speed_fraction", route.brake_onset_speed_fraction);
    put(table, "positive_acceleration_metres_per_second_squared",
        route.positive_acceleration_metres_per_second_squared);
    return table;
}

std::string_view to_string(Transmission transmission) {
    return transmission == Transmission::manual ? "manual" : "automatic";
}

std::string_view to_string(Drivetrain drivetrain) {
    switch (drivetrain) {
        case Drivetrain::front:
            return "front";
        case Drivetrain::rear:
            return "rear";
        case Drivetrain::all:
            return "all";
    }
    return "front";
}

std::string_view to_string(Parking parking) {
    switch (parking) {
        case Parking::street:
            return "street";
        case Parking::carport:
            return "carport";
        case Parking::garage:
            return "garage";
    }
    return "street";
}

}  // namespace

toml::table config_to_toml(const Config& config) {
    toml::table root;
    put(root, "schema_version", "1.0");

    {
        toml::table table;
        put(table, "name", config.case_config.name);
        put(table, "start_date", to_toml_date(config.case_config.start_date));
        put(table, "horizon_days", config.case_config.horizon_days);
        put(root, "case", std::move(table));
    }
    {
        toml::table table;
        if (config.run.run_seed.has_value()) {
            put(table, "run_seed",
                static_cast<std::int64_t>(*config.run.run_seed));
        }
        put(root, "run", std::move(table));
    }
    {
        toml::table table;
        put(table, "number_of_runs", config.monte_carlo.number_of_runs);
        put(table, "master_seed",
            static_cast<std::int64_t>(config.monte_carlo.master_seed));
        put(table, "sample_epistemic_uncertainty",
            config.monte_carlo.sample_epistemic_uncertainty);
        put(table, "runs_per_sample", config.monte_carlo.runs_per_sample);
        put(root, "monte_carlo", std::move(table));
    }
    {
        toml::table table;
        put(table, "directory", config.output.directory);
        put(table, "write_run_tables", config.output.write_run_tables);
        put(table, "write_runs_csv", config.output.write_runs_csv);
        put(table, "write_trips", config.output.write_trips);
        put(root, "output", std::move(table));
    }
    {
        toml::table table;
        const auto& vehicle = config.vehicle;
        put(table, "mass_kilograms", vehicle.mass_kilograms);
        put(table, "drag_area_square_metres", vehicle.drag_area_square_metres);
        put(table, "engine_displacement_litres", vehicle.engine_displacement_litres);
        put(table, "turbocharged", vehicle.turbocharged);
        put(table, "transmission", to_string(vehicle.transmission));
        put(table, "drivetrain", to_string(vehicle.drivetrain));
        put(table, "odometer_kilometres", vehicle.odometer_kilometres);
        put(table, "vehicle_age_years", vehicle.vehicle_age_years);
        put(table, "oil_capacity_litres", vehicle.oil_capacity_litres);
        put(table, "front_pad_new_millimetres", vehicle.front_pad_new_millimetres);
        put(table, "rear_pad_new_millimetres", vehicle.rear_pad_new_millimetres);
        put(table, "front_disc_new_millimetres", vehicle.front_disc_new_millimetres);
        put(table, "rear_disc_new_millimetres", vehicle.rear_disc_new_millimetres);
        put(table, "tire_new_tread_millimetres", vehicle.tire_new_tread_millimetres);
        put(table, "tire_pressure_specification_bar",
            vehicle.tire_pressure_specification_bar);
        put(root, "vehicle", std::move(table));
    }
    {
        toml::table table;
        const auto& state = config.current_state;
        put(table, "engine_wear_equivalent_kilometres",
            state.engine_wear_equivalent_kilometres);
        put(table, "oil_degradation", state.oil_degradation);
        put(table, "oil_level_fraction", state.oil_level_fraction);
        put(table, "turbo_wear_index", state.turbo_wear_index);
        put(table, "front_pad_thickness_millimetres", state.front_pad_millimetres);
        put(table, "rear_pad_thickness_millimetres", state.rear_pad_millimetres);
        put(table, "front_disc_thickness_millimetres", state.front_disc_millimetres);
        put(table, "rear_disc_thickness_millimetres", state.rear_disc_millimetres);
        put(table, "tire_tread_millimetres", corner_array(state.tire_tread_millimetres));
        put(table, "tire_pressure_bar", corner_array(state.tire_pressure_bar));
        put(table, "tire_age_years", state.tire_age_years);
        put(table, "winter_tire_tread_millimetres",
            corner_array(state.winter_tire_tread_millimetres));
        put(table, "winter_tire_age_years", state.winter_tire_age_years);
        put(table, "battery_state_of_health", state.battery_state_of_health);
        put(table, "battery_age_years", state.battery_age_years);
        put(table, "suspension_wear_index", state.suspension_wear_index);
        put(table, "clutch_wear_index", state.clutch_wear_index);
        put(table, "exhaust_corrosion_index", state.exhaust_corrosion_index);
        put(table, "catalyst_degradation_index", state.catalyst_degradation_index);
        put(table, "cooling_wear_index", state.cooling_wear_index);
        put(table, "paint_damage_index", state.paint_damage_index);
        put(table, "rust_index", state.rust_index);
        put(table, "windshield_chip_count", state.windshield_chip_count);
        put(table, "coolant_age_months", state.coolant_age_months);
        put(table, "oil_change_age_months", state.oil_change_age_months);
        put(table, "inspection_age_months", state.inspection_age_months);
        put(table, "brake_fluid_age_months", state.brake_fluid_age_months);
        put(table, "spark_plug_age_kilometres", state.spark_plug_age_kilometres);
        put(table, "air_filter_age_kilometres", state.air_filter_age_kilometres);
        put(table, "cabin_filter_age_kilometres", state.cabin_filter_age_kilometres);
        put(table, "wiper_blade_age_months", state.wiper_blade_age_months);
        put(table, "tire_rotation_age_kilometres", state.tire_rotation_age_kilometres);

        toml::table uncertainty;
        const auto& value = config.uncertainty;
        put(uncertainty, "engine_wear_equivalent_kilometres",
            value.engine_wear_equivalent_kilometres);
        put(uncertainty, "oil_degradation", value.oil_degradation);
        put(uncertainty, "oil_level_fraction", value.oil_level_fraction);
        put(uncertainty, "turbo_wear_index", value.turbo_wear_index);
        put(uncertainty, "front_pad_thickness_millimetres",
            value.front_pad_millimetres);
        put(uncertainty, "rear_pad_thickness_millimetres",
            value.rear_pad_millimetres);
        put(uncertainty, "front_disc_thickness_millimetres",
            value.front_disc_millimetres);
        put(uncertainty, "rear_disc_thickness_millimetres",
            value.rear_disc_millimetres);
        put(uncertainty, "tire_tread_millimetres",
            corner_array(value.tire_tread_millimetres));
        put(uncertainty, "tire_pressure_bar", corner_array(value.tire_pressure_bar));
        put(uncertainty, "tire_age_years", value.tire_age_years);
        put(uncertainty, "battery_state_of_health", value.battery_state_of_health);
        put(uncertainty, "battery_age_years", value.battery_age_years);
        put(uncertainty, "suspension_wear_index", value.suspension_wear_index);
        put(uncertainty, "clutch_wear_index", value.clutch_wear_index);
        put(uncertainty, "exhaust_corrosion_index", value.exhaust_corrosion_index);
        put(uncertainty, "catalyst_degradation_index",
            value.catalyst_degradation_index);
        put(uncertainty, "cooling_wear_index", value.cooling_wear_index);
        put(uncertainty, "paint_damage_index", value.paint_damage_index);
        put(uncertainty, "rust_index", value.rust_index);
        put(uncertainty, "windshield_chip_count", value.windshield_chip_count);
        table.insert("uncertainty", std::move(uncertainty));
        put(root, "current_state", std::move(table));
    }
    {
        toml::table table;
        const auto& user = config.user;
        put(table, "commute_distance_kilometres", user.commute_distance_kilometres);
        put(table, "commute_distance_coefficient_of_variation",
            user.commute_distance_coefficient_of_variation);
        put(table, "commute_days_per_week", user.commute_days_per_week);
        put(table, "errand_trips_per_week", user.errand_trips_per_week);
        put(table, "errand_distance_kilometres", user.errand_distance_kilometres);
        put(table, "errand_distance_coefficient_of_variation",
            user.errand_distance_coefficient_of_variation);
        put(table, "weekend_trips_per_week", user.weekend_trips_per_week);
        put(table, "weekend_distance_kilometres", user.weekend_distance_kilometres);
        put(table, "weekend_distance_coefficient_of_variation",
            user.weekend_distance_coefficient_of_variation);
        put(table, "long_trips_per_year", user.long_trips_per_year);
        put(table, "long_trip_distance_kilometres",
            user.long_trip_distance_kilometres);
        put(table, "long_trip_distance_coefficient_of_variation",
            user.long_trip_distance_coefficient_of_variation);
        put(table, "long_trip_month_weights", monthly_array(user.long_trip_month_weights));
        put(table, "style_aggressiveness", user.style_aggressiveness);
        put(table, "maintenance_adherence", user.maintenance_adherence);
        put(table, "parking", to_string(user.parking));
        put(table, "part_quality", user.part_quality);
        put(table, "washing_per_month", user.washing_per_month);
        put(table, "winter_tires", user.winter_tires);
        put(table, "air_conditioning", user.air_conditioning);
        put(root, "user", std::move(table));
    }
    {
        toml::table table;
        const auto& climate = config.climate;
        put(table, "zone", climate.zone);
        put(table, "monthly_temperature_mean_celsius",
            monthly_array(climate.monthly_temperature_mean_celsius));
        put(table, "monthly_temperature_deviation_celsius",
            monthly_array(climate.monthly_temperature_deviation_celsius));
        put(table, "monthly_precipitation_probability",
            monthly_array(climate.monthly_precipitation_probability));
        put(table, "monthly_uv_index", monthly_array(climate.monthly_uv_index));
        put(table, "temperature_autocorrelation", climate.temperature_autocorrelation);
        put(table, "precipitation_persistence", climate.precipitation_persistence);
        put(table, "snow_temperature_threshold_celsius",
            climate.snow_temperature_threshold_celsius);
        put(table, "frost_temperature_threshold_celsius",
            climate.frost_temperature_threshold_celsius);
        put(table, "salt_probability_on_frost_day",
            climate.salt_probability_on_frost_day);
        put(table, "annual_hail_days", climate.annual_hail_days);
        put(table, "road_roughness_index", climate.road_roughness_index);
        put(table, "traffic_density_index", climate.traffic_density_index);
        put(table, "humidity_index", climate.humidity_index);
        put(table, "gravel_road_share", climate.gravel_road_share);
        put(table, "uv_cloud_reduction", climate.uv_cloud_reduction);
        put(table, "rain_demand_reduction", climate.rain_demand_reduction);
        put(table, "snow_demand_reduction", climate.snow_demand_reduction);
        put(table, "rain_speed_factor", climate.rain_speed_factor);
        put(table, "snow_speed_factor", climate.snow_speed_factor);
        put(table, "rain_stop_factor", climate.rain_stop_factor);
        put(table, "snow_stop_factor", climate.snow_stop_factor);
        put(table, "winter_tires", climate.winter_tires);
        put(root, "climate", std::move(table));
    }
    {
        toml::table table;
        const auto& maintenance = config.maintenance;
        put(table, "oil_change_interval_kilometres",
            maintenance.oil_change_interval_kilometres);
        put(table, "oil_change_interval_months", maintenance.oil_change_interval_months);
        put(table, "inspection_interval_months", maintenance.inspection_interval_months);
        put(table, "brake_fluid_interval_months",
            maintenance.brake_fluid_interval_months);
        put(table, "coolant_interval_months", maintenance.coolant_interval_months);
        put(table, "spark_plug_interval_kilometres",
            maintenance.spark_plug_interval_kilometres);
        put(table, "air_filter_interval_kilometres",
            maintenance.air_filter_interval_kilometres);
        put(table, "cabin_filter_interval_kilometres",
            maintenance.cabin_filter_interval_kilometres);
        put(table, "wiper_blade_interval_months",
            maintenance.wiper_blade_interval_months);
        put(table, "tire_rotation_interval_kilometres",
            maintenance.tire_rotation_interval_kilometres);
        put(table, "seasonal_tire_swap_spring_month",
            maintenance.seasonal_tire_swap_spring_month);
        put(table, "seasonal_tire_swap_autumn_month",
            maintenance.seasonal_tire_swap_autumn_month);
        put(table, "emissions_inspection_month",
            maintenance.emissions_inspection_month);
        put(table, "max_deferral_days", maintenance.max_deferral_days);
        put(table, "max_deferral_kilometres", maintenance.max_deferral_kilometres);
        put(table, "inspection_detection_base_probability",
            maintenance.inspection_detection_base_probability);
        put(table, "front_pad_wear_limit_millimetres",
            maintenance.front_pad_wear_limit_millimetres);
        put(table, "rear_pad_wear_limit_millimetres",
            maintenance.rear_pad_wear_limit_millimetres);
        put(table, "front_disc_wear_limit_millimetres",
            maintenance.front_disc_wear_limit_millimetres);
        put(table, "rear_disc_wear_limit_millimetres",
            maintenance.rear_disc_wear_limit_millimetres);
        put(table, "front_pad_replacement_margin_millimetres",
            maintenance.front_pad_replacement_margin_millimetres);
        put(table, "rear_pad_replacement_margin_millimetres",
            maintenance.rear_pad_replacement_margin_millimetres);
        put(table, "front_disc_replacement_margin_millimetres",
            maintenance.front_disc_replacement_margin_millimetres);
        put(table, "rear_disc_replacement_margin_millimetres",
            maintenance.rear_disc_replacement_margin_millimetres);
        put(table, "tire_replacement_margin_millimetres",
            maintenance.tire_replacement_margin_millimetres);
        put(root, "maintenance", std::move(table));
    }
    {
        toml::table table;
        const auto& reliability = config.reliability;
        put(table, "reliability_prior_multiplier",
            reliability.reliability_prior_multiplier);
        put(table, "frailty_sigma", reliability.frailty_sigma);
        put(table, "engine_annual_failure_rate",
            reliability.engine_annual_failure_rate);
        put(table, "engine_wear_hazard_multiplier",
            reliability.engine_wear_hazard_multiplier);
        put(table, "suspension_annual_failure_rate",
            reliability.suspension_annual_failure_rate);
        put(table, "suspension_wear_hazard_multiplier",
            reliability.suspension_wear_hazard_multiplier);
        put(table, "clutch_annual_failure_rate",
            reliability.clutch_annual_failure_rate);
        put(table, "clutch_wear_hazard_multiplier",
            reliability.clutch_wear_hazard_multiplier);
        put(table, "exhaust_annual_failure_rate",
            reliability.exhaust_annual_failure_rate);
        put(table, "exhaust_wear_hazard_multiplier",
            reliability.exhaust_wear_hazard_multiplier);
        put(table, "cooling_annual_failure_rate",
            reliability.cooling_annual_failure_rate);
        put(table, "cooling_wear_hazard_multiplier",
            reliability.cooling_wear_hazard_multiplier);
        put(table, "wear_hazard_exponent", reliability.wear_hazard_exponent);
        put(table, "failure_cost_lognormal_sigma",
            reliability.failure_cost_lognormal_sigma);
        put(table, "engine_mode_weight_rings", reliability.engine_mode_weight_rings);
        put(table, "engine_mode_weight_bearings",
            reliability.engine_mode_weight_bearings);
        put(table, "engine_mode_weight_turbocharger",
            reliability.engine_mode_weight_turbocharger);
        put(table, "engine_mode_weight_seals", reliability.engine_mode_weight_seals);
        put(table, "engine_mode_weight_ignition",
            reliability.engine_mode_weight_ignition);
        put(table, "engine_mode_weight_timing", reliability.engine_mode_weight_timing);
        put(root, "reliability", std::move(table));
    }
    {
        toml::table table;
        const auto& model = config.model;
        put(table, "cold_start_wear_base_kilometres",
            model.cold_start_wear_base_kilometres);
        put(table, "cold_start_wear_temperature_scale_celsius",
            model.cold_start_wear_temperature_scale_celsius);
        put(table, "cold_start_wear_minimum_kilometres",
            model.cold_start_wear_minimum_kilometres);
        put(table, "cold_start_wear_maximum_kilometres",
            model.cold_start_wear_maximum_kilometres);
        put(table, "cold_start_threshold_minutes", model.cold_start_threshold_minutes);
        put(table, "short_trip_distance_kilometres",
            model.short_trip_distance_kilometres);
        put(table, "warm_up_idle_factor", model.warm_up_idle_factor);
        put(table, "engine_load_wear_sensitivity",
            model.engine_load_wear_sensitivity);
        put(table, "engine_design_life_kilometres",
            model.engine_design_life_kilometres);
        put(table, "oil_degradation_per_kilometre",
            model.oil_degradation_per_kilometre);
        put(table, "oil_degradation_per_cold_start",
            model.oil_degradation_per_cold_start);
        put(table, "oil_degradation_per_short_trip",
            model.oil_degradation_per_short_trip);
        put(table, "oil_degradation_per_day", model.oil_degradation_per_day);
        put(table, "oil_wear_sensitivity", model.oil_wear_sensitivity);
        put(table, "oil_wear_exponent", model.oil_wear_exponent);
        put(table, "oil_level_critical_fraction", model.oil_level_critical_fraction);
        put(table, "oil_level_starvation_wear_factor",
            model.oil_level_starvation_wear_factor);
        put(table, "oil_starvation_hazard_multiplier",
            model.oil_starvation_hazard_multiplier);
        put(table, "base_oil_consumption_litres_per_1000_kilometres",
            model.base_oil_consumption_litres_per_1000_kilometres);
        put(table, "wear_oil_consumption_growth", model.wear_oil_consumption_growth);
        put(table, "turbo_wear_per_cycle", model.turbo_wear_per_cycle);
        put(table, "turbo_heat_sensitivity", model.turbo_heat_sensitivity);
        put(table, "turbo_hot_shutdown_wear", model.turbo_hot_shutdown_wear);
        put(table, "turbo_hot_shutdown_load_threshold",
            model.turbo_hot_shutdown_load_threshold);

        put(table, "front_brake_energy_share", model.front_brake_energy_share);
        put(table, "pad_wear_millimetres_per_megajoule",
            model.pad_wear_millimetres_per_megajoule);
        put(table, "disc_wear_ratio", model.disc_wear_ratio);
        put(table, "brake_temperature_rise_per_megajoule_per_hour",
            model.brake_temperature_rise_per_megajoule_per_hour);
        put(table, "brake_temperature_wear_start_celsius",
            model.brake_temperature_wear_start_celsius);
        put(table, "brake_temperature_wear_factor",
            model.brake_temperature_wear_factor);
        put(table, "brake_corrosion_per_salt_day_millimetres",
            model.brake_corrosion_per_salt_day_millimetres);
        put(table, "pad_compound_factor", model.pad_compound_factor);

        put(table, "tire_wear_millimetres_per_kilometre",
            model.tire_wear_millimetres_per_kilometre);
        put(table, "tire_load_wear_exponent", model.tire_load_wear_exponent);
        put(table, "tire_pressure_wear_exponent", model.tire_pressure_wear_exponent);
        put(table, "tire_lateral_wear_sensitivity",
            model.tire_lateral_wear_sensitivity);
        put(table, "tire_temperature_wear_sensitivity",
            model.tire_temperature_wear_sensitivity);
        put(table, "tire_pressure_leak_per_month_bar",
            model.tire_pressure_leak_per_month_bar);
        put(table, "tire_age_limit_years", model.tire_age_limit_years);
        put(table, "winter_tire_wear_factor", model.winter_tire_wear_factor);
        put(table, "reference_mass_kilograms", model.reference_mass_kilograms);
        put(table, "reference_pressure_bar", model.reference_pressure_bar);
        put(table, "wet_surface_wear_increase", model.wet_surface_wear_increase);
        put(table, "gravel_surface_wear_increase",
            model.gravel_surface_wear_increase);
        put(table, "tire_tread_wear_limit_millimetres",
            model.tire_tread_wear_limit_millimetres);
        put(table, "front_axle_tire_share_front_wheel_drive",
            model.front_axle_tire_share_front_wheel_drive);
        put(table, "front_axle_tire_share_rear_wheel_drive",
            model.front_axle_tire_share_rear_wheel_drive);
        put(table, "front_axle_tire_share_all_wheel_drive",
            model.front_axle_tire_share_all_wheel_drive);

        put(table, "battery_age_factor", model.battery_age_factor);
        put(table, "battery_heat_damage_per_degree_day",
            model.battery_heat_damage_per_degree_day);
        put(table, "battery_cycle_damage_per_start",
            model.battery_cycle_damage_per_start);
        put(table, "battery_deep_discharge_damage",
            model.battery_deep_discharge_damage);
        put(table, "battery_deep_discharge_trip_distance_kilometres",
            model.battery_deep_discharge_trip_distance_kilometres);
        put(table, "battery_required_health_base",
            model.battery_required_health_base);
        put(table, "battery_required_health_per_degree_below_20",
            model.battery_required_health_per_degree_below_20);
        put(table, "battery_failure_steepness", model.battery_failure_steepness);

        put(table, "suspension_wear_per_kilometre",
            model.suspension_wear_per_kilometre);
        put(table, "road_roughness_sensitivity", model.road_roughness_sensitivity);
        put(table, "cornering_wear_sensitivity", model.cornering_wear_sensitivity);
        put(table, "suspension_replacement_wear_index",
            model.suspension_replacement_wear_index);
        put(table, "clutch_wear_per_start_manual",
            model.clutch_wear_per_start_manual);
        put(table, "clutch_wear_per_start_automatic",
            model.clutch_wear_per_start_automatic);
        put(table, "clutch_style_sensitivity", model.clutch_style_sensitivity);
        put(table, "clutch_replacement_wear_index",
            model.clutch_replacement_wear_index);

        put(table, "exhaust_corrosion_per_day", model.exhaust_corrosion_per_day);
        put(table, "short_trip_corrosion_factor",
            model.short_trip_corrosion_factor);
        put(table, "exhaust_salt_corrosion_per_day",
            model.exhaust_salt_corrosion_per_day);
        put(table, "exhaust_humidity_corrosion_per_day",
            model.exhaust_humidity_corrosion_per_day);
        put(table, "catalyst_degradation_per_kilometre",
            model.catalyst_degradation_per_kilometre);
        put(table, "catalyst_oil_poisoning_factor",
            model.catalyst_oil_poisoning_factor);
        put(table, "exhaust_emissions_threshold",
            model.exhaust_emissions_threshold);
        put(table, "catalyst_emissions_threshold",
            model.catalyst_emissions_threshold);
        put(table, "exhaust_replacement_wear_index",
            model.exhaust_replacement_wear_index);
        put(table, "cooling_replacement_wear_index",
            model.cooling_replacement_wear_index);
        put(table, "cooling_wear_per_day", model.cooling_wear_per_day);
        put(table, "cooling_wear_per_thermal_cycle",
            model.cooling_wear_per_thermal_cycle);

        put(table, "uv_damage_per_index_day", model.uv_damage_per_index_day);
        put(table, "salt_damage_per_day", model.salt_damage_per_day);
        put(table, "grit_damage_per_kilometre", model.grit_damage_per_kilometre);
        put(table, "stone_chip_rate_per_highway_kilometre",
            model.stone_chip_rate_per_highway_kilometre);
        put(table, "stone_chip_damage", model.stone_chip_damage);
        put(table, "hail_damage", model.hail_damage);
        put(table, "parking_ding_rate_per_day", model.parking_ding_rate_per_day);
        put(table, "wash_protection_per_wash_per_month",
            model.wash_protection_per_wash_per_month);
        put(table, "rust_threshold", model.rust_threshold);
        put(table, "rust_growth_per_wet_salt_day",
            model.rust_growth_per_wet_salt_day);
        put(table, "paint_repair_threshold", model.paint_repair_threshold);
        put(table, "paint_repair_reset_index", model.paint_repair_reset_index);

        put(table, "windshield_chip_rate_per_highway_kilometre",
            model.windshield_chip_rate_per_highway_kilometre);
        put(table, "windshield_crack_probability_per_chip_per_cold_day",
            model.windshield_crack_probability_per_chip_per_cold_day);
        put(table, "windshield_crack_temperature_threshold_celsius",
            model.windshield_crack_temperature_threshold_celsius);
        put(table, "windshield_speed_reference_kph",
            model.windshield_speed_reference_kph);
        put(table, "windshield_gravel_factor_per_share",
            model.windshield_gravel_factor_per_share);

        put(table, "indicated_efficiency", model.indicated_efficiency);
        put(table, "driveline_efficiency", model.driveline_efficiency);
        put(table, "friction_mean_effective_pressure_bar",
            model.friction_mean_effective_pressure_bar);
        put(table, "accessory_power_kilowatts", model.accessory_power_kilowatts);
        put(table, "air_conditioning_power_kilowatts",
            model.air_conditioning_power_kilowatts);
        put(table, "air_conditioning_temperature_threshold_celsius",
            model.air_conditioning_temperature_threshold_celsius);
        put(table, "gasoline_energy_density_megajoules_per_litre",
            model.gasoline_energy_density_megajoules_per_litre);
        put(table, "cold_start_extra_fuel_litres",
            model.cold_start_extra_fuel_litres);
        put(table, "cold_start_fuel_temperature_sensitivity",
            model.cold_start_fuel_temperature_sensitivity);
        put(table, "air_density_kilograms_per_cubic_metre",
            model.air_density_kilograms_per_cubic_metre);
        put(table, "gravity_metres_per_second_squared",
            model.gravity_metres_per_second_squared);
        put(table, "idle_engine_speed_rpm", model.idle_engine_speed_rpm);
        put(table, "engine_speed_per_kilometre_per_hour",
            model.engine_speed_per_kilometre_per_hour);
        put(table, "base_rolling_resistance", model.base_rolling_resistance);
        put(table, "wet_rolling_resistance_increase",
            model.wet_rolling_resistance_increase);
        put(table, "pressure_rolling_resistance_sensitivity",
            model.pressure_rolling_resistance_sensitivity);

        put(table, "speed_style_sensitivity", model.speed_style_sensitivity);
        put(table, "stop_style_sensitivity", model.stop_style_sensitivity);
        put(table, "lateral_style_sensitivity", model.lateral_style_sensitivity);
        put(table, "brake_onset_style_sensitivity",
            model.brake_onset_style_sensitivity);
        put(table, "acceleration_style_sensitivity",
            model.acceleration_style_sensitivity);
        put(root, "model", std::move(table));
    }
    {
        toml::table table;
        const auto& costs = config.costs;
        put(table, "engine_oil_and_filter_euros", costs.engine_oil_and_filter_euros);
        put(table, "inspection_euros", costs.inspection_euros);
        put(table, "brake_fluid_euros", costs.brake_fluid_euros);
        put(table, "coolant_euros", costs.coolant_euros);
        put(table, "spark_plugs_euros", costs.spark_plugs_euros);
        put(table, "air_filter_euros", costs.air_filter_euros);
        put(table, "cabin_filter_euros", costs.cabin_filter_euros);
        put(table, "wiper_blades_euros", costs.wiper_blades_euros);
        put(table, "tire_rotation_euros", costs.tire_rotation_euros);
        put(table, "seasonal_tire_swap_euros", costs.seasonal_tire_swap_euros);
        put(table, "front_brake_pads_euros", costs.front_brake_pads_euros);
        put(table, "rear_brake_pads_euros", costs.rear_brake_pads_euros);
        put(table, "front_brake_discs_euros", costs.front_brake_discs_euros);
        put(table, "rear_brake_discs_euros", costs.rear_brake_discs_euros);
        put(table, "tire_per_corner_euros", costs.tire_per_corner_euros);
        put(table, "battery_euros", costs.battery_euros);
        put(table, "suspension_euros", costs.suspension_euros);
        put(table, "clutch_or_transmission_euros", costs.clutch_or_transmission_euros);
        put(table, "exhaust_euros", costs.exhaust_euros);
        put(table, "catalyst_euros", costs.catalyst_euros);
        put(table, "cooling_euros", costs.cooling_euros);
        put(table, "windshield_euros", costs.windshield_euros);
        put(table, "paint_repair_euros", costs.paint_repair_euros);
        put(table, "engine_repair_rings_euros", costs.engine_repair_rings_euros);
        put(table, "engine_repair_bearings_euros",
            costs.engine_repair_bearings_euros);
        put(table, "engine_repair_turbocharger_euros",
            costs.engine_repair_turbocharger_euros);
        put(table, "engine_repair_seals_euros", costs.engine_repair_seals_euros);
        put(table, "engine_repair_ignition_euros",
            costs.engine_repair_ignition_euros);
        put(table, "engine_repair_timing_euros", costs.engine_repair_timing_euros);
        put(root, "costs", std::move(table));
    }
    {
        toml::table table;
        const auto& economics = config.economics;
        put(table, "fuel_price_euros_per_litre", economics.fuel_price_euros_per_litre);
        put(table, "regional_price_index", economics.regional_price_index);
        put(table, "towing_cost_euros", economics.towing_cost_euros);
        put(table, "downtime_value_euros_per_day",
            economics.downtime_value_euros_per_day);
        put(table, "downtime_days_per_failure", economics.downtime_days_per_failure);
        put(table, "depreciation_euros_per_kilometre",
            economics.depreciation_euros_per_kilometre);
        put(table, "depreciation_euros_per_year",
            economics.depreciation_euros_per_year);
        if (economics.budget_euros.has_value()) {
            put(table, "budget_euros", *economics.budget_euros);
        }
        put(table, "include_paint_repair_in_total_cost_of_ownership",
            economics.include_paint_repair_in_total_cost_of_ownership);
        put(root, "economics", std::move(table));
    }
    {
        toml::table table;
        table.insert("city", route_to_toml(config.route_classes.city));
        table.insert("rural", route_to_toml(config.route_classes.rural));
        table.insert("mixed", route_to_toml(config.route_classes.mixed));
        table.insert("highway", route_to_toml(config.route_classes.highway));
        put(root, "route_classes", std::move(table));
    }
    return root;
}

std::string config_to_toml_string(const Config& config) {
    std::ostringstream stream;
    stream << config_to_toml(config);
    return stream.str();
}

}  // namespace carwear
