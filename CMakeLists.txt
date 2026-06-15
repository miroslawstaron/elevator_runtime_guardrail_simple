cmake_minimum_required(VERSION 3.10)
project(elevator_guardrails C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Find CUnit
find_package(CUnit REQUIRED)

# Add main executable
add_executable(elevator_guardrails 
    elevator.c 
    guardrails.c 
    main.c
)

# Add test executable
add_executable(test_elevator_guardrails 
    elevator.c 
    guardrails.c 
    test_elevator.c 
    test_guardrails.c 
    test_runner.c
)

target_link_libraries(test_elevator_guardrails PRIVATE CUnit::CUnit)

# Add test target
enable_testing()
add_test(NAME elevator_guardrails_tests COMMAND test_elevator_guardrails)
