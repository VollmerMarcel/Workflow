#pragma once

#include <cstdint>
#include <string_view>

namespace carwear {

// Deterministic 64-bit mixing function used to derive seeds.
std::uint64_t splitmix64(std::uint64_t& state) noexcept;

// FNV-1a, used only to turn a stream name into a stable 64-bit number.
std::uint64_t fnv1a64(std::string_view text) noexcept;

// Derive an independent stream seed from a base seed and a stream name.
std::uint64_t derive_seed(std::uint64_t base, std::string_view stream_name) noexcept;
std::uint64_t derive_seed(std::uint64_t base, std::uint64_t index,
                          std::string_view stream_name) noexcept;

// xoshiro256** generator. Integer arithmetic only, so the bit stream is
// identical on every platform. Standard-library distributions are avoided
// because their implementations are allowed to differ.
class Rng {
public:
    explicit Rng(std::uint64_t seed) noexcept;

    std::uint64_t next_u64() noexcept;
    double uniform() noexcept;                  // [0, 1)
    double uniform(double lo, double hi) noexcept;
    double normal() noexcept;                   // mean 0, sd 1
    double normal(double mean, double sd) noexcept;
    bool bernoulli(double probability) noexcept;
    int poisson(double rate) noexcept;
    double exponential(double rate) noexcept;
    double log_normal(double median, double coefficient_of_variation) noexcept;
    double truncated_normal(double mean, double sd, double lo, double hi) noexcept;

    std::uint64_t seed() const noexcept { return seed_; }

private:
    std::uint64_t state_[4]{};
    std::uint64_t seed_ = 0;
    bool has_spare_ = false;
    double spare_ = 0.0;
};

}  // namespace carwear
