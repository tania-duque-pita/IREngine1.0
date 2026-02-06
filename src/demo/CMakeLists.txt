# Demo bootstrapper executable
add_executable(demo_bootstrapper
    demo_bootstrapper.cpp
)

target_link_libraries(demo_bootstrapper
    PRIVATE IREngine1.0
)