cmake_minimum_required(VERSION 3.20)

project(
    MotionBridge
    VERSION 0.1.0
    DESCRIPTION "Vendor-independent industrial motion-control kernel"
    LANGUAGES CXX
)

option(MOTIONBRIDGE_BUILD_TESTS "Build MotionBridge tests" ON)

find_package(Threads REQUIRED)

add_library(motionbridge
    src/communication/mock_plc.cpp
    src/control/pid_controller.cpp
    src/control/trapezoidal_trajectory.cpp
    src/core/controller_state_machine.cpp
    src/interfaces/plc_mapping.cpp
    src/runtime/control_loop.cpp
    src/runtime/plc_supervisor.cpp
    src/runtime/watchdog.cpp
    src/simulation/servo_plant.cpp
)

target_include_directories(
    motionbridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(motionbridge PUBLIC cxx_std_20)
target_link_libraries(motionbridge PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(motionbridge PRIVATE /W4 /permissive-)
else()
    target_compile_options(motionbridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(motionbridge_demo src/main.cpp)
target_link_libraries(motionbridge_demo PRIVATE motionbridge)

add_executable(motionbridge_plc_demo src/plc_demo.cpp)
target_link_libraries(motionbridge_plc_demo PRIVATE motionbridge)

if(MOTIONBRIDGE_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(motionbridge_tests tests/test_main.cpp)
    target_link_libraries(motionbridge_tests PRIVATE motionbridge)
    add_test(NAME motionbridge_tests COMMAND motionbridge_tests)
endif()
