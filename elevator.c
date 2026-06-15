#include "elevator.h"
#include <string.h>

// State transition table: defines next state based on current state and conditions
typedef struct {
    ElevatorState current;
    ElevatorState next;
    bool (*condition)(const Elevator *);
} StateTransition;

// Forward declarations of condition functions
static bool at_target_floor(const Elevator *elev);
static bool has_pending_buttons(const Elevator *elev);
static bool should_go_up(const Elevator *elev);
static bool should_go_down(const Elevator *elev);
static bool should_open_doors(const Elevator *elev);
static bool doors_fully_open(const Elevator *elev);
static bool doors_fully_closed(const Elevator *elev);

// State transition table - defines all possible transitions
static const StateTransition transition_table[] = {
    // From IDLE
    { IDLE, DOORS_OPENING, should_open_doors },
    { IDLE, SMOOTH_START_UP, should_go_up },
    { IDLE, SMOOTH_START_DOWN, should_go_down },

    // Smooth start transitions
    { SMOOTH_START_UP, MOVING_UP, NULL },  // Always transition after 1 tick
    { SMOOTH_START_DOWN, MOVING_DOWN, NULL },

    // Moving states - transition to smooth stop when at target
    { MOVING_UP, SMOOTH_STOP_UP, at_target_floor },
    { MOVING_DOWN, SMOOTH_STOP_DOWN, at_target_floor },

    // Smooth stop transitions
    { SMOOTH_STOP_UP, IDLE, NULL },  // Always transition after 1 tick
    { SMOOTH_STOP_DOWN, IDLE, NULL },

    // Door transitions
    { DOORS_OPENING, DOORS_OPEN, doors_fully_open },
    { DOORS_OPEN, DOORS_CLOSING, NULL },  // Close after being open
    { DOORS_CLOSING, IDLE, doors_fully_closed },

    // Emergency stop always transitions back to IDLE when released
    { EMERGENCY_STOP, IDLE, NULL },
};

#define TRANSITION_TABLE_SIZE (sizeof(transition_table) / sizeof(transition_table[0]))
#define SMOOTH_MOTION_TICKS 1  // Ticks for smooth acceleration/deceleration
#define DOOR_OPEN_TICKS 10      // Ticks doors stay open
#define DOOR_ANIM_TICKS 2       // Ticks for opening/closing animation

void elevator_init(Elevator *elev) {
    elev->current_floor = FLOOR_1;
    elev->target_floor = FLOOR_1;
    elev->state = IDLE;
    elev->previous_state = IDLE;
    elev->doors_open = false;
    elev->stop_button_pressed = false;
    elev->stop_start_time = 0;
    elev->ticks = 0;
    elev->motion_ticks = 0;
    memset(elev->button_pressed, 0, sizeof(elev->button_pressed));
}

void elevator_press_button(Elevator *elev, Floor floor) {
    if (floor >= FLOOR_0 && floor <= FLOOR_9) {
        elev->button_pressed[floor] = true;

        // If already at requested floor while idle, prepare to open doors
        if (elev->state == IDLE && elev->current_floor == floor) {
            elev->target_floor = floor;
        }
        // If in IDLE, set target immediately
        else if (elev->state == IDLE) {
            elev->target_floor = floor;
        }
        // If moving, don't change target - it will be updated when we reach IDLE
        // and transition to next movement state
    }
}

void elevator_press_stop(Elevator *elev) {
    elev->stop_button_pressed = true;
    elev->stop_start_time = time(NULL);
    elev->state = EMERGENCY_STOP;
}

void elevator_release_stop(Elevator *elev) {
    elev->stop_button_pressed = false;
    elev->stop_start_time = 0;
    elev->state = IDLE;  // Resume normal operation
    elev->ticks = 0;
}

// Condition function implementations
static bool at_target_floor(const Elevator *elev) {
    return elev->current_floor == elev->target_floor;
}

static bool has_pending_buttons(const Elevator *elev) {
    for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
        if (elev->button_pressed[f]) {
            return true;
        }
    }
    return false;
}

static bool should_go_up(const Elevator *elev) {
    if (!has_pending_buttons(elev)) {
        return false;
    }
    // Find highest pressed button
    Floor highest = FLOOR_0;
    for (Floor f = FLOOR_9; f >= FLOOR_0; f--) {
        if (elev->button_pressed[f]) {
            highest = f;
            break;
        }
    }
    return highest > elev->current_floor;
}

static bool should_go_down(const Elevator *elev) {
    if (!has_pending_buttons(elev)) {
        return false;
    }
    // Find lowest pressed button
    Floor lowest = FLOOR_9;
    for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
        if (elev->button_pressed[f]) {
            lowest = f;
            break;
        }
    }
    return lowest < elev->current_floor;
}

