cmake_minimum_required(VERSION 3.14)
project(core_tests LANGUAGES CXX)

include(FetchContent)

# Fetch Catch2 v3
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Build tests executable from existing test sources
add_executable(core_tests "core/test_date.cpp")

# Link tests against the IREngine1.0 library and Catch2 with main
target_link_libraries(core_tests
  PUBLIC
    IREngine1.0
    Catch2::Catch2WithMain
)

target_include_directories(core_tests PUBLIC ../include)

# Provide the Catch helper and register tests
# Catch2 source dir variable comes from FetchContent (Catch2_SOURCE_DIR)
include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(core_tests)