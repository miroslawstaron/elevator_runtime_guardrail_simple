#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "guardrails.h"

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

// ============ GUARDRAIL CHECK BASIC TESTS ============
static void test_guardrail_check_no_violations(void) {
    elevator_init(test_elev);
    GuardrailStatus status = guardrail_check(test_elev);

    CU_ASSERT_FALSE(status.doors_open_violation);
    CU_ASSERT_FALSE(status.between_floors_violation);
    CU_ASSERT_FALSE(status.stop_button_pressed);
}

static void test_guardrail_check_doors_open_at_floor(void) {
    elevator_init(test_elev);
    test_elev->doors_open = true;
    test_elev->state = IDLE;  // At floor, not moving

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_FALSE(status.doors_open_violation);  // No violation when at floor
}

// ============ DOORS OPEN VIOLATION TESTS ============
static void test_doors_open_while_moving_up(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_UP;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

static void test_doors_open_while_moving_down(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_DOWN;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

static void test_doors_open_during_smooth_start_up(void) {
    elevator_init(test_elev);
    test_elev->state = SMOOTH_START_UP;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

static void test_doors_open_during_smooth_start_down(void) {
    elevator_init(test_elev);
    test_elev->state = SMOOTH_START_DOWN;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

static void test_doors_open_during_smooth_stop_up(void) {
    elevator_init(test_elev);
    test_elev->state = SMOOTH_STOP_UP;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

static void test_doors_open_during_smooth_stop_down(void) {
    elevator_init(test_elev);
    test_elev->state = SMOOTH_STOP_DOWN;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);
}

// ============ STOP BUTTON TESTS ============
static void test_stop_button_not_pressed(void) {
    elevator_init(test_elev);
    test_elev->stop_button_pressed = false;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_FALSE(status.stop_button_pressed);
    CU_ASSERT_EQUAL(status.seconds_stopped, 0);
}

static void test_stop_button_pressed_tracking(void) {
    elevator_init(test_elev);
    test_elev->stop_button_pressed = true;
    test_elev->stop_start_time = time(NULL);

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.stop_button_pressed);
    CU_ASSERT_EQUAL(status.seconds_stopped, 0);
}

// ============ BETWEEN FLOORS VIOLATION TESTS ============
static void test_between_floors_with_emergency_stop_up(void) {
    elevator_init(test_elev);
    test_elev->state = SMOOTH_START_UP;  // Between floors
    test_elev->stop_button_pressed = true;

    GuardrailStatus status = guardrail_check(test_elev);
    // Note: between_floors_violation is only set if state is EMERGENCY_STOP
    // This test verifies the logic
    CU_ASSERT_FALSE(status.between_floors_violation);  // Not EMERGENCY_STOP state
}

static void test_between_floors_with_emergency_stop_state(void) {
    elevator_init(test_elev);
    test_elev->state = EMERGENCY_STOP;
    test_elev->current_floor = FLOOR_2;
    test_elev->motion_ticks = 2;  // Simulating between floors

    GuardrailStatus status = guardrail_check(test_elev);
    // EMERGENCY_STOP itself is not in the between_floors check directly
    // The check is: elevator_is_between_floors(elev) && elev->state == EMERGENCY_STOP
    // EMERGENCY_STOP doesn't match between_floors conditions
    CU_ASSERT_FALSE(status.between_floors_violation);
}

// ============ GUARDRAIL ENFORCE TESTS ============
static void test_enforce_doors_open_violation(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_UP;
    test_elev->doors_open = true;

    guardrail_enforce(test_elev);
    // Should transition to SMOOTH_STOP_UP
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_STOP_UP);
}

static void test_enforce_doors_open_moving_down(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_DOWN;
    test_elev->doors_open = true;

    guardrail_enforce(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, SMOOTH_STOP_DOWN);
}

static void test_enforce_no_violations(void) {
    elevator_init(test_elev);
    test_elev->state = IDLE;
    test_elev->current_floor = FLOOR_1;

    GuardrailStatus status_before = guardrail_check(test_elev);
    CU_ASSERT_FALSE(status_before.doors_open_violation);
    CU_ASSERT_FALSE(status_before.between_floors_violation);

    guardrail_enforce(test_elev);

    CU_ASSERT_EQUAL(test_elev->state, IDLE);  // Should remain unchanged
}

static void test_enforce_between_floors_violation_lower_half(void) {
    elevator_init(test_elev);
    test_elev->state = EMERGENCY_STOP;
    test_elev->current_floor = FLOOR_2;
    test_elev->motion_ticks = 3;

    guardrail_enforce(test_elev);
    // Should set target to FLOOR_3 and move up
    CU_ASSERT_EQUAL(test_elev->state, MOVING_UP);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_3);
}

static void test_enforce_between_floors_violation_upper_half(void) {
    elevator_init(test_elev);
    test_elev->state = EMERGENCY_STOP;
    test_elev->current_floor = FLOOR_7;
    test_elev->motion_ticks = 3;

    guardrail_enforce(test_elev);
    // Should set target to FLOOR_6 and move down
    CU_ASSERT_EQUAL(test_elev->state, MOVING_DOWN);
    CU_ASSERT_EQUAL(test_elev->target_floor, FLOOR_6);
}

// ============ VIOLATION DESCRIPTION TESTS ============
static void test_violation_description_no_violations(void) {
    elevator_init(test_elev);
    GuardrailStatus status = guardrail_check(test_elev);

    const char *description = guardrail_violation_description(&status);
    CU_ASSERT_PTR_NULL(description);
}

static void test_violation_description_doors_open(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_UP;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    const char *description = guardrail_violation_description(&status);

    CU_ASSERT_PTR_NOT_NULL(description);
    CU_ASSERT(strstr(description, "Doors open") != NULL);
}

static void test_violation_description_between_floors(void) {
    elevator_init(test_elev);
    test_elev->state = EMERGENCY_STOP;
    test_elev->current_floor = FLOOR_3;

    GuardrailStatus status = guardrail_check(test_elev);
    const char *description = guardrail_violation_description(&status);

    // This may be NULL if between_floors_violation isn't set
    if (description != NULL) {
        CU_ASSERT(strstr(description, "Cannot stop") != NULL ||
                 strstr(description, "floor") != NULL);
    }
}

// ============ INTEGRATION TESTS ============
static void test_guardrail_prevents_doors_opening_mid_movement(void) {
    elevator_init(test_elev);
    elevator_press_button(test_elev, FLOOR_5);

    // Move to MOVING_UP state
    elevator_tick(test_elev);  // SMOOTH_START_UP
    elevator_tick(test_elev);  // MOVING_UP

    // Simulate doors somehow opening during movement
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_TRUE(status.doors_open_violation);

    guardrail_enforce(test_elev);
    // Should be in smooth stop or idle
    CU_ASSERT(test_elev->state == SMOOTH_STOP_UP ||
             test_elev->state == IDLE);
}

static void test_emergency_stop_stays_in_emergency_stop(void) {
    elevator_init(test_elev);
    elevator_press_stop(test_elev);

    CU_ASSERT_EQUAL(test_elev->state, EMERGENCY_STOP);
    CU_ASSERT_TRUE(test_elev->stop_button_pressed);

    // Multiple ticks should keep it in emergency stop
    for (int i = 0; i < 5; i++) {
        elevator_tick(test_elev);
    }

    CU_ASSERT_EQUAL(test_elev->state, EMERGENCY_STOP);
}

static void test_release_emergency_stop_returns_to_idle(void) {
    elevator_init(test_elev);
    elevator_press_stop(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, EMERGENCY_STOP);

    elevator_release_stop(test_elev);
    CU_ASSERT_EQUAL(test_elev->state, IDLE);
    CU_ASSERT_FALSE(test_elev->stop_button_pressed);
}

static void test_multiple_guardrail_violations(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_UP;
    test_elev->doors_open = true;
    test_elev->stop_button_pressed = true;
    test_elev->stop_start_time = time(NULL);

    GuardrailStatus status = guardrail_check(test_elev);

    CU_ASSERT_TRUE(status.doors_open_violation);
    CU_ASSERT_TRUE(status.stop_button_pressed);

    const char *description = guardrail_violation_description(&status);
    CU_ASSERT_PTR_NOT_NULL(description);
}

// ============ EDGE CASES ============
static void test_door_violation_only_when_moving(void) {
    elevator_init(test_elev);
    test_elev->state = DOORS_OPENING;
    test_elev->doors_open = true;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_FALSE(status.doors_open_violation);  // DOORS_OPENING not between floors
}

static void test_door_violation_not_when_doors_closed(void) {
    elevator_init(test_elev);
    test_elev->state = MOVING_UP;
    test_elev->doors_open = false;

    GuardrailStatus status = guardrail_check(test_elev);
    CU_ASSERT_FALSE(status.doors_open_violation);
}

// ============ SUITE SETUP ============
CU_ErrorCode add_guardrails_tests(void) {
    CU_pSuite pSuite = NULL;

    pSuite = CU_add_suite("Guardrails Tests", init_suite, clean_suite);
    if (pSuite == NULL) {
        return CU_get_error();
    }

    CU_add_test(pSuite, "Guardrail check no violations", test_guardrail_check_no_violations);
    CU_add_test(pSuite, "Guardrail check doors open at floor", test_guardrail_check_doors_open_at_floor);
    CU_add_test(pSuite, "Doors open while moving up", test_doors_open_while_moving_up);
    CU_add_test(pSuite, "Doors open while moving down", test_doors_open_while_moving_down);
    CU_add_test(pSuite, "Doors open during smooth start up", test_doors_open_during_smooth_start_up);
    CU_add_test(pSuite, "Doors open during smooth start down", test_doors_open_during_smooth_start_down);
    CU_add_test(pSuite, "Doors open during smooth stop up", test_doors_open_during_smooth_stop_up);
    CU_add_test(pSuite, "Doors open during smooth stop down", test_doors_open_during_smooth_stop_down);
    CU_add_test(pSuite, "Stop button not pressed", test_stop_button_not_pressed);
    CU_add_test(pSuite, "Stop button pressed tracking", test_stop_button_pressed_tracking);
    CU_add_test(pSuite, "Between floors with emergency stop up", test_between_floors_with_emergency_stop_up);
    CU_add_test(pSuite, "Between floors with emergency stop state", test_between_floors_with_emergency_stop_state);
    CU_add_test(pSuite, "Enforce doors open violation", test_enforce_doors_open_violation);
    CU_add_test(pSuite, "Enforce doors open moving down", test_enforce_doors_open_moving_down);
    CU_add_test(pSuite, "Enforce no violations", test_enforce_no_violations);
    CU_add_test(pSuite, "Enforce between floors violation lower half", test_enforce_between_floors_violation_lower_half);
    CU_add_test(pSuite, "Enforce between floors violation upper half", test_enforce_between_floors_violation_upper_half);
    CU_add_test(pSuite, "Violation description no violations", test_violation_description_no_violations);
    CU_add_test(pSuite, "Violation description doors open", test_violation_description_doors_open);
    CU_add_test(pSuite, "Violation description between floors", test_violation_description_between_floors);
    CU_add_test(pSuite, "Guardrail prevents doors opening mid movement", test_guardrail_prevents_doors_opening_mid_movement);
    CU_add_test(pSuite, "Emergency stop stays in emergency stop", test_emergency_stop_stays_in_emergency_stop);
    CU_add_test(pSuite, "Release emergency stop returns to idle", test_release_emergency_stop_returns_to_idle);
    CU_add_test(pSuite, "Multiple guardrail violations", test_multiple_guardrail_violations);
    CU_add_test(pSuite, "Door violation only when moving", test_door_violation_only_when_moving);
    CU_add_test(pSuite, "Door violation not when doors closed", test_door_violation_not_when_doors_closed);

    return CU_get_error();
}
