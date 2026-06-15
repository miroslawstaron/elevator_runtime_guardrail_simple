/****************************************
*
* Elevator Guardrails -- State Machine
*
* Author: Miroslaw Staron
* miroslaw.staron@gu.se
*
* SPDX-License-Identifier: MIT
* (c) Miroslaw Staron, 2026
*
******************************************/

#include "elevator.h"
#include <string.h>

/**
 * State transition structure - defines one possible state transition
 * Uses a table-driven approach for the state machine implementation
 * Each entry specifies: current state, next state, and optional condition to check
 */
typedef struct {
    ElevatorState current;          // Current state we're in
    ElevatorState next;             // State to transition to
    bool (*condition)(const Elevator *);  // Condition function (NULL = always transition)
} StateTransition;

// Forward declarations of condition functions used in state transitions
static bool at_target_floor(const Elevator *elev);
static bool has_pending_buttons(const Elevator *elev);
static bool should_go_up(const Elevator *elev);
static bool should_go_down(const Elevator *elev);
static bool should_open_doors(const Elevator *elev);
static bool doors_fully_open(const Elevator *elev);
static bool doors_fully_closed(const Elevator *elev);

/**
 * State transition table - Defines all possible state transitions for the elevator
 * This table-driven approach makes the state machine easy to understand and modify
 * Entry format: { current_state, next_state, condition_function }
 * If condition_function is NULL, the transition always occurs on the next tick
 */
static const StateTransition transition_table[] = {
    // IDLE state - waiting at a floor with doors closed
    // Can transition to: opening doors, starting upward, or starting downward
    { IDLE, DOORS_OPENING, should_open_doors },     // Open doors if at floor with button pressed
    { IDLE, SMOOTH_START_UP, should_go_up },        // Start moving up if buttons above are pressed
    { IDLE, SMOOTH_START_DOWN, should_go_down },    // Start moving down if buttons below are pressed

    // Smooth start states - provide acceleration ramp (lasts 1 tick)
    { SMOOTH_START_UP, MOVING_UP, NULL },           // Always transition after 1 tick
    { SMOOTH_START_DOWN, MOVING_DOWN, NULL },       // Always transition after 1 tick

    // Moving states - continue moving until target floor reached
    { MOVING_UP, SMOOTH_STOP_UP, at_target_floor }, // Switch to deceleration at target
    { MOVING_DOWN, SMOOTH_STOP_DOWN, at_target_floor }, // Switch to deceleration at target

    // Smooth stop states - provide deceleration ramp (lasts 1 tick) before returning to IDLE
    { SMOOTH_STOP_UP, IDLE, NULL },                 // Always transition after 1 tick
    { SMOOTH_STOP_DOWN, IDLE, NULL },               // Always transition after 1 tick

    // Door states - open and close sequence
    { DOORS_OPENING, DOORS_OPEN, doors_fully_open }, // Wait for door animation to complete
    { DOORS_OPEN, DOORS_CLOSING, NULL },            // Close doors after being open for a bit
    { DOORS_CLOSING, IDLE, doors_fully_closed },    // Return to idle after doors fully close

    // Emergency stop - stays in this state until user releases the button
    { EMERGENCY_STOP, IDLE, NULL },                 // Transition only when stop is released
};

// Calculate number of entries in transition table
#define TRANSITION_TABLE_SIZE (sizeof(transition_table) / sizeof(transition_table[0]))

// Timing constants for state durations (in simulation ticks at 10 Hz = 100ms per tick)
#define SMOOTH_MOTION_TICKS 1  // Acceleration/deceleration phase lasts 1 tick (100ms)
#define DOOR_OPEN_TICKS 10     // Doors remain open for 10 ticks (1 second)
#define DOOR_ANIM_TICKS 2      // Door opening/closing animation takes 2 ticks (200ms)

/**
 * Initialize elevator to starting state
 * Elevator starts at floor 1 with no buttons pressed and doors closed
 */
void elevator_init(Elevator *elev) {
    elev->current_floor = FLOOR_1;      // Start at floor 1 (middle)
    elev->target_floor = FLOOR_1;       // Target is also floor 1 initially
    elev->state = IDLE;                 // Begin in idle/waiting state
    elev->previous_state = IDLE;        // No previous state yet
    elev->doors_open = false;           // Doors are closed at startup
    elev->stop_button_pressed = false;  // Emergency stop not active
    elev->stop_start_time = 0;          // No stop time recorded yet
    elev->ticks = 0;                    // Reset state duration counter
    elev->motion_ticks = 0;             // Reset floor movement counter
    
    // Clear all floor button states (no buttons pressed)
    memset(elev->button_pressed, 0, sizeof(elev->button_pressed));
}

