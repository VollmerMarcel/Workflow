#pragma once

#include <vector>

#include "carwear/config/config.h"
#include "carwear/core/date.h"
#include "carwear/core/rng.h"
#include "carwear/model/types.h"

namespace carwear {

// Changes the active tire set on the configured mount months. Called before
// the trips of the day, as required by the event ordering.
void apply_seasonal_tire_swap(const Config& config, SimulationState& state,
                              int day, const Date& date,
                              std::vector<Event>& events, CostLedger& ledger);

// Scheduled services, inspections, seasonal swaps and the resulting costs.
void apply_scheduled_maintenance(const Config& config, SimulationState& state,
                                 int day, const Date& date, Rng& rng,
                                 std::vector<Event>& events, CostLedger& ledger);

// Replacements triggered by wear thresholds and by paint damage.
void apply_threshold_replacements(const Config& config, SimulationState& state,
                                  int day, std::vector<Event>& events,
                                  CostLedger& ledger);

}  // namespace carwear
