#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "carwear/core/date.h"

namespace carwear {

enum class Transmission { manual, automatic };
enum class Drivetrain { front, rear, all };
enum class Parking { street, carport, garage };

// ---------------------------------------------------------------------------
// Case and run control
// ---------------------------------------------------------------------------

struct CaseConfig {
    std::string name = "car";
    Date start_date{std::chrono::year{2026}, std::chrono::month{1}, std::chrono::day{1}};
    bool start_date_specified = false;
    int horizon_days = 365;
};

struct RunConfig {
    std::optional<std::uint64_t> run_seed;
};

struct MonteCarloConfig {
    int number_of_runs = 20000;
    std::uint64_t master_seed = 20261021;
    bool sample_epistemic_uncertainty = true;
    // Runs sharing one resolved case. One is the default; two or more enables
    // the aleatory/epistemic variance decomposition.
    int runs_per_sample = 1;
};

struct OutputConfig {
    std::string directory = "output";
    bool write_run_tables = true;
    bool write_runs_csv = true;
    bool write_trips = false;
};

// ---------------------------------------------------------------------------
// Vehicle
// ---------------------------------------------------------------------------

struct VehicleConfig {
    double mass_kilograms = 1450.0;
    double drag_area_square_metres = 0.68;
    double engine_displacement_litres = 1.5;
    bool turbocharged = true;
    Transmission transmission = Transmission::manual;
    Drivetrain drivetrain = Drivetrain::front;
    double odometer_kilometres = 85000.0;
    double vehicle_age_years = 6.0;

    // Component specifications. These describe what a new part is.
    double oil_capacity_litres = 4.5;
    double front_pad_new_millimetres = 12.0;
    double rear_pad_new_millimetres = 10.0;
    double front_disc_new_millimetres = 25.0;
    double rear_disc_new_millimetres = 10.0;
    double tire_new_tread_millimetres = 8.0;
    double tire_pressure_specification_bar = 2.3;
};

// ---------------------------------------------------------------------------
// Current state and its uncertainty
// ---------------------------------------------------------------------------

struct CurrentStateConfig {
    double engine_wear_equivalent_kilometres = 0.0;
    double oil_degradation = 0.5;
    double oil_level_fraction = 1.0;
    double turbo_wear_index = 0.0;

    double front_pad_millimetres = 9.0;
    double rear_pad_millimetres = 8.0;
    double front_disc_millimetres = 24.4;
    double rear_disc_millimetres = 9.4;

    std::array<double, 4> tire_tread_millimetres{5.0, 5.0, 5.0, 5.0};
    std::array<double, 4> tire_pressure_bar{2.3, 2.3, 2.3, 2.3};
    double tire_age_years = 3.0;
    std::array<double, 4> winter_tire_tread_millimetres{8.0, 8.0, 8.0, 8.0};
    double winter_tire_age_years = 3.0;

    double battery_state_of_health = 0.9;
    double battery_age_years = 3.0;

    double suspension_wear_index = 0.30;
    double clutch_wear_index = 0.20;
    double exhaust_corrosion_index = 0.20;
    double catalyst_degradation_index = 0.15;
    double cooling_wear_index = 0.20;
    double paint_damage_index = 0.20;
    double rust_index = 0.0;
    int windshield_chip_count = 0;

    // Maintenance history, expressed as age at the start of the horizon.
    double coolant_age_months = 24.0;
    double oil_change_age_months = 6.0;
    double inspection_age_months = 6.0;
    double brake_fluid_age_months = 12.0;
    double spark_plug_age_kilometres = 30000.0;
    double air_filter_age_kilometres = 15000.0;
    double cabin_filter_age_kilometres = 10000.0;
    double wiper_blade_age_months = 6.0;
    double tire_rotation_age_kilometres = 5000.0;
};

struct StateUncertainty {
    double engine_wear_equivalent_kilometres = 0.0;
    double oil_degradation = 0.15;
    double oil_level_fraction = 0.10;
    double turbo_wear_index = 0.05;

    double front_pad_millimetres = 1.5;
    double rear_pad_millimetres = 1.0;
    double front_disc_millimetres = 0.5;
    double rear_disc_millimetres = 0.4;

