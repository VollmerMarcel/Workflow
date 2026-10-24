#include "carwear/model/types.h"

#include <algorithm>
#include <stdexcept>

namespace carwear {

std::string_view to_string(TripPurpose purpose) {
    switch (purpose) {
        case TripPurpose::commute:
            return "commute";
        case TripPurpose::errand:
            return "errand";
        case TripPurpose::weekend_leisure:
            return "weekend_leisure";
        case TripPurpose::long_trip:
            return "long_trip";
    }
    return "commute";
}

std::string_view to_string(RouteClass route_class) {
    switch (route_class) {
        case RouteClass::city:
            return "city";
        case RouteClass::rural:
            return "rural";
        case RouteClass::mixed:
            return "mixed";
        case RouteClass::highway:
            return "highway";
    }
    return "mixed";
}

std::string_view to_string(EventKind kind) {
    switch (kind) {
        case EventKind::scheduled_maintenance:
            return "scheduled_maintenance";
        case EventKind::wear_replacement:
            return "wear_replacement";
        case EventKind::failure:
            return "failure";
        case EventKind::inspection:
            return "inspection";
        case EventKind::tire_swap:
            return "tire_swap";
        case EventKind::paint_repair:
            return "paint_repair";
    }
    return "scheduled_maintenance";
}

CostCategory category_for(EventKind kind) {
    switch (kind) {
        case EventKind::scheduled_maintenance:
        case EventKind::inspection:
        case EventKind::tire_swap:
            return CostCategory::scheduled_maintenance;
        case EventKind::wear_replacement:
            return CostCategory::wear_replacement;
        case EventKind::failure:
            return CostCategory::unplanned_repair;
        case EventKind::paint_repair:
            return CostCategory::paint_repair;
    }
    return CostCategory::scheduled_maintenance;
}

void CostLedger::reset(int horizon_days) {
    totals_.fill(0.0);
    daily_.assign(static_cast<std::size_t>(std::max(0, horizon_days)) + 1, 0.0);
}

void CostLedger::add(CostCategory category, double amount, int day) {
    if (amount == 0.0) {
        return;
    }
    totals_[static_cast<std::size_t>(category)] += amount;
    if (day >= 0 && static_cast<std::size_t>(day) < daily_.size()) {
        daily_[static_cast<std::size_t>(day)] += amount;
    }
}

double CostLedger::category(CostCategory category) const {
    return totals_[static_cast<std::size_t>(category)];
}

double CostLedger::total_cost_of_ownership(bool include_paint_repair) const {
    double total = 0.0;
    for (std::size_t i = 0; i < totals_.size(); ++i) {
        if (static_cast<CostCategory>(i) == CostCategory::paint_repair &&
            !include_paint_repair) {
            continue;
        }
        total += totals_[i];
    }
    return total;
}

double CostLedger::sum_days(int first_day, int last_day) const {
    if (daily_.empty()) {
        return 0.0;
    }
    first_day = std::max(0, first_day);
    last_day = std::min<int>(last_day, static_cast<int>(daily_.size()) - 1);
    double sum = 0.0;
    for (int day = first_day; day <= last_day; ++day) {
        sum += daily_[static_cast<std::size_t>(day)];
    }
    return sum;
}

}  // namespace carwear
