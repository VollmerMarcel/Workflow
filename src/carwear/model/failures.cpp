#include "carwear/model/failures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "carwear/core/math.h"
#include "carwear/model/physics.h"

namespace carwear {
namespace {

struct EngineModeChoice {
    const char* item;
    double weight;
    double base_cost_euros;
};

double repair_cost(const Config& config, double base_cost_euros, Rng& rng) {
    const double installed =
        installed_cost(base_cost_euros, config.user.part_quality,
                       config.economics.regional_price_index);
    return installed *
           rng.log_normal(1.0, config.reliability.failure_cost_lognormal_sigma);
}

void record_failure(std::vector<Event>& events, CostLedger& ledger, int day,
                    std::string item, double cost_euros) {
    Event event;
    event.day = day;
    event.kind = EventKind::failure;
    event.item = std::move(item);
    event.cost_euros = cost_euros;
    events.push_back(event);
    ledger.add(EventKind::failure, cost_euros, day);
}

void add_consequences(const Config& config, CostLedger& ledger, int day,
                      FailureDayResult& result) {
    const auto& economics = config.economics;
    const double towing =
        economics.towing_cost_euros * economics.regional_price_index;
    const double downtime_days = economics.downtime_days_per_failure;
    const double downtime_value = downtime_days * economics.downtime_value_euros_per_day;

    ledger.add(CostCategory::consequential, towing, day);
    ledger.add(CostCategory::consequential, downtime_value, day);
    result.downtime_days += downtime_days;
}

// Adjusts the mode weights with the current engine state: oil condition drives
// the bearing and ring modes, turbocharger heat drives the turbo mode.
std::array<EngineModeChoice, 6> engine_modes(const Config& config,
                                             const SimulationState& state) {
    const auto& reliability = config.reliability;
    const auto& costs = config.costs;

    const double oil = clamp(state.oil_degradation, 0.0, 1.0);

    std::array<EngineModeChoice, 6> modes{{
        {"engine_rings_and_bore",
         reliability.engine_mode_weight_rings * (1.0 + 2.0 * oil),
         costs.engine_repair_rings_euros},
        {"engine_bearings",
         reliability.engine_mode_weight_bearings * (1.0 + 2.0 * oil),
         costs.engine_repair_bearings_euros},
        {"engine_turbocharger",
         config.vehicle.turbocharged
             ? reliability.engine_mode_weight_turbocharger *
                   (1.0 + 5.0 * clamp(state.turbo_wear_index, 0.0, 1.0))
             : 0.0,
         costs.engine_repair_turbocharger_euros},
        {"engine_seals",
         reliability.engine_mode_weight_seals * (1.0 + 0.5 * oil),
         costs.engine_repair_seals_euros},
        {"engine_ignition",
         reliability.engine_mode_weight_ignition,
         costs.engine_repair_ignition_euros},
        {"engine_timing",
         reliability.engine_mode_weight_timing * (1.0 + oil),
         costs.engine_repair_timing_euros},
    }};

    // A naturally aspirated engine has no turbocharger mode, and that must be
    // the case even if the configured weight is non-zero.
    if (!config.vehicle.turbocharged) {
        modes[2].weight = 0.0;
    }
    return modes;
}

std::size_t pick_weighted(const std::array<EngineModeChoice, 6>& modes,
                          Rng& rng) {
    double total = 0.0;
    for (const auto& mode : modes) {
        total += std::max(0.0, mode.weight);
    }
    if (!(total > 0.0)) {
        return 0;
    }
    double draw = rng.uniform() * total;
    for (std::size_t i = 0; i < modes.size(); ++i) {
        draw -= std::max(0.0, modes[i].weight);
        if (draw <= 0.0) {
            return i;
        }
    }
    return modes.size() - 1;
}

void apply_engine_repair(SimulationState& state, std::size_t mode) {
    ++state.engine_repair_count;
    switch (mode) {
        case 0:  // rings and bore
            state.engine_wear_equivalent_kilometres = 0.0;
            break;
        case 1:  // bearings
            state.engine_wear_equivalent_kilometres = 0.0;
            state.oil_degradation = 0.0;
            state.oil_level_fraction = 1.0;
            break;
        case 2:  // turbocharger
            state.turbo_wear_index = 0.0;
            break;
        case 3:  // seals and gaskets
            state.oil_level_fraction = 1.0;
            break;
        case 4:  // ignition system
            state.last_spark_plug_km = state.odometer_kilometres;
            break;
        case 5:  // timing chain or belt
            break;
        default:
            break;
    }
}

}  // namespace

FailureDayResult sample_failures(const Config& config, SimulationState& state,
                                 double frailty, int day,
                                 const FailureDayContext& context, Rng& rng,
                                 Rng& cost_rng, std::vector<Event>& events,
                                 CostLedger& ledger) {
    FailureDayResult result;
    const auto& reliability = config.reliability;
    const auto& model = config.model;
    const auto& costs = config.costs;

    // --- Engine: one competing-risk draw per day ---------------------------
    if (!state.engine_failed) {
        const double wear_index =
            clamp(state.engine_wear_equivalent_kilometres /
                       model.engine_design_life_kilometres,
                   0.0, 2.0);
        const double starvation =
            state.oil_level_fraction < model.oil_level_critical_fraction
                ? model.oil_starvation_hazard_multiplier
                : 1.0;
        const double probability = daily_failure_probability(
            config, reliability.engine_annual_failure_rate, wear_index,
            reliability.engine_wear_hazard_multiplier, frailty, starvation);
        if (rng.bernoulli(probability)) {
            const auto modes = engine_modes(config, state);
            const std::size_t mode = pick_weighted(modes, rng);
            const double cost =
                repair_cost(config, modes[mode].base_cost_euros, cost_rng);
            record_failure(events, ledger, day, modes[mode].item, cost);
            apply_engine_repair(state, mode);
            add_consequences(config, ledger, day, result);
            state.engine_failed = true;
            ++result.failures;
        }
    }

    // --- Lumped component hazards ------------------------------------------
    enum class ComponentKind { suspension, clutch, exhaust, cooling };

    struct ComponentHazard {
        ComponentKind kind;
        const char* item;
        double annual_rate;
        double wear_hazard_multiplier;
        double wear_index;
        double base_cost_euros;
    };

    const std::array<ComponentHazard, 4> components{{
        {ComponentKind::suspension, "suspension",
         reliability.suspension_annual_failure_rate,
         reliability.suspension_wear_hazard_multiplier,
         state.suspension_wear_index, costs.suspension_euros},
        {ComponentKind::clutch, "clutch_or_transmission",
         reliability.clutch_annual_failure_rate,
         reliability.clutch_wear_hazard_multiplier, state.clutch_wear_index,
         costs.clutch_or_transmission_euros},
        {ComponentKind::exhaust, "exhaust",
         reliability.exhaust_annual_failure_rate,
         reliability.exhaust_wear_hazard_multiplier,
         state.exhaust_corrosion_index, costs.exhaust_euros},
        {ComponentKind::cooling, "cooling_system",
         reliability.cooling_annual_failure_rate,
         reliability.cooling_wear_hazard_multiplier,
         state.cooling_wear_index, costs.cooling_euros},
    }};

    for (const auto& component : components) {
        bool* failed = nullptr;
        switch (component.kind) {
            case ComponentKind::suspension:
                failed = &state.suspension_failed;
                break;
            case ComponentKind::clutch:
                failed = &state.clutch_failed;
                break;
            case ComponentKind::exhaust:
                failed = &state.exhaust_failed;
                break;
            case ComponentKind::cooling:
                failed = &state.cooling_failed;
                break;
        }
        if (*failed) {
            continue;
        }

        const double probability = daily_failure_probability(
            config, component.annual_rate, component.wear_index,
            component.wear_hazard_multiplier, frailty);
        if (!rng.bernoulli(probability)) {
            continue;
        }

        const double cost = repair_cost(config, component.base_cost_euros, cost_rng);
        record_failure(events, ledger, day, component.item, cost);
        add_consequences(config, ledger, day, result);
        *failed = true;
        ++result.failures;

        switch (component.kind) {
            case ComponentKind::suspension:
                state.suspension_wear_index = 0.0;
                break;
            case ComponentKind::clutch:
                state.clutch_wear_index = 0.0;
                break;
            case ComponentKind::exhaust:
                state.exhaust_corrosion_index = 0.0;
                break;
            case ComponentKind::cooling:
                state.cooling_wear_index = 0.0;
                break;
        }
    }

    // --- Battery cold-cranking failure -------------------------------------
    if (context.cold_starts > 0) {
        const double health = battery_state_of_health(config, state);
        const double required =
            required_battery_health(config, context.minimum_temperature_celsius);
        const double per_start =
            battery_no_start_probability(config, required, health);
        const double per_day =
            1.0 - std::pow(1.0 - clamp(per_start, 0.0, 1.0),
                           static_cast<double>(context.cold_starts));
        if (rng.bernoulli(per_day)) {
            const double cost = repair_cost(config, costs.battery_euros, cost_rng);
            record_failure(events, ledger, day, "battery", cost);
            add_consequences(config, ledger, day, result);
            ++result.failures;

            state.battery_age_years = 0.0;
            state.battery_degree_days_above_25 = 0.0;
            state.battery_start_count = 0.0;
            state.battery_deep_discharge_count = 0.0;
            state.battery_health_offset = 0.0;
            ++state.battery_replacement_count;
        }
    }

    // --- Windshield: chip propagation to a crack ---------------------------
    if (state.windshield_chip_count > 0 &&
        context.minimum_temperature_celsius <=
            model.windshield_crack_temperature_threshold_celsius) {
        const double per_chip = clamp(
            model.windshield_crack_probability_per_chip_per_cold_day, 0.0, 1.0);
        const double probability =
            1.0 - std::pow(1.0 - per_chip,
                           static_cast<double>(state.windshield_chip_count));
        if (rng.bernoulli(probability)) {
            const double cost = repair_cost(config, costs.windshield_euros, cost_rng);
            record_failure(events, ledger, day, "windshield", cost);
            ++result.failures;
            state.windshield_chip_count = 0;
            ++state.windshield_replacement_count;
            // A cracked windshield is a repair, not a breakdown: no towing and
            // no downtime.
        }
    }

    return result;
}

}  // namespace carwear
