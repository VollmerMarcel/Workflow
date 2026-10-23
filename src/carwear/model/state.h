#pragma once

#include <cstdint>

#include "carwear/config/config.h"
#include "carwear/model/types.h"

namespace carwear {

// Build the mutable simulation state from a resolved current state.
SimulationState make_initial_state(const Config& config, const ResolvedInputs& inputs);

// Sample every uncertain current-state entry and the car-level frailty factor.
// With sampling disabled the nominal state is returned and frailty is one.
ResolvedInputs resolve_inputs(const Config& config, std::uint64_t sample_seed,
                              bool sample_epistemic_uncertainty);

// Clip a current-state configuration to physically admissible ranges. Used
// after sampling and exposed for tests.
void clip_current_state(CurrentStateConfig& state, const Config& config);

// True when the calendar month falls in the winter-tire season defined by the
// configured swap months.
bool is_winter_tire_month(int calendar_month, const Config& config);

}  // namespace carwear
