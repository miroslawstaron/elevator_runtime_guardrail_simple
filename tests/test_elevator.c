#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <string.h>
#include "elevator.h"

// Suite setup/teardown
static Elevator *test_elev = NULL;

static int init_suite(void) {
    test_elev = (Elevator *)malloc(sizeof(Elevator));
    if (test_elev == NULL) return -1;
    return 0;
}

static int clean_suite(void) {
    free(test_elev);
    return 0;
}

// ============ INITIALIZATION TESTS ============
static void test_elevator_init(void) {
    elevator_init(test_elev);
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_1);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_1);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
    CU_ASSERT_FALSE(test_elev->doors_open);
    CU_ASSERT_FALSE(test_elev->stop_button_pressed);
    CU_ASSERT_EQUAL(test_elev->ticks, 0);
    CU_ASSERT_EQUAL(test_elev->motion_ticks, 0);
}

// ============ BUTTON PRESS TESTS ============
static void test_press_button_valid_floor(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_5]);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_5);
}

static void test_press_button_while_idle(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_3);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_3]);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_3);
}

static void test_press_button_same_floor(void) {
    elevator_init(test_elev);
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_1);
    elevator_press_button(test_elev, FLOOR_1);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_1]);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_1);
}

static void test_press_multiple_buttons(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_2);
    elevator_press_button(test_elev, FLOOR_5);
    elevator_press_button(test_elev, FLOOR_8);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_2]);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_5]);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_8]);
}

static void test_press_button_floor_0(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_0);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_0]);
}

static void test_press_button_floor_9(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_9);
    CU_ASSERT_TRUE(test_elev->button_pressed[FLOOR_9]);
}

// ============ EMERGENCY STOP TESTS ============
static void test_press_stop(void) {
    elevator_init(test_elev);
    elevator_press_stop(test_elev);
    CU_ASSERT_TRUE(test_elev->stop_button_pressed);
    CU_ASSERT_EQUAL(test_elev->state, EMERGENCY_STOP);
}

static void test_release_stop(void) {
    elevator_init(test_elev);
    elevator_press_stop(test_elev);
    CU_ASSERT_TRUE(test_elev->stop_button_pressed);
    elevator_release_stop(test_elev);
    CU_ASSERT_FALSE(test_elev->stop_button_pressed);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
}

// ============ STATE TRANSITION TESTS ============
static void test_idle_to_moving_up(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_START_UP);
}

static void test_idle_to_moving_down(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_5;
    test_elev->target_floor = FLOOR_5;
    elevator_press_button(test_elev, FLOOR_1);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_START_DOWN);
}

static void test_smooth_start_up_to_moving_up(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_START_UP);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, MOVING_UP);
}

static void test_smooth_start_down_to_moving_down(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_5;
    test_elev->target_floor = FLOOR_5;
    elevator_press_button(test_elev, FLOOR_1);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_START_DOWN);
    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, MOVING_DOWN);
}

// ============ MOVEMENT TESTS ============
static void test_moving_up_advances_floor(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);

    // Move to SMOOTH_START_UP then MOVING_UP
    elevator_tick(test_elev);  // -> SMOOTH_START_UP
    elevator_tick(test_elev);  // -> MOVING_UP

    Floor start_floor = test_elev->current_floor;

    // Advance 5 ticks to move one floor
    for (int i = 0; i < 5; i++) {
        elevator_tick(test_elev);
    }

    CU_ASSERT_EQUAL(test_elev->current_floor, start_floor + 1);
}

static void test_moving_down_decreases_floor(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_5;
    test_elev->target_floor = FLOOR_5;
    elevator_press_button(test_elev, FLOOR_2);

    // Move to SMOOTH_START_DOWN then MOVING_DOWN
    elevator_tick(test_elev);  // -> SMOOTH_START_DOWN
    elevator_tick(test_elev);  // -> MOVING_DOWN

    Floor start_floor = test_elev->current_floor;

    // Advance 5 ticks to move one floor
    for (int i = 0; i < 5; i++) {
        elevator_tick(test_elev);
    }

    CU_ASSERT_EQUAL(test_elev->current_floor, start_floor - 1);
}

static void test_floor_bounds_up(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_9;
    test_elev->state = MOVING_UP;
    test_elev->motion_ticks = 4;  // Next tick will try to move

    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_9);  // Should not exceed FLOOR_9
}

static void test_floor_bounds_down(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_0;
    test_elev->state = MOVING_DOWN;
    test_elev->motion_ticks = 4;  // Next tick will try to move

    elevator_tick(test_elev);
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_0);  // Should not go below FLOOR_0
}

// ============ DOOR OPERATION TESTS ============
static void test_doors_opening_animation(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_1);  // Already at floor 1

    // Advance to DOORS_OPENING state
    elevator_tick(test_elev);  // -> DOORS_OPENING
    CU_ASSERT_TRUE(test_elev->doors_open);
    CU_ASSERT_EQUAL(test_elev->state, DOORS_OPENING);
}

static void test_button_cleared_when_doors_open(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_1);

    // Reach DOORS_OPEN state
    elevator_tick(test_elev);  // -> DOORS_OPENING
    elevator_tick(test_elev);  // ticks++
    elevator_tick(test_elev);  // -> DOORS_OPEN, button cleared

    CU_ASSERT_FALSE(test_elev->button_pressed[FLOOR_1]);
}

static void test_is_between_floors_moving_up(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);
    elevator_tick(test_elev);  // -> SMOOTH_START_UP
    CU_ASSERT_TRUE(elevator_is_between_floors(test_elev));
}

static void test_is_between_floors_moving_down(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_5;
    test_elev->target_floor = FLOOR_5;
    elevator_press_button(test_elev, FLOOR_2);
    elevator_tick(test_elev);  // -> SMOOTH_START_DOWN
    CU_ASSERT_TRUE(elevator_is_between_floors(test_elev));
}

