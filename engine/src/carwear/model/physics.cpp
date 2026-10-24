#include "carwear/model/physics.h"

#include <algorithm>
#include <cmath>

#include "carwear/core/math.h"

namespace carwear {

double cold_start_equivalent_distance(const Config& config, double temperature_celsius) {
    const auto& model = config.model;
    const double value =
        model.cold_start_wear_base_kilometres *
        std::exp(-(temperature_celsius - 20.0) /
                 model.cold_start_wear_temperature_scale_celsius);
    return clamp(value, model.cold_start_wear_minimum_kilometres,
                 model.cold_start_wear_maximum_kilometres);
}

double stop_count(const Drive& drive) {
    return drive.stop_density_per_kilometre * drive.distance_kilometres;
}

double brake_energy_megajoules(const Config& config, const Drive& drive) {
    const double onset_speed =
        drive.mean_speed_kph * drive.brake_onset_speed_fraction / 3.6;
    const double energy_joules = 0.5 * config.vehicle.mass_kilograms *
                                 square(onset_speed) * stop_count(drive);
    return energy_joules / 1.0e6;
}

double acceleration_energy_megajoules(const Config& config, const Drive& drive) {
    const double reference_speed = 0.8 * drive.mean_speed_kph / 3.6;
    const double energy_joules = 0.5 * config.vehicle.mass_kilograms *
                                 square(reference_speed) * stop_count(drive);
    return energy_joules / 1.0e6;
}

double fuel_litres(const Config& config, const Drive& drive,
                   const SimulationState& state, const Weather& weather) {
    const auto& model = config.model;
    const auto& vehicle = config.vehicle;

    const double pressure_bar =
        mean_active_tire_pressure(state, weather.temperature_celsius);
    const double rolling_resistance =
        model.base_rolling_resistance *
        (1.0 + model.wet_rolling_resistance_increase * indicator(weather.road_wet)) *
        (1.0 + model.pressure_rolling_resistance_sensitivity *
                   std::max(0.0, model.reference_pressure_bar - pressure_bar) /
                   model.reference_pressure_bar);

    const double distance_metres = drive.distance_kilometres * 1000.0;
    const double mean_speed_mps = drive.mean_speed_kph / 3.6;

    const double rolling_energy =
        vehicle.mass_kilograms * model.gravity_metres_per_second_squared *
        rolling_resistance * distance_metres;
    const double aerodynamic_energy = 0.5 * model.air_density_kilograms_per_cubic_metre *
                                      vehicle.drag_area_square_metres * distance_metres *
                                      square(mean_speed_mps);
    const double acceleration_energy =
        acceleration_energy_megajoules(config, drive) * 1.0e6;
    const double wheel_energy = rolling_energy + aerodynamic_energy + acceleration_energy;

    const double mean_engine_speed =
        model.idle_engine_speed_rpm +
        model.engine_speed_per_kilometre_per_hour * drive.mean_speed_kph;
    const double friction_power =
        model.friction_mean_effective_pressure_bar * 100.0 *
        vehicle.engine_displacement_litres * mean_engine_speed / 120000.0;
    const double friction_energy = friction_power * drive.duration_hours * 3.6;

    const bool air_conditioning =
        config.user.air_conditioning &&
        weather.temperature_celsius >=
            model.air_conditioning_temperature_threshold_celsius;
    const double accessory_energy =
        (model.accessory_power_kilowatts +
         model.air_conditioning_power_kilowatts * indicator(air_conditioning)) *
        drive.duration_hours * 3.6;

    const double fuel_energy =
        (wheel_energy / 1.0e6 / model.driveline_efficiency + friction_energy +
         accessory_energy) /
        model.indicated_efficiency;
    const double litres =
        fuel_energy / model.gasoline_energy_density_megajoules_per_litre;

    const double cold_start_extra =
        model.cold_start_extra_fuel_litres *
        (1.0 + model.cold_start_fuel_temperature_sensitivity *
                   std::max(0.0, 20.0 - weather.temperature_celsius) / 20.0) *
        indicator(drive.cold_start);

    return litres + cold_start_extra;
}

double battery_state_of_health(const Config& config, const SimulationState& state) {
    const auto& model = config.model;
    const double age = std::max(0.0, state.battery_age_years);
    const double health =
        1.0 - model.battery_age_factor * std::sqrt(age) -
        model.battery_heat_damage_per_degree_day * state.battery_degree_days_above_25 -
        model.battery_cycle_damage_per_start * state.battery_start_count -
        model.battery_deep_discharge_damage * state.battery_deep_discharge_count +
        state.battery_health_offset;
    return clamp(health, 0.0, 1.0);
}

double required_battery_health(const Config& config, double temperature_celsius) {
    const auto& model = config.model;
    return clamp(model.battery_required_health_base +
                     model.battery_required_health_per_degree_below_20 *
                         (20.0 - temperature_celsius),
                 0.0, 0.95);
}

double battery_no_start_probability(const Config& config, double required_health,
                                    double state_of_health) {
    const double steepness = config.model.battery_failure_steepness;
    return 1.0 / (1.0 + std::exp(-steepness * (required_health - state_of_health)));
}

double effective_tire_pressure(double measured_bar, double ambient_celsius) {
    // Gay-Lussac: the pressure is specified cold at 20 degrees Celsius and
    // corrected to the ambient temperature.
    return measured_bar * ((273.15 + ambient_celsius) / 293.15);
}

double mean_active_tire_pressure(const SimulationState& state,
                                 double ambient_celsius) {
    double sum = 0.0;
    for (double value : state.tire_pressure_bar) {
        sum += effective_tire_pressure(value, ambient_celsius);
    }
    return sum / 4.0;
}

std::array<double, 4> tire_corner_shares(const Config& config) {
    double front_axle_share = config.model.front_axle_tire_share_front_wheel_drive;
    switch (config.vehicle.drivetrain) {
        case Drivetrain::front:
            front_axle_share = config.model.front_axle_tire_share_front_wheel_drive;
            break;
        case Drivetrain::rear:
            front_axle_share = config.model.front_axle_tire_share_rear_wheel_drive;
            break;
        case Drivetrain::all:
            front_axle_share = config.model.front_axle_tire_share_all_wheel_drive;
            break;
    }
    front_axle_share = clamp(front_axle_share, 0.0, 1.0);
    const double per_front = 0.5 * front_axle_share;
    const double per_rear = 0.5 * (1.0 - front_axle_share);
    return {per_front, per_front, per_rear, per_rear};
}

double daily_failure_probability(const Config& config, double annual_rate,
                                 double wear_index, double wear_hazard_multiplier,
                                 double frailty, double extra_multiplier) {
    const auto& reliability = config.reliability;
    const double worn = std::pow(std::max(0.0, wear_index),
                                 reliability.wear_hazard_exponent);
    const double hazard_daily = reliability.reliability_prior_multiplier * frailty *
                                annual_rate *
                                (1.0 + wear_hazard_multiplier * worn) / 365.0 *
                                extra_multiplier;
    return 1.0 - std::exp(-hazard_daily);
}

double replacement_trigger(double wear_limit, double margin, double adherence) {
    return wear_limit + clamp(adherence, 0.0, 1.0) * margin;
}

double installed_cost(double base_cost_euros, double part_quality,
                      double regional_price_index) {
    return base_cost_euros * (0.75 + 0.5 * clamp(part_quality, 0.0, 1.0)) *
           regional_price_index;
}

double monthly_weight_mean(const std::array<double, 12>& weights) {
    double sum = 0.0;
    for (double value : weights) {
        sum += value;
    }
    return sum / 12.0;
}

}  // namespace carwear
