#pragma once

#include "carwear/config/config.h"
#include "carwear/core/date.h"
#include "carwear/core/rng.h"
#include "carwear/model/types.h"

namespace carwear {

// Stochastic weather process: AR(1) temperature around the monthly mean, a
// two-state Markov chain for precipitation, and derived snow, salt, UV and
// hail indicators.
class WeatherProcess {
public:
    WeatherProcess(const Config& config, const Date& start_date, Rng& rng);

    Weather next_day();

private:
    const Config& config_;
    Rng& rng_;
    Date start_date_;
    int day_ = 0;
    double anomaly_celsius_ = 0.0;
    bool previous_precipitation_ = false;
};

}  // namespace carwear
