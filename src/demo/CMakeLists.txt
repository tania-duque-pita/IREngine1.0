# Demo bootstrapper executable
add_executable(demo_bootstrapper
    demo_bootstrapper.cpp
)

target_link_libraries(demo_bootstrapper
    PRIVATE
        IREngine1.0
)

target_compile_features(demo_bootstrapper PRIVATE cxx_std_20)

# Ensure it can find the headers
target_include_directories(demo_bootstrapper
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)