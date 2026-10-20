/****************************************
*
* Elevator Lab -- Telemetry Recorder and Query Interface
*
* SPDX-License-Identifier: MIT
*
******************************************/

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "simulation.h"
#include <stdint.h>

/**
 * Opaque recorder ownership hides SQLite statements, locks and committed cursors
 */
typedef struct Telemetry Telemetry;
/**
 * Results distinguish invalid input, missing identities and storage failures
 */
typedef enum { TELEMETRY_OK, TELEMETRY_INVALID, TELEMETRY_NOT_FOUND, TELEMETRY_STORAGE_ERROR } TelemetryResult;
/**
 * A run ID of zero selects the current run; INT_MAX to_tick selects its latest recorded tick
 * Limits are 1-200, offset is nonnegative, and passenger_id zero disables that filter
 */
typedef struct {
    int64_t run_id;
    int from_tick, to_tick, limit, offset, passenger_id;
    bool csv;
} TelemetryQuery;

/**
 * Open an owned recorder and start a fresh live run; NULL reports an initialization error
 */
Telemetry *telemetry_open(const char *path, Simulation *sim, char *error, size_t capacity);
/**
 * Release recorder resources; a non-NULL simulation requests final capture and archival
 */
void telemetry_close(Telemetry *store, const Simulation *sim);
/**
 * Persist one atomic capture; false reports failure without advancing the committed cache
 */
bool telemetry_capture(Telemetry *store, const Simulation *sim);
/* Archive the previous run before resetting live state. History is never deleted. */
/**
 * Archive the previous run, allocate a new identity and reset live state without deleting history
 */
bool telemetry_start_run(Telemetry *store, Simulation *sim, const char *name, const char *reason);
/**
 * Return the active experiment identity used to isolate passengers and feedback
 */
int64_t telemetry_run_id(const Telemetry *store);
/**
 * Report whether the latest capture succeeded
 */
bool telemetry_healthy(const Telemetry *store);
/**
 * Return failed capture or transition attempts in the current run
 */
unsigned long telemetry_missed_writes(const Telemetry *store);
/**
 * Return source events overwritten before a successful capture
 */
unsigned long telemetry_lost_events(const Telemetry *store);
/**
 * Return a recorder-owned error message; callers must not free this pointer
 */
const char *telemetry_error(const Telemetry *store);
/**
 * Insert feedback for the run/passenger pair; rating zero omits a rating
 * Write feedback_id only on success and retain feedback for archived experiments
 */
TelemetryResult telemetry_feedback(Telemetry *store, int64_t run_id, int passenger_id,
    const char *text, int rating, int64_t *feedback_id);
/**
 * Serialize an allowlisted dataset into the caller buffer using bound filters
 * Return the typed query result and set length to the successfully written byte count
 */
TelemetryResult telemetry_read(Telemetry *store, const char *dataset, TelemetryQuery query,
    char *buffer, size_t capacity, size_t *length);

#endif