static bool should_open_doors(const Elevator *elev) {
    // Only open doors if: at target floor AND button for this floor is pressed
    return elev->current_floor == elev->target_floor &&
           elev->button_pressed[elev->current_floor];
}

static bool doors_fully_open(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;
}

static bool doors_fully_closed(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;
}

// Find next state based on current state and conditions
static ElevatorState find_next_state(const Elevator *elev) {
    // Handle emergency stop separately
    if (elev->state == EMERGENCY_STOP) {
        return EMERGENCY_STOP;  // Stay in emergency stop
    }

    for (size_t i = 0; i < TRANSITION_TABLE_SIZE; i++) {
        if (transition_table[i].current == elev->state) {
            // If no condition specified, always transition
            if (transition_table[i].condition == NULL) {
                return transition_table[i].next;
            }
            // Otherwise check the condition
            if (transition_table[i].condition(elev)) {
                return transition_table[i].next;
            }
        }
    }
    return elev->state;  // Stay in current state if no transition found
}

void elevator_tick(Elevator *elev) {
    // Track state changes
    elev->previous_state = elev->state;

    // Find and execute state transition
    ElevatorState next_state = find_next_state(elev);

    // Update state
    if (next_state != elev->state) {
        elev->state = next_state;
        elev->ticks = 0;      // Reset tick counter on state change
        elev->motion_ticks = 0;  // Reset motion counter on state change

        // When starting movement, set correct target floor
        if (next_state == SMOOTH_START_UP) {
            // Find the highest floor button at or above current floor
            for (Floor f = FLOOR_9; f >= FLOOR_0; f--) {
                if (elev->button_pressed[f] && f >= elev->current_floor) {
                    elev->target_floor = f;
                    break;
                }
            }
        } else if (next_state == SMOOTH_START_DOWN) {
            // Find the lowest floor button at or below current floor
            for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
                if (elev->button_pressed[f] && f <= elev->current_floor) {
                    elev->target_floor = f;
                    break;
                }
            }
        }
    } else {
        elev->ticks++;
    }

    // Update elevator position based on state
    // Motion is slowed by requiring 5 ticks per floor movement (500ms per floor at 100ms tick rate)
    switch (elev->state) {
        case MOVING_UP:
            elev->motion_ticks++;
            if (elev->motion_ticks >= 5) {  // Move every 5 ticks (500ms)
                elev->current_floor++;
                elev->motion_ticks = 0;
                if (elev->current_floor > FLOOR_9) {
                    elev->current_floor = FLOOR_9;
                }
            }
            break;

        case MOVING_DOWN:
            elev->motion_ticks++;
            if (elev->motion_ticks >= 5) {  // Move every 5 ticks (500ms)
                elev->current_floor--;
                elev->motion_ticks = 0;
                if (elev->current_floor < FLOOR_0) {
                    elev->current_floor = FLOOR_0;
                }
            }
            break;

        case IDLE:
            // Idle, waiting for buttons
            break;

        case DOORS_OPEN:
            // Clear button for this floor when doors are fully open
            elev->button_pressed[elev->current_floor] = false;
            break;

        case DOORS_OPENING:
            elev->doors_open = true;
            break;

        case DOORS_CLOSING:
            elev->doors_open = false;
            break;

        default:
            break;
    }
}

const char* elevator_state_to_string(ElevatorState state) {
    switch (state) {
        case IDLE: return "IDLE";
        case SMOOTH_START_UP: return "SMOOTH_START_UP";
        case MOVING_UP: return "MOVING_UP";
        case SMOOTH_STOP_UP: return "SMOOTH_STOP_UP";
        case SMOOTH_START_DOWN: return "SMOOTH_START_DOWN";
        case MOVING_DOWN: return "MOVING_DOWN";
        case SMOOTH_STOP_DOWN: return "SMOOTH_STOP_DOWN";
        case DOORS_OPENING: return "DOORS_OPENING";
        case DOORS_OPEN: return "DOORS_OPEN";
        case DOORS_CLOSING: return "DOORS_CLOSING";
        case EMERGENCY_STOP: return "EMERGENCY_STOP";
        default: return "UNKNOWN";
    }
}

bool elevator_is_between_floors(const Elevator *elev) {
    return elev->state == MOVING_UP || elev->state == MOVING_DOWN ||
           elev->state == SMOOTH_START_UP || elev->state == SMOOTH_START_DOWN ||
           elev->state == SMOOTH_STOP_UP || elev->state == SMOOTH_STOP_DOWN;
}

float elevator_get_position(const Elevator *elev) {
    // Returns position as a float between 1.0 and 5.0
    return (float)elev->current_floor;
}

void elevator_open_doors(Elevator *elev) {
    if (elev->state == IDLE && !elevator_is_between_floors(elev)) {
        elev->doors_open = true;
    }
}

void elevator_close_doors(Elevator *elev) {
    if (!elevator_is_between_floors(elev)) {
        elev->doors_open = false;
    }
}
