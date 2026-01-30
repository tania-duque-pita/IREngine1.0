#pragma once
#include <string>
#include <utility>

namespace ir {

	struct CurveId {
		std::string value;
		explicit CurveId(std::string v) : value(std::move(v)) {}
		friend bool operator==(const CurveId&, const CurveId&) = default;
	};

	struct IndexId {
		std::string value;
		explicit IndexId(std::string v) : value(std::move(v)) {}
		friend bool operator==(const IndexId&, const IndexId&) = default;
	};

}
