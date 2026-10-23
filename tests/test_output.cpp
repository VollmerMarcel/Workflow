#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <toml++/toml.hpp>

#include "carwear/harness/statistics.h"
#include "carwear/model/simulation.h"
#include "carwear/model/state.h"
#include "carwear/output/aggregate_writer.h"
#include "carwear/output/csv.h"
#include "carwear/output/run_writer.h"
#include "test_support.h"

namespace carwear {
namespace {

Config short_config() {
    const ConfigLoadResult loaded = test::load_case();
    Config config = *loaded.config;
    config.case_config.horizon_days = 30;
    return config;
}

class TempDirectory {
public:
    TempDirectory() {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("carwear-test-" + std::to_string(stamp));
    }
    ~TempDirectory() {
        std::error_code code;
        std::filesystem::remove_all(path_, code);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

TEST(OutputTest, CsvFieldsAreEscaped) {
    EXPECT_EQ(csv_field("plain"), "plain");
    EXPECT_EQ(csv_field("with,comma"), "\"with,comma\"");
    EXPECT_EQ(csv_field("with\"quote"), "\"with\"\"quote\"");
    EXPECT_EQ(csv_field("with\nnewline"), "\"with\nnewline\"");
}

TEST(OutputTest, NumbersUseStableFormatting) {
    EXPECT_EQ(format_number(1.5), "1.5");
    EXPECT_EQ(format_number(0.0), "0");
}

TEST(OutputTest, RunTomlIsValidAndContainsContractFields) {
    const Config config = short_config();
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, 42);

    const std::string text = run_to_toml_string(result);
    toml::table parsed = toml::parse(text);

    EXPECT_EQ(parsed["schema_version"].value<std::string>(), "1.0");
    const toml::table* run = parsed["run"].as_table();
    ASSERT_NE(run, nullptr);
    EXPECT_EQ(run->get("case_name")->value<std::string>(), "unit-test");
    EXPECT_EQ(run->get("horizon_days")->value<std::int64_t>(), 30);
    EXPECT_EQ(run->get("run_seed")->value<std::int64_t>(), 42);

    const toml::table* totals = run->get("totals")->as_table();
    ASSERT_NE(totals, nullptr);
    EXPECT_TRUE(totals->contains("distance_kilometres"));
    EXPECT_TRUE(totals->contains("total_cost_of_ownership_euros"));

    const toml::table* breakdown = run->get("cost_breakdown")->as_table();
    ASSERT_NE(breakdown, nullptr);
    EXPECT_TRUE(breakdown->contains("fuel_euros"));
    EXPECT_TRUE(breakdown->contains("consequential_euros"));

    const toml::table* final_state = run->get("final_state")->as_table();
    ASSERT_NE(final_state, nullptr);
    EXPECT_TRUE(final_state->contains("battery_state_of_health"));
}

TEST(OutputTest, RunWriterCreatesFiles) {
    const Config config = short_config();
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, 8);

    TempDirectory directory;
    std::string error;
    ASSERT_TRUE(write_run_outputs(config, directory.path(), result, error))
        << error;

    EXPECT_TRUE(std::filesystem::exists(directory.path() / "run.toml"));
    EXPECT_TRUE(std::filesystem::exists(directory.path() / "run_events.csv"));
    EXPECT_TRUE(std::filesystem::exists(directory.path() / "run_monthly.csv"));

    const std::string events = read_file(directory.path() / "run_events.csv");
    EXPECT_NE(events.find("day,kind,item,cost_euros"), std::string::npos);

    const std::string monthly = read_file(directory.path() / "run_monthly.csv");
    EXPECT_NE(monthly.find("month,start_day,end_day"), std::string::npos);
}

TEST(OutputTest, AggregateTomlIsValidAndContainsPrognoses) {
    const Config config = short_config();
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, 13);
    const RunRecord record = make_run_record(0, result);
    const Aggregate aggregate = aggregate_records(config, {record});

    const std::string text = aggregate_to_toml_string(aggregate);
    toml::table parsed = toml::parse(text);

    EXPECT_EQ(parsed["schema_version"].value<std::string>(), "1.0");
    const toml::table* table = parsed["aggregate"].as_table();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->get("number_of_runs")->value<std::int64_t>(), 1);
    EXPECT_TRUE(table->contains("total_cost_of_ownership_euros"));
    EXPECT_TRUE(table->contains("part_prognoses"));
    EXPECT_TRUE(table->contains("failures"));
    EXPECT_TRUE(table->contains("variance_decomposition"));
}

TEST(OutputTest, AggregateWriterCreatesFiles) {
    const Config config = short_config();
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, 21);
    const std::vector<RunRecord> records{make_run_record(0, result)};
    const Aggregate aggregate = aggregate_records(config, records);

    TempDirectory directory;
    std::string error;
    ASSERT_TRUE(write_aggregate_outputs(directory.path(), aggregate, records,
                                        true, error))
        << error;

    EXPECT_TRUE(std::filesystem::exists(directory.path() / "aggregate.toml"));
    EXPECT_TRUE(std::filesystem::exists(directory.path() / "runs.csv"));

    const std::string runs = read_file(directory.path() / "runs.csv");
    EXPECT_NE(runs.find("run_index,run_seed"), std::string::npos);
    EXPECT_NE(runs.find("total_cost_of_ownership_euros"),
              std::string::npos);
}

TEST(OutputTest, AggregateWriterHonoursWriteRunsCsv) {
    const Config config = short_config();
    const ResolvedInputs inputs = resolve_inputs(config, 0, false);
    const RunResult result = run_model(config, inputs, 22);
    const std::vector<RunRecord> records{make_run_record(0, result)};
    const Aggregate aggregate = aggregate_records(config, records);

    TempDirectory directory;
    std::string error;
    ASSERT_TRUE(write_aggregate_outputs(directory.path(), aggregate, records,
                                        false, error))
        << error;

    EXPECT_TRUE(std::filesystem::exists(directory.path() / "aggregate.toml"));
    EXPECT_FALSE(std::filesystem::exists(directory.path() / "runs.csv"));
}

}  // namespace
}  // namespace carwear
