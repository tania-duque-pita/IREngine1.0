# IREngine1.0 — Multi-Curve Interest Rate Engine (C++)

**IREngine1.0** is an in-progress C++ quantitative finance library focused on a **multi-curve interest rate framework** (OIS discounting + IBOR/RFR forwarding) as a foundation for **Market Risk (VaR)** extensions.

The purpose of this repository is to showcase:
- C++ library architecture (headers in `include/`, implementations in `src/`)
- robust numeric utilities (interpolation, root finding)
- market layer components (quotes, fixings, curves, bootstrapping)
- unit tests (Catch2)

> **Status (in progress):** Implemented up to **Market data processing** (curves, quotes, bootstrapping, fixings).  
> Planned next:
> - Cashflow and Product Pricing
> - Valuation Framework
> - IR Simulation (HW)
> - Market Risk Framework (VaR)

---

## Current Features (Implemented)

### Core (`include/ir/core`)
Foundational building blocks:
- `Date` utilities
- conventions (day count, business day conventions)
- IDs (`CurveId`, `IndexId`)
- lightweight `Result<T>` / error types

### Utils (`include/ir/utils`)
Numerics and plumbing:
- 1D interpolation: **Linear** and **Log-Linear**
- Brent root finding (robust for bootstrapping)
- piecewise nodes container + validation

### Market (`include/ir/market`)
Market layer components:
- Quotes + fixings (fixing store for indices)
- Curves:
  - `PiecewiseDiscountCurve` (log-linear interpolation on discount factors)
  - `PiecewiseForwardCurve` (pseudo-discount curve used to derive forwards)
- Bootstrapping:
  - OIS discount curve bootstrapping via `OisSwapHelper`
  - Forward curve bootstrapping via `FraHelper` / `IrsHelper`
- `MarketData` snapshot container for curves/quotes/fixings

---

## Quick Demo (Bootstrap an OIS Discount Curve)

A small runnable demo is included at:

- `src/demo/bootstrap_demo.cpp`

What it does:
1. creates a small set of hardcoded OIS par rates
2. bootstraps a `PiecewiseDiscountCurve`
3. prints pillar times and discount factors
4. queries a non-pillar discount factor

### Build and run

```bash
cmake -S . -B build
cmake --build build -j
