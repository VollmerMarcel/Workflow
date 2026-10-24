#include "carwear/model/components.h"

#include <algorithm>
#include <cmath>

#include "carwear/core/math.h"
#include "carwear/model/physics.h"

namespace carwear {
namespace {

constexpr double kDaysPerMonth = 30.4375;

double effective_wear_factor(double installed_factor) {
    return installed_factor > 0.0 ? installed_factor : 1.0;
}

double parking_ding_multiplier(const Config& config) {
    switch (config.user.parking) {
        case Parking::street:
            return config.model.parking_ding_multiplier_street;
        case Parking::carport:
            return config.model.parking_ding_multiplier_carport;
        case Parking::garage:
            return config.model.parking_ding_multiplier_garage;
    }
    return 1.0;
}

}  // namespace

double update_engine_trip(const Config& config, SimulationState& state,
                          const Drive& drive, const Weather& weather) {
    const auto& model = config.model;
    const double style = clamp(config.user.style_aggressiveness, 0.0, 1.0);

    const double load_factor =
        1.0 + model.engine_load_wear_sensitivity * square(style) *
                  (0.5 + 0.5 * drive.mean_speed_kph / 100.0);
    const double oil_wear_factor =
        1.0 + model.oil_wear_sensitivity *
                  std::pow(std::max(0.0, state.oil_degradation),
                           model.oil_wear_exponent);
    const double level_factor =
        1.0 + model.oil_level_starvation_wear_factor *
                  indicator(state.oil_level_fraction < model.oil_level_critical_fraction);

    double cold_start_increment = 0.0;
    if (drive.cold_start) {
        cold_start_increment =
            cold_start_equivalent_distance(config, weather.temperature_celsius) *
            (1.0 + model.warm_up_idle_factor);
    }

    const double increment = drive.distance_kilometres * load_factor * oil_wear_factor *
                                 level_factor +
                             cold_start_increment;
    state.engine_wear_equivalent_kilometres += increment;
    state.engine_consumed_kilometres += increment;

    state.oil_degradation =
        clamp(state.oil_degradation +
                  drive.distance_kilometres * model.oil_degradation_per_kilometre +
                  indicator(drive.cold_start) * model.oil_degradation_per_cold_start +
                  indicator(drive.short_trip) * model.oil_degradation_per_short_trip,
              0.0, 1.0);

    const double wear_index =
        clamp(state.engine_wear_equivalent_kilometres /
                  model.engine_design_life_kilometres,
              0.0, 2.0);
    const double consumption_per_1000 =
        model.base_oil_consumption_litres_per_1000_kilometres +
        model.wear_oil_consumption_growth * std::pow(wear_index, 1.5);
    const double oil_burned =
        consumption_per_1000 * drive.distance_kilometres / 1000.0;
    state.oil_consumed_litres += oil_burned;
    state.oil_level_fraction =
        clamp(state.oil_level_fraction -
                  oil_burned / std::max(0.1, config.vehicle.oil_capacity_litres),
              0.0, 1.0);

    if (config.vehicle.turbocharged) {
        const double thermal_load =
            load_factor * (1.0 + model.turbo_heat_sensitivity *
                                     drive.mean_speed_kph / 100.0);
        state.turbo_wear_index +=
            model.turbo_wear_per_cycle * square(thermal_load);
        if (load_factor > model.turbo_hot_shutdown_load_threshold) {
            state.turbo_wear_index += model.turbo_hot_shutdown_wear;
        }
        state.turbo_wear_index = clamp(state.turbo_wear_index, 0.0, 1.0);
    }

    return oil_burned;
}

void update_brakes_trip(const Config& config, SimulationState& state,
                        const Drive& drive, const Weather& weather) {
    const auto& model = config.model;
    const double energy = brake_energy_megajoules(config, drive);
    if (!(energy > 0.0)) {
        return;
    }
    const double duration = std::max(0.01, drive.duration_hours);
    const double pad_temperature =
        weather.temperature_celsius +
        model.brake_temperature_rise_per_megajoule_per_hour * energy / duration;
    const double temperature_factor =
        1.0 + model.brake_temperature_wear_factor *
                  std::max(0.0, pad_temperature -
                                    model.brake_temperature_wear_start_celsius) /
                  100.0;

    const double front_wear =
        energy * model.front_brake_energy_share *
        model.pad_wear_millimetres_per_megajoule * temperature_factor *
        model.pad_compound_factor * effective_wear_factor(state.front_pad_wear_factor);
    const double rear_wear =
        energy * (1.0 - model.front_brake_energy_share) *
        model.pad_wear_millimetres_per_megajoule * temperature_factor *
        model.pad_compound_factor * effective_wear_factor(state.rear_pad_wear_factor);

    state.front_pad_millimetres =
        std::max(0.0, state.front_pad_millimetres - front_wear);
    state.rear_pad_millimetres =
        std::max(0.0, state.rear_pad_millimetres - rear_wear);
    state.front_pad_consumed_millimetres += front_wear;
    state.rear_pad_consumed_millimetres += rear_wear;

    const double front_disc_wear =
        front_wear * model.disc_wear_ratio * effective_wear_factor(state.front_disc_wear_factor);
    const double rear_disc_wear =
        rear_wear * model.disc_wear_ratio * effective_wear_factor(state.rear_disc_wear_factor);
    state.front_disc_millimetres =
        std::max(0.0, state.front_disc_millimetres - front_disc_wear);
    state.rear_disc_millimetres =
        std::max(0.0, state.rear_disc_millimetres - rear_disc_wear);
    state.front_disc_consumed_millimetres += front_disc_wear;
    state.rear_disc_consumed_millimetres += rear_disc_wear;
}

void update_tires_trip(const Config& config, SimulationState& state,
                       const Drive& drive, const Weather& weather) {
    const auto& model = config.model;
    const auto& vehicle = config.vehicle;
    const double style = clamp(config.user.style_aggressiveness, 0.0, 1.0);

    auto& tire_set = state.tire_sets[static_cast<std::size_t>(state.active_tire_set)];
    const auto shares = tire_corner_shares(config);

    const double corner_load_factor =
        std::pow(vehicle.mass_kilograms / model.reference_mass_kilograms,
                 model.tire_load_wear_exponent);
    const double lateral_factor =
        1.0 + model.tire_lateral_wear_sensitivity *
                  square(drive.lateral_acceleration * (1.0 + style));
    const double temperature_factor =
        1.0 + model.tire_temperature_wear_sensitivity *
                  std::max(0.0, weather.temperature_celsius - 20.0) / 20.0;
    const double surface_factor =
        1.0 + model.wet_surface_wear_increase * indicator(weather.road_wet) +
        model.gravel_surface_wear_increase * config.climate.gravel_road_share;
    const double compound_factor =
        state.active_tire_set == 1 ? model.winter_tire_wear_factor : 1.0;
    const double installed_factor =
        effective_wear_factor(state.tire_wear_factor[static_cast<std::size_t>(
            state.active_tire_set)]);

    for (std::size_t corner = 0; corner < 4; ++corner) {
        const double effective_pressure =
            effective_tire_pressure(state.tire_pressure_bar[corner],
                                    weather.temperature_celsius);
        const double pressure_factor =
            std::pow(model.reference_pressure_bar /
                         std::max(0.5, effective_pressure),
                     model.tire_pressure_wear_exponent);
        const double loss =
            drive.distance_kilometres * model.tire_wear_millimetres_per_kilometre *
            shares[corner] * corner_load_factor * pressure_factor * lateral_factor *
            temperature_factor * surface_factor * compound_factor * installed_factor;
        tire_set.tread_millimetres[corner] =
            std::max(0.0, tire_set.tread_millimetres[corner] - loss);
        state.tire_consumed_millimetres += loss;
    }
}

void update_suspension_trip(const Config& config, SimulationState& state,
                            const Drive& drive) {
    const auto& model = config.model;
    const double style = clamp(config.user.style_aggressiveness, 0.0, 1.0);
    const double load_factor =
        1.0 + model.engine_load_wear_sensitivity * square(style) *
                  (0.5 + 0.5 * drive.mean_speed_kph / 100.0);
    const double increment =
        drive.distance_kilometres * model.suspension_wear_per_kilometre *
        (1.0 + model.road_roughness_sensitivity *
                   config.climate.road_roughness_index) *
        (1.0 + model.cornering_wear_sensitivity * drive.lateral_acceleration) *
        load_factor;
    state.suspension_wear_index = clamp(state.suspension_wear_index + increment, 0.0, 1.0);
}

void update_clutch_trip(const Config& config, SimulationState& state,
                        const Drive& drive) {
    const auto& model = config.model;
    const double style = clamp(config.user.style_aggressiveness, 0.0, 1.0);
    const double load_factor =
        1.0 + model.engine_load_wear_sensitivity * square(style) *
                  (0.5 + 0.5 * drive.mean_speed_kph / 100.0);
    const double per_start = config.vehicle.transmission == Transmission::manual
                                 ? model.clutch_wear_per_start_manual
                                 : model.clutch_wear_per_start_automatic;
    const double increment =
        per_start * load_factor * square(1.0 + model.clutch_style_sensitivity * style);
    state.clutch_wear_index = clamp(state.clutch_wear_index + increment, 0.0, 1.0);
}

void update_exhaust_and_catalyst_trip(const Config& config, SimulationState& state,
                                      const Drive& drive, double oil_burned_litres) {
    const auto& model = config.model;
    state.catalyst_degradation_index =
        clamp(state.catalyst_degradation_index +
                  drive.distance_kilometres *
                      model.catalyst_degradation_per_kilometre +
                  oil_burned_litres * model.catalyst_oil_poisoning_per_litre,
              0.0, 1.0);
}

void update_cooling_trip(const Config& config, SimulationState& state,
                         const Drive& drive) {
    (void)drive;
    state.cooling_wear_index =
        clamp(state.cooling_wear_index + config.model.cooling_wear_per_thermal_cycle,
              0.0, 1.0);
}

void update_paint_trip(const Config& config, SimulationState& state,
                       const Drive& drive, const Weather& weather, Rng& rng) {
    const auto& model = config.model;
    state.paint_damage_index =
        clamp(state.paint_damage_index +
                  drive.distance_kilometres * model.grit_damage_per_kilometre *
                      config.climate.traffic_density_index,
              0.0, 1.0);

    if (drive.route_class == RouteClass::highway) {
        const double speed_factor =
            clamp(drive.mean_speed_kph / model.stone_chip_speed_reference_kph,
                  0.5, 1.5);
        const int chips = rng.poisson(model.stone_chip_rate_per_highway_kilometre *
                                      drive.distance_kilometres * speed_factor);
        state.paint_damage_index =
            clamp(state.paint_damage_index +
                      chips * model.stone_chip_damage *
                          (1.0 - 0.5 * clamp(config.user.washing_per_month *
                                                model.wash_protection_per_wash_per_month,
                                            0.0, 1.0)),
                  0.0, 1.0);
    }
    (void)weather;
}

void update_windshield_trip(const Config& config, SimulationState& state,
                            const Drive& drive, const Weather& weather, Rng& rng) {
    (void)weather;
    if (drive.route_class != RouteClass::highway) {
        return;
    }
    const auto& model = config.model;
    const double speed_factor =
        clamp(drive.mean_speed_kph / model.windshield_speed_reference_kph, 0.5,
              1.5);
    const double gravel_factor =
        1.0 + model.windshield_gravel_factor_per_share *
                  config.climate.gravel_road_share;
    const int chips = rng.poisson(model.windshield_chip_rate_per_highway_kilometre *
                                  drive.distance_kilometres * speed_factor *
                                  gravel_factor);
    state.windshield_chip_count += chips;
}

void apply_daily_aging(const Config& config, SimulationState& state,
                       const Weather& weather, bool short_trip_day, Rng& rng) {
    const auto& model = config.model;
    const auto& climate = config.climate;

    state.oil_degradation =
        clamp(state.oil_degradation + model.oil_degradation_per_day, 0.0, 1.0);

    for (std::size_t corner = 0; corner < 4; ++corner) {
        state.tire_pressure_bar[corner] -=
            model.tire_pressure_leak_per_month_bar / kDaysPerMonth;
        state.tire_pressure_bar[corner] =
            std::max(0.5, state.tire_pressure_bar[corner]);
    }
    for (auto& tire_set : state.tire_sets) {
        tire_set.age_years += 1.0 / 365.25;
    }

    state.battery_age_years += 1.0 / 365.25;
    state.battery_degree_days_above_25 +=
        std::max(0.0, weather.temperature_celsius - 25.0);

    state.exhaust_corrosion_index =
        clamp(state.exhaust_corrosion_index + model.exhaust_corrosion_per_day +
                  indicator(short_trip_day) * model.short_trip_corrosion_factor *
                      model.exhaust_corrosion_per_day +
                  indicator(weather.salt_day) * model.exhaust_salt_corrosion_per_day +
                  climate.humidity_index * model.exhaust_humidity_corrosion_per_day,
              0.0, 1.0);

    state.cooling_wear_index =
        clamp(state.cooling_wear_index + model.cooling_wear_per_day, 0.0, 1.0);
    state.coolant_age_months += 1.0 / kDaysPerMonth;

    if (weather.salt_day) {
        state.front_pad_millimetres =
            std::max(0.0, state.front_pad_millimetres -
                              model.brake_corrosion_per_salt_day_millimetres);
        state.rear_pad_millimetres =
            std::max(0.0, state.rear_pad_millimetres -
                              model.brake_corrosion_per_salt_day_millimetres);
        state.front_pad_consumed_millimetres +=
            model.brake_corrosion_per_salt_day_millimetres;
        state.rear_pad_consumed_millimetres +=
            model.brake_corrosion_per_salt_day_millimetres;
    }

    const double wash_protection =
        clamp(config.user.washing_per_month *
                  model.wash_protection_per_wash_per_month,
              0.0, 1.0);
    state.paint_damage_index =
        clamp(state.paint_damage_index +
                  model.uv_damage_per_index_day * weather.uv_index *
                      (1.0 - 0.5 * wash_protection) +
                  model.salt_damage_per_day * indicator(weather.salt_day) *
                      (1.0 - wash_protection),
              0.0, 1.0);
    if (weather.hail) {
        state.paint_damage_index =
            clamp(state.paint_damage_index + model.hail_damage, 0.0, 1.0);
    }
    const int dings = rng.poisson(model.parking_ding_rate_per_day *
                                  parking_ding_multiplier(config));
    if (dings > 0) {
        state.paint_damage_index =
            clamp(state.paint_damage_index + dings * model.stone_chip_damage, 0.0, 1.0);
    }

    if (state.paint_damage_index > model.rust_threshold &&
        (weather.road_wet || weather.salt_day)) {
        state.rust_index =
            clamp(state.rust_index + model.rust_growth_per_wet_salt_day, 0.0, 1.0);
        state.rust_started = true;
    }
}

}  // namespace carwear
