#include "carwear/model/state.h"

#include <algorithm>
#include <cmath>

#include "carwear/core/math.h"
#include "carwear/core/rng.h"

namespace carwear {
namespace {

constexpr double kDaysPerMonth = 30.4375;

}  // namespace

bool is_winter_tire_month(int calendar_month, const Config& config) {
    const auto& maintenance = config.maintenance;
    return calendar_month >= maintenance.seasonal_tire_swap_autumn_month ||
           calendar_month < maintenance.seasonal_tire_swap_spring_month;
}

void clip_current_state(CurrentStateConfig& state, const Config& config) {
    const auto& model = config.model;
    const auto& vehicle = config.vehicle;
    const auto& maintenance = config.maintenance;

    state.engine_wear_equivalent_kilometres =
        clamp(state.engine_wear_equivalent_kilometres, 0.0,
              model.engine_design_life_kilometres);
    state.oil_degradation = clamp(state.oil_degradation, 0.0, 1.0);
    state.oil_level_fraction = clamp(state.oil_level_fraction, 0.0, 1.0);
    state.turbo_wear_index = clamp(state.turbo_wear_index, 0.0, 1.0);

    state.front_pad_millimetres =
        clamp(state.front_pad_millimetres,
              maintenance.front_pad_wear_limit_millimetres,
              vehicle.front_pad_new_millimetres);
    state.rear_pad_millimetres =
        clamp(state.rear_pad_millimetres,
              maintenance.rear_pad_wear_limit_millimetres,
              vehicle.rear_pad_new_millimetres);
    state.front_disc_millimetres =
        clamp(state.front_disc_millimetres,
              maintenance.front_disc_wear_limit_millimetres,
              vehicle.front_disc_new_millimetres);
    state.rear_disc_millimetres =
        clamp(state.rear_disc_millimetres,
              maintenance.rear_disc_wear_limit_millimetres,
              vehicle.rear_disc_new_millimetres);

    for (std::size_t i = 0; i < 4; ++i) {
        state.tire_tread_millimetres[i] =
            clamp(state.tire_tread_millimetres[i],
                  model.tire_tread_wear_limit_millimetres,
                  vehicle.tire_new_tread_millimetres);
        state.winter_tire_tread_millimetres[i] =
            clamp(state.winter_tire_tread_millimetres[i],
                  model.tire_tread_wear_limit_millimetres,
                  vehicle.tire_new_tread_millimetres);
        state.tire_pressure_bar[i] = clamp(state.tire_pressure_bar[i], 0.5, 5.0);
    }
    state.tire_age_years = clamp(state.tire_age_years, 0.0, 20.0);
    state.winter_tire_age_years = clamp(state.winter_tire_age_years, 0.0, 20.0);

    state.battery_state_of_health =
        clamp(state.battery_state_of_health, 0.0, 1.0);
    state.battery_age_years = clamp(state.battery_age_years, 0.0, 30.0);

    state.suspension_wear_index = clamp(state.suspension_wear_index, 0.0, 1.0);
    state.clutch_wear_index = clamp(state.clutch_wear_index, 0.0, 1.0);
    state.exhaust_corrosion_index = clamp(state.exhaust_corrosion_index, 0.0, 1.0);
    state.catalyst_degradation_index =
        clamp(state.catalyst_degradation_index, 0.0, 1.0);
    state.cooling_wear_index = clamp(state.cooling_wear_index, 0.0, 1.0);
    state.paint_damage_index = clamp(state.paint_damage_index, 0.0, 1.0);
    state.rust_index = clamp(state.rust_index, 0.0, 1.0);
    state.windshield_chip_count = std::max(0, state.windshield_chip_count);

    state.coolant_age_months = std::max(0.0, state.coolant_age_months);
    state.oil_change_age_months = std::max(0.0, state.oil_change_age_months);
    state.inspection_age_months = std::max(0.0, state.inspection_age_months);
    state.brake_fluid_age_months = std::max(0.0, state.brake_fluid_age_months);
    state.spark_plug_age_kilometres =
        std::max(0.0, state.spark_plug_age_kilometres);
    state.air_filter_age_kilometres =
        std::max(0.0, state.air_filter_age_kilometres);
    state.cabin_filter_age_kilometres =
        std::max(0.0, state.cabin_filter_age_kilometres);
    state.wiper_blade_age_months = std::max(0.0, state.wiper_blade_age_months);
    state.tire_rotation_age_kilometres =
        std::max(0.0, state.tire_rotation_age_kilometres);
}

ResolvedInputs resolve_inputs(const Config& config, std::uint64_t sample_seed,
                              bool sample_epistemic_uncertainty) {
    ResolvedInputs resolved;
    resolved.initial_state = config.current_state;
    resolved.frailty = 1.0;
    if (!sample_epistemic_uncertainty) {
        clip_current_state(resolved.initial_state, config);
        return resolved;
    }

    Rng rng(sample_seed);
    const auto& uncertainty = config.uncertainty;
    auto& state = resolved.initial_state;

    auto sample = [&](double nominal, double standard_deviation, double lo, double hi) {
        if (!(standard_deviation > 0.0)) {
            return nominal;
        }
        return rng.truncated_normal(nominal, standard_deviation, lo, hi);
    };

    const auto& model = config.model;
    const auto& vehicle = config.vehicle;
    const auto& maintenance = config.maintenance;

    state.engine_wear_equivalent_kilometres =
        sample(state.engine_wear_equivalent_kilometres,
               uncertainty.engine_wear_equivalent_kilometres, 0.0,
               model.engine_design_life_kilometres);
    state.oil_degradation =
        sample(state.oil_degradation, uncertainty.oil_degradation, 0.0, 1.0);
    state.oil_level_fraction = sample(state.oil_level_fraction,
                                      uncertainty.oil_level_fraction, 0.0, 1.0);
    state.turbo_wear_index =
        sample(state.turbo_wear_index, uncertainty.turbo_wear_index, 0.0, 1.0);

    state.front_pad_millimetres =
        sample(state.front_pad_millimetres,
               uncertainty.front_pad_millimetres,
               maintenance.front_pad_wear_limit_millimetres,
               vehicle.front_pad_new_millimetres);
    state.rear_pad_millimetres =
        sample(state.rear_pad_millimetres,
               uncertainty.rear_pad_millimetres,
               maintenance.rear_pad_wear_limit_millimetres,
               vehicle.rear_pad_new_millimetres);
    state.front_disc_millimetres =
        sample(state.front_disc_millimetres,
               uncertainty.front_disc_millimetres,
               maintenance.front_disc_wear_limit_millimetres,
               vehicle.front_disc_new_millimetres);
    state.rear_disc_millimetres =
        sample(state.rear_disc_millimetres,
               uncertainty.rear_disc_millimetres,
               maintenance.rear_disc_wear_limit_millimetres,
               vehicle.rear_disc_new_millimetres);

    for (std::size_t i = 0; i < 4; ++i) {
        state.tire_tread_millimetres[i] =
            sample(state.tire_tread_millimetres[i],
                   uncertainty.tire_tread_millimetres[i],
                   model.tire_tread_wear_limit_millimetres,
                   vehicle.tire_new_tread_millimetres);
        state.winter_tire_tread_millimetres[i] =
            sample(state.winter_tire_tread_millimetres[i],
                   uncertainty.winter_tire_tread_millimetres[i],
                   model.tire_tread_wear_limit_millimetres,
                   vehicle.tire_new_tread_millimetres);
        state.tire_pressure_bar[i] =
            sample(state.tire_pressure_bar[i], uncertainty.tire_pressure_bar[i], 0.5,
                   5.0);
    }
    state.tire_age_years =
        sample(state.tire_age_years, uncertainty.tire_age_years, 0.0, 20.0);
    state.winter_tire_age_years =
        sample(state.winter_tire_age_years, uncertainty.winter_tire_age_years, 0.0,
               20.0);

    state.battery_state_of_health =
        sample(state.battery_state_of_health, uncertainty.battery_state_of_health,
               0.0, 1.0);
    state.battery_age_years =
        sample(state.battery_age_years, uncertainty.battery_age_years, 0.0, 30.0);

    state.suspension_wear_index =
        sample(state.suspension_wear_index, uncertainty.suspension_wear_index, 0.0,
               1.0);
    state.clutch_wear_index =
        sample(state.clutch_wear_index, uncertainty.clutch_wear_index, 0.0, 1.0);
    state.exhaust_corrosion_index =
        sample(state.exhaust_corrosion_index, uncertainty.exhaust_corrosion_index,
               0.0, 1.0);
    state.catalyst_degradation_index =
        sample(state.catalyst_degradation_index,
               uncertainty.catalyst_degradation_index, 0.0, 1.0);
    state.cooling_wear_index =
        sample(state.cooling_wear_index, uncertainty.cooling_wear_index, 0.0, 1.0);
    state.paint_damage_index =
        sample(state.paint_damage_index, uncertainty.paint_damage_index, 0.0, 1.0);
    state.rust_index = sample(state.rust_index, uncertainty.rust_index, 0.0, 1.0);
    if (uncertainty.windshield_chip_count > 0.0) {
        const double draw = rng.truncated_normal(
            static_cast<double>(state.windshield_chip_count),
            uncertainty.windshield_chip_count, 0.0, 50.0);
        state.windshield_chip_count = static_cast<int>(std::lround(draw));
    }

    clip_current_state(state, config);

    if (config.reliability.frailty_sigma > 0.0) {
        resolved.frailty =
            std::exp(rng.normal(0.0, config.reliability.frailty_sigma));
    }
    return resolved;
}

SimulationState make_initial_state(const Config& config,
                                   const ResolvedInputs& inputs) {
    SimulationState state;
    const auto& source = inputs.initial_state;
    const auto& vehicle = config.vehicle;
    const auto& model = config.model;

    state.engine_wear_equivalent_kilometres =
        source.engine_wear_equivalent_kilometres;
    state.oil_degradation = source.oil_degradation;
    state.oil_level_fraction = source.oil_level_fraction;
    state.turbo_wear_index = source.turbo_wear_index;

    state.front_pad_millimetres = source.front_pad_millimetres;
    state.rear_pad_millimetres = source.rear_pad_millimetres;
    state.front_disc_millimetres = source.front_disc_millimetres;
    state.rear_disc_millimetres = source.rear_disc_millimetres;
    state.brake_fluid_age_months = source.brake_fluid_age_months;

    state.tire_sets[0].tread_millimetres = source.tire_tread_millimetres;
    state.tire_sets[0].age_years = source.tire_age_years;
    state.tire_sets[1].tread_millimetres = source.winter_tire_tread_millimetres;
    state.tire_sets[1].age_years = source.winter_tire_age_years;
    state.tire_pressure_bar = source.tire_pressure_bar;
    state.tire_wear_factor = {1.0, 1.0};
    state.winter_tires_enabled = config.user.winter_tires;

    const bool winter_season =
        config.user.winter_tires &&
        is_winter_tire_month(month_of(config.case_config.start_date), config);
    state.active_tire_set = winter_season ? 1 : 0;

    state.battery_age_years = source.battery_age_years;
    const double baseline_health =
        1.0 - model.battery_age_factor * std::sqrt(std::max(0.0, source.battery_age_years));
    state.battery_health_offset =
        source.battery_state_of_health - baseline_health;

    state.suspension_wear_index = source.suspension_wear_index;
    state.clutch_wear_index = source.clutch_wear_index;
    state.exhaust_corrosion_index = source.exhaust_corrosion_index;
    state.catalyst_degradation_index = source.catalyst_degradation_index;
    state.cooling_wear_index = source.cooling_wear_index;
    state.paint_damage_index = source.paint_damage_index;
    state.rust_index = source.rust_index;
    state.rust_started = source.rust_index > 0.0;
    state.windshield_chip_count = source.windshield_chip_count;
    state.coolant_age_months = source.coolant_age_months;

    state.last_oil_change_day =
        -source.oil_change_age_months * kDaysPerMonth;
    state.last_oil_change_km = vehicle.odometer_kilometres;
    state.last_inspection_day =
        -source.inspection_age_months * kDaysPerMonth;
    state.last_brake_fluid_day =
        -source.brake_fluid_age_months * kDaysPerMonth;
    state.last_coolant_day = -source.coolant_age_months * kDaysPerMonth;
    state.last_wiper_blade_day =
        -source.wiper_blade_age_months * kDaysPerMonth;
    state.last_spark_plug_km =
        vehicle.odometer_kilometres - source.spark_plug_age_kilometres;
    state.last_air_filter_km =
        vehicle.odometer_kilometres - source.air_filter_age_kilometres;
    state.last_cabin_filter_km =
        vehicle.odometer_kilometres - source.cabin_filter_age_kilometres;
    state.last_tire_rotation_km =
        vehicle.odometer_kilometres - source.tire_rotation_age_kilometres;

    state.odometer_kilometres = vehicle.odometer_kilometres;
    return state;
}

}  // namespace carwear
