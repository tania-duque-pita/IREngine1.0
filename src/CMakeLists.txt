# CMakeList.txt : CMake project for IREngine1.0, include source and define
#
# Build a static library named IREngine1.0 so tests can link against IREngine1.0.lib

add_library(IREngine1.0 STATIC
    "core/date.cpp"
)

target_include_directories(IREngine1.0 PUBLIC ../include)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET IREngine1.0 PROPERTY CXX_STANDARD 20)
  set_property(TARGET IREngine1.0 PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET IREngine1.0 PROPERTY CXX_EXTENSIONS OFF)
endif()