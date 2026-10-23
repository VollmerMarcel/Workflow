#pragma once

#include "carwear/config/config.h"
#include "carwear/core/rng.h"
#include "carwear/model/types.h"

namespace carwear {

// Per-trip updates. Each function depends only on the resolved configuration,
// the current state, and the trip's sufficient statistics.
double update_engine_trip(const Config& config, SimulationState& state,
                          const Drive& drive, const Weather& weather);

void update_brakes_trip(const Config& config, SimulationState& state,
                        const Drive& drive, const Weather& weather);

void update_tires_trip(const Config& config, SimulationState& state,
                       const Drive& drive, const Weather& weather);

void update_suspension_trip(const Config& config, SimulationState& state,
                            const Drive& drive);

void update_clutch_trip(const Config& config, SimulationState& state,
                        const Drive& drive);

void update_exhaust_and_catalyst_trip(const Config& config, SimulationState& state,
                                      const Drive& drive, double oil_burned_litres);

void update_cooling_trip(const Config& config, SimulationState& state,
                         const Drive& drive);

void update_paint_trip(const Config& config, SimulationState& state,
                       const Drive& drive, const Weather& weather, Rng& rng);

void update_windshield_trip(const Config& config, SimulationState& state,
                            const Drive& drive, const Weather& weather, Rng& rng);

// Calendar and weather-driven aging, applied once per day after the trips.
void apply_daily_aging(const Config& config, SimulationState& state,
                       const Weather& weather, bool short_trip_day, Rng& rng);

}  // namespace carwear
