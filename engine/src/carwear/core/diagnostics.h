#pragma once

#include <string>
#include <vector>

namespace carwear {

enum class Severity { warning, error };

struct Diagnostic {
    Severity severity = Severity::error;
    std::string message;
};

class Diagnostics {
public:
    void warn(std::string message) { items_.push_back({Severity::warning, std::move(message)}); }
    void error(std::string message) { items_.push_back({Severity::error, std::move(message)}); }

    bool has_errors() const noexcept {
        for (const auto& item : items_) {
            if (item.severity == Severity::error) {
                return true;
            }
        }
        return false;
    }

    const std::vector<Diagnostic>& items() const noexcept { return items_; }
    std::vector<Diagnostic>& items() noexcept { return items_; }

private:
    std::vector<Diagnostic> items_;
};

}  // namespace carwear
