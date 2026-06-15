cmake_minimum_required(VERSION 3.10)
project(elevator_guardrails C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Add include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(CORE_SOURCES
    src/elevator.c
    src/guardrails.c
)

# Find CUnit (optional)
find_package(CUnit QUIET)

# Add main executable
add_executable(elevator_guardrails
    ${CORE_SOURCES}
    src/main.c
)

# Add simple test executable (no dependencies)
add_executable(test_simple
    ${CORE_SOURCES}
    tests/test_simple.c
)

# Add CUnit test executable (optional)
if(CUnit_FOUND)
    add_executable(test_elevator_guardrails
        ${CORE_SOURCES}
        tests/test_elevator.c
        tests/test_guardrails.c
        tests/test_runner.c
    )
    target_link_libraries(test_elevator_guardrails PRIVATE CUnit::CUnit)

    # Add test target for CUnit
    enable_testing()
    add_test(NAME elevator_guardrails_tests COMMAND test_elevator_guardrails)
else()
    message(STATUS "CUnit not found - CUnit tests will not be built")
endif()

# Add test target for simple tests
enable_testing()
add_test(NAME simple_tests COMMAND test_simple)

# Set output directories
set_target_properties(elevator_guardrails PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
set_target_properties(test_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(CUnit_FOUND)
    set_target_properties(test_elevator_guardrails PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Print build information
message(STATUS "====================================")
message(STATUS "Elevator Control System - Build Info")
message(STATUS "====================================")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "C Compiler Flags: ${CMAKE_C_FLAGS}")
message(STATUS "Include Directory: ${CMAKE_SOURCE_DIR}/include")
message(STATUS "CUnit Support: ${CUnit_FOUND}")
message(STATUS "=====================================")
