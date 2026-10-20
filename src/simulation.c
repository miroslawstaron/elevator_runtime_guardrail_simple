#include "simulation.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void event(Simulation *s, const char *format, ...) {
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

void simulation_init(Simulation *s) {
    memset(s, 0, sizeof(*s));
    elevator_init(&s->elevator);
    s->direction = 1;
    event(s, "System ready. Elevator at floor 1.");
}

int simulation_occupancy(const Simulation *s) {
    int n = 0;
    for (int i = 0; i < s->people_count; i++) n += s->people[i].state == RIDING;
    return n;
}

static bool valid_name(const char *name) {
    const unsigned char *p = (const unsigned char *)name;
    while (*p) {
        unsigned int code = *p++;
        if (code < 128) { if (code < 32 || code == 127) return false; continue; }
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

bool simulation_add_people(Simulation *s, const char *name, int from, int to, int count) {
    if (!name || !name[0] || strlen(name) > 40 || from < 0 || from > 9 ||
        to < 0 || to > 9 || from == to || count < 1 || count > 8 ||
        s->people_count + count > MAX_PEOPLE) return false;
    if (!valid_name(name)) return false;
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

static void demand(const Simulation *s, bool floors[10]) {
    memcpy(floors, s->requests, sizeof(s->requests));
    bool room = simulation_occupancy(s) < CABIN_CAPACITY;
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        if (p->state == RIDING) floors[p->to] = true;
        if (p->state == WAITING && room) floors[p->from] = true;
    }
}

static int ahead(const bool floors[10], int floor, int direction) {
    for (int f = floor + direction; f >= 0 && f <= 9; f += direction)
        if (floors[f]) return f;
    return -1;
}

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

static void service(Simulation *s) {
    int floor = s->elevator.current_floor;
    s->requests[floor] = false;
    for (int i = 0; i < s->people_count; i++) {
        Passenger *p = &s->people[i];
        if (p->state == RIDING && p->to == floor) {
            p->state = ARRIVED; p->arrived_tick = s->tick; s->completed++;
            event(s, "%s arrived at floor %d.", p->name, floor);
        }
    }
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
    int occupancy = simulation_occupancy(s);
    for (int i = 0; i < s->people_count && occupancy < CABIN_CAPACITY; i++) {
        Passenger *p = &s->people[i];
        if (p->state == WAITING && p->from == floor &&
            (p->to > floor ? 1 : -1) == s->direction) {
            p->state = RIDING; p->boarded_tick = s->tick; occupancy++;
            event(s, "%s boarded at floor %d, going to %d.", p->name, floor, p->to);
        }
    }
}

void simulation_tick(Simulation *s) {
    Elevator *e = &s->elevator;
    s->tick++;
    /* Finish the current floor interval before latching stop; never park between floors. */
    if (s->stop_requested && e->motion_ticks == 0) {
        elevator_press_stop(e);
        s->stop_requested = false;
        event(s, "Emergency stop active at floor %d.", e->current_floor);
    }
    s->guardrails = guardrail_check(e);
    guardrail_enforce(e);
    if (e->stop_button_pressed) return;

    bool floors[10];
    demand(s, floors);
    if (e->state == IDLE) {
        int next = service_here(s, floors) ? (int)e->current_floor : ahead(floors, e->current_floor, s->direction);
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
    ElevatorState before = e->state;
    /* The original core opens for one tick. The adapter gives passengers a two-second dwell. */
    if (e->state == DOORS_OPEN && e->ticks < 19) e->ticks++;
    else elevator_tick(e);
    if (e->state == DOORS_OPEN) {
        if (before != DOORS_OPEN) { s->stops++; event(s, "Doors opened at floor %d.", e->current_floor); }
        service(s);
    }
    s->guardrails = guardrail_check(e);
    guardrail_enforce(e);
}

bool simulation_control(Simulation *s, const char *action, int floor) {
    Elevator *e = &s->elevator;
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
static void append(Json *j, const char *format, ...) {
    if (j->failed) return;
    va_list args; va_start(args, format);
    int n = vsnprintf(j->data + j->used, j->capacity - j->used, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= j->capacity - j->used) { j->failed = true; return; }
    j->used += (size_t)n;
}
static void string(Json *j, const char *value) {
    append(j, "\"");
    for (const unsigned char *c = (const unsigned char *)value; *c; c++) {
        if (*c == '"' || *c == '\\') append(j, "\\%c", *c);
        else if (*c < 32) append(j, "\\u%04x", *c);
        else append(j, "%c", *c);
    }
    append(j, "\"");
}
size_t simulation_json(const Simulation *s, char *buffer, size_t capacity) {
    if (!capacity) return 0;
    Json j = { buffer, 0, capacity, false };
    const Elevator *e = &s->elevator;
    float position = (float)e->current_floor;
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
        append(&j, ",\"from\":%d,\"to\":%d,\"status\":\"%s\",\"addedTick\":%lu,\"boardedTick\":%lu,\"arrivedTick\":%lu}", p->from, p->to,
            p->state == WAITING ? "waiting" : p->state == RIDING ? "riding" : "arrived", p->added_tick, p->boarded_tick, p->arrived_tick);
    }
    append(&j, "],\"events\":[");
    for (int i = 0; i < s->event_count; i++) {
        const SimulationEvent *v = &s->events[i];
        append(&j, "%s{\"id\":%lu,\"tick\":%lu,\"message\":", i ? "," : "", v->id, v->tick);
        string(&j, v->message); append(&j, "}");
    }
    append(&j, "]}");
    return j.failed ? 0 : j.used;
}
