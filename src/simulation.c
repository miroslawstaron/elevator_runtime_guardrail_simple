/****************************************
*
* Elevator Lab -- Passenger Dispatch and Simulation Adapter
*
* SPDX-License-Identifier: MIT
*
******************************************/

#include "simulation.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/**
 * Append a timestamped message to the bounded live event history
 * Discard the oldest message when the buffer is full; source IDs keep increasing
 * The recorder can use those IDs to detect messages lost before persistence
 */
static void event(Simulation *s, const char *format, ...) {
    // LIVE HISTORY: Retain the newest events without exceeding the fixed buffer
    if (s->event_count == MAX_EVENTS) {
        memmove(s->events, s->events + 1, sizeof(s->events[0]) * (MAX_EVENTS - 1));
        s->event_count--;
    }
    SimulationEvent *e = &s->events[s->event_count++];
    e->id = ++s->event_id;
    e->tick = s->tick;
    va_list args;
    va_start(args, format);
    vsnprintf(e->message, sizeof(e->message), format, args);
    va_end(args);
}

/**
 * Reset the live adapter and initialize the original C elevator
 * The caller owns experiment archival; this function does not delete stored telemetry
 */
void simulation_init(Simulation *s) {
    memset(s, 0, sizeof(*s));
    elevator_init(&s->elevator);
    s->direction = 1;
    s->speed = 1;
    event(s, "System ready. Elevator at floor 1.");
}

/**
 * Count passengers currently riding in the cabin
 * Returns the occupancy used by dispatch and capacity checks
 */
int simulation_occupancy(const Simulation *s) {
    int n = 0;
    for (int i = 0; i < s->people_count; i++) n += s->people[i].state == RIDING;
    return n;
}

/**
 * Validate a NUL-terminated UTF-8 string without changing it
 * Allow tabs and line breaks only for multiline text; reject malformed encodings
 * Returns false for control characters, overlong encodings, surrogates or invalid code points
 */
