#include "carwear/model/trips.h"

#include <algorithm>
#include <cmath>

#include "carwear/core/math.h"
#include "carwear/model/physics.h"

namespace carwear {

TripGenerator::TripGenerator(const Config& config, Rng& rng)
    : config_(config), rng_(rng) {
    mean_month_weight_ = monthly_weight_mean(config_.user.long_trip_month_weights);
    if (!(mean_month_weight_ > 0.0)) {
        mean_month_weight_ = 1.0;
    }
}

Drive TripGenerator::make_drive(TripPurpose purpose, int day, const Weather& weather) {
    (void)day;
    const auto& user = config_.user;
    const auto& routes = config_.route_classes;
    const auto& model = config_.model;

    RouteClass route_class = RouteClass::mixed;
    double median_distance = user.commute_distance_kilometres;
    double coefficient_of_variation = user.commute_distance_coefficient_of_variation;
    int start_minute = 8 * 60;

    switch (purpose) {
        case TripPurpose::commute:
            route_class = RouteClass::mixed;
            median_distance = user.commute_distance_kilometres;
            coefficient_of_variation = user.commute_distance_coefficient_of_variation;
            start_minute = 8 * 60 + static_cast<int>(rng_.uniform(-30.0, 30.0));
            break;
        case TripPurpose::errand:
            route_class = RouteClass::city;
            median_distance = user.errand_distance_kilometres;
            coefficient_of_variation =
                user.errand_distance_coefficient_of_variation;
            start_minute = 10 * 60 + static_cast<int>(rng_.uniform(0.0, 8 * 60.0));
            break;
        case TripPurpose::weekend_leisure:
            route_class = RouteClass::rural;
            median_distance = user.weekend_distance_kilometres;
            coefficient_of_variation =
                user.weekend_distance_coefficient_of_variation;
            start_minute = 10 * 60 + static_cast<int>(rng_.uniform(-60.0, 120.0));
            break;
        case TripPurpose::long_trip:
            route_class = RouteClass::highway;
            median_distance = user.long_trip_distance_kilometres;
            coefficient_of_variation =
                user.long_trip_distance_coefficient_of_variation;
            start_minute = 8 * 60 + static_cast<int>(rng_.uniform(-60.0, 60.0));
            break;
    }

    const RouteClassConfig* route = &routes.mixed;
    switch (route_class) {
        case RouteClass::city:
            route = &routes.city;
            break;
        case RouteClass::rural:
            route = &routes.rural;
            break;
        case RouteClass::mixed:
            route = &routes.mixed;
            break;
        case RouteClass::highway:
            route = &routes.highway;
            break;
    }

    const double style = clamp(user.style_aggressiveness, 0.0, 1.0);
    const double weather_speed_factor =
        weather.snowy ? config_.climate.snow_speed_factor
                      : (weather.road_wet ? config_.climate.rain_speed_factor : 1.0);
    const double weather_stop_factor =
        weather.snowy ? config_.climate.snow_stop_factor
                      : (weather.road_wet ? config_.climate.rain_stop_factor : 1.0);

    Drive drive;
    drive.purpose = purpose;
    drive.route_class = route_class;
    drive.distance_kilometres =
        std::max(0.1, rng_.log_normal(median_distance, coefficient_of_variation));
    drive.mean_speed_kph =
        clamp(route->mean_speed_kph *
                  (1.0 + model.speed_style_sensitivity * (style - 0.5)) *
                  weather_speed_factor,
              5.0, 200.0);
    drive.speed_variation_kph = 0.15 * drive.mean_speed_kph;
    drive.stop_density_per_kilometre =
        std::max(0.0, route->stop_density_per_kilometre *
                          (1.0 - model.stop_style_sensitivity * (style - 0.5)) *
                          weather_stop_factor);
    drive.brake_onset_speed_fraction =
        clamp(route->brake_onset_speed_fraction *
                  (1.0 + model.brake_onset_style_sensitivity * (style - 0.5)),
              0.10, 1.0);
    drive.lateral_acceleration =
        std::max(0.0, route->lateral_acceleration_metres_per_second_squared *
                          (1.0 + model.lateral_style_sensitivity * (style - 0.5)));
    drive.positive_acceleration =
        std::max(0.0,
                 route->positive_acceleration_metres_per_second_squared *
                     (1.0 + model.acceleration_style_sensitivity * (style - 0.5)));
    drive.idle_fraction = clamp(route->idle_fraction, 0.0, 1.0);
    drive.start_minute = start_minute;
    drive.duration_hours = drive.distance_kilometres / drive.mean_speed_kph;
    drive.short_trip =
        drive.distance_kilometres < model.short_trip_distance_kilometres;
    return drive;
}

std::vector<Drive> TripGenerator::generate(int day, const Date& date,
                                           const Weather& weather) {
    const auto& user = config_.user;
    const auto& climate = config_.climate;
    const auto& model = config_.model;

    const double demand = clamp(
        1.0 - climate.rain_demand_reduction * indicator(weather.road_wet) -
            climate.snow_demand_reduction * indicator(weather.snowy),
        0.3, 1.0);

    std::vector<Drive> drives;

    const int weekday = weekday_index(date);
    const bool weekend = weekday == 5 || weekday == 6;
    const bool workday = weekday >= 0 && weekday <= 4;

    if (workday) {
        const double probability =
            clamp(static_cast<double>(user.commute_days_per_week) / 5.0 * demand,
                  0.0, 1.0);
        if (rng_.bernoulli(probability)) {
            drives.push_back(make_drive(TripPurpose::commute, day, weather));
        }
    }

    const int errands = rng_.poisson(user.errand_trips_per_week / 7.0 * demand);
    for (int i = 0; i < errands; ++i) {
        drives.push_back(make_drive(TripPurpose::errand, day, weather));
    }

    if (weekend) {
        const int leisure = rng_.poisson(user.weekend_trips_per_week / 2.0 * demand);
        for (int i = 0; i < leisure; ++i) {
            drives.push_back(make_drive(TripPurpose::weekend_leisure, day, weather));
        }
    }

    const int month_index = month_of(date) - 1;
    const double month_weight =
        user.long_trip_month_weights[static_cast<std::size_t>(month_index)];
    const double long_rate = user.long_trips_per_year / 365.0 * month_weight /
                             mean_month_weight_ * demand;
    const int long_trips = rng_.poisson(long_rate);
    for (int i = 0; i < long_trips; ++i) {
        drives.push_back(make_drive(TripPurpose::long_trip, day, weather));
    }

    std::stable_sort(drives.begin(), drives.end(),
                     [](const Drive& lhs, const Drive& rhs) {
                         return lhs.start_minute < rhs.start_minute;
                     });

    const double threshold_minutes = model.cold_start_threshold_minutes;
    int previous_end = -1;
    for (auto& drive : drives) {
        if (previous_end < 0) {
            drive.cold_start = true;
        } else {
            int gap = drive.start_minute - previous_end;
            if (gap < 5) {
                drive.start_minute = previous_end + 5;
                gap = 5;
            }
            drive.cold_start = static_cast<double>(gap) > threshold_minutes;
        }
        if (drive.start_minute < 0) {
            drive.start_minute = 0;
        }
        previous_end =
            drive.start_minute + static_cast<int>(std::ceil(drive.duration_hours * 60.0));
    }

    return drives;
}

}  // namespace carwear
