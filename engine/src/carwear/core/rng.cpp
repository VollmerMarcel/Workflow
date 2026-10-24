#include "carwear/core/rng.h"

#include <cmath>
#include <limits>

namespace carwear {
namespace {

inline std::uint64_t rotl(std::uint64_t value, int amount) noexcept {
    return (value << amount) | (value >> (64 - amount));
}

}  // namespace

std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

std::uint64_t derive_seed(std::uint64_t base, std::string_view stream_name) noexcept {
    std::uint64_t state = base ^ fnv1a64(stream_name);
    return splitmix64(state);
}

std::uint64_t derive_seed(std::uint64_t base, std::uint64_t index,
                          std::string_view stream_name) noexcept {
    std::uint64_t state = base ^ fnv1a64(stream_name);
    state ^= index * 0xD6E8FEB86659FD93ULL;
    return splitmix64(state);
}

Rng::Rng(std::uint64_t seed) noexcept : seed_(seed) {
    std::uint64_t init = seed;
    for (auto& word : state_) {
        word = splitmix64(init);
    }
    // xoshiro requires a non-zero state.
    if ((state_[0] | state_[1] | state_[2] | state_[3]) == 0) {
        state_[0] = 0x9E3779B97F4A7C15ULL;
    }
}

std::uint64_t Rng::next_u64() noexcept {
    const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
    const std::uint64_t t = state_[1] << 17;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = rotl(state_[3], 45);
    return result;
}

double Rng::uniform() noexcept {
    // 53 random bits, the resolution of a double in [1, 2).
    return static_cast<double>(next_u64() >> 11) * 0x1.0p-53;
}

double Rng::uniform(double lo, double hi) noexcept {
    return lo + (hi - lo) * uniform();
}

double Rng::normal() noexcept {
    if (has_spare_) {
        has_spare_ = false;
        return spare_;
    }
    // Box-Muller. u1 is in (0, 1].
    double u1 = uniform();
    if (u1 <= 0.0) {
        u1 = std::numeric_limits<double>::min();
    }
    const double u2 = uniform();
    const double radius = std::sqrt(-2.0 * std::log(u1));
    const double angle = 2.0 * 3.14159265358979323846 * u2;
    spare_ = radius * std::sin(angle);
    has_spare_ = true;
    return radius * std::cos(angle);
}

double Rng::normal(double mean, double sd) noexcept {
    return mean + sd * normal();
}

bool Rng::bernoulli(double probability) noexcept {
    if (probability <= 0.0) {
        return false;
    }
    if (probability >= 1.0) {
        return true;
    }
    return uniform() < probability;
}

int Rng::poisson(double rate) noexcept {
    if (!(rate > 0.0)) {
        return 0;
    }
    if (rate > 60.0) {
        // Normal approximation is adequate and avoids a long loop.
        const double draw = normal(rate, std::sqrt(rate));
        const double rounded = std::floor(draw + 0.5);
        return rounded <= 0.0 ? 0 : static_cast<int>(rounded);
    }
    const double limit = std::exp(-rate);
    double product = 1.0;
    int count = 0;
    do {
        ++count;
        product *= uniform();
    } while (product > limit && count < 10000);
    return count - 1;
}

double Rng::exponential(double rate) noexcept {
    if (!(rate > 0.0)) {
        return std::numeric_limits<double>::infinity();
    }
    double u = uniform();
    if (u <= 0.0) {
        u = std::numeric_limits<double>::min();
    }
    return -std::log(u) / rate;
}

double Rng::log_normal(double median, double coefficient_of_variation) noexcept {
    if (!(median > 0.0)) {
        return 0.0;
    }
    if (!(coefficient_of_variation > 0.0)) {
        return median;
    }
    const double sigma = std::sqrt(std::log(1.0 + coefficient_of_variation *
                                                      coefficient_of_variation));
    return std::exp(std::log(median) + sigma * normal());
}

double Rng::truncated_normal(double mean, double sd, double lo, double hi) noexcept {
    if (!(sd > 0.0)) {
        return mean < lo ? lo : (mean > hi ? hi : mean);
    }
    // Rejection sampling; the truncation windows used by the engine are wide
    // enough that this terminates quickly.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const double draw = normal(mean, sd);
        if (draw >= lo && draw <= hi) {
            return draw;
        }
    }
    return mean < lo ? lo : (mean > hi ? hi : mean);
}

}  // namespace carwear
