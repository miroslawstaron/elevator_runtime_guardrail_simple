/****************************************
*
* Elevator Guardrails -- Guardrails Interface
*
* Author: Miroslaw Staron
* miroslaw.staron@gu.se
*
* SPDX-License-Identifier: MIT
* (c) Miroslaw Staron, 2026
*
******************************************/

#ifndef GUARDRAILS_H
#define GUARDRAILS_H

#include <stdbool.h>
#include "elevator.h"

typedef struct {
    bool doors_open_violation;
    bool between_floors_violation;
    bool stop_button_pressed;
    int seconds_stopped;
    bool needs_user_prompt;
    bool invalid_floor_violation;
} GuardrailStatus;

GuardrailStatus guardrail_check(const Elevator *elev);
void guardrail_enforce(Elevator *elev);
const char* guardrail_violation_description(const GuardrailStatus *status);

#endif // GUARDRAILS_H
