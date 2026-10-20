/****************************************
*
* Elevator Lab -- SQLite Recorder Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

#include "telemetry.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif

static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
/**
 * Read one integer from a test SQL query and assert preparation and row availability
 */
static int scalar(sqlite3 *db, const char *sql) {
    sqlite3_stmt *q = NULL;
    CHECK(sqlite3_prepare_v2(db, sql, -1, &q, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(q) == SQLITE_ROW);
    int n = sqlite3_column_int(q, 0); sqlite3_finalize(q); return n;
}
/** Verify scheduled journeys never acquire boarding timestamps and analysis uses the whole run. */
static void scenario_analysis_checks(void) {
    static Simulation sim;
    char error[256], output[100000]; size_t length;
    simulation_init(&sim);
    Telemetry *t = telemetry_open(":memory:", &sim, error, sizeof(error)); CHECK(t != NULL);
    ScenarioConfig c = {0}; snprintf(c.kind, sizeof(c.kind), "morning"); c.count = 300; c.duration = 600; c.seed = 42;
    CHECK(simulation_start_scenario(&sim, &c)); CHECK(telemetry_capture(t, &sim));
    TelemetryQuery query = {0, 0, INT_MAX, 200, 0, 0, false};
    CHECK(telemetry_read(t, "analysis", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"passengerCount\":300") && strstr(output, "\"scheduledCount\":299"));
    CHECK(strstr(output, "\"boardedCount\":0") && strstr(output, "\"p95WaitSeconds\":null"));
    CHECK(strstr(output, "\"releasedCount\":1"));
    query.from_tick = 1; query.to_tick = 6000;
    CHECK(telemetry_read(t, "passengers", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"status\":\"scheduled\"") && strstr(output, "\"boardedTick\":null"));
    CHECK(simulation_control(&sim, "cancel_scenario", 0)); CHECK(telemetry_capture(t, &sim));
    query.from_tick = 0; query.to_tick = INT_MAX;
    CHECK(telemetry_read(t, "analysis", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"cancelledCount\":299") && strstr(output, "\"boardedCount\":0"));
    CHECK(telemetry_start_run(t, &sim, "Known timing fixture", "test"));
    CHECK(simulation_add_people(&sim, "Known", 0, 5, 5));
    sim.tick = 1000;
    for (int i = 0; i < 4; i++) {
        sim.people[i].state = ARRIVED; sim.people[i].boarded_tick = (unsigned long)(i + 1) * 100;
        sim.people[i].arrived_tick = sim.people[i].boarded_tick + 50;
    }
    sim.people[0].vip = true; sim.completed = 4;
    CHECK(telemetry_capture(t, &sim));
    CHECK(telemetry_read(t, "analysis", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"p95WaitSeconds\":40") && strstr(output, "\"averageWaitSeconds\":25"));
    CHECK(strstr(output, "\"oldestWaitingSeconds\":100") && strstr(output, "\"averageRideSeconds\":5"));
    CHECK(strstr(output, "\"vipWaitSeconds\":10") && strstr(output, "\"regularWaitSeconds\":30"));
    CHECK(strstr(output, "\"waitingCount\":1") && strstr(output, "\"completedCount\":4"));
    telemetry_close(t, &sim);
}
/**
 * Exercise the real on-disk recorder, feedback, filtering and recovery behavior
 * Use an isolated process-specific database and remove it after verification
 */
int main(void) {
    char path[160], error[256], output[65536]; size_t length;
    snprintf(path, sizeof(path), "test-telemetry-%ld.sqlite3", (long)process_id());
    Simulation sim; simulation_init(&sim);
    Telemetry *t = telemetry_open(path, &sim, error, sizeof(error));
    if (!t) fprintf(stderr, "%s\n", error);
    CHECK(t != NULL); CHECK(telemetry_healthy(t));
    int64_t first_run = telemetry_run_id(t);
    sqlite3 *db = NULL;
    CHECK(sqlite3_open(path, &db) == SQLITE_OK);
    CHECK(scalar(db, "PRAGMA user_version") == 3);
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 1);
    CHECK(telemetry_open(path, &sim, error, sizeof(error)) == NULL);
    CHECK(scalar(db, "SELECT count(*) FROM runs") == 1);
    CHECK(simulation_add_people(&sim, "=Alex \"A\"", 1, 3, 1));
    CHECK(telemetry_capture(t, &sim));
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM passengers") == 1);
    for (int i = 0; i < 120; i++) { simulation_tick(&sim); CHECK(telemetry_capture(t, &sim)); }
    CHECK(sim.completed == 1);
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 121);
    CHECK(scalar(db, "SELECT count(*) FROM passengers WHERE status='arrived' AND arrived_tick>boarded_tick AND boarded_tick>=added_tick") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM events") == (int)sim.event_id);
    CHECK(telemetry_capture(t, &sim));
    CHECK(scalar(db, "SELECT count(*) FROM events") == (int)sim.event_id);

    // SCENARIO 1: Bind hostile-looking text as data and reject invalid feedback identities and ratings
    int64_t feedback;
    const char *text = "=Great \"ride\", caf\xc3\xa9\n'); DROP TABLE samples; -- <script>";
    CHECK(telemetry_feedback(t, first_run, 1, text, 5, &feedback) == TELEMETRY_OK);
    CHECK(feedback > 0);
    CHECK(telemetry_feedback(t, first_run, 1, "\n \t", 0, &feedback) == TELEMETRY_INVALID);
    CHECK(telemetry_feedback(t, first_run, 1, "Invalid\xff", 0, &feedback) == TELEMETRY_INVALID);
    CHECK(telemetry_feedback(t, first_run, 1, "Rating", 6, &feedback) == TELEMETRY_INVALID);
    CHECK(telemetry_feedback(t, first_run, 2, "Missing", 1, &feedback) == TELEMETRY_NOT_FOUND);
    CHECK(telemetry_feedback(t, first_run + 1000, 1, "Missing run", 1, &feedback) == TELEMETRY_NOT_FOUND);
    CHECK(scalar(db, "SELECT count(*) FROM feedback") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 121);

    // SCENARIO 2: Verify bounded tick pagination, CSV quoting, JSON escaping and response overflow
    TelemetryQuery query = {first_run, 20, 40, 7, 0, 0, false};
    CHECK(telemetry_read(t, "samples", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"tick\":20,") != NULL);
    CHECK(strstr(output, "\"hasMore\":true") != NULL);
    CHECK(strstr(output, "\"nextOffset\":7") != NULL);
    query.offset = 14;
    CHECK(telemetry_read(t, "samples", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"tick\":34,") != NULL);
    CHECK(strstr(output, "\"hasMore\":false") != NULL);
    query.from_tick = 0; query.to_tick = INT_MAX; query.limit = 100; query.offset = 0; query.csv = true;
    CHECK(telemetry_read(t, "feedback", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"'=Great \"\"ride\"\"") != NULL);
    query.csv = false;
    CHECK(telemetry_read(t, "feedback", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\\u000a") != NULL);
    CHECK(strstr(output, "\"rating\":5") != NULL);
    CHECK(telemetry_read(t, "summary", query, output, sizeof(output), &length) == TELEMETRY_OK);
    CHECK(strstr(output, "\"completedCount\":1") != NULL);
    CHECK(strstr(output, "\"feedbackCount\":1") != NULL);
    query.limit = 201;
    CHECK(telemetry_read(t, "samples", query, output, sizeof(output), &length) == TELEMETRY_INVALID);
    query.limit = 100;
    CHECK(telemetry_read(t, "samples;DROP TABLE runs", query, output, sizeof(output), &length) == TELEMETRY_NOT_FOUND);
    CHECK(telemetry_read(t, "samples", query, output, 10, &length) == TELEMETRY_STORAGE_ERROR);

    // SCENARIO 3: Force a writer lock and verify capture recovery without corrupting committed history
    CHECK(sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK);
    simulation_tick(&sim);
    CHECK(!telemetry_capture(t, &sim)); CHECK(!telemetry_healthy(t));
    CHECK(telemetry_missed_writes(t) == 1);
    CHECK(sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(telemetry_capture(t, &sim)); CHECK(telemetry_healthy(t));
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 122);
    // SCENARIO 4: Overwrite live events deliberately and verify lost-event accounting
    for (int i = 0; i < 60; i++) CHECK(simulation_control(&sim, "request", 5));
    CHECK(telemetry_capture(t, &sim));
    CHECK(telemetry_lost_events(t) == 12);
    CHECK(scalar(db, "SELECT lost_events FROM runs WHERE end_reason='active'") == 12);
    CHECK(sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK);
    simulation_tick(&sim); CHECK(!telemetry_capture(t, &sim));
    CHECK(sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);

    // SCENARIO 5: Reused passenger IDs remain isolated by their experiment identity
    CHECK(telemetry_start_run(t, &sim, "Second experiment", "new_experiment"));
    int64_t second_run = telemetry_run_id(t);
    CHECK(second_run != first_run && sim.people_count == 0 && sim.tick == 0);
    CHECK(scalar(db, "SELECT missed_writes FROM runs WHERE end_reason='new_experiment'") == 2);
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 123);
    CHECK(telemetry_feedback(t, first_run, 1, "Later feedback on archived trip", 0, &feedback) == TELEMETRY_OK);
    CHECK(simulation_add_people(&sim, "Reused ID", 0, 9, 1));
    CHECK(telemetry_capture(t, &sim));
    CHECK(telemetry_feedback(t, second_run, 1, "Still waiting", 2, &feedback) == TELEMETRY_OK);
    CHECK(scalar(db, "SELECT count(DISTINCT run_id) FROM feedback") == 2);
    // SCENARIO 6: Leave a run active, reopen a version-1 schema and verify migration and interruption recovery
    telemetry_close(t, NULL); /* Simulate an unclean end: leave the run active. */
    CHECK(sqlite3_exec(db, "ALTER TABLE runs DROP COLUMN lost_events; ALTER TABLE passengers DROP COLUMN vip; PRAGMA user_version=1", NULL, NULL, NULL) == SQLITE_OK);
    t = telemetry_open(path, &sim, error, sizeof(error)); CHECK(t != NULL);
    CHECK(scalar(db, "PRAGMA user_version") == 3);
    CHECK(scalar(db, "SELECT lost_events FROM runs ORDER BY id DESC LIMIT 1") == 0);
    CHECK(telemetry_run_id(t) > second_run);
    CHECK(scalar(db, "SELECT count(*) FROM runs WHERE end_reason='interrupted'") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM passengers WHERE status='cancelled'") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM feedback") == 3);
    CHECK(scalar(db, "SELECT count(*) FROM samples") == 124);
    telemetry_close(t, &sim);
    CHECK(scalar(db, "SELECT count(*) FROM runs WHERE end_reason='shutdown'") == 1);
    CHECK(scalar(db, "SELECT count(*) FROM pragma_foreign_key_check") == 0);
    CHECK(sqlite3_exec(db, "PRAGMA user_version=4", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(telemetry_open(path, &sim, error, sizeof(error)) == NULL);
    CHECK(scalar(db, "SELECT count(*) FROM feedback") == 3);
    // CLEANUP: Close test readers before removing the isolated database and lock file
    sqlite3_close(db);
    CHECK(remove(path) == 0);
    char lock_path[176]; snprintf(lock_path, sizeof(lock_path), "%s.lock", path); remove(lock_path);
    scenario_analysis_checks();
    printf("PASS: %d telemetry assertions; persisted samples, events, journeys, feedback, filtering/CSV, run isolation, exclusive ownership, crash recovery and write-failure recovery.\n", checks);
    return 0;
}
