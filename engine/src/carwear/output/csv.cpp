#include "carwear/output/csv.h"

#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>

namespace carwear {

std::string csv_field(std::string_view text) {
    const bool needs_quotes = text.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!needs_quotes) {
        return std::string{text};
    }
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back('"');
    for (char character : text) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::string format_number(double value, int precision) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(precision) << value;
    return stream.str();
}

void write_csv(std::ostream& out, const std::vector<std::string>& header,
               const std::vector<std::vector<std::string>>& rows) {
    for (std::size_t column = 0; column < header.size(); ++column) {
        if (column != 0) {
            out << ',';
        }
        out << csv_field(header[column]);
    }
    out << '\n';
    for (const auto& row : rows) {
        for (std::size_t column = 0; column < row.size(); ++column) {
            if (column != 0) {
                out << ',';
            }
            out << csv_field(row[column]);
        }
        out << '\n';
    }
}

}  // namespace carwear
