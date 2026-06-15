/****************************************
*
* Elevator Guardrails -- Elevator Interface
*
* Author: Miroslaw Staron
* miroslaw.staron@gu.se
*
* SPDX-License-Identifier: MIT
* (c) Miroslaw Staron, 2026
*
******************************************/

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <time.h>
#include <stdbool.h>

typedef enum {
    FLOOR_0 = 0,
    FLOOR_1 = 1,
    FLOOR_2 = 2,
    FLOOR_3 = 3,
    FLOOR_4 = 4,
    FLOOR_5 = 5,
    FLOOR_6 = 6,
    FLOOR_7 = 7,
    FLOOR_8 = 8,
    FLOOR_9 = 9,
} Floor;

typedef enum {
    IDLE,                    // Waiting at a floor, doors closed
    SMOOTH_START_UP,         // Starting motion upward (1 tick)
    MOVING_UP,               // Moving upward between floors
    SMOOTH_STOP_UP,          // Stopping motion upward (1 tick)
    SMOOTH_START_DOWN,       // Starting motion downward (1 tick)
    MOVING_DOWN,             // Moving downward between floors
    SMOOTH_STOP_DOWN,        // Stopping motion downward (1 tick)
    DOORS_OPENING,           // Doors in process of opening
    DOORS_OPEN,              // Doors fully open at a floor
    DOORS_CLOSING,           // Doors in process of closing
    EMERGENCY_STOP,          // Emergency stop (stop button pressed)
} ElevatorState;

typedef struct {
    Floor current_floor;
    Floor target_floor;
    ElevatorState state;
    ElevatorState previous_state;
    bool doors_open;
    bool stop_button_pressed;
    bool button_pressed[10];  // buttons[0] through buttons[9] for floors
    time_t stop_start_time;  // When stop button was first pressed
    int ticks;               // Simulation ticks for smooth motion
    int motion_ticks;        // Ticks spent in current floor during motion (for smooth travel)
} Elevator;

// Initialize elevator at floor 1
void elevator_init(Elevator *elev);

// Register a button press for a specific floor
void elevator_press_button(Elevator *elev, Floor floor);

// Press emergency stop button
void elevator_press_stop(Elevator *elev);

// Release emergency stop button
void elevator_release_stop(Elevator *elev);

// Simulate one tick of elevator operation
void elevator_tick(Elevator *elev);

// Get string representation of state
const char* elevator_state_to_string(ElevatorState state);

// Check if elevator is between floors (for guardrails)
bool elevator_is_between_floors(const Elevator *elev);

// Get the position as a float (1.0 to 5.0)
float elevator_get_position(const Elevator *elev);

// Open doors (only if at a floor)
void elevator_open_doors(Elevator *elev);

// Close doors (only if at a floor)
void elevator_close_doors(Elevator *elev);

#endif // ELEVATOR_H
