/****************************************
*
* Elevator Guardrails -- Guardrail Logic
*
* Author: Miroslaw Staron
* miroslaw.staron@gu.se
*
* SPDX-License-Identifier: MIT
* (c) Miroslaw Staron, 2026
*
******************************************/

#include "guardrails.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

/**
 * Check all guardrails and return their status
 * Guardrails are safety rules that the elevator must obey:
 * 1. Doors cannot be open while elevator is moving
 * 2. Stop button tracking - requires user prompt every 20 seconds
 * 3. Elevator cannot stop between floors
 * 
 * Returns a GuardrailStatus struct with violation flags and metadata
 */
GuardrailStatus guardrail_check(const Elevator *elev) {
    // Initialize status struct with all flags set to false/0
    GuardrailStatus status = {0};

    // GUARDRAIL 1: Check if doors are open while elevator is moving
    // This is a critical safety violation that must be corrected immediately
    if (elev->doors_open && elevator_is_between_floors(elev)) {
        status.doors_open_violation = true;  // Flag this violation
    }

    // GUARDRAIL 2: Track stop button state and elapsed time
    // If stop button is pressed, update status with timing info
    if (elev->stop_button_pressed) {
        status.stop_button_pressed = true;  // Mark stop button as active
        
        // Calculate how many seconds have elapsed since stop was pressed
        time_t now = time(NULL);
        status.seconds_stopped = (int)(now - elev->stop_start_time);

        // Prompt user every 20 seconds to confirm they want to stay stopped
        // This is a safety feature requiring periodic user acknowledgment
        if (status.seconds_stopped > 0 && status.seconds_stopped % 20 == 0) {
            status.needs_user_prompt = true;  // Signal that user should be prompted
        }
    }

    // GUARDRAIL 3: Check if elevator is stopped between floors
    // Detect if in EMERGENCY_STOP state while still in motion (motion_ticks > 0)
    // This is unsafe because emergency stops should only happen at floor landings
    if (elev->state == EMERGENCY_STOP && elev->motion_ticks > 0) {
        status.between_floors_violation = true;  // Flag this safety violation
    }

    return status;  // Return status with all violation flags set
}

/**
 * Enforce all guardrails by taking corrective action if violations are detected
 * This function modifies elevator state to prevent or correct unsafe conditions
 */
void guardrail_enforce(Elevator *elev) {
    // Check current state for any guardrail violations
    GuardrailStatus status = guardrail_check(elev);

    // ENFORCEMENT 1: Doors open while moving - force immediate stop
    // Safety-critical: Must stop smoothly in the direction of current motion
    if (status.doors_open_violation) {
        // Save the current state to determine which direction we're moving
        ElevatorState original_state = elev->state;

        // Determine direction of motion and switch to corresponding smooth stop state
        // Smooth stop allows graceful deceleration rather than abrupt halt
        if (original_state == MOVING_UP || original_state == SMOOTH_START_UP ||
            original_state == SMOOTH_STOP_UP) {
            // Moving upward - switch to upward smooth stop
            elev->state = SMOOTH_STOP_UP;
        } else if (original_state == MOVING_DOWN || original_state == SMOOTH_START_DOWN ||
                   original_state == SMOOTH_STOP_DOWN) {
            // Moving downward - switch to downward smooth stop
            elev->state = SMOOTH_STOP_DOWN;
        }
    }

    // ENFORCEMENT 2: Elevator stopped between floors - force move to nearest floor
    // Safety rule: Elevator must never be stopped or parked between floors
    if (status.between_floors_violation) {
        // Determine which floor is nearest and force elevator to move toward it
        
        // If current floor is below floor 5, move UP to the next floor
        if (elev->current_floor < FLOOR_5) {
            elev->target_floor = (Floor)(elev->current_floor + 1);  // Set destination to next floor up
            elev->state = MOVING_UP;  // Force state to moving upward
        } 
        // If current floor is at or above floor 5, move DOWN to the next floor
        else if (elev->current_floor >= FLOOR_5) {
            elev->target_floor = (Floor)(elev->current_floor - 1);  // Set destination to next floor down
            elev->state = MOVING_DOWN;  // Force state to moving downward
        }
    }
}

/**
 * Generate a human-readable description of guardrail violations
 * Used for displaying violation messages to the user
 * Returns a pointer to a static string describing violations, or NULL if no violations
 */
const char* guardrail_violation_description(const GuardrailStatus *status) {
    // Use static buffer to hold the violation description
    // Static ensures the buffer persists across function calls
    static char buffer[256];
    buffer[0] = '\0';  // Initialize buffer as empty string

    // Check for doors open violation and append description
    if (status->doors_open_violation) {
        strcat(buffer, "VIOLATION: Doors open while moving! Force stop.\n");
    }

    // Check for between-floors violation and append description
    if (status->between_floors_violation) {
        strcat(buffer, "VIOLATION: Cannot stop between floors. Continuing to next floor.\n");
    }

    // Check if user prompt is needed (stop button active for 20 seconds)
    // Include elapsed time and instructions for user
    if (status->needs_user_prompt && status->stop_button_pressed) {
        // Use snprintf to safely append formatted text at end of buffer
        // This shows elapsed time and prompts for user action
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                "Elevator stopped for %d seconds. Press 'R' to resume or 'W' to wait.\n",
                status->seconds_stopped);
    }

    // Return description string if buffer contains text, otherwise return NULL
    // Caller can check for NULL to know if there are any violations to display
    return buffer[0] != '\0' ? buffer : NULL;
}
