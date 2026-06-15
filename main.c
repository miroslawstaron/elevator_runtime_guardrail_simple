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

// Get input with timeout (works on both Windows and Unix)
int get_input_with_timeout(void) {
#ifdef _WIN32
    // Windows: Use GetAsyncKeyState for more reliable input
    if (_kbhit()) {
        return _getch();
    }
    return 0;
#else
    // Unix: Set non-blocking mode
    static int flags_set = 0;
    int oldf, newf;

    if (!flags_set) {
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        newf = oldf | O_NONBLOCK;
        fcntl(STDIN_FILENO, F_SETFL, newf);
        flags_set = 1;
    }

    int ch = getchar();
    if (ch == EOF) {
        return 0;
    }
    return ch;
#endif
}

void draw_elevator_cabin(const Elevator *elev) {
    printf("\n");
    printf("     +---------------------+\n");

    for (int floor = 9; floor >= 0; floor--) {
        printf("  F%d | ", floor);

        // Draw elevator cabin
        if (elev->current_floor == (Floor)floor) {
            if (elev->doors_open) {
                printf("[DOORS OPEN  ]");
            } else {
                printf("[  *  ELEV  ]");
            }
        } else {
            printf("[            ]");
        }

        printf(" |");

        // Draw floor buttons
        if (elev->button_pressed[floor]) {
            printf("  *");  // Button pressed (lit)
        } else {
            printf("  o");  // Button not pressed
        }

        printf("\n");
    }

    printf("     +---------------------+\n");
}

void draw_status(const Elevator *elev, const GuardrailStatus *status) {
    printf("\n+======================================================+\n");
    printf("| ELEVATOR SIMULATOR - Status                         |\n");
    printf("+======================================================+\n");
    printf("| Current Floor:     Floor %d\n", elev->current_floor);
    printf("| Target Floor:      Floor %d\n", elev->target_floor);
    printf("| State:             %-27s |\n", elevator_state_to_string(elev->state));
    printf("| Doors:             %s\n", elev->doors_open ? "OPEN " : "CLOSED");

    if (status->stop_button_pressed) {
        printf("| Stop Button:       PRESSED (%lld seconds)\n", (long long)(time(NULL) - elev->stop_start_time));
    } else {
        printf("| Stop Button:       Released\n");
    }

    // Guardrail status
    printf("| Guardrails:        ");
    if (status->doors_open_violation) {
        printf("DOORS_OPEN_VIOLATION");
    } else if (status->between_floors_violation) {
        printf("BETWEEN_FLOORS_VIOLATION");
    } else if (status->stop_button_pressed) {
        printf("STOP_ACTIVE (%ds)", status->seconds_stopped);
        if (status->needs_user_prompt) {
            printf(" [PROMPT]");
        }
    } else {
        printf("OK");
    }
    printf("\n");

    printf("+======================================================+\n");
    printf("| CONTROLS:\n");
    printf("|   [0-9] - Request floor       [S] - Stop elevator\n");
    printf("|   [R]   - Resume (after stop) [Q] - Quit\n");
    printf("+======================================================+\n");
}

void draw_warnings(const GuardrailStatus *status) {
    if (status->doors_open_violation) {
        printf("\n*** WARNING: Doors open while moving!\n");
    }

    if (status->between_floors_violation) {
        printf("\n*** WARNING: Cannot stop between floors. Moving to nearest floor.\n");
    }

    if (status->needs_user_prompt && status->stop_button_pressed) {
        printf("\n!!! STOP BUTTON ACTIVE (for %d seconds)\n", status->seconds_stopped);
        printf("    Enter 'R' to resume or 'W' to wait\n");
    }
}

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
    printf("|   - Prompts every 20 seconds when stopped           |\n");
    printf("|                                                     |\n");
    printf("| Press any key to return to simulation...            |\n");
    printf("+======================================================+\n");
}

int main(void) {
    Elevator elev;
    elevator_init(&elev);

    bool running = true;

    printf("\n");
    printf("+======================================================+\n");
    printf("|         ELEVATOR SIMULATOR v1.0                    |\n");
    printf("|     With State Machine & Guardrails (10 Floors)    |\n");
    printf("+======================================================+\n");
    printf("\nStarting simulation...\n");
    printf("Press buttons [0-9], [S]top, [R]esume, [H]elp, [Q]uit\n\n");

    SLEEP_MS(1500);

    while (running) {
        // 1. Check guardrails
        GuardrailStatus status = guardrail_check(&elev);

        // 2. Enforce guardrails
        guardrail_enforce(&elev);

        // 3. Update elevator state FIRST (before rendering)
        elevator_tick(&elev);

        // 4. Clear and redraw display
        system(CLEAR_SCREEN);
        printf("\n");
        draw_elevator_cabin(&elev);
        draw_status(&elev, &status);
        draw_warnings(&status);

        // 5. Handle user input (non-blocking)
        int ch = get_input_with_timeout();

        if (ch > 0) {
            switch (ch) {
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
                    Floor floor = (Floor)(ch - '0');
                    elevator_press_button(&elev, floor);
                    printf("\n>>> Button %d pressed\n", floor);
                    SLEEP_MS(300);
                    break;
                }

                case 's':
                case 'S':
                    if (!elev.stop_button_pressed) {
                        elevator_press_stop(&elev);
                        printf("\n>>> EMERGENCY STOP activated\n");
                        SLEEP_MS(300);
                    }
                    break;

                case 'r':
                case 'R':
                    if (elev.stop_button_pressed) {
                        elevator_release_stop(&elev);
                        printf("\n>>> Elevator resumed\n");
                        SLEEP_MS(300);
                    }
                    break;

                case 'w':
                case 'W':
                    printf("\n>>> Waiting at current floor\n");
                    SLEEP_MS(300);
                    break;

                case 'h':
                case 'H':
                    system(CLEAR_SCREEN);
                    draw_help();
                    printf("\nPress any key to return...\n");
                    SLEEP_MS(3000);
                    break;

                case 'q':
                case 'Q':
                    running = false;
                    break;

                default:
                    break;
            }
        }

        // 6. Sleep for simulation tick (100ms = 10 Hz)
        SLEEP_MS(100);
    }

    system(CLEAR_SCREEN);
    printf("\n+======================================================+\n");
    printf("| Thank you for using the Elevator Simulator!         |\n");
    printf("+======================================================+\n\n");

    return 0;
}