/**
 * Handle a floor button press
 * Marks the button as pressed and updates the target floor if appropriate
 */
void elevator_press_button(Elevator *elev, Floor floor) {
    // Validate floor is in valid range (0-9)
    if (floor >= FLOOR_0 && floor <= FLOOR_9) {
        // Mark button as pressed (will trigger movement if needed)
        elev->button_pressed[floor] = true;

        // If elevator is already at the requested floor while idle, prepare to open doors
        if (elev->state == IDLE && elev->current_floor == floor) {
            elev->target_floor = floor;  // Set target to current floor
        }
        // If elevator is idle but at a different floor, set new destination
        else if (elev->state == IDLE) {
            elev->target_floor = floor;  // Set new target floor
        }
        // If currently moving, don't change target
        // The target will be properly set when elevator reaches IDLE state
        // and begins the next movement sequence
    }
}

/**
 * Handle emergency stop button press
 * Immediately stops the elevator and transitions to emergency stop state
 */
void elevator_press_stop(Elevator *elev) {
    elev->stop_button_pressed = true;       // Mark stop button as active
    elev->stop_start_time = time(NULL);     // Record when stop was pressed (for timing)
    elev->state = EMERGENCY_STOP;           // Switch to emergency stop state
}

/**
 * Release emergency stop button
 * Allows elevator to resume normal operation
 */
void elevator_release_stop(Elevator *elev) {
    elev->stop_button_pressed = false;      // Mark stop button as released
    elev->stop_start_time = 0;              // Clear the stop timer
    elev->state = IDLE;                     // Transition to idle (normal operation resumes)
    elev->ticks = 0;                        // Reset state counter
}

/**
 * CONDITION FUNCTION: Check if elevator has reached target floor
 * Used to trigger deceleration when approaching destination
 */
static bool at_target_floor(const Elevator *elev) {
    return elev->current_floor == elev->target_floor;  // True when at destination
}

/**
 * CONDITION FUNCTION: Check if any floor buttons are currently pressed
 * Determines if elevator should move or stay idle
 */
static bool has_pending_buttons(const Elevator *elev) {
    // Loop through all floors (0-9) checking for any pressed buttons
    for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
        if (elev->button_pressed[f]) {
            return true;  // Found a pressed button - work to do
        }
    }
    return false;  // No buttons pressed - can stay idle
}

/**
 * CONDITION FUNCTION: Determine if elevator should move upward
 * True if there are any pressed buttons above the current floor
 */
static bool should_go_up(const Elevator *elev) {
    // If no buttons are pressed at all, don't move
    if (!has_pending_buttons(elev)) {
        return false;
    }
    
    // Find the highest floor with a pressed button
    Floor highest = FLOOR_0;
    // Loop from top floor down to find the highest pressed button
    for (Floor f = FLOOR_9; f >= FLOOR_0; f--) {
        if (elev->button_pressed[f]) {
            highest = f;  // Found a pressed button at floor f
            break;  // Stop searching (we found the highest)
        }
    }
    
    // Move up only if highest button is above current position
    return highest > elev->current_floor;
}

/**
 * CONDITION FUNCTION: Determine if elevator should move downward
 * True if there are any pressed buttons below the current floor
 */
static bool should_go_down(const Elevator *elev) {
    // If no buttons are pressed at all, don't move
    if (!has_pending_buttons(elev)) {
        return false;
    }
    
    // Find the lowest floor with a pressed button
    Floor lowest = FLOOR_9;
    // Loop from bottom floor up to find the lowest pressed button
    for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
        if (elev->button_pressed[f]) {
            lowest = f;  // Found a pressed button at floor f
            break;  // Stop searching (we found the lowest)
        }
    }
    
    // Move down only if lowest button is below current position
    return lowest < elev->current_floor;
}

/**
 * CONDITION FUNCTION: Determine if elevator should open doors
 * True only if at target floor AND a button was pressed for this floor
 */
static bool should_open_doors(const Elevator *elev) {
    // Both conditions must be true:
    // 1. Elevator is at the target destination floor
    // 2. Someone pressed the button for this floor (not a pass-through)
    return elev->current_floor == elev->target_floor &&
           elev->button_pressed[elev->current_floor];
}

