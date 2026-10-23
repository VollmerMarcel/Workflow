#pragma once

#include <cstdint>
#include <vector>

#include "carwear/config/config.h"
#include "carwear/harness/statistics.h"
#include "carwear/model/types.h"

namespace carwear {

struct MonteCarloResult {
    Aggregate aggregate;
    std::vector<RunRecord> records;
};

// Runs the model many times. Each sampled case resolves the latent current
// state and frailty once and then runs `runs_per_sample` times with different
// run seeds, which makes the aleatory/epistemic variance decomposition
// possible when at least two runs share a sample.
MonteCarloResult run_monte_carlo(const Config& config, int number_of_runs,
                                 std::uint64_t master_seed,
                                 bool sample_epistemic_uncertainty,
                                 int runs_per_sample);

}  // namespace carwear