bool simulation_valid_text(const char *name, bool multiline) {
    const unsigned char *p = (const unsigned char *)name;
    while (*p) {
        unsigned int code = *p++;
        if (code < 128) {
            if ((code < 32 && !(multiline && (code == 9 || code == 10 || code == 13))) || code == 127) return false;
            continue;
        }
        int more;
        unsigned int minimum;
        if (code >= 0xc2 && code <= 0xdf) { more = 1; minimum = 0x80; code &= 0x1f; }
        else if (code >= 0xe0 && code <= 0xef) { more = 2; minimum = 0x800; code &= 0x0f; }
        else if (code >= 0xf0 && code <= 0xf4) { more = 3; minimum = 0x10000; code &= 0x07; }
        else return false;
        for (int i = 0; i < more; i++) {
            if ((*p & 0xc0) != 0x80) return false;
            code = (code << 6) | (*p++ & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

/**
 * Validate and insert an entire passenger group into the waiting queue
 * Assign IDs within the current run and record each passenger
 * Returns false without inserting anyone when the group or journey is invalid
 */
bool simulation_add_people(Simulation *s, const char *name, int from, int to, int count) {
    // VALIDATION: Check the entire group before changing passenger state
    if (!name || !name[0] || strlen(name) > 40 || from < 0 || from > 9 ||
        to < 0 || to > 9 || from == to || count < 1 || count > 8 ||
        s->people_count + count > MAX_PEOPLE) return false;
    if (!simulation_valid_text(name, false)) return false;
    // INSERTION: Create independent journeys so each rider keeps their own timestamps
    for (int i = 0; i < count; i++) {
        Passenger *p = &s->people[s->people_count++];
        p->id = s->people_count;
        if (count > 1) snprintf(p->name, sizeof(p->name), "%s %d", name, i + 1);
        else snprintf(p->name, sizeof(p->name), "%s", name);
        p->from = from; p->to = to; p->state = WAITING;
        p->added_tick = s->tick;
        event(s, "%s is waiting: floor %d to %d.", p->name, from, to);
    }
    return true;
}

/** Validate preset bounds and every custom stream before any experiment is archived. */
bool simulation_valid_scenario(const ScenarioConfig *c) {
    if (!c || c->seed < 1 || c->duration < 0 || c->duration > 86400 ||
        c->count < 1 || c->count > MAX_PEOPLE) return false;
    if (strcmp(c->kind, "custom"))
        return !strcmp(c->kind, "morning") || !strcmp(c->kind, "afternoon") ||
            !strcmp(c->kind, "occasional") || !strcmp(c->kind, "vip");
    if (c->flow_count < 1 || c->flow_count > 20) return false;
    int total = 0;
    for (int i = 0; i < c->flow_count; i++) {
        const ScenarioFlow *f = &c->flows[i];
        if (f->from < 0 || f->from > 9 || f->to < 0 || f->to > 9 || f->from == f->to ||
            f->count < 1 || f->count > MAX_PEOPLE || f->start < 0 || f->start > 86400 ||
            f->interval < 0 || f->interval > 86400 || (f->vip != 0 && f->vip != 1) ||
            (long long)f->start + (long long)(f->count - 1) * f->interval > 86400) return false;
        total += f->count;
    }
    return total == c->count && total <= MAX_PEOPLE;
}

/**
 * Preallocate reproducible journeys in an empty experiment; scheduled people exert no demand.
 * Record one configuration event instead of flooding the bounded live event buffer.
 */
bool simulation_start_scenario(Simulation *s, const ScenarioConfig *c) {
    if (s->people_count || !simulation_valid_scenario(c)) return false;
    snprintf(s->scenario, sizeof(s->scenario), "%s", c->kind);
    s->scenario_seed = (unsigned int)c->seed;
    unsigned int random = (unsigned int)c->seed;
    for (int i = 0, flow = 0, member = 0; i < c->count; i++) {
        Passenger *p = &s->people[s->people_count++];
        memset(p, 0, sizeof(*p));
        p->id = i + 1;
        random = random * 1664525u + 1013904223u;
        int floor = 1 + (int)((random >> 16) % 9);
        p->from = !strcmp(c->kind, "morning") ? 0 : floor;
        p->to = !strcmp(c->kind, "afternoon") ? 0 : (p->from + 1 + (int)(random % 9)) % 10;
        if (!strcmp(c->kind, "morning")) p->to = floor;
        p->vip = !strcmp(c->kind, "vip") && i % 5 == 0;
        p->added_tick = s->tick + (c->count == 1 ? 0 : (unsigned long)i * (unsigned long)c->duration * 10 / (unsigned long)(c->count - 1));
        if (!strcmp(c->kind, "custom")) {
            const ScenarioFlow *f = &c->flows[flow];
            p->from = f->from; p->to = f->to; p->vip = f->vip != 0;
            p->added_tick = s->tick + ((unsigned long)f->start + (unsigned long)member * f->interval) * 10;
            if (++member == f->count) { member = 0; flow++; }
        }
        p->state = p->added_tick <= s->tick ? WAITING : SCHEDULED;
        snprintf(p->name, sizeof(p->name), "%s %d", p->vip ? "VIP" : "Traveller", p->id);
    }
    event(s, "Scenario %s: %d people, window %ds, seed %d.", c->kind, c->count, c->duration, c->seed);
    for (int i = 0; i < c->flow_count; i++) {
        const ScenarioFlow *f = &c->flows[i];
        event(s, "Flow %d: %d people, %d to %d, start %ds, interval %ds, VIP %d.",
            i + 1, f->count, f->from, f->to, f->start, f->interval, f->vip);
    }
    return true;
}

/**
 * Rank waiting people by a 120-second aging threshold, then VIP priority, then arrival.
 * Aging prevents a continuous stream of new VIPs from overtaking old regular requests.
 */
static bool precedes(const Simulation *s, const Passenger *a, const Passenger *b) {
    if (!b) return true;
    bool aged_a = s->tick - a->added_tick >= 1200, aged_b = s->tick - b->added_tick >= 1200;
    if (aged_a != aged_b) return aged_a;
    if (aged_a) return a->added_tick < b->added_tick || (a->added_tick == b->added_tick && a->id < b->id);
    if (a->vip != b->vip) return a->vip;
    return a->added_tick < b->added_tick || (a->added_tick == b->added_tick && a->id < b->id);
}

/** Return an urgent pickup only for an empty cabin; existing riders retain their stops. */
static const Passenger *priority_pickup(const Simulation *s) {
    if (simulation_occupancy(s)) return NULL;
    const Passenger *best = NULL;
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        if (p->state == WAITING && (p->vip || s->tick - p->added_tick >= 1200) && precedes(s, p, best)) best = p;
    }
    return best;
}

/**
 * Combine manual requests, rider destinations and eligible pickup floors
 * Do not request additional pickups while the cabin is full
 */
static void demand(const Simulation *s, bool floors[10]) {
    memcpy(floors, s->requests, sizeof(s->requests));
    // CAPACITY: Full cabins still serve rider destinations before accepting new pickups
    bool room = simulation_occupancy(s) < CABIN_CAPACITY;
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        if (p->state == RIDING) floors[p->to] = true;
        if (p->state == WAITING && room) floors[p->from] = true;
    }
}

/**
 * Find the nearest requested floor strictly ahead in the given direction
 * Returns -1 when no demand remains ahead
 */
static int ahead(const bool floors[10], int floor, int direction) {
    for (int f = floor + direction; f >= 0 && f <= 9; f += direction)
        if (floors[f]) return f;
    return -1;
}

/**
 * Determine whether the current landing needs service
 * Consider cabin calls, arriving riders and waiting passengers in the travel direction
 * Opposite-direction pickups are allowed when no demand remains ahead
 */
static bool service_here(const Simulation *s, const bool floors[10]) {
    int floor = s->elevator.current_floor;
    if (s->requests[floor]) return true;
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        if (p->state == RIDING && p->to == floor) return true;
        if (p->state == WAITING && p->from == floor && simulation_occupancy(s) < CABIN_CAPACITY &&
            ((p->to > floor ? 1 : -1) == s->direction || ahead(floors, floor, s->direction) < 0)) return true;
    }
    return false;
}

/**
 * Unload arriving passengers before boarding eligible waiting passengers
 * Update journey timestamps and reverse direction when required by remaining demand
 * Board only passengers travelling in the chosen direction, up to cabin capacity
 */
static void service(Simulation *s) {
    int floor = s->elevator.current_floor;
    // SERVICE 1: Clear the landing request and let arriving passengers leave first
    s->requests[floor] = false;
    for (int i = 0; i < s->people_count; i++) {
        Passenger *p = &s->people[i];
        if (p->state == RIDING && p->to == floor) {
            p->state = ARRIVED; p->arrived_tick = s->tick; s->completed++;
            event(s, "%s arrived at floor %d.", p->name, floor);
        }
    }
    // SERVICE 2: Re-evaluate demand after unloading to choose the next travel direction
    bool floors[10];
    demand(s, floors);
    if (ahead(floors, floor, s->direction) < 0) s->direction = -s->direction;
    /* At a terminal stop, choose the waiting passengers' direction if no calls remain ahead. */
    if (ahead(floors, floor, s->direction) < 0 && simulation_occupancy(s) == 0) {
        for (int i = 0; i < s->people_count; i++) {
            Passenger *p = &s->people[i];
            if (p->state == WAITING && p->from == floor) {
                s->direction = p->to > floor ? 1 : -1;
                break;
            }
        }
    }
    // SERVICE 3: Board compatible waiting passengers while cabin space remains
    const Passenger *urgent = priority_pickup(s);
    if (urgent && urgent->from == floor) s->direction = urgent->to > floor ? 1 : -1;
    int occupancy = simulation_occupancy(s);
    while (occupancy < CABIN_CAPACITY) {
        Passenger *p = NULL;
        for (int i = 0; i < s->people_count; i++) {
            Passenger *candidate = &s->people[i];
            if (candidate->state == WAITING && candidate->from == floor &&
                (candidate->to > floor ? 1 : -1) == s->direction && precedes(s, candidate, p)) p = candidate;
        }
        if (!p) break;
        p->state = RIDING; p->boarded_tick = s->tick; occupancy++;
        event(s, "%s boarded at floor %d, going to %d.", p->name, floor, p->to);
    }
}

/**
 * Advance the adapter by one 100 ms simulation tick
 * Check guardrails, dispatch demand, run the original state machine and service landings
 * A requested stop is latched only after the current floor interval finishes
 */
void simulation_tick(Simulation *s) {
    Elevator *e = &s->elevator;
    s->tick++;
    // ARRIVALS: Release from the C clock even when every browser has disconnected.
    int released = 0;
    for (int i = 0; i < s->people_count; i++) {
        Passenger *p = &s->people[i];
        if (p->state == SCHEDULED && p->added_tick <= s->tick) { p->state = WAITING; released++; }
    }
    if (released) event(s, "%d scheduled traveller%s joined the queue.", released, released == 1 ? "" : "s");
    /* Finish the current floor interval before latching stop; never park between floors. */
    if (s->stop_requested && e->motion_ticks == 0) {
        elevator_press_stop(e);
        s->stop_requested = false;
        event(s, "Emergency stop active at floor %d.", e->current_floor);
    }
    // TICK 1: Enforce the original safety rules before dispatching any motion
    s->guardrails = guardrail_check(e);
    guardrail_enforce(e);
    if (e->stop_button_pressed) return;

    bool floors[10];
    demand(s, floors);
    // TICK 2: Select a landing from current demand and reconsider routes only at landings
    if (e->state == IDLE) {
        int next = service_here(s, floors) ? (int)e->current_floor : ahead(floors, e->current_floor, s->direction);
        const Passenger *urgent = priority_pickup(s);
        if (urgent) {
            next = urgent->from;
            s->direction = next == (int)e->current_floor ? (urgent->to > next ? 1 : -1) : (next > (int)e->current_floor ? 1 : -1);
        }
        if (next < 0) {
            s->direction = -s->direction;
            next = ahead(floors, e->current_floor, s->direction);
        }
        memset(e->button_pressed, 0, sizeof(e->button_pressed));
        if (next >= 0) elevator_press_button(e, (Floor)next);
    } else if ((e->state == MOVING_UP || e->state == MOVING_DOWN) && e->motion_ticks == 0) {
        /* Reconsider at each landing, so new calls can be collected along the route. */
        int next = service_here(s, floors) ? (int)e->current_floor : ahead(floors, e->current_floor, s->direction);
        if (next >= 0) e->target_floor = (Floor)next;
        memset(e->button_pressed, 0, sizeof(e->button_pressed));
        e->button_pressed[e->target_floor] = true;
    }
    // TICK 3: Advance the original state machine and apply passenger dwell timing
    ElevatorState before = e->state;
    /* The original core opens for one tick. The adapter gives passengers a two-second dwell. */
    if (e->state == DOORS_OPEN && e->ticks < 19) e->ticks++;
    else elevator_tick(e);
    // TICK 4: Count each door opening once, then update boarding and arrival state
    if (e->state == DOORS_OPEN) {
        if (before != DOORS_OPEN) { s->stops++; event(s, "Doors opened at floor %d.", e->current_floor); }
        service(s);
    }
    s->guardrails = guardrail_check(e);
    guardrail_enforce(e);
}

/**
 * Apply a cabin command when its state and parameters permit it
 * Returns false for invalid floors, unsupported actions or unavailable door commands
 * Reset and demo alter live state; HTTP callers manage experiment boundaries
 */
bool simulation_control(Simulation *s, const char *action, int floor) {
    Elevator *e = &s->elevator;
    // COMMANDS: Keep floor requests, stop state and door interlocks in the C adapter
    if (!strcmp(action, "request")) {
        if (floor < 0 || floor > 9) return false;
        s->requests[floor] = true;
        event(s, "Floor %d requested from cabin panel.", floor);
    } else if (!strcmp(action, "stop")) {
        if (!e->stop_button_pressed && !s->stop_requested) {
            s->stop_requested = true;
            event(s, "Stop requested. Securing cabin at next landing.");
        }
    } else if (!strcmp(action, "resume")) {
        s->stop_requested = false;
        if (e->stop_button_pressed) {
            elevator_release_stop(e);
            if (e->doors_open) { e->state = DOORS_CLOSING; e->ticks = 0; e->doors_open = false; }
            event(s, "Elevator resumed.");
        }
    } else if (!strcmp(action, "open")) {
        if (e->state != IDLE || e->stop_button_pressed || s->stop_requested) return false;
        elevator_open_doors(e);
        e->state = DOORS_OPENING; e->ticks = 0;
        event(s, "Door opening requested.");
    } else if (!strcmp(action, "close")) {
        if (e->stop_button_pressed || s->stop_requested ||
            (e->state != DOORS_OPEN && e->state != DOORS_OPENING)) return false;
        elevator_close_doors(e); e->state = DOORS_CLOSING; e->ticks = 0;
        event(s, "Door closing requested.");
    } else if (!strcmp(action, "speed")) {
        if (floor != 1 && floor != 5 && floor != 20) return false;
        s->speed = floor;
        event(s, "Simulation speed set to %dx.", floor);
    } else if (!strcmp(action, "cancel_scenario")) {
        int cancelled = 0;
        for (int i = 0; i < s->people_count; i++)
            if (s->people[i].state == SCHEDULED) { s->people[i].state = CANCELLED; cancelled++; }
        event(s, "Cancelled %d future arrivals. Existing journeys continue.", cancelled);
    } else if (!strcmp(action, "reset")) simulation_init(s);
    else if (!strcmp(action, "demo")) {
        if (s->people_count + 6 > MAX_PEOPLE) return false;
        simulation_add_people(s, "Alex", 1, 7, 1);
        simulation_add_people(s, "Sam", 3, 8, 1);
        simulation_add_people(s, "Jamie", 6, 0, 1);
        simulation_add_people(s, "Robin", 0, 5, 1);
        simulation_add_people(s, "Morgan", 9, 2, 1);
        simulation_add_people(s, "Casey", 4, 1, 1);
    } else return false;
    return true;
}

typedef struct { char *data; size_t used, capacity; bool failed; } Json;
/**
 * Append formatted JSON text while tracking the caller-provided buffer
 * Latch failure on truncation so no partial response is reported as successful
 */
static void append(Json *j, const char *format, ...) {
    if (j->failed) return;
    va_list args; va_start(args, format);
    int n = vsnprintf(j->data + j->used, j->capacity - j->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= j->capacity - j->used) { j->failed = true; return; }
    j->used += (size_t)n;
}
/**
 * Write a JSON string with quotes, backslashes and control bytes escaped
 * Validated UTF-8 bytes otherwise pass through unchanged
 */
static void string(Json *j, const char *value) {
    append(j, "\"");
    for (const unsigned char *c = (const unsigned char *)value; *c; c++) {
        if (*c == '"' || *c == '\\') append(j, "\\%c", *c);
        else if (*c < 32) append(j, "\\u%04x", *c);
        else append(j, "%c", *c);
    }
    append(j, "\"");
}
/**
 * Serialize the authoritative cabin, passengers, guardrails and recent events
 * Derive fractional position from the C motion interval for browser animation
 * Returns bytes written, or zero when the caller buffer cannot hold the snapshot
 */
size_t simulation_json(const Simulation *s, char *buffer, size_t capacity) {
    if (!capacity) return 0;
    Json j = { buffer, 0, capacity, false };
    const Elevator *e = &s->elevator;
    float position = (float)e->current_floor;
    // ANIMATION: Publish progress within the current floor interval without changing it
    if (e->state == MOVING_UP) position += e->motion_ticks / 5.0f;
    if (e->state == MOVING_DOWN) position -= e->motion_ticks / 5.0f;
    append(&j, "{\"tick\":%lu,\"floor\":%d,\"target\":%d,\"position\":%.2f,\"state\":", s->tick, e->current_floor, e->target_floor, position);
    string(&j, elevator_state_to_string(e->state));
    append(&j, ",\"direction\":%d,\"doorsOpen\":%s,\"stopped\":%s,\"stopPending\":%s,\"capacity\":%d,\"occupancy\":%d,\"completed\":%d,\"stops\":%d,\"guardrails\":{\"doors\":%s,\"betweenFloors\":%s,\"invalidFloor\":%s,\"needsPrompt\":%s,\"secondsStopped\":%d},\"requests\":[",
        s->direction, e->doors_open ? "true" : "false", e->stop_button_pressed ? "true" : "false", s->stop_requested ? "true" : "false", CABIN_CAPACITY, simulation_occupancy(s), s->completed, s->stops,
        s->guardrails.doors_open_violation ? "true" : "false", s->guardrails.between_floors_violation ? "true" : "false", s->guardrails.invalid_floor_violation ? "true" : "false", s->guardrails.needs_user_prompt ? "true" : "false", s->guardrails.seconds_stopped);
    bool floors[10]; demand(s, floors);
    for (int f = 0; f < 10; f++) append(&j, "%s%s", f ? "," : "", floors[f] ? "true" : "false");
    append(&j, "],\"people\":[");
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        append(&j, "%s{\"id\":%d,\"name\":", i ? "," : "", p->id); string(&j, p->name);
        append(&j, ",\"from\":%d,\"to\":%d,\"status\":\"%s\",\"vip\":%s,\"addedTick\":%lu,\"boardedTick\":%lu,\"arrivedTick\":%lu}", p->from, p->to,
            p->state == WAITING ? "waiting" : p->state == RIDING ? "riding" : p->state == SCHEDULED ? "scheduled" : p->state == CANCELLED ? "cancelled" : "arrived",
            p->vip ? "true" : "false", p->added_tick, p->boarded_tick, p->arrived_tick);
    }
    append(&j, "],\"events\":[");
    for (int i = 0; i < s->event_count; i++) {
        const SimulationEvent *v = &s->events[i];
        append(&j, "%s{\"id\":%lu,\"tick\":%lu,\"message\":", i ? "," : "", v->id, v->tick);
        string(&j, v->message); append(&j, "}");
    }
    append(&j, "],\"maxPeople\":%d,\"speed\":%d,\"scenario\":", MAX_PEOPLE, s->speed);
    string(&j, s->scenario);
    append(&j, ",\"scenarioSeed\":%u}", s->scenario_seed);
    return j.failed ? 0 : j.used;
}