    std::array<double, 4> tire_tread_millimetres{0.5, 0.5, 0.5, 0.5};
    std::array<double, 4> tire_pressure_bar{0.1, 0.1, 0.1, 0.1};
    double tire_age_years = 0.0;

    double battery_state_of_health = 0.05;
    double battery_age_years = 0.0;

    double suspension_wear_index = 0.10;
    double clutch_wear_index = 0.10;
    double exhaust_corrosion_index = 0.10;
    double catalyst_degradation_index = 0.05;
    double cooling_wear_index = 0.10;
    double paint_damage_index = 0.10;
    double rust_index = 0.0;
    double windshield_chip_count = 0.0;
};

// ---------------------------------------------------------------------------
// User
// ---------------------------------------------------------------------------

struct UserConfig {
    double commute_distance_kilometres = 25.0;
    double commute_distance_coefficient_of_variation = 0.25;
    int commute_days_per_week = 5;

    double errand_trips_per_week = 4.0;
    double errand_distance_kilometres = 4.0;
    double errand_distance_coefficient_of_variation = 0.40;

    double weekend_trips_per_week = 1.0;
    double weekend_distance_kilometres = 30.0;
    double weekend_distance_coefficient_of_variation = 0.50;

    double long_trips_per_year = 6.0;
    double long_trip_distance_kilometres = 400.0;
    double long_trip_distance_coefficient_of_variation = 0.40;
    std::array<double, 12> long_trip_month_weights{0.6, 0.6, 0.8, 0.9, 1.0, 1.4,
                                                   1.8, 1.8, 1.1, 0.8, 0.7, 1.2};

    double style_aggressiveness = 0.4;
    double maintenance_adherence = 0.7;
    Parking parking = Parking::street;
    double part_quality = 0.6;
    double washing_per_month = 1.0;
    bool winter_tires = false;
    bool air_conditioning = true;
};

// ---------------------------------------------------------------------------
// Climate
// ---------------------------------------------------------------------------

struct ClimateConfig {
    std::string zone = "temperate-maritime";

    std::array<double, 12> monthly_temperature_mean_celsius{
        2.0, 3.0, 6.0, 10.0, 14.0, 17.0, 19.0, 19.0, 16.0, 11.0, 6.0, 3.0};
    std::array<double, 12> monthly_temperature_deviation_celsius{
        3.5, 3.5, 3.0, 3.0, 3.0, 2.5, 2.5, 3.0, 3.0, 3.0, 3.0, 3.5};
    std::array<double, 12> monthly_precipitation_probability{
        0.55, 0.45, 0.45, 0.40, 0.40, 0.42, 0.45, 0.45, 0.45, 0.50, 0.55, 0.58};
    std::array<double, 12> monthly_uv_index{0.8, 1.5, 3.0, 4.5, 5.5, 6.0,
                                            6.0, 5.0, 3.5, 2.0, 1.0, 0.6};

    double temperature_autocorrelation = 0.70;
    double precipitation_persistence = 0.60;
    double snow_temperature_threshold_celsius = 1.0;
    double frost_temperature_threshold_celsius = 1.0;
    double salt_probability_on_frost_day = 0.70;
    double annual_hail_days = 1.0;
    double road_roughness_index = 0.25;
    double traffic_density_index = 0.45;
    double humidity_index = 0.65;
    double gravel_road_share = 0.03;
    double uv_cloud_reduction = 0.50;

    double rain_demand_reduction = 0.15;
    double snow_demand_reduction = 0.35;
    double rain_speed_factor = 0.90;
    double snow_speed_factor = 0.75;
    double rain_stop_factor = 1.10;
    double snow_stop_factor = 1.25;

    bool winter_tires = false;
};

// ---------------------------------------------------------------------------
// Maintenance policy
// ---------------------------------------------------------------------------

struct MaintenanceConfig {
    double oil_change_interval_kilometres = 15000.0;
    double oil_change_interval_months = 12.0;
    double inspection_interval_months = 12.0;
    double brake_fluid_interval_months = 24.0;
    double coolant_interval_months = 60.0;
    double spark_plug_interval_kilometres = 60000.0;
    double air_filter_interval_kilometres = 30000.0;
    double cabin_filter_interval_kilometres = 20000.0;
    double wiper_blade_interval_months = 12.0;
    double tire_rotation_interval_kilometres = 10000.0;
    int seasonal_tire_swap_spring_month = 4;
    int seasonal_tire_swap_autumn_month = 10;
    int emissions_inspection_month = 10;

