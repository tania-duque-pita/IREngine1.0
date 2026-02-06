# Collect all library sources
file(GLOB_RECURSE LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ir/*.cpp"
)

# Create the library
add_library(IREngine1.0 STATIC ${LIB_SOURCES})

# Set include directories
target_include_directories(IREngine1.0
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link system libraries if needed
target_compile_features(IREngine1.0 PUBLIC cxx_std_20)

# Add demo executable
add_subdirectory(demo)