#pragma once

#include <vector>

#include "carwear/config/config.h"
#include "carwear/core/rng.h"
#include "carwear/model/types.h"

namespace carwear {

// Information about the elapsed day that the failure sampler needs. Failures
// are resolved once per day, after maintenance, because a failure that happens
// mid-trip is attributed to the end of that day.
struct FailureDayContext {
    int cold_starts = 0;
    double minimum_temperature_celsius = 15.0;
};

struct FailureDayResult {
    int failures = 0;
    double downtime_days = 0.0;
};

// Samples the unplanned failure modes of one day: the competing engine risks,
// suspension, clutch, exhaust, cooling, the battery cold-cranking failure and
// windshield crack propagation. Repair costs are booked as unplanned repairs;
// towing and downtime are booked as consequential costs.
FailureDayResult sample_failures(const Config& config, SimulationState& state,
                                 double frailty, int day,
                                 const FailureDayContext& context, Rng& rng,
                                 Rng& cost_rng, std::vector<Event>& events,
                                 CostLedger& ledger);

}  // namespace carwear
