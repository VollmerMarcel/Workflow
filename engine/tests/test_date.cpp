#include <gtest/gtest.h>

#include <chrono>

#include "carwear/core/date.h"

namespace carwear {
namespace {

TEST(DateTest, ParsesAndFormatsIsoDate) {
    Date date{};
    ASSERT_TRUE(parse_date("2026-10-23", date));
    EXPECT_EQ(format_date(date), "2026-10-23");
    EXPECT_EQ(year_of(date), 2026);
    EXPECT_EQ(month_of(date), 10);
}

TEST(DateTest, RejectsMalformedDates) {
    Date date{};
    EXPECT_FALSE(parse_date("2026-13-01", date));
    EXPECT_FALSE(parse_date("not-a-date", date));
    EXPECT_FALSE(parse_date("2026-02-30", date));
}

TEST(DateTest, AddsDaysAcrossMonthAndYearBoundaries) {
    Date date{};
    ASSERT_TRUE(parse_date("2024-02-28", date));
    EXPECT_EQ(format_date(add_days(date, 1)), "2024-02-29");
    EXPECT_EQ(format_date(add_days(date, 2)), "2024-03-01");
    EXPECT_EQ(format_date(add_days(date, -1)), "2024-02-27");
}

TEST(DateTest, AddsAcrossNewYear) {
    Date date{};
    ASSERT_TRUE(parse_date("2026-12-31", date));
    EXPECT_EQ(format_date(add_days(date, 1)), "2027-01-01");
}

TEST(DateTest, WeekdayIndexMatchesCalendar) {
    Date date{};
    ASSERT_TRUE(parse_date("2026-10-23", date));  // Friday
    EXPECT_EQ(weekday_index(date), 4);
    EXPECT_FALSE(is_weekend(date));
    ASSERT_TRUE(parse_date("2026-10-25", date));  // Sunday
    EXPECT_TRUE(is_weekend(date));
}

TEST(DateTest, DaysInMonthHandlesLeapYears) {
    EXPECT_EQ(days_in_month(2024, 2), 29);
    EXPECT_EQ(days_in_month(2026, 2), 28);
    EXPECT_EQ(days_in_month(2026, 4), 30);
    EXPECT_EQ(days_in_month(2026, 12), 31);
}

}  // namespace
}  // namespace carwear
