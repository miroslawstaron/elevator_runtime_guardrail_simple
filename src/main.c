/****************************************
*
* Elevator Guardrails -- Main Program
*
* Author: Miroslaw Staron
* miroslaw.staron@gu.se
*
* SPDX-License-Identifier: MIT
* (c) Miroslaw Staron, 2026
*
******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <termios.h>
    #include <fcntl.h>
    #define CLEAR_SCREEN "clear"
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#include "elevator.h"
#include "guardrails.h"

/**
 * Get input from user with timeout (works on both Windows and Unix)
 * Returns the character code of the pressed key, or 0 if no input available
 * Uses platform-specific methods:
 * - Windows: _kbhit() and _getch() for non-blocking keyboard input
 * - Unix: fcntl() to set stdin to non-blocking mode
 */
int get_input_with_timeout(void) {
#ifdef _WIN32
    // Windows: Use GetAsyncKeyState for more reliable input
    // _kbhit() returns non-zero if a key is available
    if (_kbhit()) {
        return _getch();  // Get and return the character code
    }
    return 0;  // No input available
#else
    // Unix: Set non-blocking mode on first call
    static int flags_set = 0;  // Track if non-blocking mode is already configured
    int oldf, newf;

    // One-time initialization: configure stdin as non-blocking
    if (!flags_set) {
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);  // Get current flags
        newf = oldf | O_NONBLOCK;  // Add non-blocking flag
        fcntl(STDIN_FILENO, F_SETFL, newf);  // Apply new flags
        flags_set = 1;  // Mark as initialized
    }

    // Attempt to read one character (won't block due to non-blocking mode)
    int ch = getchar();
    // If no input (EOF), return 0 to indicate nothing available
    if (ch == EOF) {
        return 0;
    }
    return ch;  // Return the character code
#endif
}

/**
 * Draw the ASCII art representation of the elevator cabin
 * Shows the elevator position on each floor and button states
 */
void draw_elevator_cabin(const Elevator *elev) {
    printf("\n");
    printf("     +---------------------+\n");

    // Loop through all floors from top (9) to bottom (0)
    for (int floor = 9; floor >= 0; floor--) {
        printf("  F%d | ", floor);

        // Draw elevator cabin position and state
        // If elevator is on this floor, show its state; otherwise show empty
        if (elev->current_floor == (Floor)floor) {
            // Elevator is on this floor - show doors or cabin
            if (elev->doors_open) {
                printf("[DOORS OPEN  ]");  // Show open doors
            } else {
                printf("[  *  ELEV  ]");  // Show cabin (asterisk represents elevator car)
            }
        } else {
            // Elevator not on this floor - show empty shaft
            printf("[            ]");
        }

        printf(" |");

        // Draw floor button state
        if (elev->button_pressed[floor]) {
            printf("  *");  // Asterisk: Button is pressed/lit
        } else {
            printf("  o");  // Circle: Button not pressed
        }

        printf("\n");
    }

    printf("     +---------------------+\n");
}

/**
 * Draw the status panel showing elevator state, floor info, and guardrail status
 * Also displays control instructions to the user
 */
void draw_status(const Elevator *elev, const GuardrailStatus *status) {
    printf("\n+======================================================+\n");
    printf("| ELEVATOR SIMULATOR - Status                         |\n");
    printf("+======================================================+\n");
    printf("| Current Floor:     Floor %d\n", elev->current_floor);  // Display current position
    printf("| Target Floor:      Floor %d\n", elev->target_floor);    // Display destination
    printf("| State:             %-27s |\n", elevator_state_to_string(elev->state));  // Show state machine state
    printf("| Doors:             %s\n", elev->doors_open ? "OPEN " : "CLOSED");  // Show door state

    // Display stop button status with elapsed time if pressed
    if (status->stop_button_pressed) {
        printf("| Stop Button:       PRESSED (%lld seconds)\n", (long long)(time(NULL) - elev->stop_start_time));
    } else {
        printf("| Stop Button:       Released\n");
    }

    // Display guardrail violation status
    printf("| Guardrails:        ");
    // Check for violations in priority order
    if (status->doors_open_violation) {
        printf("DOORS_OPEN_VIOLATION");  // Critical: doors open while moving
    } else if (status->between_floors_violation) {
        printf("BETWEEN_FLOORS_VIOLATION");  // Critical: stopped between floors
    } else if (status->invalid_floor_violation) {
        printf("INVALID_FLOOR_VIOLATION");
    } else if (status->stop_button_pressed) {
        // Stop button is active - show elapsed time and prompt flag
        printf("STOP_ACTIVE (%ds)", status->seconds_stopped);
        if (status->needs_user_prompt) {
            printf(" [PROMPT]");  // Indicate user should be prompted
        }
    } else {
        printf("OK");  // No violations or active stops
    }
    printf("\n");

    // Display control instructions
    printf("+======================================================+\n");
    printf("| CONTROLS:\n");
    printf("|   [0-9] - Request floor       [S] - Stop elevator\n");
    printf("|   [R]   - Resume (after stop) [Q] - Quit\n");
    printf("+======================================================+\n");
}

/**
 * Display warning messages for any active guardrail violations or prompts
 */
