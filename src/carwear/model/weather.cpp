#include "carwear/model/weather.h"

#include <cmath>

#include "carwear/core/math.h"

namespace carwear {

WeatherProcess::WeatherProcess(const Config& config, const Date& start_date, Rng& rng)
    : config_(config), rng_(rng), start_date_(start_date) {
    const int month_index = month_of(start_date) - 1;
    const auto& climate = config_.climate;
    anomaly_celsius_ =
        rng_.normal(0.0, climate.monthly_temperature_deviation_celsius[
                             static_cast<std::size_t>(month_index)]);
    previous_precipitation_ = rng_.bernoulli(
        climate.monthly_precipitation_probability[static_cast<std::size_t>(month_index)]);
}

Weather WeatherProcess::next_day() {
    ++day_;
    const Date date = add_days(start_date_, day_ - 1);
    const int month_index = month_of(date) - 1;
    const auto index = static_cast<std::size_t>(month_index);

    const auto& climate = config_.climate;

    const double phi = clamp(climate.temperature_autocorrelation, 0.0, 0.999);
    anomaly_celsius_ =
        phi * anomaly_celsius_ +
        std::sqrt(1.0 - phi * phi) *
            climate.monthly_temperature_deviation_celsius[index] * rng_.normal();

    Weather weather;
    weather.day = day_;
    weather.temperature_celsius =
        climate.monthly_temperature_mean_celsius[index] + anomaly_celsius_;

    const double p = clamp(climate.monthly_precipitation_probability[index], 0.0, 1.0);
    const double q = clamp(climate.precipitation_persistence, 0.0, 1.0);
    double dry_to_wet = q;
    if (p > 0.0 && p < 1.0) {
        dry_to_wet = clamp(p * (1.0 - q) / (1.0 - p), 0.0, 1.0);
    } else if (p <= 0.0) {
        dry_to_wet = 0.0;
    }
    weather.precipitation = previous_precipitation_ ? rng_.bernoulli(q)
                                                    : rng_.bernoulli(dry_to_wet);
    weather.road_wet = weather.precipitation || previous_precipitation_;
    weather.snowy = weather.precipitation &&
                    weather.temperature_celsius <
                        climate.snow_temperature_threshold_celsius;
    const bool frosty = weather.temperature_celsius <
                        climate.frost_temperature_threshold_celsius;
    weather.salt_day =
        (weather.snowy || frosty) &&
        rng_.bernoulli(climate.salt_probability_on_frost_day);
    weather.uv_index = climate.monthly_uv_index[index] *
                       (weather.precipitation ? (1.0 - climate.uv_cloud_reduction) : 1.0);
    weather.hail = rng_.bernoulli(climate.annual_hail_days / 365.0);

    previous_precipitation_ = weather.precipitation;
    return weather;
}

}  // namespace carwear
