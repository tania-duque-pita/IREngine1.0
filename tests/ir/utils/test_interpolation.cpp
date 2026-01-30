#include "ir/utils/interpolation.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

using namespace ir::utils;

TEST_CASE("LinearInterpolator: interpolation and flat extrapolation") {
    Interp1DData data;
    data.x = { 0.0, 1.0, 2.0 };
    data.y = { 0.0, 10.0, 20.0 };

    LinearInterpolator li(std::move(data));

    // Exact nodes
    REQUIRE(li.value(0.0) == 0.0);
    REQUIRE(li.value(1.0) == 10.0);
    REQUIRE(li.value(2.0) == 20.0);

    // Midpoint interpolation
    REQUIRE_THAT(li.value(0.5), Catch::Matchers::WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(li.value(1.5), Catch::Matchers::WithinAbs(15.0, 1e-12));

    // Flat extrapolation beyond ends
    REQUIRE_THAT(li.value(-1.0), Catch::Matchers::WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(li.value(3.0), Catch::Matchers::WithinAbs(20.0, 1e-12));
}

TEST_CASE("LogLinearInterpolator: log-space linearization and flat extrapolation") {
    // Choose y = exp(x) so log(y) = x; this makes the expected values exact.
    Interp1DData data;
    data.x = { 0.0, 1.0, 2.0 };
    data.y = { std::exp(0.0), std::exp(1.0), std::exp(2.0) };

    LogLinearInterpolator lli(std::move(data));

    // Exact nodes (should reproduce the original y)
    REQUIRE_THAT(lli.value(0.0), Catch::Matchers::WithinAbs(std::exp(0.0), 1e-12));
    REQUIRE_THAT(lli.value(1.0), Catch::Matchers::WithinAbs(std::exp(1.0), 1e-12));
    REQUIRE_THAT(lli.value(2.0), Catch::Matchers::WithinAbs(std::exp(2.0), 1e-12));

    // Midpoint in log-space: expected exp(0.5)
    REQUIRE_THAT(lli.value(0.5), Catch::Matchers::WithinAbs(std::exp(0.5), 1e-12));
    REQUIRE_THAT(lli.value(1.5), Catch::Matchers::WithinAbs(std::exp(1.5), 1e-12));

    // Flat extrapolation (returns end y values)
    REQUIRE_THAT(lli.value(-1.0), Catch::Matchers::WithinAbs(std::exp(0.0), 1e-12));
    REQUIRE_THAT(lli.value(3.0), Catch::Matchers::WithinAbs(std::exp(2.0), 1e-12));
}