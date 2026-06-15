#include "guardrails.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

GuardrailStatus guardrail_check(const Elevator *elev) {
    GuardrailStatus status = {0};

    // Guardrail 1: Doors open and moving - immediate violation
    if (elev->doors_open && elevator_is_between_floors(elev)) {
        status.doors_open_violation = true;
    }

    // Guardrail 2: Stop button pressed - track timing
    if (elev->stop_button_pressed) {
        status.stop_button_pressed = true;
        time_t now = time(NULL);
        status.seconds_stopped = (int)(now - elev->stop_start_time);

        // Prompt user every 20 seconds
        if (status.seconds_stopped > 0 && status.seconds_stopped % 20 == 0) {
            status.needs_user_prompt = true;
        }
    }

    // Guardrail 3: Can't stop between floors (enforced elsewhere)
    if (elevator_is_between_floors(elev) && elev->state == EMERGENCY_STOP) {
        status.between_floors_violation = true;
    }

    return status;
}

void guardrail_enforce(Elevator *elev) {
    GuardrailStatus status = guardrail_check(elev);

    // If doors are open and elevator is moving, force immediate stop
    if (status.doors_open_violation) {
        elev->state = IDLE;
        // Let the state machine move to smooth stop on next tick if moving
        if (elevator_is_between_floors(elev)) {
            // Force to nearest floor
            if (elev->state == MOVING_UP || elev->state == SMOOTH_START_UP ||
                elev->state == SMOOTH_STOP_UP) {
                elev->state = SMOOTH_STOP_UP;
            } else {
                elev->state = SMOOTH_STOP_DOWN;
            }
        }
    }

    // Prevent stopping between floors - force to nearest floor
    if (status.between_floors_violation && elevator_is_between_floors(elev)) {
        // Continue to next floor instead of stopping
        if (elev->current_floor < FLOOR_5) {
            elev->target_floor = (Floor)(elev->current_floor + 1);
            elev->state = MOVING_UP;
        } else if (elev->current_floor > FLOOR_1) {
            elev->target_floor = (Floor)(elev->current_floor - 1);
            elev->state = MOVING_DOWN;
        }
    }
}

const char* guardrail_violation_description(const GuardrailStatus *status) {
    static char buffer[256];
    buffer[0] = '\0';

    if (status->doors_open_violation) {
        strcat(buffer, "VIOLATION: Doors open while moving! Force stop.\n");
    }

    if (status->between_floors_violation) {
        strcat(buffer, "VIOLATION: Cannot stop between floors. Continuing to next floor.\n");
    }

    if (status->needs_user_prompt && status->stop_button_pressed) {
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                "Elevator stopped for %d seconds. Press 'R' to resume or 'W' to wait.\n",
                status->seconds_stopped);
    }

    return buffer[0] != '\0' ? buffer : NULL;
}