    double max_deferral_days = 60.0;
    double max_deferral_kilometres = 2000.0;
    double inspection_detection_base_probability = 0.30;

    double front_pad_wear_limit_millimetres = 2.0;
    double rear_pad_wear_limit_millimetres = 2.0;
    double front_disc_wear_limit_millimetres = 22.0;
    double rear_disc_wear_limit_millimetres = 8.0;
    double front_pad_replacement_margin_millimetres = 2.0;
    double rear_pad_replacement_margin_millimetres = 2.0;
    double front_disc_replacement_margin_millimetres = 1.0;
    double rear_disc_replacement_margin_millimetres = 1.0;
    double tire_replacement_margin_millimetres = 1.0;
};

// ---------------------------------------------------------------------------
// Reliability
// ---------------------------------------------------------------------------

struct ReliabilityConfig {
    double reliability_prior_multiplier = 1.0;
    double frailty_sigma = 0.45;

    double engine_annual_failure_rate = 0.008;
    double engine_wear_hazard_multiplier = 15.0;
    double suspension_annual_failure_rate = 0.02;
    double suspension_wear_hazard_multiplier = 5.0;
    double clutch_annual_failure_rate = 0.01;
    double clutch_wear_hazard_multiplier = 5.0;
    double exhaust_annual_failure_rate = 0.015;
    double exhaust_wear_hazard_multiplier = 8.0;
    double cooling_annual_failure_rate = 0.015;
    double cooling_wear_hazard_multiplier = 8.0;
    double wear_hazard_exponent = 3.0;
    double failure_cost_lognormal_sigma = 0.25;

    // Engine failure modes are competing risks. Weights are normalized after
    // the turbocharger weight is zeroed for naturally aspirated engines.
    double engine_mode_weight_rings = 0.25;
    double engine_mode_weight_bearings = 0.20;
    double engine_mode_weight_turbocharger = 0.15;
    double engine_mode_weight_seals = 0.15;
    double engine_mode_weight_ignition = 0.10;
    double engine_mode_weight_timing = 0.15;
};

// ---------------------------------------------------------------------------
// Physical and engineering constants
// ---------------------------------------------------------------------------

struct ModelConfig {
    // Engine
    double cold_start_wear_base_kilometres = 5.0;
    double cold_start_wear_temperature_scale_celsius = 20.0;
    double cold_start_wear_minimum_kilometres = 0.5;
    double cold_start_wear_maximum_kilometres = 20.0;
    double cold_start_threshold_minutes = 120.0;
    double short_trip_distance_kilometres = 5.0;
    double warm_up_idle_factor = 0.10;
    double engine_load_wear_sensitivity = 0.30;
    double engine_design_life_kilometres = 250000.0;
    double oil_degradation_per_kilometre = 1.0 / 15000.0;
    double oil_degradation_per_cold_start = 1.0 / 900.0;
    double oil_degradation_per_short_trip = 1.0 / 2000.0;
    double oil_degradation_per_day = 1.0 / 900.0;
    double oil_wear_sensitivity = 1.5;
    double oil_wear_exponent = 2.0;
    double oil_level_critical_fraction = 0.25;
    double oil_level_starvation_wear_factor = 2.0;
    double oil_starvation_hazard_multiplier = 30.0;
    double base_oil_consumption_litres_per_1000_kilometres = 0.05;
    double wear_oil_consumption_growth = 0.50;
    double turbo_wear_per_cycle = 0.00001;
    double turbo_heat_sensitivity = 0.50;
    double turbo_hot_shutdown_wear = 0.01;
    double turbo_hot_shutdown_load_threshold = 1.25;

    // Brakes
    double front_brake_energy_share = 0.72;
    double pad_wear_millimetres_per_megajoule = 0.010;
    double disc_wear_ratio = 0.15;
    double brake_temperature_rise_per_megajoule_per_hour = 8.0;
    double brake_temperature_wear_start_celsius = 250.0;
    double brake_temperature_wear_factor = 2.0;
    double brake_corrosion_per_salt_day_millimetres = 0.003;
    double pad_compound_factor = 1.0;

