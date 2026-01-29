#include "../../include/utils/root_finding.hpp"
#include "../../include/core/error.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

using namespace ir::utils;
using namespace ir;

TEST_CASE("brent: simple linear root f(x)=x") {
    auto f = [](double x) { return x; };
    RootFindOptions opts;
    opts.max_iter = 200;

    auto res = brent(f, -1.0, 1.0, opts);
    REQUIRE(res.has_value());
    RootFindResult r = res.value();

    REQUIRE(r.report.converged);
    REQUIRE(r.report.iterations > 0);
    REQUIRE_THAT(r.root, Catch::Matchers::WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(r.report.f_at_root, Catch::Matchers::WithinAbs(0.0, 1e-10));
}

TEST_CASE("brent: quadratic root f(x)=x^2-2 -> sqrt(2)") {
    auto f = [](double x) { return x * x - 2.0; };
    RootFindOptions opts;
    opts.max_iter = 200;

    auto res = brent(f, 1.0, 2.0, opts);
    REQUIRE(res.has_value());
    RootFindResult r = res.value();

    REQUIRE(r.report.converged);
    REQUIRE_THAT(r.root, Catch::Matchers::WithinAbs(std::sqrt(2.0), 1e-10));
    REQUIRE_THAT(r.report.f_at_root, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("brent: invalid bracket returns error") {
    // f(x) = x^2 + 1 has no real root; f(-1) and f(1) have same sign.
    auto f = [](double x) { return x * x + 1.0; };
    auto res = brent(f, -1.0, 1.0);
    REQUIRE_FALSE(res.has_value());
    REQUIRE(res.error().code == ErrorCode::InvalidArgument);
}