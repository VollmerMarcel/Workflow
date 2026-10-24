#pragma once

#include <array>

#include "carwear/config/config.h"
#include "carwear/model/types.h"

namespace carwear {

double cold_start_equivalent_distance(const Config& config, double temperature_celsius);

double stop_count(const Drive& drive);
double brake_energy_megajoules(const Config& config, const Drive& drive);
double acceleration_energy_megajoules(const Config& config, const Drive& drive);

double fuel_litres(const Config& config, const Drive& drive,
                   const SimulationState& state, const Weather& weather);

double battery_state_of_health(const Config& config, const SimulationState& state);
double required_battery_health(const Config& config, double temperature_celsius);
double battery_no_start_probability(const Config& config, double required_health,
                                    double state_of_health);

double effective_tire_pressure(double measured_bar, double ambient_celsius);
double mean_active_tire_pressure(const SimulationState& state,
                                 double ambient_celsius = 20.0);
std::array<double, 4> tire_corner_shares(const Config& config);

double daily_failure_probability(const Config& config, double annual_rate,
                                 double wear_index, double wear_hazard_multiplier,
                                 double frailty, double extra_multiplier = 1.0);

double replacement_trigger(double wear_limit, double margin, double adherence);

double installed_cost(double base_cost_euros, double part_quality,
                      double regional_price_index);

double monthly_weight_mean(const std::array<double, 12>& weights);

}  // namespace carwear