/**
 * CONDITION FUNCTION: Check if door opening animation has completed
 * Doors take DOOR_ANIM_TICKS ticks to fully open
 */
static bool doors_fully_open(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;  // Animation complete when ticks reach threshold
}

/**
 * CONDITION FUNCTION: Check if door closing animation has completed
 * Doors take DOOR_ANIM_TICKS ticks to fully close
 */
static bool doors_fully_closed(const Elevator *elev) {
    return elev->ticks >= DOOR_ANIM_TICKS;  // Animation complete when ticks reach threshold
}

/**
 * Find the next state for the elevator based on current state and conditions
 * Implements the table-driven state machine logic
 * Returns the next state (may be same as current if no transition applies)
 */
static ElevatorState find_next_state(const Elevator *elev) {
    // Special case: Emergency stop state stays in emergency stop
    // (only released when user explicitly releases the button)
    if (elev->state == EMERGENCY_STOP) {
        return EMERGENCY_STOP;  // Remain in emergency stop
    }

    // Search transition table for an entry matching current state
    for (size_t i = 0; i < TRANSITION_TABLE_SIZE; i++) {
        // Check if this transition entry is for the current state
        if (transition_table[i].current == elev->state) {
            // If condition function is NULL, unconditionally transition
            // (Used for timed transitions that always occur)
            if (transition_table[i].condition == NULL) {
                return transition_table[i].next;  // Always take this transition
            }
            // If condition function exists, call it to determine if we should transition
            if (transition_table[i].condition(elev)) {
                return transition_table[i].next;  // Condition was true - take transition
            }
        }
    }
    
    // No matching transition found - stay in current state
    return elev->state;
}

/**
 * Simulate one tick (100ms) of elevator operation
 * This is called 10 times per second to update elevator state and position
 * Core operation order:
 * 1. Check for state transitions
 * 2. Execute state change if needed
 * 3. Update elevator position/behavior based on current state
 */
void elevator_tick(Elevator *elev) {
    // Step 1: Save current state (for debugging/tracking state changes)
    elev->previous_state = elev->state;

    // Step 2: Determine the next state based on conditions
    ElevatorState next_state = find_next_state(elev);

    // Step 3: Execute state transition if one is occurring
    if (next_state != elev->state) {
        // Transition is happening - move to new state
        elev->state = next_state;
        elev->ticks = 0;            // Reset duration counter for new state
        elev->motion_ticks = 0;     // Reset floor movement counter

        // When starting upward movement, set target to highest pressed button above current floor
        if (next_state == SMOOTH_START_UP) {
            // Search from top floor down for highest pressed button
            for (Floor f = FLOOR_9; f >= FLOOR_0; f--) {
                // Check if button is pressed AND floor is at or above current position
                if (elev->button_pressed[f] && f >= elev->current_floor) {
                    elev->target_floor = f;  // Set this as our destination
                    break;  // Found the highest - no need to continue
                }
            }
        } 
        // When starting downward movement, set target to lowest pressed button below current floor
        else if (next_state == SMOOTH_START_DOWN) {
            // Search from bottom floor up for lowest pressed button
            for (Floor f = FLOOR_0; f <= FLOOR_9; f++) {
                // Check if button is pressed AND floor is at or below current position
                if (elev->button_pressed[f] && f <= elev->current_floor) {
                    elev->target_floor = f;  // Set this as our destination
                    break;  // Found the lowest - no need to continue
                }
            }
        }
    } else {
        // No state transition - increment duration counter
        // This tracks how long we've been in current state (used for timed transitions)
        elev->ticks++;
    }

    // Step 4: Update elevator position and behavior based on current state
    // The state determines what happens each tick
    switch (elev->state) {
        // MOVING_UP: Physically move elevator up one floor every 5 ticks (500ms)
        case MOVING_UP:
            elev->motion_ticks++;  // Increment movement counter
            // Move up one floor when enough ticks have elapsed
            if (elev->motion_ticks >= 5) {  // Every 5 ticks at 100ms = 500ms per floor
                elev->current_floor++;      // Move up to next floor
                elev->motion_ticks = 0;     // Reset movement counter
                // Prevent going above top floor
                if (elev->current_floor > FLOOR_9) {
                    elev->current_floor = FLOOR_9;  // Clamp to max floor
                }
            }
            break;

        // MOVING_DOWN: Physically move elevator down one floor every 5 ticks (500ms)
        case MOVING_DOWN:
            elev->motion_ticks++;  // Increment movement counter
            // Move down one floor when enough ticks have elapsed
            if (elev->motion_ticks >= 5) {  // Every 5 ticks at 100ms = 500ms per floor
                elev->current_floor--;      // Move down to next floor
                elev->motion_ticks = 0;     // Reset movement counter
                // Prevent going below ground floor
                if (elev->current_floor < FLOOR_0) {
                    elev->current_floor = FLOOR_0;  // Clamp to min floor
                }
            }
            break;

        // IDLE: No movement, waiting for button presses
        case IDLE:
            // Elevator is stopped and waiting - nothing special to do
            break;

        // DOORS_OPEN: Doors are fully open at a floor
        case DOORS_OPEN:
            // Clear the button for this floor when doors open
            // This indicates the passenger has exited and floor is serviced
            elev->button_pressed[elev->current_floor] = false;
            break;

        // DOORS_OPENING: Animation of doors opening
        case DOORS_OPENING:
            // Update visual state - doors are now open
            elev->doors_open = true;
            break;

        // DOORS_CLOSING: Animation of doors closing
        case DOORS_CLOSING:
            // Update visual state - doors are now closed
            elev->doors_open = false;
            break;

        // All other states (SMOOTH_START_UP/DOWN, SMOOTH_STOP_UP/DOWN, EMERGENCY_STOP)
        // Don't require special handling each tick
        default:
            break;
    }
}

