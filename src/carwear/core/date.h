#pragma once

#include <chrono>
#include <string>

namespace carwear {

using Date = std::chrono::year_month_day;

// Parse an ISO 8601 calendar date (YYYY-MM-DD).
bool parse_date(const std::string& text, Date& out);

std::string format_date(const Date& date);

// Add a number of calendar days. Negative values are allowed.
Date add_days(const Date& date, int days);

int month_of(const Date& date) noexcept;
int year_of(const Date& date) noexcept;

// 0 = Monday, 6 = Sunday.
int weekday_index(const Date& date) noexcept;
bool is_weekend(const Date& date) noexcept;

int days_in_month(int year, int month) noexcept;

}  // namespace carwear
