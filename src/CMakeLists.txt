# Collect all library sources
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
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


# Add executables
add_executable(ire_price apps/ire_price.cpp)
target_link_libraries(ire_price PRIVATE IREngine1.0)
target_include_directories(ire_price PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(demo_bootstrapper apps/demo_bootstrapper.cpp)
target_link_libraries(demo_bootstrapper PRIVATE IREngine1.0)
target_include_directories(demo_bootstrapper PRIVATE ${PROJECT_SOURCE_DIR}/include)