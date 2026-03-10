#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ir/core/result.hpp"

namespace ir::io {

	using CsvRow = std::unordered_map<std::string, std::string>;

	struct CsvTable {
		std::vector<std::string> headers;
		std::vector<CsvRow> rows;
	};

	struct CsvOptions {
		char sep = ',';
		bool trim = true;
		bool allow_empty_lines = true;
	};

	// Reads from a local file path OR a URL (if your reader supports it).
	ir::Result<CsvTable> read_csv_text(std::string_view text, const CsvOptions& opt = {});
	ir::Result<CsvTable> read_csv_file(const std::string& file_path, const CsvOptions& opt = {});

	// Helper utilities
	std::string trim_copy(std::string s);
	std::vector<std::string> split_line(std::string_view line, char sep);

} // namespace ir::io