static void test_is_not_between_floors_idle(void) {
    elevator_init(test_elev);
    CU_ASSERT_FALSE(elevator_is_between_floors(test_elev));
}

static void test_elevator_get_position(void) {
    elevator_init(test_elev);
    CU_ASSERT_EQUAL(elevator_get_position(test_elev), 1.0f);

    test_elev->current_floor = FLOOR_5;
    CU_ASSERT_EQUAL(elevator_get_position(test_elev), 5.0f);
}

// ============ STATE STRING TESTS ============
static void test_state_to_string_idle(void) {
    const char *state_str = elevator_state_to_string(IDLE);
    CU_ASSERT_STRING_EQUAL(state_str, "IDLE");
}

static void test_state_to_string_moving_up(void) {
    const char *state_str = elevator_state_to_string(MOVING_UP);
    CU_ASSERT_STRING_EQUAL(state_str, "MOVING_UP");
}

static void test_state_to_string_emergency_stop(void) {
    const char *state_str = elevator_state_to_string(EMERGENCY_STOP);
    CU_ASSERT_STRING_EQUAL(state_str, "EMERGENCY_STOP");
}

// ============ INTEGRATION TESTS ============
static void test_complete_journey_up(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_3);

    // Simulate until we reach floor 3
    while (test_elev->current_floor != FLOOR_3 || test_elev->state != IDLE) {
        elevator_tick(test_elev);
        if (test_elev->ticks > 1000) break;  // Safety guard
    }

    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_3);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
    CU_ASSERT_FALSE(test_elev->button_pressed[FLOOR_3]);
}

static void test_complete_journey_down(void) {
    elevator_init(test_elev);
    test_elev->current_floor = FLOOR_7;
    test_elev->target_floor = FLOOR_7;
    elevator_press_button(test_elev, FLOOR_2);

    // Simulate until we reach floor 2
    while (test_elev->current_floor != FLOOR_2 || test_elev->state != IDLE) {
        elevator_tick(test_elev);
        if (test_elev->ticks > 1000) break;  // Safety guard
    }

    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_2);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
}

static void test_multiple_stops(void) {
    elevator_init(test_elev);

    // First stop at floor 3
    elevator_press_button(test_elev, FLOOR_3);
    while (test_elev->current_floor != FLOOR_3 || test_elev->state != IDLE) {
        elevator_tick(test_elev);
        if (test_elev->ticks > 500) break;
    }
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_3);

    // Second stop at floor 7
    elevator_press_button(test_elev, FLOOR_7);
    while (test_elev->current_floor != FLOOR_7 || test_elev->state != IDLE) {
        elevator_tick(test_elev);
        if (test_elev->ticks > 500) break;
    }
    CU_ASSERT_EQUAL(test_elev->current_floor, FLOOR_7);
}

// ============ SUITE SETUP ============
CU_ErrorCode add_elevator_tests(void) {
    CU_pSuite pSuite = NULL;

    pSuite = CU_add_suite("Elevator Tests", init_suite, clean_suite);
    if (pSuite == NULL) {
        return CU_get_error();
    }

    CU_add_test(pSuite, "Initialization", test_elevator_init);
    CU_add_test(pSuite, "Press button valid floor", test_press_button_valid_floor);
    CU_add_test(pSuite, "Press button while idle", test_press_button_while_idle);
    CU_add_test(pSuite, "Press button same floor", test_press_button_same_floor);
    CU_add_test(pSuite, "Press multiple buttons", test_press_multiple_buttons);
    CU_add_test(pSuite, "Press button floor 0", test_press_button_floor_0);
    CU_add_test(pSuite, "Press button floor 9", test_press_button_floor_9);
    CU_add_test(pSuite, "Emergency stop press", test_press_stop);
    CU_add_test(pSuite, "Emergency stop release", test_release_stop);
    CU_add_test(pSuite, "Idle to moving up", test_idle_to_moving_up);
    CU_add_test(pSuite, "Idle to moving down", test_idle_to_moving_down);
    CU_add_test(pSuite, "Smooth start up to moving up", test_smooth_start_up_to_moving_up);
    CU_add_test(pSuite, "Smooth start down to moving down", test_smooth_start_down_to_moving_down);
    CU_add_test(pSuite, "Moving up advances floor", test_moving_up_advances_floor);
    CU_add_test(pSuite, "Moving down decreases floor", test_moving_down_decreases_floor);
    CU_add_test(pSuite, "Floor bounds up", test_floor_bounds_up);
    CU_add_test(pSuite, "Floor bounds down", test_floor_bounds_down);
    CU_add_test(pSuite, "Doors opening animation", test_doors_opening_animation);
    CU_add_test(pSuite, "Button cleared when doors open", test_button_cleared_when_doors_open);
    CU_add_test(pSuite, "Is between floors moving up", test_is_between_floors_moving_up);
    CU_add_test(pSuite, "Is between floors moving down", test_is_between_floors_moving_down);
    CU_add_test(pSuite, "Is not between floors idle", test_is_not_between_floors_idle);
    CU_add_test(pSuite, "Get position", test_elevator_get_position);
    CU_add_test(pSuite, "State to string idle", test_state_to_string_idle);
    CU_add_test(pSuite, "State to string moving up", test_state_to_string_moving_up);
    CU_add_test(pSuite, "State to string emergency stop", test_state_to_string_emergency_stop);
    CU_add_test(pSuite, "Complete journey up", test_complete_journey_up);
    CU_add_test(pSuite, "Complete journey down", test_complete_journey_down);
    CU_add_test(pSuite, "Multiple stops", test_multiple_stops);

    return CU_get_error();
}
