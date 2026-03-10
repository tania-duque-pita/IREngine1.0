#include "ir/io/csv_io.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "ir/core/error.hpp"

namespace ir::io {

    std::string trim_copy(std::string s) {
        auto is_ws = [](unsigned char c) { return std::isspace(c); };
        while (!s.empty() && is_ws((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && is_ws((unsigned char)s.back()))  s.pop_back();
        return s;
    }

    std::vector<std::string> split_line(std::string_view line, char sep) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : line) {
            if (c == sep) { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        out.push_back(cur);
        return out;
    }

    ir::Result<CsvTable> read_csv_text(std::string_view text, const CsvOptions& opt) {
        CsvTable table;
        std::istringstream iss(static_cast<std::string>(text));

        std::string line;
        bool header_done = false;

        while (std::getline(iss, line)) {
            if (opt.allow_empty_lines && trim_copy(line).empty()) continue;

            auto cols = split_line(line, opt.sep);
            if (opt.trim) for (auto& c : cols) c = trim_copy(c);

            if (!header_done) {
                table.headers = cols;
                header_done = true;
                continue;
            }

            if (cols.size() != table.headers.size()) {
                return ir::Error::make(ir::ErrorCode::ParseError,
                    "CSV: column count mismatch.");
            }

            CsvRow row;
            row.reserve(cols.size());
            for (std::size_t i = 0; i < cols.size(); ++i) {
                row[table.headers[i]] = cols[i];
            }
            table.rows.push_back(std::move(row));
        }

        if (!header_done) {
            return ir::Error::make(ir::ErrorCode::ParseError, "CSV: missing header.");
        }

        return table;
    }

    ir::Result<CsvTable> read_csv_file(const std::string& file_path, const CsvOptions& opt) {
        std::ifstream in(file_path);
        if (!in) {
            return ir::Error::make(ir::ErrorCode::InvalidArgument,
                "CSV: cannot open file: " + file_path);
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return read_csv_text(buffer.str(), opt);
    }

} // namespace ir::io