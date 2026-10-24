#include "carwear/model/simulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "carwear/core/date.h"
#include "carwear/core/math.h"
#include "carwear/core/rng.h"
#include "carwear/model/components.h"
#include "carwear/model/failures.h"
#include "carwear/model/maintenance.h"
#include "carwear/model/physics.h"
#include "carwear/model/state.h"
#include "carwear/model/trips.h"
#include "carwear/model/weather.h"

namespace carwear {
namespace {

MonthSnapshot take_snapshot(const Config& config, const SimulationState& state,
                            int month, int start_day, int end_day,
                            double month_cost, double cumulative_cost,
                            double month_fuel_litres, int month_failures) {
    MonthSnapshot snapshot;
    snapshot.month = month;
    snapshot.start_day = start_day;
    snapshot.end_day = end_day;
    snapshot.cost_euros = month_cost;
    snapshot.cumulative_cost_euros = cumulative_cost;
    snapshot.fuel_litres = month_fuel_litres;
    snapshot.failures = month_failures;
    snapshot.front_pad_millimetres = state.front_pad_millimetres;
    snapshot.rear_pad_millimetres = state.rear_pad_millimetres;
    snapshot.front_disc_millimetres = state.front_disc_millimetres;
    snapshot.rear_disc_millimetres = state.rear_disc_millimetres;
    snapshot.tire_tread_millimetres =
        state.tire_sets[static_cast<std::size_t>(state.active_tire_set)]
            .tread_millimetres;
    snapshot.battery_state_of_health = battery_state_of_health(config, state);
    snapshot.exhaust_corrosion_index = state.exhaust_corrosion_index;
    snapshot.paint_damage_index = state.paint_damage_index;
    snapshot.odometer_kilometres = state.odometer_kilometres;
    return snapshot;
}

}  // namespace

RunResult run_model(const Config& config, const ResolvedInputs& inputs,
                    std::uint64_t run_seed) {
    RunResult result;
    result.case_name = config.case_config.name;
    result.start_date = config.case_config.start_date;
    result.horizon_days = std::max(1, config.case_config.horizon_days);
    result.run_seed = run_seed;
    result.include_paint_repair_in_total_cost_of_ownership =
        config.economics.include_paint_repair_in_total_cost_of_ownership;
    result.frailty = inputs.frailty;
    result.initial_state = inputs.initial_state;

    const int horizon = result.horizon_days;
    const Date start_date = config.case_config.start_date;

    Rng weather_rng(derive_seed(run_seed, "weather"));
    Rng trips_rng(derive_seed(run_seed, "trips"));
    Rng failures_rng(derive_seed(run_seed, "failures"));
    Rng events_rng(derive_seed(run_seed, "events"));
    Rng costs_rng(derive_seed(run_seed, "costs"));

    WeatherProcess weather_process(config, start_date, weather_rng);
    TripGenerator trip_generator(config, trips_rng);

    SimulationState state = make_initial_state(config, inputs);
    CostLedger ledger;
    ledger.reset(horizon);

    std::vector<Event> events;
    events.reserve(64);
    std::vector<MonthSnapshot> monthly;
    std::vector<TripRecord> trip_records;

    double fuel_litres_total = 0.0;
    int month_start_day = 1;
    double month_fuel_litres = 0.0;
    int month_failures = 0;
    int failure_count = 0;
    double downtime_days = 0.0;

    for (int day = 1; day <= horizon; ++day) {
        const Date date = add_days(start_date, day - 1);
        const Weather weather = weather_process.next_day();

        apply_seasonal_tire_swap(config, state, day, date, events, ledger);

        const std::vector<Drive> drives =
            trip_generator.generate(day, date, weather);

        bool short_trip_day = false;
        int cold_starts = 0;
        double minimum_temperature = weather.temperature_celsius;

        for (const Drive& drive : drives) {
            const double litres = fuel_litres(config, drive, state, weather);
            const double fuel_cost =
                litres * config.economics.fuel_price_euros_per_litre;
            fuel_litres_total += litres;
            month_fuel_litres += litres;
            ledger.add(CostCategory::fuel, fuel_cost, day);

            state.odometer_kilometres += drive.distance_kilometres;
            state.battery_start_count += 1.0;
            if (drive.cold_start) {
                ++cold_starts;
                if (drive.distance_kilometres <
                    config.model.battery_deep_discharge_trip_distance_kilometres) {
                    state.battery_deep_discharge_count += 1.0;
                }
            }

            if (drive.short_trip) {
                short_trip_day = true;
            }

            const double oil_burned = update_engine_trip(config, state, drive, weather);
            update_brakes_trip(config, state, drive, weather);
            update_tires_trip(config, state, drive, weather);
            update_clutch_trip(config, state, drive);
            update_suspension_trip(config, state, drive);
            update_exhaust_and_catalyst_trip(config, state, drive, oil_burned);
            update_cooling_trip(config, state, drive);
            update_paint_trip(config, state, drive, weather, events_rng);
            update_windshield_trip(config, state, drive, weather, events_rng);

            if (config.output.write_trips) {
                TripRecord record;
                record.day = day;
                record.purpose = drive.purpose;
                record.route_class = drive.route_class;
                record.distance_kilometres = drive.distance_kilometres;
                record.mean_speed_kph = drive.mean_speed_kph;
                record.cold_start = drive.cold_start;
                record.fuel_litres = litres;
                record.ambient_temperature_celsius =
                    weather.temperature_celsius;
                record.road_wet = weather.road_wet;
                record.snowy = weather.snowy;
                trip_records.push_back(record);
            }
        }

        apply_daily_aging(config, state, weather, short_trip_day, events_rng);

        apply_scheduled_maintenance(config, state, day, date, events_rng, events,
                                    ledger);
        apply_threshold_replacements(config, state, day, events, ledger);

        FailureDayContext failure_context;
        failure_context.cold_starts = cold_starts;
        failure_context.minimum_temperature_celsius = minimum_temperature;
        const FailureDayResult failure_result = sample_failures(
            config, state, inputs.frailty, day, failure_context, failures_rng,
            costs_rng, events, ledger);
        failure_count += failure_result.failures;
        month_failures += failure_result.failures;
        downtime_days += failure_result.downtime_days;

        const Date next_date = add_days(date, 1);
        const bool month_ends = month_of(next_date) != month_of(date);
        if (month_ends || day == horizon) {
            const double month_cost = ledger.sum_days(month_start_day, day);
            const double cumulative_cost = ledger.sum_days(0, day);
            monthly.push_back(take_snapshot(
                config, state, month_of(date), month_start_day, day,
                month_cost, cumulative_cost, month_fuel_litres,
                month_failures));
            month_start_day = day + 1;
            month_fuel_litres = 0.0;
            month_failures = 0;
        }
    }

    result.final_state = state;
    result.events = std::move(events);
    result.monthly = std::move(monthly);
    result.trips = std::move(trip_records);
    result.terminal_battery_state_of_health =
        battery_state_of_health(config, state);

    RunTotals& totals = result.totals;
    totals.distance_kilometres = state.odometer_kilometres -
                                 config.vehicle.odometer_kilometres;
    totals.fuel_litres = fuel_litres_total;
    totals.fuel_cost_euros = ledger.category(CostCategory::fuel);
    totals.scheduled_maintenance_euros =
        ledger.category(CostCategory::scheduled_maintenance);
    totals.wear_replacement_euros =
        ledger.category(CostCategory::wear_replacement);
    totals.unplanned_repair_euros =
        ledger.category(CostCategory::unplanned_repair);
    totals.consequential_euros =
        ledger.category(CostCategory::consequential);
    totals.paint_repair_euros = ledger.category(CostCategory::paint_repair);
    totals.total_cost_of_ownership_euros = ledger.total_cost_of_ownership(
        config.economics.include_paint_repair_in_total_cost_of_ownership);
    totals.depreciation_euros =
        totals.distance_kilometres *
            config.economics.depreciation_euros_per_kilometre +
        (static_cast<double>(horizon) / 365.0) *
            config.economics.depreciation_euros_per_year;
    totals.failure_count = failure_count;
    totals.downtime_days = downtime_days;

    return result;
}

}  // namespace carwear
