#include "carwear/harness/monte_carlo.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "carwear/core/rng.h"
#include "carwear/model/simulation.h"
#include "carwear/model/state.h"

namespace carwear {
namespace {

double sample_variance(const std::vector<double>& values, double mean) {
    if (values.size() < 2) {
        return 0.0;
    }
    double sum = 0.0;
    for (double value : values) {
        sum += (value - mean) * (value - mean);
    }
    return sum / static_cast<double>(values.size() - 1);
}

VarianceDecomposition decompose_variance(
    const std::vector<std::vector<double>>& groups, int runs_per_sample,
    bool sample_epistemic_uncertainty) {
    VarianceDecomposition variance;
    variance.runs_per_sample = runs_per_sample;
    variance.samples = static_cast<int>(groups.size());
    if (!sample_epistemic_uncertainty || runs_per_sample < 2 ||
        groups.size() < 2) {
        return variance;
    }

    std::vector<double> group_means;
    std::vector<double> within_variances;
    std::vector<double> group_sizes;
    group_means.reserve(groups.size());
    within_variances.reserve(groups.size());
    group_sizes.reserve(groups.size());

    for (const auto& group : groups) {
        if (group.empty()) {
            continue;
        }
        const double mean =
            std::accumulate(group.begin(), group.end(), 0.0) /
            static_cast<double>(group.size());
        group_means.push_back(mean);
        within_variances.push_back(sample_variance(group, mean));
        group_sizes.push_back(static_cast<double>(group.size()));
    }

    if (group_means.size() < 2) {
        return variance;
    }

    const double group_mean =
        std::accumulate(group_means.begin(), group_means.end(), 0.0) /
        static_cast<double>(group_means.size());
    const double between_variance =
        sample_variance(group_means, group_mean);

    double within_variance = 0.0;
    double total_weight = 0.0;
    for (std::size_t i = 0; i < within_variances.size(); ++i) {
        within_variance += within_variances[i] * group_sizes[i];
        total_weight += group_sizes[i];
    }
    within_variance = total_weight > 0.0 ? within_variance / total_weight : 0.0;

    const double sampling_variance_of_mean =
        runs_per_sample > 0 ? within_variance / static_cast<double>(runs_per_sample)
                            : 0.0;
    const double epistemic_variance =
        std::max(0.0, between_variance - sampling_variance_of_mean);
    const double total_variance = within_variance + epistemic_variance;

    variance.available = total_variance > 0.0;
    if (variance.available) {
        variance.aleatory_fraction = within_variance / total_variance;
        variance.epistemic_fraction = epistemic_variance / total_variance;
    }
    return variance;
}

}  // namespace

MonteCarloResult run_monte_carlo(const Config& config, int number_of_runs,
                                 std::uint64_t master_seed,
                                 bool sample_epistemic_uncertainty,
                                 int runs_per_sample) {
    MonteCarloResult result;
    const int total_runs = std::max(1, number_of_runs);
    const int group_size = std::max(1, runs_per_sample);

    result.records.reserve(static_cast<std::size_t>(total_runs));
    std::vector<std::vector<double>> groups;

    int run_index = 0;
    int sample_index = 0;
    while (run_index < total_runs) {
        const std::uint64_t sample_seed =
            derive_seed(master_seed, static_cast<std::uint64_t>(sample_index),
                        "resolve");
        const ResolvedInputs resolved = resolve_inputs(
            config, sample_seed, sample_epistemic_uncertainty);

        std::vector<double> group_costs;
        const int group_end = std::min(total_runs, run_index + group_size);
        for (; run_index < group_end; ++run_index) {
            const std::uint64_t run_seed = derive_seed(
                master_seed, static_cast<std::uint64_t>(run_index), "run");
            const RunResult run = run_model(config, resolved, run_seed);
            result.records.push_back(make_run_record(run_index, run));
            group_costs.push_back(run.totals.total_cost_of_ownership_euros);
        }
        groups.push_back(std::move(group_costs));
        ++sample_index;
    }

    const VarianceDecomposition variance =
        decompose_variance(groups, group_size, sample_epistemic_uncertainty);
    result.aggregate = aggregate_records(config, result.records, variance);
    result.aggregate.master_seed = master_seed;
    return result;
}

}  // namespace carwear
