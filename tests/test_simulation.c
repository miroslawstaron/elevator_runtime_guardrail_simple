/****************************************
*
* Elevator Lab -- Passenger Simulation Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

#include "simulation.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int checks = 0;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

/**
 * Advance one test tick and assert floor, capacity, guardrail and journey timing invariants
 */
static void tick(Simulation *s) {
    simulation_tick(s);
    CHECK(s->elevator.current_floor >= FLOOR_0 && s->elevator.current_floor <= FLOOR_9);
    CHECK(simulation_occupancy(s) <= CABIN_CAPACITY);
    CHECK(!s->guardrails.doors_open_violation);
    CHECK(!s->guardrails.between_floors_violation);
    CHECK(!s->guardrails.invalid_floor_violation);
    if (s->elevator.stop_button_pressed) CHECK(s->elevator.motion_ticks == 0);
    for (int i = 0; i < s->people_count; i++) {
        Passenger *p = &s->people[i];
        if (p->state == ARRIVED) {
            CHECK(p->boarded_tick >= p->added_tick);
            CHECK(p->arrived_tick > p->boarded_tick);
        }
    }
}
/**
 * Run all journeys to completion within a fixed budget
 * Print unfinished traffic before failing so dispatcher starvation is diagnosable
 */
static void finish(Simulation *s) {
    int budget = 100000;
    while (s->completed != s->people_count && budget--) tick(s);
    if (s->completed != s->people_count) {
        fprintf(stderr, "Stalled at floor %d, target %d, state %s, direction %d, occupancy %d\n", s->elevator.current_floor, s->elevator.target_floor, elevator_state_to_string(s->elevator.state), s->direction, simulation_occupancy(s));
        for (int i = 0; i < s->people_count; i++) if (s->people[i].state != ARRIVED)
            fprintf(stderr, "Pending %s: %d -> %d, state %d\n", s->people[i].name, s->people[i].from, s->people[i].to, s->people[i].state);
    }
    CHECK(s->completed == s->people_count);
}

/** Verify scheduled traffic, repeatability, VIP ordering, aging and cancellation against C state. */
static void scenario_checks(void) {
    static Simulation s, repeated;
    const char *kinds[] = {"morning", "afternoon", "occasional", "vip"};
    for (int kind = 0; kind < 4; kind++) {
        ScenarioConfig c = {0};
        snprintf(c.kind, sizeof(c.kind), "%s", kinds[kind]);
        c.count = kind < 2 ? 300 : kind == 2 ? 24 : 40;
        c.duration = kind < 2 ? 600 : kind == 2 ? 1200 : 300; c.seed = 42;
        simulation_init(&s); simulation_init(&repeated);
        CHECK(simulation_start_scenario(&s, &c)); CHECK(simulation_start_scenario(&repeated, &c));
        CHECK(s.people_count == c.count && s.people[0].state == WAITING && s.people[1].state == SCHEDULED);
        CHECK(!simulation_start_scenario(&s, &c));
        for (int i = 0; i < c.count; i++) {
            CHECK(s.people[i].from == repeated.people[i].from && s.people[i].to == repeated.people[i].to);
            CHECK(s.people[i].added_tick == repeated.people[i].added_tick);
            CHECK(s.people[i].from != s.people[i].to);
            if (kind == 0) CHECK(s.people[i].from == 0 && s.people[i].to > 0);
            if (kind == 1) CHECK(s.people[i].from > 0 && s.people[i].to == 0);
            if (kind == 3) CHECK(s.people[i].vip == (i % 5 == 0));
        }
        finish(&s); CHECK(s.completed == c.count);
    }
    ScenarioConfig c = {0}; snprintf(c.kind, sizeof(c.kind), "custom"); c.seed = 7;
    c.count = 10; c.flow_count = 2;
    c.flows[0] = (ScenarioFlow){1, 9, 9, 0, 0, 0};
    c.flows[1] = (ScenarioFlow){1, 9, 1, 0, 0, 1};
    simulation_init(&s); CHECK(simulation_start_scenario(&s, &c));
    for (int i = 0; i < 30 && s.people[9].state == WAITING; i++) tick(&s);
    CHECK(s.people[9].state == RIDING && simulation_occupancy(&s) == 8);
    CHECK(s.people[7].state == WAITING && s.people[8].state == WAITING);
    finish(&s);
    c.count = 2; c.flows[0] = (ScenarioFlow){0, 9, 1, 0, 0, 0};
    c.flows[1] = (ScenarioFlow){9, 0, 1, 1, 0, 1};
    simulation_init(&s); CHECK(simulation_start_scenario(&s, &c));
    s.tick = 1200; tick(&s);
    CHECK(s.elevator.target_floor == FLOOR_0); /* Old regular demand outranks a new VIP. */
    simulation_init(&s); c.flows[1].start = 10;
    CHECK(simulation_start_scenario(&s, &c));
    CHECK(simulation_control(&s, "cancel_scenario", 0));
    CHECK(s.people[0].state == WAITING && s.people[1].state == CANCELLED);
    for (int i = 0; i < 110; i++) tick(&s);
    CHECK(s.people[1].state == CANCELLED);
    CHECK(simulation_control(&s, "speed", 20) && s.speed == 20);
    CHECK(!simulation_control(&s, "speed", 2) && s.speed == 20);
    // ATOMIC VALIDATION: Invalid routes, totals and time multiplication never install partial streams.
    simulation_init(&s); c.flows[1].to = 9;
    CHECK(!simulation_start_scenario(&s, &c) && s.people_count == 0);
    c.flows[1].to = 0; c.flows[1].count = 1000; c.count = 1001;
    CHECK(!simulation_start_scenario(&s, &c));
    c.count = 1000; c.flows[0].count = 999; c.flows[1].count = 1; c.flows[0].interval = 86400;
    CHECK(!simulation_start_scenario(&s, &c));
    puts("PASS: all four traffic presets complete, deterministic schedules, VIP boarding, aging, cancellation and atomic custom validation");
}
/**
 * Advance with invariant checks until a target elevator state is reached
 * A finite budget turns a stalled transition into a test failure
 */
