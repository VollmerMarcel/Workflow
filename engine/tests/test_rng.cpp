#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#include "carwear/core/rng.h"

namespace carwear {
namespace {

TEST(RngTest, SameSeedProducesSameStream) {
    Rng lhs(12345);
    Rng rhs(12345);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(lhs.next_u64(), rhs.next_u64());
    }
}

TEST(RngTest, DifferentSeedsProduceDifferentStreams) {
    Rng lhs(1);
    Rng rhs(2);
    EXPECT_NE(lhs.next_u64(), rhs.next_u64());
}

TEST(RngTest, UniformIsInUnitInterval) {
    Rng rng(7);
    double sum = 0.0;
    for (int i = 0; i < 100000; ++i) {
        const double value = rng.uniform();
        ASSERT_GE(value, 0.0);
        ASSERT_LT(value, 1.0);
        sum += value;
    }
    EXPECT_NEAR(sum / 100000.0, 0.5, 0.01);
}

TEST(RngTest, NormalHasExpectedMeanAndDeviation) {
    Rng rng(99);
    double sum = 0.0;
    double sum_squares = 0.0;
    constexpr int kCount = 100000;
    for (int i = 0; i < kCount; ++i) {
        const double value = rng.normal(2.0, 3.0);
        sum += value;
        sum_squares += value * value;
    }
    const double mean = sum / kCount;
    const double variance = sum_squares / kCount - mean * mean;
    EXPECT_NEAR(mean, 2.0, 0.05);
    EXPECT_NEAR(std::sqrt(variance), 3.0, 0.05);
}

TEST(RngTest, BernoulliHonoursBoundaries) {
    Rng rng(3);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(rng.bernoulli(0.0));
        EXPECT_TRUE(rng.bernoulli(1.0));
    }
}

TEST(RngTest, PoissonMeanMatchesRate) {
    Rng rng(42);
    double sum = 0.0;
    constexpr int kCount = 50000;
    for (int i = 0; i < kCount; ++i) {
        sum += rng.poisson(3.5);
    }
    EXPECT_NEAR(sum / kCount, 3.5, 0.06);
}

TEST(RngTest, TruncatedNormalStaysInsideBounds) {
    Rng rng(11);
    for (int i = 0; i < 2000; ++i) {
        const double value = rng.truncated_normal(0.0, 1.0, -1.5, 1.5);
        ASSERT_GE(value, -1.5);
        ASSERT_LE(value, 1.5);
    }
}

TEST(RngTest, LogNormalMedianIsCloseToTarget) {
    Rng rng(17);
    std::vector<double> values;
    values.reserve(20000);
    for (int i = 0; i < 20000; ++i) {
        values.push_back(rng.log_normal(10.0, 0.3));
    }
    std::sort(values.begin(), values.end());
    EXPECT_NEAR(values[values.size() / 2], 10.0, 0.2);
}

TEST(RngTest, DerivedSeedsAreStableAndDistinct) {
    const std::uint64_t base = 2026;
    EXPECT_EQ(derive_seed(base, "weather"), derive_seed(base, "weather"));
    EXPECT_NE(derive_seed(base, "weather"), derive_seed(base, "trips"));
    EXPECT_NE(derive_seed(base, 0, "run"), derive_seed(base, 1, "run"));
}

}  // namespace
}  // namespace carwear
