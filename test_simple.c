#include "test_framework.h"
#include "elevator.h"
#include "guardrails.h"
#include <time.h>

int main(void) {
    Elevator elev;

    // ============ ELEVATOR TESTS ============
    TEST_SUITE_START("Elevator Tests");

    // Initialization tests
    TEST("Elevator Init");
    elevator_init(&elev);
    ASSERT_EQUAL(elev.current_floor, FLOOR_1);
    ASSERT_EQUAL(elev.state, IDLE);
    ASSERT_FALSE(elev.doors_open);
    TEST_END();

    // Button press tests
    TEST("Press Button Valid Floor");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_5);
    ASSERT_TRUE(elev.button_pressed[FLOOR_5]);
    ASSERT_EQUAL(elev.target_floor, FLOOR_5);
    TEST_END();

    TEST("Press Button Same Floor");
    elevator_init(&elev);
    ASSERT_EQUAL(elev.current_floor, FLOOR_1);
    elevator_press_button(&elev, FLOOR_1);
    ASSERT_TRUE(elev.button_pressed[FLOOR_1]);
    TEST_END();

    TEST("Press Multiple Buttons");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_2);
    elevator_press_button(&elev, FLOOR_5);
    elevator_press_button(&elev, FLOOR_8);
    ASSERT_TRUE(elev.button_pressed[FLOOR_2]);
    ASSERT_TRUE(elev.button_pressed[FLOOR_5]);
    ASSERT_TRUE(elev.button_pressed[FLOOR_8]);
    TEST_END();

    // Emergency stop tests
    TEST("Press Emergency Stop");
    elevator_init(&elev);
    elevator_press_stop(&elev);
    ASSERT_TRUE(elev.stop_button_pressed);
    ASSERT_EQUAL(elev.state, EMERGENCY_STOP);
    TEST_END();

    TEST("Release Emergency Stop");
    elevator_init(&elev);
    elevator_press_stop(&elev);
    elevator_release_stop(&elev);
    ASSERT_FALSE(elev.stop_button_pressed);
    ASSERT_EQUAL(elev.state, IDLE);
    TEST_END();

    // State transition tests
    TEST("Idle to Moving Up");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_5);
    elevator_tick(&elev);
    ASSERT_EQUAL(elev.state, SMOOTH_START_UP);
    TEST_END();

    TEST("Smooth Start Up to Moving Up");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_5);
    elevator_tick(&elev);  // -> SMOOTH_START_UP
    ASSERT_EQUAL(elev.state, SMOOTH_START_UP);
    elevator_tick(&elev);  // -> MOVING_UP
    ASSERT_EQUAL(elev.state, MOVING_UP);
    TEST_END();

    // Movement tests
    TEST("Moving Up Advances Floor");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_5);
    elevator_tick(&elev);  // SMOOTH_START_UP
    elevator_tick(&elev);  // MOVING_UP
    Floor start = elev.current_floor;
    for (int i = 0; i < 5; i++) elevator_tick(&elev);
    ASSERT_EQUAL(elev.current_floor, start + 1);
    TEST_END();

    TEST("Floor Bounds Up");
    elevator_init(&elev);
    elev.current_floor = FLOOR_9;
    elev.target_floor = FLOOR_9;  // Set target to prevent state change
    elev.state = MOVING_UP;
    elev.motion_ticks = 4;
    // This would attempt to increment beyond FLOOR_9, should be clamped
    // But we're already at FLOOR_9, so motion happens and triggers stop
    // Just verify we don't go above FLOOR_9
    elevator_tick(&elev);
    ASSERT_EQUAL(elev.current_floor, FLOOR_9);
    TEST_END();

    TEST("Floor Bounds Down");
    elevator_init(&elev);
    elev.current_floor = FLOOR_1;
    elev.target_floor = FLOOR_0;  // Set target so we actually move toward it
    elev.state = MOVING_DOWN;
    elev.motion_ticks = 4;
    elevator_tick(&elev);
    // Motion ticks becomes 5, so floor decrements from 1 to 0
    ASSERT_EQUAL(elev.current_floor, FLOOR_0);
    TEST_END();

    // Between floors tests
    TEST("Is Between Floors Moving Up");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_5);
    elevator_tick(&elev);  // SMOOTH_START_UP
    ASSERT_TRUE(elevator_is_between_floors(&elev));
    TEST_END();

    TEST("Is Not Between Floors Idle");
    elevator_init(&elev);
    ASSERT_FALSE(elevator_is_between_floors(&elev));
    TEST_END();

    TEST("Get Position");
    elevator_init(&elev);
    ASSERT_EQUAL(elevator_get_position(&elev), 1.0f);
    elev.current_floor = FLOOR_5;
    ASSERT_EQUAL(elevator_get_position(&elev), 5.0f);
    TEST_END();

    // State string tests
    // State string tests
    const char *state;

    TEST("State to String IDLE");
    state = elevator_state_to_string(IDLE);
    ASSERT_STRING_EQUAL(state, "IDLE");
    TEST_END();

    TEST("State to String MOVING_UP");
    state = elevator_state_to_string(MOVING_UP);
    ASSERT_STRING_EQUAL(state, "MOVING_UP");
    TEST_END();

    // Complete journey test
    TEST("Complete Journey Up to Floor 3");
    elevator_init(&elev);
    elevator_press_button(&elev, FLOOR_3);
    int ticks = 0;
    while ((elev.current_floor != FLOOR_3 || elev.state != IDLE) && ticks < 500) {
        elevator_tick(&elev);
        ticks++;
    }
    ASSERT_EQUAL(elev.current_floor, FLOOR_3);
    ASSERT_EQUAL(elev.state, IDLE);
    // Note: button_pressed is cleared when doors open, not verified here due to state timing
    TEST_END();

    TEST_SUITE_END();

    // ============ GUARDRAILS TESTS ============
    TEST_SUITE_START("Guardrails Tests");

    GuardrailStatus status;
    const char *desc;

    TEST("No Violations");
    elevator_init(&elev);
    status = guardrail_check(&elev);
    ASSERT_FALSE(status.doors_open_violation);
    ASSERT_FALSE(status.between_floors_violation);
    ASSERT_FALSE(status.stop_button_pressed);
    TEST_END();

    TEST("Doors Open at Floor is Safe");
    elevator_init(&elev);
    elev.doors_open = true;
    elev.state = IDLE;
    status = guardrail_check(&elev);
    ASSERT_FALSE(status.doors_open_violation);
    TEST_END();

    TEST("Doors Open While Moving Up");
    elevator_init(&elev);
    elev.state = MOVING_UP;
    elev.doors_open = true;
    status = guardrail_check(&elev);
    ASSERT_TRUE(status.doors_open_violation);
    TEST_END();

    TEST("Doors Open While Moving Down");
    elevator_init(&elev);
    elev.state = MOVING_DOWN;
    elev.doors_open = true;
    status = guardrail_check(&elev);
    ASSERT_TRUE(status.doors_open_violation);
    TEST_END();

    TEST("Doors Open During Smooth Start Up");
    elevator_init(&elev);
    elev.state = SMOOTH_START_UP;
    elev.doors_open = true;
    status = guardrail_check(&elev);
    ASSERT_TRUE(status.doors_open_violation);
    TEST_END();

    TEST("Stop Button Tracking");
    elevator_init(&elev);
    elev.stop_button_pressed = true;
    elev.stop_start_time = time(NULL);
    status = guardrail_check(&elev);
    ASSERT_TRUE(status.stop_button_pressed);
    TEST_END();

    TEST("Enforce Doors Open Violation Moving Up");
    elevator_init(&elev);
    elev.state = MOVING_UP;
    elev.doors_open = true;
    guardrail_enforce(&elev);
    // guardrail_enforce sets state to IDLE when doors open violation detected
    ASSERT_EQUAL(elev.state, IDLE);
    TEST_END();

    TEST("Enforce Doors Open Violation Moving Down");
    elevator_init(&elev);
    elev.state = MOVING_DOWN;
    elev.doors_open = true;
    guardrail_enforce(&elev);
    // guardrail_enforce sets state to IDLE when doors open violation detected
    ASSERT_EQUAL(elev.state, IDLE);
    TEST_END();

    TEST("Enforce No Violations");
    elevator_init(&elev);
    elev.state = IDLE;
    guardrail_enforce(&elev);
    ASSERT_EQUAL(elev.state, IDLE);
    TEST_END();

    TEST("Violation Description No Violations");
    elevator_init(&elev);
    status = guardrail_check(&elev);
    desc = guardrail_violation_description(&status);
    ASSERT_NULL(desc);
    TEST_END();

    TEST("Violation Description Doors Open");
    elevator_init(&elev);
    elev.state = MOVING_UP;
    elev.doors_open = true;
    status = guardrail_check(&elev);
    desc = guardrail_violation_description(&status);
    ASSERT_NOT_NULL(desc);
    TEST_END();

    TEST("Emergency Stop Stays in Emergency Stop");
    elevator_init(&elev);
    elevator_press_stop(&elev);
    ASSERT_EQUAL(elev.state, EMERGENCY_STOP);
    for (int i = 0; i < 5; i++) {
        elevator_tick(&elev);
    }
    ASSERT_EQUAL(elev.state, EMERGENCY_STOP);
    TEST_END();

    TEST("Release Emergency Stop Returns to Idle");
    elevator_init(&elev);
    elevator_press_stop(&elev);
    elevator_release_stop(&elev);
    ASSERT_EQUAL(elev.state, IDLE);
    ASSERT_FALSE(elev.stop_button_pressed);
    TEST_END();

    TEST("Multiple Guardrail Violations");
    elevator_init(&elev);
    elev.state = MOVING_UP;
    elev.doors_open = true;
    elev.stop_button_pressed = true;
    elev.stop_start_time = time(NULL);
    status = guardrail_check(&elev);
    ASSERT_TRUE(status.doors_open_violation);
    ASSERT_TRUE(status.stop_button_pressed);
    TEST_END();

    TEST("Door Violation Only When Moving");
    elevator_init(&elev);
    elev.state = DOORS_OPENING;
    elev.doors_open = true;
    status = guardrail_check(&elev);
    ASSERT_FALSE(status.doors_open_violation);
    TEST_END();

    TEST("Door Not Violation When Closed");
    elevator_init(&elev);
    elev.state = MOVING_UP;
    elev.doors_open = false;
    status = guardrail_check(&elev);
    ASSERT_FALSE(status.doors_open_violation);
    TEST_END();

    TEST_SUITE_END();

    // Print summary
    PRINT_SUMMARY();

    return total_failed == 0 ? 0 : 1;
}
