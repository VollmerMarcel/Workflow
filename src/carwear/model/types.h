#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "carwear/config/config.h"
#include "carwear/core/date.h"

namespace carwear {

// ---------------------------------------------------------------------------
// Weather and trips
// ---------------------------------------------------------------------------

struct Weather {
    int day = 0;
    double temperature_celsius = 15.0;
    bool precipitation = false;
    bool snowy = false;
    bool road_wet = false;
    bool salt_day = false;
    bool hail = false;
    double uv_index = 0.0;
};

enum class TripPurpose { commute, errand, weekend_leisure, long_trip };
enum class RouteClass { city, rural, mixed, highway };

std::string_view to_string(TripPurpose purpose);
std::string_view to_string(RouteClass route_class);

struct Drive {
    TripPurpose purpose = TripPurpose::commute;
    RouteClass route_class = RouteClass::mixed;
    double distance_kilometres = 0.0;
    double mean_speed_kph = 0.0;
    double speed_variation_kph = 0.0;
    double stop_density_per_kilometre = 0.0;
    double brake_onset_speed_fraction = 0.7;
    double lateral_acceleration = 0.0;
    double positive_acceleration = 0.0;
    double idle_fraction = 0.0;
    bool cold_start = false;
    bool short_trip = false;
    int start_minute = 0;
    double duration_hours = 0.0;
};

// ---------------------------------------------------------------------------
// Events and money
// ---------------------------------------------------------------------------

enum class EventKind {
    scheduled_maintenance,
    wear_replacement,
    failure,
    inspection,
    tire_swap,
    paint_repair
};

std::string_view to_string(EventKind kind);

struct Event {
    int day = 0;
    EventKind kind = EventKind::scheduled_maintenance;
    std::string item;
    double cost_euros = 0.0;
};

enum class CostCategory {
    fuel,
    scheduled_maintenance,
    wear_replacement,
    unplanned_repair,
    consequential,
    paint_repair
};

CostCategory category_for(EventKind kind);

class CostLedger {
public:
    void reset(int horizon_days);

    void add(CostCategory category, double amount, int day);
    void add(EventKind kind, double amount, int day) {
        add(category_for(kind), amount, day);
    }

    double category(CostCategory category) const;
    double total_cost_of_ownership(bool include_paint_repair) const;
    double sum_days(int first_day, int last_day) const;
    const std::vector<double>& daily() const noexcept { return daily_; }

private:
    std::array<double, 6> totals_{};
    std::vector<double> daily_;
};

// ---------------------------------------------------------------------------
// Simulation state
// ---------------------------------------------------------------------------

struct TireSet {
    std::array<double, 4> tread_millimetres{};
    double age_years = 0.0;
};

struct SimulationState {
    // Engine
    double engine_wear_equivalent_kilometres = 0.0;
    double oil_degradation = 0.0;
    double oil_level_fraction = 1.0;
    double turbo_wear_index = 0.0;

    // Brakes
    double front_pad_millimetres = 0.0;
    double rear_pad_millimetres = 0.0;
    double front_disc_millimetres = 0.0;
    double rear_disc_millimetres = 0.0;
    double brake_fluid_age_months = 0.0;

    // Tires: index 0 = summer/all-season, index 1 = winter.
    std::array<TireSet, 2> tire_sets{};
    std::array<double, 4> tire_pressure_bar{};
    std::array<double, 2> tire_wear_factor{1.0, 1.0};
    int active_tire_set = 0;
    bool winter_tires_enabled = false;

    // Wear rate of the currently installed replacement, set when a part is
    // replaced: cheap parts wear faster.
    double front_pad_wear_factor = 1.0;
    double rear_pad_wear_factor = 1.0;
    double front_disc_wear_factor = 1.0;
    double rear_disc_wear_factor = 1.0;

    // Battery
    double battery_age_years = 0.0;
    double battery_degree_days_above_25 = 0.0;
    double battery_start_count = 0.0;
    double battery_deep_discharge_count = 0.0;
    double battery_health_offset = 0.0;

