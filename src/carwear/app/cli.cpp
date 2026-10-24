#include "carwear/app/cli.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "carwear/config/loader.h"
#include "carwear/config/serialize.h"
#include "carwear/core/rng.h"
#include "carwear/harness/monte_carlo.h"
#include "carwear/harness/statistics.h"
#include "carwear/model/simulation.h"
#include "carwear/model/state.h"
#include "carwear/output/aggregate_writer.h"
#include "carwear/output/run_writer.h"

namespace carwear {
namespace {

constexpr std::string_view kVersion = "0.1.0";
constexpr int kMaxRuns = 1000000;

void print_usage(std::ostream& out) {
    out << "Car Wear Prognosis Engine " << kVersion << "\n\n"
        << "Usage:\n"
        << "  carwear run <case.toml> [--output DIR] [--seed N] [--trips]\n"
        << "      Run the model once and write run.toml plus run tables.\n\n"
        << "  carwear monte-carlo <case.toml> [--output DIR] [--runs N] [--seed N]\n"
        << "                              [--runs-per-sample N] [--nominal]\n"
        << "      Run the harness and write aggregate.toml; runs.csv when enabled.\n\n"
        << "  carwear config <case.toml>\n"
        << "      Load a case and print the fully resolved configuration.\n\n"
        << "  carwear zones\n"
        << "      List the built-in climate zones.\n\n"
        << "  carwear --version\n"
        << "  carwear --help\n";
}

void print_diagnostics(const Diagnostics& diagnostics) {
    for (const Diagnostic& item : diagnostics.items()) {
        std::cerr << (item.severity == Severity::error ? "error: " : "warning: ")
                  << item.message << '\n';
    }
}

std::optional<std::uint64_t> parse_u64(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() ||
        value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> parse_int(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

struct Options {
    std::string command;
    std::string config_path;
    std::optional<std::string> output;
    std::optional<std::uint64_t> seed;
    std::optional<int> runs;
    std::optional<int> runs_per_sample;
    bool trips = false;
    bool nominal = false;
};

std::optional<Options> parse_options(int argc, char** argv, int& exit_code) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(std::cout);
            exit_code = 0;
            return std::nullopt;
        }
        if (argument == "--version") {
            std::cout << "carwear " << kVersion << '\n';
            exit_code = 0;
            return std::nullopt;
        }
        if (argument == "--trips") {
            options.trips = true;
            continue;
        }
        if (argument == "--nominal") {
            options.nominal = true;
            continue;
        }
        if (argument == "--output" || argument == "-o") {
            if (index + 1 >= argc) {
                std::cerr << "error: --output requires a directory\n";
                exit_code = 2;
                return std::nullopt;
            }
            options.output = std::string{argv[++index]};
            continue;
        }
        if (argument == "--seed") {
            if (index + 1 >= argc) {
                std::cerr << "error: --seed requires a non-negative integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            const auto value = parse_u64(argv[++index]);
            if (!value.has_value()) {
                std::cerr << "error: --seed requires a non-negative integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            options.seed = *value;
            continue;
        }
        if (argument == "--runs") {
            if (index + 1 >= argc) {
                std::cerr << "error: --runs requires a positive integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            const auto value = parse_int(argv[++index]);
            if (!value.has_value() || *value <= 0) {
                std::cerr << "error: --runs requires a positive integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            options.runs = *value;
            continue;
        }
        if (argument == "--runs-per-sample") {
            if (index + 1 >= argc) {
                std::cerr << "error: --runs-per-sample requires a positive integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            const auto value = parse_int(argv[++index]);
            if (!value.has_value() || *value <= 0) {
                std::cerr << "error: --runs-per-sample requires a positive integer\n";
                exit_code = 2;
                return std::nullopt;
            }
            options.runs_per_sample = *value;
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "error: unknown option " << argument << '\n';
            exit_code = 2;
            return std::nullopt;
        }
        if (options.command.empty()) {
            options.command = std::string{argument};
        } else if (options.config_path.empty()) {
            options.config_path = std::string{argument};
        } else {
            std::cerr << "error: unexpected argument " << argument << '\n';
            exit_code = 2;
            return std::nullopt;
        }
    }

    if (options.command.empty()) {
        print_usage(std::cerr);
        exit_code = 2;
        return std::nullopt;
    }
    return options;
}

std::optional<Config> load_or_report(const std::string& path) {
    ConfigLoadResult loaded = load_config(path);
    print_diagnostics(loaded.diagnostics);
    if (!loaded.ok()) {
        if (!loaded.diagnostics.has_errors()) {
            std::cerr << "error: cannot load " << path << '\n';
        }
        return std::nullopt;
    }
    return std::move(loaded.config);
}

std::filesystem::path output_directory(const Config& config,
                                       const Options& options) {
    return options.output.has_value()
               ? std::filesystem::path{*options.output}
               : std::filesystem::path{config.output.directory};
}

int run_single(const Options& options) {
    if (options.config_path.empty()) {
        std::cerr << "error: run requires a case file\n";
        return 2;
    }
    auto loaded = load_or_report(options.config_path);
    if (!loaded.has_value()) {
        return 1;
    }
    Config config = std::move(*loaded);
    if (options.trips) {
        config.output.write_trips = true;
    }

    const std::uint64_t seed = options.seed.value_or(
        config.run.run_seed.value_or(1));
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, seed);

    const std::filesystem::path directory = output_directory(config, options);
    std::string error;
    if (!write_run_outputs(config, directory, result, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    std::cout << "run: case=" << result.case_name << " seed=" << result.run_seed
              << " distance_km=" << result.totals.distance_kilometres
              << " fuel_l=" << result.totals.fuel_litres
              << " total_cost_of_ownership_euros="
              << result.totals.total_cost_of_ownership_euros
              << " failures=" << result.totals.failure_count
              << " downtime_days=" << result.totals.downtime_days << '\n';
    std::cout << "wrote: " << directory.string() << '\n';
    return 0;
}

int run_monte_carlo_command(const Options& options) {
    if (options.config_path.empty()) {
        std::cerr << "error: monte-carlo requires a case file\n";
        return 2;
    }
    auto loaded = load_or_report(options.config_path);
    if (!loaded.has_value()) {
        return 1;
    }
    Config config = std::move(*loaded);

    const int runs = options.runs.value_or(config.monte_carlo.number_of_runs);
    const std::uint64_t master_seed =
        options.seed.value_or(config.monte_carlo.master_seed);
    const int runs_per_sample =
        options.runs_per_sample.value_or(config.monte_carlo.runs_per_sample);
    if (runs > kMaxRuns) {
        std::cerr << "error: --runs must not exceed " << kMaxRuns << '\n';
        return 2;
    }
    if (runs_per_sample > runs) {
        std::cerr << "error: --runs-per-sample must not exceed --runs\n";
        return 2;
    }
    const bool sample_epistemic =
        options.nominal ? false : config.monte_carlo.sample_epistemic_uncertainty;

    const MonteCarloResult result = run_monte_carlo(
        config, runs, master_seed, sample_epistemic, runs_per_sample);

    const std::filesystem::path directory = output_directory(config, options);
    std::string error;
    if (!write_aggregate_outputs(directory, result.aggregate, result.records,
                                 config.output.write_runs_csv, error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
    }

    const Aggregate& aggregate = result.aggregate;
    std::cout << "monte-carlo: runs=" << aggregate.number_of_runs
              << " master_seed=" << aggregate.master_seed
              << " mean_total_cost_of_ownership_euros="
              << aggregate.total_cost_of_ownership_euros.mean
              << " p50_total_cost_of_ownership_euros="
              << aggregate.total_cost_of_ownership_euros.p50
              << " p95_total_cost_of_ownership_euros="
              << aggregate.total_cost_of_ownership_euros.p95 << '\n';
    if (aggregate.budget_defined) {
        std::cout << "probability_exceeding_budget="
                  << aggregate.probability_exceeding_budget << '\n';
    }
    if (aggregate.variance.available) {
        std::cout << "variance: aleatory="
                  << aggregate.variance.aleatory_fraction
                  << " epistemic=" << aggregate.variance.epistemic_fraction
                  << '\n';
    }
    std::cout << "wrote: " << directory.string() << '\n';
    return 0;
}

bool has_run_options(const Options& options) {
    return options.output.has_value() || options.seed.has_value() ||
           options.runs.has_value() || options.runs_per_sample.has_value() ||
           options.trips || options.nominal;
}

int print_config(const Options& options) {
    if (options.config_path.empty()) {
        std::cerr << "error: config requires a case file\n";
        return 2;
    }
    if (has_run_options(options)) {
        std::cerr << "error: config accepts only a case file\n";
        return 2;
    }
    auto loaded = load_or_report(options.config_path);
    if (!loaded.has_value()) {
        return 1;
    }
    std::cout << config_to_toml_string(*loaded);
    return 0;
}

int list_zones(const Options& options) {
    if (!options.config_path.empty() || has_run_options(options)) {
        std::cerr << "error: zones accepts no arguments or options\n";
        return 2;
    }
    for (const std::string& zone : known_climate_zones()) {
        std::cout << zone << '\n';
    }
    return 0;
}

}  // namespace

int run_cli(int argc, char** argv) {
    int exit_code = 0;
    const auto options = parse_options(argc, argv, exit_code);
    if (!options.has_value()) {
        return exit_code;
    }
    if (options->command == "run") {
        if (options->runs.has_value() || options->runs_per_sample.has_value() ||
            options->nominal) {
            std::cerr << "error: run is a single-run command; --runs, "
                         "--runs-per-sample and --nominal belong to monte-carlo\n";
            return 2;
        }
        return run_single(*options);
    }
    if (options->command == "monte-carlo" || options->command == "mc") {
        if (options->trips) {
            std::cerr << "error: --trips belongs to run, not monte-carlo\n";
            return 2;
        }
        return run_monte_carlo_command(*options);
    }
    if (options->command == "config") {
        return print_config(*options);
    }
    if (options->command == "zones") {
        return list_zones(*options);
    }
    std::cerr << "error: unknown command " << options->command << '\n';
    print_usage(std::cerr);
    return 2;
}

}  // namespace carwear
