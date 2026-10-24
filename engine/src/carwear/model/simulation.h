#pragma once

#include <cstdint>

#include "carwear/config/config.h"
#include "carwear/model/types.h"

namespace carwear {

// Runs the model once for one fully resolved case. The model never samples
// epistemic uncertainty: that belongs to the harness and is delivered through
// the resolved inputs. The run seed drives the independent weather, trip,
// failure, event and cost streams.
RunResult run_model(const Config& config, const ResolvedInputs& inputs,
                    std::uint64_t run_seed);

}  // namespace carwear