    // Other components
    double suspension_wear_index = 0.0;
    double clutch_wear_index = 0.0;
    double exhaust_corrosion_index = 0.0;
    double catalyst_degradation_index = 0.0;
    double cooling_wear_index = 0.0;
    double paint_damage_index = 0.0;
    double rust_index = 0.0;
    bool rust_started = false;
    int windshield_chip_count = 0;
    double coolant_age_months = 0.0;

    // A part fails at most once per run, except for consumable-like parts
    // (battery, windshield) where repeated replacement is allowed.
    bool engine_failed = false;
    bool suspension_failed = false;
    bool clutch_failed = false;
    bool exhaust_failed = false;
    bool cooling_failed = false;

    // Service history, in days relative to the first simulated day (negative =
    // before the horizon) and in odometer kilometres.
    double last_oil_change_day = 0.0;
    double last_oil_change_km = 0.0;
    double last_inspection_day = 0.0;
    double last_brake_fluid_day = 0.0;
    double last_coolant_day = 0.0;
    double last_wiper_blade_day = 0.0;
    double last_spark_plug_km = 0.0;
    double last_air_filter_km = 0.0;
    double last_cabin_filter_km = 0.0;
    double last_tire_rotation_km = 0.0;
    int last_emissions_inspection_year = -1;
    int last_tire_swap_year = -1;
    int last_tire_swap_month = -1;

    // Cumulative consumed life. Never reset by a replacement.
    double engine_consumed_kilometres = 0.0;
    double front_pad_consumed_millimetres = 0.0;
    double rear_pad_consumed_millimetres = 0.0;
    double front_disc_consumed_millimetres = 0.0;
    double rear_disc_consumed_millimetres = 0.0;
    double tire_consumed_millimetres = 0.0;
    double oil_consumed_litres = 0.0;
    int battery_replacement_count = 0;
    int windshield_replacement_count = 0;
    int engine_repair_count = 0;

    double odometer_kilometres = 0.0;
};

struct RunTotals {
    double distance_kilometres = 0.0;
    double fuel_litres = 0.0;
    double fuel_cost_euros = 0.0;
    double scheduled_maintenance_euros = 0.0;
    double wear_replacement_euros = 0.0;
    double unplanned_repair_euros = 0.0;
    double consequential_euros = 0.0;
    double paint_repair_euros = 0.0;
    double total_cost_of_ownership_euros = 0.0;
    double depreciation_euros = 0.0;
    int failure_count = 0;
    double downtime_days = 0.0;
};

struct MonthSnapshot {
    int month = 0;
    int start_day = 0;
    int end_day = 0;
    double cost_euros = 0.0;
    double cumulative_cost_euros = 0.0;
    double fuel_litres = 0.0;
    int failures = 0;
    double front_pad_millimetres = 0.0;
    double rear_pad_millimetres = 0.0;
    double front_disc_millimetres = 0.0;
    double rear_disc_millimetres = 0.0;
    std::array<double, 4> tire_tread_millimetres{};
    double battery_state_of_health = 0.0;
    double exhaust_corrosion_index = 0.0;
    double paint_damage_index = 0.0;
    double odometer_kilometres = 0.0;
};

struct TripRecord {
    int day = 0;
    TripPurpose purpose = TripPurpose::commute;
    RouteClass route_class = RouteClass::mixed;
    double distance_kilometres = 0.0;
    double mean_speed_kph = 0.0;
    bool cold_start = false;
    double fuel_litres = 0.0;
    double ambient_temperature_celsius = 0.0;
    bool road_wet = false;
    bool snowy = false;
};

struct ResolvedInputs {
    CurrentStateConfig initial_state;
    double frailty = 1.0;
};

struct RunResult {
    std::string case_name;
    Date start_date{};
    int horizon_days = 365;
    std::uint64_t run_seed = 0;
    double frailty = 1.0;
    CurrentStateConfig initial_state;

    RunTotals totals;
    SimulationState final_state;
    std::vector<Event> events;
    std::vector<MonthSnapshot> monthly;
    std::vector<TripRecord> trips;

    double terminal_battery_state_of_health = 0.0;
};

}  // namespace carwear