/**
 * Convert elevator state enum to human-readable string
 * Used for displaying state in UI and debugging
 */
const char* elevator_state_to_string(ElevatorState state) {
    // Return string representation of each state
    switch (state) {
        case IDLE: return "IDLE";                          // Waiting at floor, doors closed
        case SMOOTH_START_UP: return "SMOOTH_START_UP";    // Starting to accelerate upward
        case MOVING_UP: return "MOVING_UP";                // Moving upward at constant speed
        case SMOOTH_STOP_UP: return "SMOOTH_STOP_UP";      // Decelerating while moving up
        case SMOOTH_START_DOWN: return "SMOOTH_START_DOWN"; // Starting to accelerate downward
        case MOVING_DOWN: return "MOVING_DOWN";            // Moving downward at constant speed
        case SMOOTH_STOP_DOWN: return "SMOOTH_STOP_DOWN";  // Decelerating while moving down
        case DOORS_OPENING: return "DOORS_OPENING";        // Doors in process of opening
        case DOORS_OPEN: return "DOORS_OPEN";              // Doors fully open at a floor
        case DOORS_CLOSING: return "DOORS_CLOSING";        // Doors in process of closing
        case EMERGENCY_STOP: return "EMERGENCY_STOP";      // Emergency stop active
        default: return "UNKNOWN";                         // Unknown state (should not occur)
    }
}

/**
 * Check if elevator is currently between floors
 * Returns true if elevator is in any moving state (not at a floor landing)
 * Used by guardrails to prevent stopping/opening doors between floors
 */
bool elevator_is_between_floors(const Elevator *elev) {
    // Between floors if in any of these states:
    return elev->state == MOVING_UP ||           // Moving up at constant speed
           elev->state == MOVING_DOWN ||         // Moving down at constant speed
           elev->state == SMOOTH_START_UP ||     // Starting to move up
           elev->state == SMOOTH_START_DOWN ||   // Starting to move down
           elev->state == SMOOTH_STOP_UP ||      // Stopping while moving up
           elev->state == SMOOTH_STOP_DOWN;      // Stopping while moving down
}

/**
 * Get elevator's current position as a floating-point floor number
 * Can be used for smooth animations or precise positioning
 */
float elevator_get_position(const Elevator *elev) {
    // Convert integer floor to float representation
    // Currently returns exact floor (future enhancement: interpolate between floors during motion)
    return (float)elev->current_floor;
}

/**
 * Manually open elevator doors (only safe when stopped at floor)
 * Provides manual control for doors
 */
void elevator_open_doors(Elevator *elev) {
    // Only allow door opening when:
    // 1. Elevator is in IDLE state (not transitioning)
    // 2. Elevator is not between floors
    if (elev->state == IDLE && !elevator_is_between_floors(elev)) {
        elev->doors_open = true;  // Open the doors
    }
}

/**
 * Manually close elevator doors (safe when stopped at floor)
 * Provides manual control for doors
 */
void elevator_close_doors(Elevator *elev) {
    // Only allow closing when not between floors (safety rule)
    if (!elevator_is_between_floors(elev)) {
        elev->doors_open = false;  // Close the doors
    }
}
