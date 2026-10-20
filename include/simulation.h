/****************************************
*
* Elevator Lab -- Simulation State and Public Interface
*
* SPDX-License-Identifier: MIT
*
******************************************/

#ifndef SIMULATION_H
#define SIMULATION_H

#include "elevator.h"
#include "guardrails.h"
#include <stddef.h>

/**
 * Fixed limits keep live state, JSON responses and cabin occupancy bounded
 */
#define MAX_PEOPLE 1000
#define CABIN_CAPACITY 8
#define MAX_EVENTS 48

/**
 * Passenger states describe one journey within the current simulation run
 */
typedef enum { WAITING, RIDING, ARRIVED, SCHEDULED, CANCELLED } PassengerState;
/**
 * Passenger identity is local to a run; all journey timestamps count 100 ms ticks
 */
typedef struct {
    int id, from, to;
    char name[49];
    PassengerState state;
    bool vip;
    unsigned long added_tick, boarded_tick, arrived_tick;
} Passenger;
/**
 * Source event IDs increase within a run even when older buffer entries are discarded
 */
typedef struct {
    unsigned long id, tick;
    char message[160];
} SimulationEvent;
/**
 * The adapter owns passenger dispatch while the original Elevator owns motion state
 */
typedef struct {
    Elevator elevator;
    GuardrailStatus guardrails;
    Passenger people[MAX_PEOPLE];
    int people_count, direction, completed, stops;
    bool requests[10], stop_requested;
    unsigned long tick, event_id;
    SimulationEvent events[MAX_EVENTS];
    int event_count;
    char scenario[24];
    unsigned int scenario_seed;
    int speed;
} Simulation;

/** One custom arrival stream; timing is in simulation seconds, priority is per stream. */
typedef struct { int from, to, count, start, interval, vip; } ScenarioFlow;
/** Validated scenario configuration; at most 20 streams and 1000 total passengers. */
typedef struct {
    char kind[24];
    int count, duration, seed, flow_count;
    ScenarioFlow flows[20];
} ScenarioConfig;
/** Validate a whole scenario without mutating the live run. */
bool simulation_valid_scenario(const ScenarioConfig *config);
/** Install a validated schedule in an empty run; return false without mutation on rejection. */
bool simulation_start_scenario(Simulation *sim, const ScenarioConfig *config);

/**
 * Initialize live state; persistent experiment archival belongs to the telemetry caller
 */
void simulation_init(Simulation *sim);
/**
 * Validate UTF-8 and permit line breaks only when multiline is true
 */
bool simulation_valid_text(const char *text, bool multiline);
/**
 * Advance one 100 ms simulation step with guardrail enforcement
 */
void simulation_tick(Simulation *sim);
/* Validate an entire group before inserting; returns false without mutation on failure. */
/**
 * Insert a validated group atomically; return false without partial insertion on rejection
 */
bool simulation_add_people(Simulation *sim, const char *name, int from, int to, int count);
/**
 * Apply an available command; return false for unsupported or invalid requests
 */
bool simulation_control(Simulation *sim, const char *action, int floor);
/**
 * Return the count of passengers currently riding in the cabin
 */
int simulation_occupancy(const Simulation *sim);
/* Returns bytes written, or zero if the supplied buffer is too small. */
/**
 * Serialize into the caller-owned buffer; return bytes written or zero on overflow
 */
size_t simulation_json(const Simulation *sim, char *buffer, size_t capacity);

#endif
