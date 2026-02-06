# CMakeList.txt : Top-level CMake project file, do global configuration

cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("IREngine1.0" LANGUAGES CXX)

# Prefer C++20 if supported, otherwise fall back to at least C++17.
include(CheckCXXCompilerFlag)

# Check for C++20 support.
if (MSVC)
  check_cxx_compiler_flag("/std:c++20" COMPILER_SUPPORTS_STD20)
else()
  check_cxx_compiler_flag("-std=c++20" COMPILER_SUPPORTS_STD20)
endif()

if (COMPILER_SUPPORTS_STD20)
  set(CMAKE_CXX_STANDARD 20)
else()
  # Fallback: check for C++17
  if (MSVC)
    check_cxx_compiler_flag("/std:c++17" COMPILER_SUPPORTS_STD17)
  else()
    check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_STD17)
  endif()

  if (COMPILER_SUPPORTS_STD17)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "C++20 not available; falling back to C++17")
  else()
    message(WARNING "Neither C++20 nor C++17 detected. Using compiler default standard.")
  endif()
endif()

# Request the chosen standard and disable compiler-specific extensions.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# For MSVC: ensure __cplusplus reflects the standard (so printing __cplusplus shows the correct value).
if (MSVC)
  add_compile_options("/Zc:__cplusplus")
endif()

enable_testing()

# Include sub-projects.
add_subdirectory ("src")
add_subdirectory("tests")
