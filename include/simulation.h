#ifndef SIMULATION_H
#define SIMULATION_H

#include "elevator.h"
#include "guardrails.h"
#include <stddef.h>

#define MAX_PEOPLE 160
#define CABIN_CAPACITY 8
#define MAX_EVENTS 48

typedef enum { WAITING, RIDING, ARRIVED } PassengerState;
typedef struct {
    int id, from, to;
    char name[49];
    PassengerState state;
    unsigned long added_tick, boarded_tick, arrived_tick;
} Passenger;
typedef struct {
    unsigned long id, tick;
    char message[160];
} SimulationEvent;
typedef struct {
    Elevator elevator;
    GuardrailStatus guardrails;
    Passenger people[MAX_PEOPLE];
    int people_count, direction, completed, stops;
    bool requests[10], stop_requested;
    unsigned long tick, event_id;
    SimulationEvent events[MAX_EVENTS];
    int event_count;
} Simulation;

void simulation_init(Simulation *sim);
void simulation_tick(Simulation *sim);
/* Validate an entire group before inserting; returns false without mutation on failure. */
bool simulation_add_people(Simulation *sim, const char *name, int from, int to, int count);
bool simulation_control(Simulation *sim, const char *action, int floor);
int simulation_occupancy(const Simulation *sim);
/* Returns bytes written, or zero if the supplied buffer is too small. */
size_t simulation_json(const Simulation *sim, char *buffer, size_t capacity);

#endif