    // Tires
    double tire_wear_millimetres_per_kilometre = 0.00014;
    double tire_load_wear_exponent = 1.0;
    double tire_pressure_wear_exponent = 0.5;
    double tire_lateral_wear_sensitivity = 0.8;
    double tire_temperature_wear_sensitivity = 0.25;
    double tire_pressure_leak_per_month_bar = 0.05;
    double tire_age_limit_years = 6.0;
    double winter_tire_wear_factor = 1.2;
    double reference_mass_kilograms = 1450.0;
    double reference_pressure_bar = 2.3;
    double wet_surface_wear_increase = 0.05;
    double gravel_surface_wear_increase = 0.30;
    double tire_tread_wear_limit_millimetres = 1.6;
    double front_axle_tire_share_front_wheel_drive = 0.60;
    double front_axle_tire_share_rear_wheel_drive = 0.40;
    double front_axle_tire_share_all_wheel_drive = 0.50;

    // Battery
    double battery_age_factor = 0.04;
    double battery_heat_damage_per_degree_day = 0.00001;
    double battery_cycle_damage_per_start = 0.000001;
    double battery_deep_discharge_damage = 0.005;
    double battery_deep_discharge_trip_distance_kilometres = 2.0;
    double battery_required_health_base = 0.30;
    double battery_required_health_per_degree_below_20 = 0.005;
    double battery_failure_steepness = 25.0;

    // Suspension, clutch, transmission
    double suspension_wear_per_kilometre = 1.0 / 120000.0;
    double road_roughness_sensitivity = 1.0;
    double cornering_wear_sensitivity = 0.50;
    double suspension_replacement_wear_index = 0.80;
    double clutch_wear_per_start_manual = 1.0 / 150000.0;
    double clutch_wear_per_start_automatic = 1.0 / 600000.0;
    double clutch_style_sensitivity = 0.50;
    double clutch_replacement_wear_index = 0.80;

    // Exhaust, catalyst, cooling
    double exhaust_corrosion_per_day = 1.0 / 3650.0;
    double short_trip_corrosion_factor = 3.0;
    double exhaust_salt_corrosion_per_day = 1.0 / 1200.0;
    double exhaust_humidity_corrosion_per_day = 1.0 / 8000.0;
    double catalyst_degradation_per_kilometre = 1.0 / 200000.0;
    double catalyst_oil_poisoning_factor = 1.0;
    double exhaust_emissions_threshold = 0.70;
    double catalyst_emissions_threshold = 0.70;
    double exhaust_replacement_wear_index = 0.80;
    double cooling_replacement_wear_index = 0.80;
    double cooling_wear_per_day = 1.0 / 3650.0;
    double cooling_wear_per_thermal_cycle = 1.0 / 5000.0;

    // Paint
    double uv_damage_per_index_day = 1.0 / 15000.0;
    double salt_damage_per_day = 1.0 / 6000.0;
    double grit_damage_per_kilometre = 1.0 / 400000.0;
    double stone_chip_rate_per_highway_kilometre = 0.002;
    double stone_chip_damage = 0.004;
    double hail_damage = 0.15;
    double parking_ding_rate_per_day = 0.0002;
    double wash_protection_per_wash_per_month = 0.05;
    double rust_threshold = 0.55;
    double rust_growth_per_wet_salt_day = 0.002;
    double paint_repair_threshold = 0.85;
    double paint_repair_reset_index = 0.20;

    // Windshield
    double windshield_chip_rate_per_highway_kilometre = 0.0004;
    double windshield_crack_probability_per_chip_per_cold_day = 0.02;
    double windshield_crack_temperature_threshold_celsius = -5.0;
    double windshield_speed_reference_kph = 90.0;
    double windshield_gravel_factor_per_share = 2.0;

