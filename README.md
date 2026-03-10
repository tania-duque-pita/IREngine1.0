# IREngine1.0

IREngine1.0 is a C++ quantitative finance project for **multi-curve interest-rate pricing and bootstrapping**.

The repository is designed as a portfolio project for **Quant / Quant Developer** roles. It showcases:

- **C++ architecture** with headers in `include/` and implementations in `src/`
- **market infrastructure** such as quotes, fixings, discount curves, forward curves, and bootstrapping helpers
- **instrument construction** for fixed, IBOR, and RFR legs
- **pricing** in both single-curve and multi-curve settings
- **testing** with Catch2
- **executable demos** that can be built from examples in the repository.

## Main capabilities
The repo currently supports:
- **OIS discount curves**
- **IBOR / RFR forward curves**
- **CSV-based pricing demos**
- **IRS / OIS / multi-leg trade pricing**
- **unit-tested core, market, and pricer components**

## Project structure

```text
IREngine1.0/
├─ include/ir/
│  ├─ core/          # dates, conventions, IDs, Result/Error
│  ├─ utils/         # interpolation, root finding, node validation
│  ├─ market/        # quotes, fixings, curves, market data, helpers
│  ├─ instruments/   # cashflows, coupons, legs, products
│  ├─ pricers/       # leg and swap pricing logic
│  └─ io/            # CSV loaders for trades and market data
│
├─ src/
│  ├─ ir/            # implementations
│  └─ apps/          # runnable executables (e.g. ire_price)
│
├─ tests/            # unit tests
├─ example/          # demo input folders
├─ CMakeLists.txt
└─ CMakePresets.json
```
## Build and run
This project uses CMake.
```bash
cmake -S . -B build
cmake --build build -j
```
To build with CMake Presets:
- Configure and build:
```bash
cmake --preset x64-debug
cmake --build --preset build-x64-debug
```
- For an optimized build:
```bash
cmake --preset x64-release
cmake --build --preset build-x64-release
```
## Demos
### 1. Pricing demo

The pricing demo uses the executable ire_price and reads input files from folder IREngine1.0/example/<example_name> in this repo. The following files are inputs in the folder:

- deal_data.csv
- discount.csv
- optional forward curve files (depends on deal structure)
- optional fixing files (depends on deal structure)

Supported trade types include:

- IRS (Fixed v Float-IBOR)
- OIS (Fixed v Float-OIS)
- multi-leg trades

The app prices each leg independently and outputs the files below in IREngine1.0/example/<example_name>:

- result_cashflows.csv
- result.csv

#### Run demo
Example with files located in the repository.

Debug build
```bash
.\out\build\x64-debug\src\ire_price.exe --example IRS
```
Release build
```bash
.\out\build\x64-release\src\ire_price.exe --example IRS
```

### 2. Bootstrapping demo

The repository also includes a bootstrapping demo showing how discount curves are constructed from market inputs and how curve nodes are queried after calibration.

#### Run demo
Debug build
```bash
.\out\build\x64-debug\src\demo_bootstrapper.exe
```
Release build
```bash
.\out\build\x64-release\src\demo_bootstrapper.exe
```


## Run tests
Run the unittests based on Catch2. All unittests are located under tests/.
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```
