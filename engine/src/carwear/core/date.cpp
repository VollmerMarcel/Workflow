#include "carwear/core/date.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace carwear {

bool parse_date(const std::string& text, Date& out) {
    // Strict YYYY-MM-DD.
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    auto number = [&](std::size_t offset, std::size_t length) {
        int value = 0;
        for (std::size_t i = 0; i < length; ++i) {
            const char c = text[offset + i];
            if (c < '0' || c > '9') {
                return -1;
            }
            value = value * 10 + (c - '0');
        }
        return value;
    };
    const int year = number(0, 4);
    const int month = number(5, 2);
    const int day = number(8, 2);
    if (year < 0 || month < 0 || day < 0) {
        return false;
    }
    const Date candidate{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                         std::chrono::day{static_cast<unsigned>(day)}};
    if (!candidate.ok()) {
        return false;
    }
    out = candidate;
    return true;
}

std::string format_date(const Date& date) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", static_cast<int>(date.year()),
                  static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()));
    return buffer;
}

Date add_days(const Date& date, int days) {
    const auto shifted = std::chrono::sys_days{date} + std::chrono::days{days};
    return Date{shifted};
}

int month_of(const Date& date) noexcept {
    return static_cast<int>(static_cast<unsigned>(date.month()));
}

int year_of(const Date& date) noexcept {
    return static_cast<int>(date.year());
}

int weekday_index(const Date& date) noexcept {
    const auto weekday = std::chrono::weekday{std::chrono::sys_days{date}};
    // std::chrono uses 0 = Sunday; the model uses 0 = Monday.
    return (static_cast<int>(weekday.c_encoding()) + 6) % 7;
}

bool is_weekend(const Date& date) noexcept {
    const int day = weekday_index(date);
    return day == 5 || day == 6;
}

int days_in_month(int year, int month) noexcept {
    const Date first{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                     std::chrono::day{1}};
    const Date next = add_days(first, 32);
    const Date next_first{next.year(), next.month(), std::chrono::day{1}};
    return static_cast<int>(
        (std::chrono::sys_days{next_first} - std::chrono::sys_days{first}).count());
}

}  // namespace carwear
