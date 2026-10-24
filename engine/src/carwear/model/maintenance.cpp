#include "carwear/model/maintenance.h"

#include <algorithm>
#include <cmath>

#include "carwear/core/math.h"
#include "carwear/model/physics.h"

namespace carwear {
namespace {

constexpr double kDaysPerMonth = 30.4375;

void record(std::vector<Event>& events, int day, EventKind kind, std::string item,
            double cost, CostLedger& ledger) {
    Event event;
    event.day = day;
    event.kind = kind;
    event.item = std::move(item);
    event.cost_euros = cost;
    events.push_back(event);
    ledger.add(kind, cost, day);
}

double service_cost(double base, const Config& config) {
    return base * config.economics.regional_price_index;
}

double part_cost(double base, const Config& config) {
    return installed_cost(base, config.user.part_quality,
                          config.economics.regional_price_index);
}

double wear_factor_for_replacement(const Config& config) {
    return 1.15 - 0.30 * clamp(config.user.part_quality, 0.0, 1.0);
}

bool due_by_day(double day, double last_day, double interval_months,
                double deferral_days) {
    return day - last_day >= interval_months * kDaysPerMonth + deferral_days;
}

bool due_by_distance(double odometer, double last_km, double interval_km,
                     double deferral_km) {
    return odometer - last_km >= interval_km + deferral_km;
}

void reset_tire_pressure(SimulationState& state, const Config& config) {
    state.tire_pressure_bar.fill(config.vehicle.tire_pressure_specification_bar);
}

void replace_exhaust(const Config& config, SimulationState& state, int day,
                     std::vector<Event>& events, CostLedger& ledger,
                     const char* item) {
    record(events, day, EventKind::wear_replacement, item,
           part_cost(config.costs.exhaust_euros, config), ledger);
    state.exhaust_corrosion_index = 0.0;
}

void replace_catalyst(const Config& config, SimulationState& state, int day,
                      std::vector<Event>& events, CostLedger& ledger) {
    record(events, day, EventKind::wear_replacement, "catalyst",
           part_cost(config.costs.catalyst_euros, config), ledger);
    state.catalyst_degradation_index = 0.0;
}

void replace_suspension(const Config& config, SimulationState& state, int day,
                        std::vector<Event>& events, CostLedger& ledger) {
    record(events, day, EventKind::wear_replacement, "suspension",
           part_cost(config.costs.suspension_euros, config), ledger);
    state.suspension_wear_index = 0.0;
}

void replace_clutch(const Config& config, SimulationState& state, int day,
                    std::vector<Event>& events, CostLedger& ledger) {
    record(events, day, EventKind::wear_replacement, "clutch_or_transmission",
           part_cost(config.costs.clutch_or_transmission_euros, config), ledger);
    state.clutch_wear_index = 0.0;
}

void replace_cooling(const Config& config, SimulationState& state, int day,
                     std::vector<Event>& events, CostLedger& ledger) {
    record(events, day, EventKind::wear_replacement, "cooling_system",
           part_cost(config.costs.cooling_euros, config), ledger);
    state.cooling_wear_index = 0.0;
}

}  // namespace

void apply_seasonal_tire_swap(const Config& config, SimulationState& state,
                              int day, const Date& date,
                              std::vector<Event>& events, CostLedger& ledger) {
    if (!state.winter_tires_enabled) {
        return;
    }
    const auto& maintenance = config.maintenance;
    const int calendar_month = month_of(date);
    const int year = year_of(date);
    const bool swap_month =
        calendar_month == maintenance.seasonal_tire_swap_autumn_month ||
        calendar_month == maintenance.seasonal_tire_swap_spring_month;
    const bool already_done = state.last_tire_swap_year == year &&
                              state.last_tire_swap_month == calendar_month;
    if (!swap_month || already_done) {
        return;
    }

    const int target_set =
        calendar_month == maintenance.seasonal_tire_swap_autumn_month ? 1 : 0;
    state.last_tire_swap_year = year;
    state.last_tire_swap_month = calendar_month;
    if (state.active_tire_set != target_set) {
        state.active_tire_set = target_set;
        record(events, day, EventKind::tire_swap, "seasonal_tire_swap",
               service_cost(config.costs.seasonal_tire_swap_euros, config),
               ledger);
    }
}

void apply_scheduled_maintenance(const Config& config, SimulationState& state,
                                 int day, const Date& date, Rng& rng,
                                 std::vector<Event>& events, CostLedger& ledger) {
    const auto& maintenance = config.maintenance;
    const auto& costs = config.costs;
    const double adherence = clamp(config.user.maintenance_adherence, 0.0, 1.0);
    const double deferral_days = (1.0 - adherence) * maintenance.max_deferral_days;
    const double deferral_km = (1.0 - adherence) * maintenance.max_deferral_kilometres;

    state.brake_fluid_age_months += 1.0 / kDaysPerMonth;

    if (due_by_day(day, state.last_oil_change_day,
                   maintenance.oil_change_interval_months, deferral_days) ||
        due_by_distance(state.odometer_kilometres, state.last_oil_change_km,
                        maintenance.oil_change_interval_kilometres, deferral_km)) {
        record(events, day, EventKind::scheduled_maintenance,
               "engine_oil_and_filter",
               service_cost(costs.engine_oil_and_filter_euros, config), ledger);
        state.oil_degradation = 0.0;
        state.oil_level_fraction = 1.0;
        state.last_oil_change_day = day;
        state.last_oil_change_km = state.odometer_kilometres;
        reset_tire_pressure(state, config);
    }

    if (due_by_day(day, state.last_inspection_day,
                   maintenance.inspection_interval_months, deferral_days)) {
        record(events, day, EventKind::inspection, "inspection",
               service_cost(costs.inspection_euros, config), ledger);
        state.last_inspection_day = day;
        reset_tire_pressure(state, config);

        const double detection =
            maintenance.inspection_detection_base_probability +
            (1.0 - maintenance.inspection_detection_base_probability) * adherence;
        const bool detected = rng.bernoulli(detection);

        if (detected &&
            state.suspension_wear_index >=
                config.model.suspension_replacement_wear_index) {
            replace_suspension(config, state, day, events, ledger);
        }
        if (detected &&
            state.clutch_wear_index >= config.model.clutch_replacement_wear_index) {
            replace_clutch(config, state, day, events, ledger);
        }
        if (detected && state.exhaust_corrosion_index >=
                            config.model.exhaust_replacement_wear_index) {
            replace_exhaust(config, state, day, events, ledger, "exhaust");
        }
        if (detected && state.cooling_wear_index >=
                            config.model.cooling_replacement_wear_index) {
            replace_cooling(config, state, day, events, ledger);
        }
    }

    if (due_by_day(day, state.last_brake_fluid_day,
                   maintenance.brake_fluid_interval_months, deferral_days)) {
        record(events, day, EventKind::scheduled_maintenance, "brake_fluid",
               service_cost(costs.brake_fluid_euros, config), ledger);
        state.last_brake_fluid_day = day;
        state.brake_fluid_age_months = 0.0;
    }

    if (due_by_day(day, state.last_coolant_day, maintenance.coolant_interval_months,
                   deferral_days)) {
        record(events, day, EventKind::scheduled_maintenance, "coolant",
               service_cost(costs.coolant_euros, config), ledger);
        state.last_coolant_day = day;
        state.coolant_age_months = 0.0;
    }

    if (due_by_distance(state.odometer_kilometres, state.last_spark_plug_km,
                        maintenance.spark_plug_interval_kilometres, deferral_km)) {
        record(events, day, EventKind::scheduled_maintenance, "spark_plugs",
               service_cost(costs.spark_plugs_euros, config), ledger);
        state.last_spark_plug_km = state.odometer_kilometres;
    }

    if (due_by_distance(state.odometer_kilometres, state.last_air_filter_km,
                        maintenance.air_filter_interval_kilometres, deferral_km)) {
        record(events, day, EventKind::scheduled_maintenance, "air_filter",
               service_cost(costs.air_filter_euros, config), ledger);
        state.last_air_filter_km = state.odometer_kilometres;
    }

    if (due_by_distance(state.odometer_kilometres, state.last_cabin_filter_km,
                        maintenance.cabin_filter_interval_kilometres, deferral_km)) {
        record(events, day, EventKind::scheduled_maintenance, "cabin_filter",
               service_cost(costs.cabin_filter_euros, config), ledger);
        state.last_cabin_filter_km = state.odometer_kilometres;
    }

    if (due_by_day(day, state.last_wiper_blade_day,
                   maintenance.wiper_blade_interval_months, deferral_days)) {
        record(events, day, EventKind::scheduled_maintenance, "wiper_blades",
               service_cost(costs.wiper_blades_euros, config), ledger);
        state.last_wiper_blade_day = day;
    }

    if (due_by_distance(state.odometer_kilometres, state.last_tire_rotation_km,
                        maintenance.tire_rotation_interval_kilometres, deferral_km)) {
        record(events, day, EventKind::scheduled_maintenance, "tire_rotation",
               service_cost(costs.tire_rotation_euros, config), ledger);
        auto& tire_set =
            state.tire_sets[static_cast<std::size_t>(state.active_tire_set)];
        double mean_tread = 0.0;
        for (double value : tire_set.tread_millimetres) {
            mean_tread += value;
        }
        mean_tread /= 4.0;
        tire_set.tread_millimetres.fill(mean_tread);
        state.last_tire_rotation_km = state.odometer_kilometres;
    }

    const int year = year_of(date);
    if (month_of(date) == maintenance.emissions_inspection_month &&
        state.last_emissions_inspection_year != year) {
        state.last_emissions_inspection_year = year;
        if (state.exhaust_corrosion_index >=
            config.model.exhaust_emissions_threshold) {
            replace_exhaust(config, state, day, events, ledger,
                            "exhaust_emissions_failure");
        }
        if (state.catalyst_degradation_index >=
            config.model.catalyst_emissions_threshold) {
            replace_catalyst(config, state, day, events, ledger);
        }
    }
}

void apply_threshold_replacements(const Config& config, SimulationState& state,
                                  int day, std::vector<Event>& events,
                                  CostLedger& ledger) {
    const auto& maintenance = config.maintenance;
    const auto& costs = config.costs;
    const double adherence = clamp(config.user.maintenance_adherence, 0.0, 1.0);

    const double front_pad_trigger =
        replacement_trigger(maintenance.front_pad_wear_limit_millimetres,
                            maintenance.front_pad_replacement_margin_millimetres,
                            adherence);
    const double rear_pad_trigger =
        replacement_trigger(maintenance.rear_pad_wear_limit_millimetres,
                            maintenance.rear_pad_replacement_margin_millimetres,
                            adherence);
    if (state.front_pad_millimetres <= front_pad_trigger) {
        record(events, day, EventKind::wear_replacement, "front_brake_pads",
               part_cost(costs.front_brake_pads_euros, config), ledger);
        state.front_pad_millimetres = config.vehicle.front_pad_new_millimetres;
        state.front_pad_wear_factor = wear_factor_for_replacement(config);
    }
    if (state.rear_pad_millimetres <= rear_pad_trigger) {
        record(events, day, EventKind::wear_replacement, "rear_brake_pads",
               part_cost(costs.rear_brake_pads_euros, config), ledger);
        state.rear_pad_millimetres = config.vehicle.rear_pad_new_millimetres;
        state.rear_pad_wear_factor = wear_factor_for_replacement(config);
    }

    const double front_disc_trigger =
        replacement_trigger(maintenance.front_disc_wear_limit_millimetres,
                            maintenance.front_disc_replacement_margin_millimetres,
                            adherence);
    const double rear_disc_trigger =
        replacement_trigger(maintenance.rear_disc_wear_limit_millimetres,
                            maintenance.rear_disc_replacement_margin_millimetres,
                            adherence);
    if (state.front_disc_millimetres <= front_disc_trigger) {
        record(events, day, EventKind::wear_replacement, "front_brake_discs",
               part_cost(costs.front_brake_discs_euros, config), ledger);
        state.front_disc_millimetres = config.vehicle.front_disc_new_millimetres;
        state.front_disc_wear_factor = wear_factor_for_replacement(config);
    }
    if (state.rear_disc_millimetres <= rear_disc_trigger) {
        record(events, day, EventKind::wear_replacement, "rear_brake_discs",
               part_cost(costs.rear_brake_discs_euros, config), ledger);
        state.rear_disc_millimetres = config.vehicle.rear_disc_new_millimetres;
        state.rear_disc_wear_factor = wear_factor_for_replacement(config);
    }

    {
        auto& tire_set =
            state.tire_sets[static_cast<std::size_t>(state.active_tire_set)];
        const double tread_trigger =
            replacement_trigger(config.model.tire_tread_wear_limit_millimetres,
                                maintenance.tire_replacement_margin_millimetres,
                                adherence);

        const bool aged = tire_set.age_years >= config.model.tire_age_limit_years;
        const bool front_worn = tire_set.tread_millimetres[0] <= tread_trigger ||
                                tire_set.tread_millimetres[1] <= tread_trigger;
        const bool rear_worn = tire_set.tread_millimetres[2] <= tread_trigger ||
                               tire_set.tread_millimetres[3] <= tread_trigger;

        if (aged) {
            const double cost =
                4.0 * part_cost(costs.tire_per_corner_euros, config);
            record(events, day, EventKind::wear_replacement, "tires_age_limit",
                   cost, ledger);
            tire_set.tread_millimetres.fill(config.vehicle.tire_new_tread_millimetres);
            tire_set.age_years = 0.0;
            state.tire_wear_factor[static_cast<std::size_t>(state.active_tire_set)] =
                wear_factor_for_replacement(config);
        } else if (front_worn || rear_worn) {
            double cost = 0.0;
            if (front_worn) {
                cost += 2.0 * part_cost(costs.tire_per_corner_euros, config);
                tire_set.tread_millimetres[0] = config.vehicle.tire_new_tread_millimetres;
                tire_set.tread_millimetres[1] = config.vehicle.tire_new_tread_millimetres;
            }
            if (rear_worn) {
                cost += 2.0 * part_cost(costs.tire_per_corner_euros, config);
                tire_set.tread_millimetres[2] = config.vehicle.tire_new_tread_millimetres;
                tire_set.tread_millimetres[3] = config.vehicle.tire_new_tread_millimetres;
            }
            record(events, day, EventKind::wear_replacement,
                   front_worn && rear_worn ? "tires_all" : (front_worn ? "tires_front"
                                                                       : "tires_rear"),
                   cost, ledger);
            state.tire_wear_factor[static_cast<std::size_t>(state.active_tire_set)] =
                wear_factor_for_replacement(config);
        }
    }

    if (state.paint_damage_index >= config.model.paint_repair_threshold) {
        record(events, day, EventKind::paint_repair, "paint_repair",
               part_cost(costs.paint_repair_euros, config), ledger);
        state.paint_damage_index = config.model.paint_repair_reset_index;
    }
}

}  // namespace carwear
