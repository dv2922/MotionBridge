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
option(MOTIONBRIDGE_ENABLE_S7 "Build the Snap7 Siemens S7 adapter" OFF)
option(MOTIONBRIDGE_FETCH_SNAP7 "Download Snap7 when the S7 adapter is enabled" ON)
set(SNAP7_ROOT "" CACHE PATH "Path to an existing Snap7 source package")

find_package(Threads REQUIRED)

add_library(motionbridge
    src/communication/mock_plc.cpp
    src/communication/opcua_config.cpp
    src/communication/plc_communication_worker.cpp
    src/communication/s7_db_layout.cpp
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

if(MOTIONBRIDGE_ENABLE_S7)
    if(MOTIONBRIDGE_FETCH_SNAP7)
        include(FetchContent)
        FetchContent_Declare(
            snap7
            GIT_REPOSITORY https://github.com/davenardella/snap7.git
            GIT_TAG 30f37da3114024a71ba93f7fd855c680b97a406f
            GIT_SHALLOW FALSE
            SOURCE_SUBDIR _motionbridge_no_cmake_project
        )
        FetchContent_MakeAvailable(snap7)
        FetchContent_GetProperties(snap7 SOURCE_DIR SNAP7_ROOT)
    endif()

    if(NOT SNAP7_ROOT)
        message(FATAL_ERROR
            "Snap7 was not downloaded. Set SNAP7_ROOT to an extracted Snap7 package.")
    endif()
    if(NOT WIN32)
        message(FATAL_ERROR
            "The current optional Snap7 packaging is configured for Windows only.")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SNAP7_PLATFORM_DIRECTORY win64)
    else()
        set(SNAP7_PLATFORM_DIRECTORY win32)
    endif()

    add_library(snap7 SHARED IMPORTED GLOBAL)
    set_target_properties(
        snap7
        PROPERTIES
            IMPORTED_IMPLIB
                "${SNAP7_ROOT}/build/bin/${SNAP7_PLATFORM_DIRECTORY}/snap7.lib"
            IMPORTED_LOCATION
                "${SNAP7_ROOT}/build/bin/${SNAP7_PLATFORM_DIRECTORY}/snap7.dll"
            INTERFACE_INCLUDE_DIRECTORIES
                "${SNAP7_ROOT}/release/wrappers/c-cpp"
    )

    add_library(motionbridge_s7 src/communication/s7_plc.cpp)
    target_include_directories(
        motionbridge_s7
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(motionbridge_s7 PUBLIC cxx_std_20)
    target_link_libraries(motionbridge_s7 PUBLIC motionbridge snap7)

    add_executable(motionbridge_s7_probe src/s7_probe.cpp)
    target_link_libraries(motionbridge_s7_probe PRIVATE motionbridge_s7)

    add_executable(motionbridge_s7_live_demo src/s7_live_demo.cpp)
    target_link_libraries(motionbridge_s7_live_demo PRIVATE motionbridge_s7)

    add_custom_command(
        TARGET motionbridge_s7_probe
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:snap7>"
            "$<TARGET_FILE_DIR:motionbridge_s7_probe>"
    )
    add_custom_command(
        TARGET motionbridge_s7_live_demo
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:snap7>"
            "$<TARGET_FILE_DIR:motionbridge_s7_live_demo>"
    )
endif()

if(MOTIONBRIDGE_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(motionbridge_tests tests/test_main.cpp)
    target_link_libraries(motionbridge_tests PRIVATE motionbridge)
    add_test(NAME motionbridge_tests COMMAND motionbridge_tests)
endif()