    // Fuel
    double indicated_efficiency = 0.36;
    double driveline_efficiency = 0.92;
    double friction_mean_effective_pressure_bar = 1.2;
    double accessory_power_kilowatts = 0.6;
    double air_conditioning_power_kilowatts = 0.7;
    double air_conditioning_temperature_threshold_celsius = 22.0;
    double gasoline_energy_density_megajoules_per_litre = 32.0;
    double cold_start_extra_fuel_litres = 0.22;
    double cold_start_fuel_temperature_sensitivity = 0.05;
    double air_density_kilograms_per_cubic_metre = 1.2;
    double gravity_metres_per_second_squared = 9.81;
    double idle_engine_speed_rpm = 800.0;
    double engine_speed_per_kilometre_per_hour = 18.0;
    double base_rolling_resistance = 0.010;
    double wet_rolling_resistance_increase = 0.15;
    double pressure_rolling_resistance_sensitivity = 0.30;

    // Driver style
    double speed_style_sensitivity = 0.30;
    double stop_style_sensitivity = 0.20;
    double lateral_style_sensitivity = 0.50;
    double brake_onset_style_sensitivity = 0.30;
    double acceleration_style_sensitivity = 0.40;
};

// ---------------------------------------------------------------------------
// Costs and economics
// ---------------------------------------------------------------------------

struct CostConfig {
    double engine_oil_and_filter_euros = 110.0;
    double inspection_euros = 80.0;
    double brake_fluid_euros = 70.0;
    double coolant_euros = 100.0;
    double spark_plugs_euros = 180.0;
    double air_filter_euros = 40.0;
    double cabin_filter_euros = 35.0;
    double wiper_blades_euros = 30.0;
    double tire_rotation_euros = 30.0;
    double seasonal_tire_swap_euros = 40.0;

    double front_brake_pads_euros = 160.0;
    double rear_brake_pads_euros = 140.0;
    double front_brake_discs_euros = 320.0;
    double rear_brake_discs_euros = 260.0;
    double tire_per_corner_euros = 90.0;
    double battery_euros = 160.0;
    double suspension_euros = 700.0;
    double clutch_or_transmission_euros = 1000.0;
    double exhaust_euros = 450.0;
    double catalyst_euros = 700.0;
    double cooling_euros = 350.0;
    double windshield_euros = 400.0;
    double paint_repair_euros = 300.0;

    double engine_repair_rings_euros = 2500.0;
    double engine_repair_bearings_euros = 2800.0;
    double engine_repair_turbocharger_euros = 1800.0;
    double engine_repair_seals_euros = 800.0;
    double engine_repair_ignition_euros = 300.0;
    double engine_repair_timing_euros = 900.0;
};

struct EconomicsConfig {
    double fuel_price_euros_per_litre = 1.75;
    double regional_price_index = 1.0;
    double towing_cost_euros = 150.0;
    double downtime_value_euros_per_day = 80.0;
    double downtime_days_per_failure = 2.0;
    double depreciation_euros_per_kilometre = 0.06;
    double depreciation_euros_per_year = 600.0;
    std::optional<double> budget_euros;
    bool include_paint_repair_in_total_cost_of_ownership = false;
};

struct RouteClassConfig {
    double mean_speed_kph = 45.0;
    double stop_density_per_kilometre = 0.35;
    double lateral_acceleration_metres_per_second_squared = 1.5;
    double idle_fraction = 0.15;
    double brake_onset_speed_fraction = 0.70;
    double positive_acceleration_metres_per_second_squared = 1.8;
};

struct RouteClassesConfig {
    RouteClassConfig city{30.0, 0.60, 1.2, 0.25, 0.70, 2.2};
    RouteClassConfig rural{70.0, 0.15, 0.8, 0.08, 0.70, 1.5};
    RouteClassConfig mixed{45.0, 0.35, 1.0, 0.15, 0.70, 1.8};
    RouteClassConfig highway{110.0, 0.02, 0.4, 0.02, 0.70, 1.2};
};

// ---------------------------------------------------------------------------
// Root configuration
// ---------------------------------------------------------------------------

struct Config {
    std::string source_path;

    CaseConfig case_config;
    RunConfig run;
    MonteCarloConfig monte_carlo;
    OutputConfig output;
    VehicleConfig vehicle;
    CurrentStateConfig current_state;
    StateUncertainty uncertainty;
    UserConfig user;
    ClimateConfig climate;
    MaintenanceConfig maintenance;
    ReliabilityConfig reliability;
    ModelConfig model;
    CostConfig costs;
    EconomicsConfig economics;
    RouteClassesConfig route_classes;
};

}  // namespace carwear