void draw_warnings(const GuardrailStatus *status) {
    // Warning: Doors are open while elevator is moving
    if (status->doors_open_violation) {
        printf("\n*** WARNING: Doors open while moving!\n");
    }

    // Warning: Elevator stopped between floors and is being forced to nearest floor
    if (status->between_floors_violation) {
        printf("\n*** WARNING: Cannot stop between floors. Moving to nearest floor.\n");
    }

    if (status->invalid_floor_violation) {
        printf("\n*** WARNING: Floor out of range detected. Clamped to safe bounds.\n");
    }

    // User prompt: Every 20 seconds while stop button is active
    if (status->needs_user_prompt && status->stop_button_pressed) {
        printf("\n!!! STOP BUTTON ACTIVE (for %d seconds)\n", status->seconds_stopped);
        printf("    Enter 'R' to resume or 'W' to wait\n");
    }
}

/**
 * Display help screen explaining controls and guardrails
 */
void draw_help() {
    printf("\n+======================================================+\n");
    printf("| ELEVATOR SIMULATOR - Help                           |\n");
    printf("+======================================================+\n");
    printf("| Press number keys 0-9 to request a floor.           |\n");
    printf("|                                                     |\n");
    printf("| The elevator will move to requested floors in       |\n");
    printf("| order. You can request multiple floors even while   |\n");
    printf("| the elevator is moving.                             |\n");
    printf("|                                                     |\n");
    printf("| Press 'S' to trigger the emergency stop button.     |\n");
    printf("| The elevator will immediately halt wherever it is.  |\n");
    printf("|                                                     |\n");
    printf("| GUARDRAILS:                                         |\n");
    printf("|   - Never stops between floors                      |\n");
    printf("|   - Stops immediately if doors open while moving    |\n");
    printf("|   - Clamps out-of-range floors to [0, 9]            |\n");
    printf("|   - Prompts every 20 seconds when stopped           |\n");
    printf("|                                                     |\n");
    printf("| Press any key to return to simulation...            |\n");
    printf("+======================================================+\n");
}

/**
 * Main entry point for the elevator simulator
 * Implements the main simulation loop that:
 * 1. Checks guardrails for violations
 * 2. Enforces guardrails (corrects violations)
 * 3. Updates elevator state machine
 * 4. Renders the display
 * 5. Handles user input
 * 6. Sleeps for next tick
 */
int main(void) {
    // Initialize elevator to floor 1, idle state
    Elevator elev;
    elevator_init(&elev);

    // Flag to control main simulation loop
    bool running = true;

    // Display welcome message
    printf("\n");
    printf("+======================================================+\n");
    printf("|         ELEVATOR SIMULATOR v1.0                    |\n");
    printf("|     With State Machine & Guardrails (10 Floors)    |\n");
    printf("+======================================================+\n");
    printf("\nStarting simulation...\n");
    printf("Press buttons [0-9], [S]top, [R]esume, [H]elp, [Q]uit\n\n");

    // Wait for user to read welcome message
    SLEEP_MS(1500);

    // Main simulation loop - runs until user quits
    while (running) {
        // Step 1: Check all guardrails for violations
        GuardrailStatus status = guardrail_check(&elev);

        // Step 2: Enforce guardrails - take corrective action if violations detected
        guardrail_enforce(&elev);

        // Step 3: Update elevator state machine and move elevator
        elevator_tick(&elev);

        // Step 4: Clear screen and render all display elements
        system(CLEAR_SCREEN);
        printf("\n");
        draw_elevator_cabin(&elev);       // ASCII art of elevator
        draw_status(&elev, &status);      // Status panel with state info
        draw_warnings(&status);            // Warning messages if needed

        // Step 5: Handle user input (non-blocking, only if key pressed)
        int ch = get_input_with_timeout();

        // Process user input if a key was pressed
        if (ch > 0) {
            switch (ch) {
                // Floor request buttons (0-9)
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    // Convert ASCII digit to floor number
                    Floor floor = (Floor)(ch - '0');
                    elevator_press_button(&elev, floor);
                    printf("\n>>> Button %d pressed\n", floor);
                    SLEEP_MS(300);  // Brief pause to let user see the message
                    break;
                }

                // Emergency stop button
                case 's':
                case 'S':
                    // Only activate if not already active
                    if (!elev.stop_button_pressed) {
                        elevator_press_stop(&elev);
                        printf("\n>>> EMERGENCY STOP activated\n");
                        SLEEP_MS(300);
                    }
                    break;

                // Resume after emergency stop
                case 'r':
                case 'R':
                    // Only resume if stop button was previously pressed
                    if (elev.stop_button_pressed) {
                        elevator_release_stop(&elev);
                        printf("\n>>> Elevator resumed\n");
                        SLEEP_MS(300);
                    }
                    break;

                // Wait/acknowledge (user acknowledgment during stop prompt)
                case 'w':
                case 'W':
                    printf("\n>>> Waiting at current floor\n");
                    SLEEP_MS(300);
                    break;

                // Display help screen
                case 'h':
                case 'H':
                    system(CLEAR_SCREEN);
                    draw_help();
                    printf("\nPress any key to return...\n");
                    SLEEP_MS(3000);  // Allow time to read help
                    break;

                // Quit the simulation
                case 'q':
                case 'Q':
                    running = false;  // Exit main loop
                    break;

                // Ignore any other keys
                default:
                    break;
            }
        }

        // Step 6: Sleep for one simulation tick (100ms = 10 Hz update rate)
        SLEEP_MS(100);
    }

    // Display goodbye message
    system(CLEAR_SCREEN);
    printf("\n+======================================================+\n");
    printf("| Thank you for using the Elevator Simulator!         |\n");
    printf("+======================================================+\n\n");

    return 0;  // Exit program successfully
}
