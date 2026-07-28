cmake_minimum_required(VERSION 3.20)

project(
    MotionBridge
    VERSION 0.1.0
    DESCRIPTION "Vendor-independent industrial motion-control kernel"
    LANGUAGES CXX
)

option(MOTIONBRIDGE_BUILD_TESTS "Build MotionBridge tests" ON)
option(MOTIONBRIDGE_ENABLE_OPCUA "Build the open62541 OPC UA adapter" OFF)
option(MOTIONBRIDGE_FETCH_OPEN62541 "Download open62541 when OPC UA is enabled" ON)

find_package(Threads REQUIRED)

add_library(motionbridge
    src/communication/mock_plc.cpp
    src/communication/opcua_config.cpp
    src/communication/plc_communication_worker.cpp
    src/control/pid_controller.cpp
    src/control/trapezoidal_trajectory.cpp
    src/core/controller_state_machine.cpp
    src/interfaces/plc_mapping.cpp
    src/runtime/async_plc_supervisor.cpp
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

add_executable(motionbridge_async_plc_demo src/async_plc_demo.cpp)
target_link_libraries(motionbridge_async_plc_demo PRIVATE motionbridge)

if(MOTIONBRIDGE_ENABLE_OPCUA)
    if(MOTIONBRIDGE_FETCH_OPEN62541)
        include(FetchContent)
        set(UA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(UA_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(UA_ENABLE_ENCRYPTION OFF CACHE STRING "" FORCE)
        set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            open62541
            GIT_REPOSITORY https://github.com/open62541/open62541.git
            GIT_TAG v1.5.4
            GIT_SHALLOW TRUE
            GIT_SUBMODULES ""
            GIT_SUBMODULES_RECURSE FALSE
        )
        FetchContent_MakeAvailable(open62541)
    else()
        find_package(open62541 CONFIG REQUIRED)
    endif()

    add_library(motionbridge_opcua
        src/communication/opcua_plc.cpp
    )
    target_include_directories(
        motionbridge_opcua
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(motionbridge_opcua PUBLIC cxx_std_20)
    target_link_libraries(motionbridge_opcua PUBLIC motionbridge open62541::open62541)

    add_executable(motionbridge_opcua_probe src/opcua_probe.cpp)
    target_link_libraries(motionbridge_opcua_probe PRIVATE motionbridge_opcua)
endif()

if(MOTIONBRIDGE_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(motionbridge_tests tests/test_main.cpp)
    target_link_libraries(motionbridge_tests PRIVATE motionbridge)
    add_test(NAME motionbridge_tests COMMAND motionbridge_tests)
endif()