static void wait_for(Simulation *s, ElevatorState state) {
    int budget = 1000;
    while (s->elevator.state != state && budget--) tick(s);
    CHECK(s->elevator.state == state);
}
/**
 * Verify shared journeys, intermediate pickups, stop safety and bounded serialization
 * Finish with deterministic traffic at the maximum live passenger capacity
 */
int main(void) {
    Simulation s;
    simulation_init(&s);
    // SCENARIO 1: Mixed directions must all complete without starvation
    CHECK(simulation_control(&s, "demo", -1));
    finish(&s); CHECK(s.completed == 6);
    puts("PASS: mixed-direction busy morning completes every journey");

    simulation_init(&s);
    // SCENARIO 2: Groups exceeding cabin capacity wait for later trips
    CHECK(simulation_add_people(&s, "Up", 1, 9, 8));
    CHECK(simulation_add_people(&s, "Next group", 1, 8, 8));
    CHECK(simulation_add_people(&s, "Down", 1, 0, 3));
    CHECK(simulation_add_people(&s, "Return", 9, 0, 8));
    finish(&s); CHECK(s.completed == 27);
    puts("PASS: capacity overflow waits for later trips; opposite hall directions are served");

    simulation_init(&s);
    // SCENARIO 3: New requests can board and arrive before an existing through rider
    simulation_add_people(&s, "Through", 1, 9, 1);
    wait_for(&s, MOVING_UP);
    CHECK(simulation_add_people(&s, "Collect", 3, 7, 1));
    finish(&s);
    CHECK(s.people[1].boarded_tick < s.people[0].arrived_tick);
    CHECK(s.people[1].arrived_tick < s.people[0].arrived_tick);
    puts("PASS: new hall calls are collected and dropped off along the route");

    // SCENARIO 4: Emergency stops in both directions finish at a landing and preserve journeys
    for (int direction = 0; direction < 2; direction++) {
        simulation_init(&s);
        if (direction) s.elevator.current_floor = s.elevator.target_floor = FLOOR_8;
        simulation_add_people(&s, "Stop rider", direction ? 8 : 1, direction ? 0 : 9, 1);
        wait_for(&s, direction ? MOVING_DOWN : MOVING_UP);
        CHECK(s.elevator.motion_ticks > 0);
        CHECK(simulation_control(&s, "stop", -1));
        int budget = 20;
        while (!s.elevator.stop_button_pressed && budget--) tick(&s);
        CHECK(s.elevator.stop_button_pressed);
        Floor stopped = s.elevator.current_floor;
        for (int i = 0; i < 50; i++) tick(&s);
        CHECK(s.elevator.current_floor == stopped);
        CHECK(s.completed == 0);
        CHECK(simulation_control(&s, "resume", -1));
        finish(&s);
    }
    puts("PASS: stop in either travel direction secures a landing and preserves journeys");

    simulation_init(&s);
    // SCENARIO 5: Door dwell and manual requests obey moving-door interlocks
    CHECK(simulation_control(&s, "open", -1));
    wait_for(&s, DOORS_OPEN);
    for (int i = 0; i < 10; i++) tick(&s);
    CHECK(s.elevator.state == DOORS_OPEN);
    CHECK(simulation_control(&s, "request", 7));
    CHECK(simulation_control(&s, "close", -1));
    wait_for(&s, MOVING_UP);
    CHECK(!simulation_control(&s, "open", -1));
    CHECK(!simulation_control(&s, "close", -1));
    CHECK(!s.elevator.doors_open);
    for (int i = 0; i < 150; i++) tick(&s);
    CHECK(!s.requests[7]);
    puts("PASS: door dwell, manual controls, and moving-door interlocks");

    simulation_init(&s);
    // SCENARIO 6: Invalid inputs cannot partially mutate passengers or overflow serialized output
    CHECK(!simulation_add_people(&s, "", 0, 9, 1));
    CHECK(!simulation_add_people(&s, "No trip", 2, 2, 1));
    CHECK(!simulation_add_people(&s, "Invalid", -1, 2, 1));
    CHECK(!simulation_add_people(&s, "Invalid", 1, 10, 1));
    CHECK(!simulation_add_people(&s, "Too many", 0, 9, 9));
    CHECK(!simulation_add_people(&s, "Bad\nname", 0, 9, 1));
    CHECK(!simulation_add_people(&s, "Invalid\xff", 0, 9, 1));
    CHECK(!simulation_add_people(&s, "Overlong\xc0\xaf", 0, 9, 1));
    CHECK(!simulation_add_people(&s, "Truncated\xe2\x82", 0, 9, 1));
    CHECK(!simulation_add_people(&s, "Surrogate\xed\xa0\x80", 0, 9, 1));
    CHECK(!simulation_control(&s, "request", -1));
    CHECK(!simulation_control(&s, "request", 10));
    CHECK(!simulation_control(&s, "unknown", 0));
    CHECK(s.people_count == 0);
    CHECK(simulation_add_people(&s, "<script>\"\\", 1, 2, 1));
    static char json[512 * 1024];
    CHECK(simulation_json(&s, json, sizeof(json)) > 0);
    CHECK(strstr(json, "\\\"\\\\") != NULL);
    CHECK(simulation_json(&s, json, 10) == 0);
    puts("PASS: validation rejects invalid input atomically; JSON escaping and bounds");

    /* Deterministic adversarial traffic: all floors, both directions, mid-trip additions. */
    unsigned int random = 1234567;
    simulation_init(&s);
    for (int i = 0; i < MAX_PEOPLE; i++) {
        random = random * 1664525u + 1013904223u; int from = (int)((random >> 16) % 10);
        random = random * 1664525u + 1013904223u; int to = (from + 1 + (int)((random >> 16) % 9)) % 10;
        CHECK(simulation_add_people(&s, "Traffic", from, to, 1));
        for (int j = 0; j < 3; j++) tick(&s);
    }
    CHECK(!simulation_add_people(&s, "Overflow", 0, 1, 1));
    CHECK(!simulation_control(&s, "demo", 0));
    CHECK(s.people_count == MAX_PEOPLE);
    finish(&s);
    CHECK(simulation_json(&s, json, sizeof(json)) > 0);
    CHECK(simulation_control(&s, "reset", -1));
    CHECK(s.people_count == 0 && s.completed == 0 && s.tick == 0 && s.elevator.current_floor == FLOOR_1);
    puts("PASS: 1000 mixed journeys complete without starvation; session reset");
    scenario_checks();
    printf("%d simulation assertions passed.\n", checks);
    return 0;
}
