#pragma once

#include <vector>

#include "carwear/config/config.h"
#include "carwear/core/date.h"
#include "carwear/core/rng.h"
#include "carwear/model/types.h"

namespace carwear {

// Generates one day of trips from the driver's weekly rhythm, the route
// classes, the driving style, and the weather. Cold starts are assigned after
// the trips are ordered by start time.
class TripGenerator {
public:
    TripGenerator(const Config& config, Rng& rng);

    std::vector<Drive> generate(int day, const Date& date, const Weather& weather);

private:
    Drive make_drive(TripPurpose purpose, int day, const Weather& weather);

    const Config& config_;
    Rng& rng_;
    double mean_month_weight_ = 1.0;
};

}  // namespace carwear
