#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace carwear {

// Escapes one CSV field: quotes are doubled and the field is quoted when it
// contains a comma, a quote or a line break.
std::string csv_field(std::string_view text);

// Fixed, locale-independent numeric formatting shared by the CSV writers.
std::string format_number(double value, int precision = 10);

void write_csv(std::ostream& out, const std::vector<std::string>& header,
               const std::vector<std::vector<std::string>>& rows);

}  // namespace carwear
