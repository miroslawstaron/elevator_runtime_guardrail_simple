#ifndef GUARDRAILS_H
#define GUARDRAILS_H

#include "elevator.h"
#include <stdbool.h>

typedef struct {
    bool doors_open_violation;      // Attempting to move while doors open
    bool between_floors_violation;  // Attempting to stop between floors
    bool stop_button_pressed;       // Stop button is currently active
    int seconds_stopped;            // Seconds elapsed since stop pressed
    bool needs_user_prompt;         // User should be prompted (every 20s)
} GuardrailStatus;

// Check all guardrails and return status
GuardrailStatus guardrail_check(const Elevator *elev);

// Enforce guardrails - force stop if violations detected
void guardrail_enforce(Elevator *elev);

// Get description of guardrail violations
const char* guardrail_violation_description(const GuardrailStatus *status);

#endif // GUARDRAILS_H
