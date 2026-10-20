/****************************************
*
* Elevator Lab -- Embedded SQLite Experiment Recorder
*
* SPDX-License-Identifier: MIT
*
******************************************/

/* Embedded experiment recorder. SQLite is linked into the same C process. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "telemetry.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// RECORDER STATE: Own the database, reusable statements and committed capture cursor
struct Telemetry {
    sqlite3 *db;
    sqlite3_stmt *sample, *passenger, *event, *progress;
    int64_t run_id;
    unsigned long last_event, missed_writes, lost_events;
    Passenger cached[MAX_PEOPLE];
    int cached_count;
    bool healthy;
    char error[192];
#ifdef _WIN32
    HANDLE lock;
#else
    int lock;
#endif
};

/**
 * Return wall-clock time as Unix milliseconds for persisted timestamps
 * Simulation ordering uses ticks rather than this adjustable wall clock
 */
static int64_t now_ms(void) {
#ifdef _WIN32
    FILETIME ft; ULARGE_INTEGER value;
    GetSystemTimeAsFileTime(&ft); value.LowPart = ft.dwLowDateTime; value.HighPart = ft.dwHighDateTime;
    return (int64_t)((value.QuadPart - 116444736000000000ULL) / 10000);
#else
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}
/**
 * Execute fixed SQL for schema changes or transaction boundaries
 * Returns true only when SQLite completes the operation
 */
static bool exec_sql(Telemetry *t, const char *sql) { return sqlite3_exec(t->db, sql, NULL, NULL, NULL) == SQLITE_OK; }
/**
 * Compile fixed SQL into a prepared statement owned by the caller
 * User-provided values are bound separately instead of interpolated into SQL
 */
static bool prepare(Telemetry *t, sqlite3_stmt **stmt, const char *sql) { return sqlite3_prepare_v2(t->db, sql, -1, stmt, NULL) == SQLITE_OK; }
/**
 * Execute a write statement and clear it for reuse
 * Reset bindings after both success and failure so optional values do not leak
 */
static bool done(sqlite3_stmt *stmt) {
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_reset(stmt); sqlite3_clear_bindings(stmt);
    return ok;
}
/**
 * Bind text with a private SQLite copy
 * The caller may reuse its input buffer after this call
 */
static void bind_text(sqlite3_stmt *stmt, int i, const char *text) { sqlite3_bind_text(stmt, i, text, -1, SQLITE_TRANSIENT); }
/**
 * Save the SQLite error, roll back pending writes and mark recording unhealthy
 * Increment failed-write accounting and return false to the caller
 */
static bool storage_failure(Telemetry *t) {
    snprintf(t->error, sizeof(t->error), "%s", sqlite3_errmsg(t->db));
    exec_sql(t, "ROLLBACK");
    t->healthy = false; t->missed_writes++;
    return false;
}
/**
 * Translate a live passenger state into its stored journey label
 */
static const char *status(PassengerState state) {
    return state == WAITING ? "waiting" : state == RIDING ? "riding" :
        state == SCHEDULED ? "scheduled" : state == CANCELLED ? "cancelled" : "arrived";
}

/**
 * Return the current persistent experiment ID
 */
int64_t telemetry_run_id(const Telemetry *t) { return t->run_id; }
/**
 * Report whether the latest capture completed successfully
 */
bool telemetry_healthy(const Telemetry *t) { return t->healthy; }
/**
 * Return the number of failed capture or run-transition writes in this run
 */
unsigned long telemetry_missed_writes(const Telemetry *t) { return t->missed_writes; }
/**
 * Return the count of source events overwritten before successful capture
 */
unsigned long telemetry_lost_events(const Telemetry *t) { return t->lost_events; }
/**
 * Return the recorder-owned error buffer; an empty string means no capture error
 */
const char *telemetry_error(const Telemetry *t) { return t->error; }

/**
 * Persist one simulation snapshot, changed passengers and unseen events atomically
 * Advance the in-memory cache and event cursor only after a successful commit
 * Returns false on storage failure while leaving the live simulation available
 */
bool telemetry_capture(Telemetry *t, const Simulation *s) {
    int64_t timestamp = now_ms();
    unsigned long lost_events = t->lost_events;
    // CAPTURE 1: Detect events already overwritten in the live buffer before this capture
    if (s->event_count && s->events[0].id > t->last_event + 1) lost_events += s->events[0].id - t->last_event - 1;
    // CAPTURE 2: Write samples, journey updates, new events and progress in one transaction
    if (!exec_sql(t, "BEGIN IMMEDIATE")) return storage_failure(t);
    const Elevator *e = &s->elevator;
    int waiting = 0, mask = 0;
    bool room = simulation_occupancy(s) < CABIN_CAPACITY;
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        waiting += p->state == WAITING;
        if (p->state == RIDING) mask |= 1 << p->to;
        if (p->state == WAITING && room) mask |= 1 << p->from;
    }
    for (int i = 0; i < 10; i++) if (s->requests[i] || e->button_pressed[i]) mask |= 1 << i;
    double position = (double)e->current_floor;
    if (e->state == MOVING_UP) position += e->motion_ticks / 5.0;
    if (e->state == MOVING_DOWN) position -= e->motion_ticks / 5.0;
    sqlite3_stmt *q = t->sample;
    sqlite3_bind_int64(q, 1, t->run_id); sqlite3_bind_int64(q, 2, (sqlite3_int64)s->tick);
    sqlite3_bind_int64(q, 3, timestamp); sqlite3_bind_int(q, 4, e->current_floor); sqlite3_bind_int(q, 5, e->target_floor);
    sqlite3_bind_double(q, 6, position); bind_text(q, 7, elevator_state_to_string(e->state));
    sqlite3_bind_int(q, 8, s->direction); sqlite3_bind_int(q, 9, e->doors_open); sqlite3_bind_int(q, 10, e->stop_button_pressed);
    sqlite3_bind_int(q, 11, s->stop_requested); sqlite3_bind_int(q, 12, simulation_occupancy(s)); sqlite3_bind_int(q, 13, waiting);
    sqlite3_bind_int(q, 14, s->completed); sqlite3_bind_int(q, 15, s->stops); sqlite3_bind_int(q, 16, mask);
    sqlite3_bind_int(q, 17, s->guardrails.doors_open_violation); sqlite3_bind_int(q, 18, s->guardrails.between_floors_violation);
    sqlite3_bind_int(q, 19, s->guardrails.invalid_floor_violation); sqlite3_bind_int(q, 20, s->guardrails.seconds_stopped);
    if (!done(q)) return storage_failure(t);
    // CAPTURE 3: Update only passengers whose in-memory state differs from the committed cache
    for (int i = 0; i < s->people_count; i++) {
        const Passenger *p = &s->people[i];
        if (i < t->cached_count && !memcmp(p, &t->cached[i], sizeof(*p))) continue;
        q = t->passenger;
        sqlite3_bind_int64(q, 1, t->run_id); sqlite3_bind_int(q, 2, p->id); bind_text(q, 3, p->name);
        sqlite3_bind_int(q, 4, p->from); sqlite3_bind_int(q, 5, p->to); bind_text(q, 6, status(p->state));
        sqlite3_bind_int64(q, 7, (sqlite3_int64)p->added_tick);
        if (p->state == RIDING || p->state == ARRIVED) sqlite3_bind_int64(q, 8, (sqlite3_int64)p->boarded_tick);
        if (p->state == ARRIVED) sqlite3_bind_int64(q, 9, (sqlite3_int64)p->arrived_tick);
        sqlite3_bind_int64(q, 10, timestamp);
        sqlite3_bind_int(q, 11, p->vip);
        if (!done(q)) return storage_failure(t);
    }
    // CAPTURE 4: Persist each newly observed source event once within this run
    for (int i = 0; i < s->event_count; i++) {
        const SimulationEvent *event = &s->events[i];
        if (event->id <= t->last_event) continue;
        q = t->event;
        sqlite3_bind_int64(q, 1, t->run_id); sqlite3_bind_int64(q, 2, (sqlite3_int64)event->id);
        sqlite3_bind_int64(q, 3, (sqlite3_int64)event->tick); sqlite3_bind_int64(q, 4, timestamp); bind_text(q, 5, event->message);
        if (!done(q)) return storage_failure(t);
    }
    q = t->progress;
    sqlite3_bind_int64(q, 1, (sqlite3_int64)s->tick); sqlite3_bind_int64(q, 2, (sqlite3_int64)t->missed_writes);
    sqlite3_bind_int64(q, 3, (sqlite3_int64)lost_events); sqlite3_bind_int64(q, 4, t->run_id);
    if (!done(q) || !exec_sql(t, "COMMIT")) return storage_failure(t);
    // COMMIT: Advance cursors only after SQLite commits; failed transactions remain retryable
    memcpy(t->cached, s->people, sizeof(Passenger) * s->people_count); t->cached_count = s->people_count;
    t->last_event = s->event_id; t->lost_events = lost_events; t->healthy = true; t->error[0] = 0;
    return true;
}

/**
 * Archive the current run and mark unfinished journeys as cancelled
 * The caller supplies the surrounding transaction and the reason for ending the run
 */
static bool end_run(Telemetry *t, const char *reason) {
    sqlite3_stmt *q = NULL;
    if (!prepare(t, &q, "UPDATE runs SET ended_at_ms=?, end_reason=?,missed_writes=?,lost_events=? WHERE id=? AND ended_at_ms IS NULL")) return false;
    sqlite3_bind_int64(q, 1, now_ms()); bind_text(q, 2, reason);
    sqlite3_bind_int64(q, 3, (sqlite3_int64)t->missed_writes); sqlite3_bind_int64(q, 4, (sqlite3_int64)t->lost_events); sqlite3_bind_int64(q, 5, t->run_id);
    bool ok = done(q); sqlite3_finalize(q);
    if (!ok || !prepare(t, &q, "UPDATE passengers SET status='cancelled' WHERE run_id=? AND status IN ('waiting','riding','scheduled')")) return false;
    sqlite3_bind_int64(q, 1, t->run_id); ok = done(q); sqlite3_finalize(q); return ok;
}

/**
 * Archive the old run and allocate a new experiment before resetting live state
 * Clear per-run caches and counters, then record the new initial snapshot
 * Returns false if validation, the transition transaction or initial capture fails
 */
bool telemetry_start_run(Telemetry *t, Simulation *s, const char *name, const char *reason) {
    if (!name || !name[0] || strlen(name) > 80 || !simulation_valid_text(name, false)) return false;
    if (!exec_sql(t, "BEGIN IMMEDIATE")) return storage_failure(t);
    if (t->run_id && !end_run(t, reason)) return storage_failure(t);
    sqlite3_stmt *q = NULL;
    if (!prepare(t, &q, "INSERT INTO runs(name,started_at_ms) VALUES(?,?)")) return storage_failure(t);
    bind_text(q, 1, name); sqlite3_bind_int64(q, 2, now_ms());
    bool ok = done(q); sqlite3_finalize(q);
    int64_t next = sqlite3_last_insert_rowid(t->db);
    if (!ok || !exec_sql(t, "COMMIT")) return storage_failure(t);
    t->run_id = next; t->last_event = 0; t->cached_count = 0; t->missed_writes = 0; t->lost_events = 0;
    simulation_init(s);
    return telemetry_capture(t, s);
}

/**
 * Acquire exclusive recorder ownership and open or migrate the embedded database
 * Recover interrupted runs, prepare reusable statements and start a fresh live run
 * Returns an owned recorder, or NULL with a bounded error message on failure
 */
Telemetry *telemetry_open(const char *path, Simulation *s, char *error, size_t capacity) {
    Telemetry *t = calloc(1, sizeof(*t));
    if (!t) { snprintf(error, capacity, "Cannot allocate telemetry recorder."); return NULL; }
#ifdef _WIN32
    t->lock = INVALID_HANDLE_VALUE;
#else
    t->lock = -1;
#endif
    // OWNERSHIP: Hold an OS lock for file-backed recorders so two simulators cannot own one live history
    if (strcmp(path, ":memory:")) {
        char lock_path[2048];
        if (snprintf(lock_path, sizeof(lock_path), "%s.lock", path) >= (int)sizeof(lock_path)) {
            snprintf(error, capacity, "Database path is too long."); goto failed;
        }
#ifdef _WIN32
        t->lock = CreateFileA(lock_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (t->lock == INVALID_HANDLE_VALUE) {
#else
        t->lock = open(lock_path, O_CREAT | O_RDWR, 0600);
        if (t->lock < 0 || flock(t->lock, LOCK_EX | LOCK_NB)) {
#endif
            snprintf(error, capacity, "Database is already owned by another simulator, or its directory is unavailable."); goto failed;
        }
    }
    if (sqlite3_open_v2(path, &t->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        snprintf(error, capacity, "Cannot open telemetry database: %s", t->db ? sqlite3_errmsg(t->db) : "allocation failure"); goto failed;
    }
    // STARTUP 1: Bound lock waits so database contention does not stall the simulation loop
    sqlite3_busy_timeout(t->db, 5);
    sqlite3_stmt *version = NULL;
    if (!prepare(t, &version, "PRAGMA user_version")) goto sql_failed;
    int rc = sqlite3_step(version); int schema_version = rc == SQLITE_ROW ? sqlite3_column_int(version, 0) : -1;
    sqlite3_finalize(version);
    if (schema_version < 0 || schema_version > 3) { snprintf(error, capacity, "Unsupported telemetry schema version."); goto failed; }
    /* Upgrade the first recorder schema without replacing existing experiments. */
    if (schema_version == 1) {
        sqlite3_stmt *column = NULL;
        if (!prepare(t, &column, "SELECT count(*) FROM pragma_table_info('runs') WHERE name='lost_events'")) goto sql_failed;
        int result = sqlite3_step(column);
        bool has_column = result == SQLITE_ROW && sqlite3_column_int(column, 0) != 0;
        sqlite3_finalize(column);
        if (result != SQLITE_ROW || (!has_column && !exec_sql(t, "ALTER TABLE runs ADD COLUMN lost_events INTEGER NOT NULL DEFAULT 0"))) goto sql_failed;
    }
    // MIGRATION: Add priority without replacing historical rows or changing their meaning.
    if (schema_version > 0 && schema_version < 3) {
        sqlite3_stmt *column = NULL;
        if (!prepare(t, &column, "SELECT count(*) FROM pragma_table_info('passengers') WHERE name='vip'")) goto sql_failed;
        int result = sqlite3_step(column);
        bool has_column = result == SQLITE_ROW && sqlite3_column_int(column, 0) != 0;
        sqlite3_finalize(column);
        if (result != SQLITE_ROW || (!has_column && !exec_sql(t, "ALTER TABLE passengers ADD COLUMN vip INTEGER NOT NULL DEFAULT 0"))) goto sql_failed;
    }
    // STARTUP 2: Create the schema, enable referential integrity and archive interrupted runs
    if (!exec_sql(t,
        "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;"
        "BEGIN IMMEDIATE;"
        "CREATE TABLE IF NOT EXISTS runs(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,started_at_ms INTEGER NOT NULL,ended_at_ms INTEGER,last_tick INTEGER NOT NULL DEFAULT 0,end_reason TEXT NOT NULL DEFAULT 'active',missed_writes INTEGER NOT NULL DEFAULT 0,lost_events INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS samples(run_id INTEGER NOT NULL REFERENCES runs(id),tick INTEGER NOT NULL,captured_at_ms INTEGER NOT NULL,floor INTEGER NOT NULL,target INTEGER NOT NULL,position REAL NOT NULL,state TEXT NOT NULL,direction INTEGER NOT NULL,doors_open INTEGER NOT NULL,stopped INTEGER NOT NULL,stop_pending INTEGER NOT NULL,occupancy INTEGER NOT NULL,waiting INTEGER NOT NULL,completed INTEGER NOT NULL,stops INTEGER NOT NULL,request_mask INTEGER NOT NULL,guard_doors INTEGER NOT NULL,guard_between INTEGER NOT NULL,guard_floor INTEGER NOT NULL,stop_seconds INTEGER NOT NULL,PRIMARY KEY(run_id,tick));"
        "CREATE TABLE IF NOT EXISTS passengers(run_id INTEGER NOT NULL REFERENCES runs(id),passenger_id INTEGER NOT NULL,name TEXT NOT NULL,from_floor INTEGER NOT NULL,to_floor INTEGER NOT NULL,status TEXT NOT NULL,added_tick INTEGER NOT NULL,boarded_tick INTEGER,arrived_tick INTEGER,updated_at_ms INTEGER NOT NULL,vip INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(run_id,passenger_id));"
        "CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY AUTOINCREMENT,run_id INTEGER NOT NULL REFERENCES runs(id),source_id INTEGER NOT NULL,tick INTEGER NOT NULL,recorded_at_ms INTEGER NOT NULL,message TEXT NOT NULL,UNIQUE(run_id,source_id));"
        "CREATE INDEX IF NOT EXISTS events_run_tick ON events(run_id,tick,id);"
        "CREATE TABLE IF NOT EXISTS feedback(id INTEGER PRIMARY KEY AUTOINCREMENT,run_id INTEGER NOT NULL,passenger_id INTEGER NOT NULL,created_at_ms INTEGER NOT NULL,submitted_tick INTEGER NOT NULL,text TEXT NOT NULL CHECK(length(text)>0),rating INTEGER CHECK(rating BETWEEN 1 AND 5),FOREIGN KEY(run_id,passenger_id) REFERENCES passengers(run_id,passenger_id));"
        "CREATE INDEX IF NOT EXISTS feedback_run_tick ON feedback(run_id,submitted_tick,id);"
        "CREATE INDEX IF NOT EXISTS passengers_run_tick ON passengers(run_id,added_tick,passenger_id);"
        "UPDATE passengers SET status='cancelled' WHERE status IN ('waiting','riding','scheduled') AND run_id IN (SELECT id FROM runs WHERE ended_at_ms IS NULL);"
        "UPDATE runs SET ended_at_ms=CAST((julianday('now')-2440587.5)*86400000 AS INTEGER),end_reason='interrupted' WHERE ended_at_ms IS NULL;"
        "PRAGMA user_version=3; COMMIT;")) goto sql_failed;
    // STARTUP 3: Prepare the fixed write statements reused by every successful capture
    if (!prepare(t, &t->sample, "INSERT OR REPLACE INTO samples VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)") ||
        !prepare(t, &t->passenger, "INSERT INTO passengers VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(run_id,passenger_id) DO UPDATE SET status=excluded.status,boarded_tick=excluded.boarded_tick,arrived_tick=excluded.arrived_tick,updated_at_ms=excluded.updated_at_ms") ||
        !prepare(t, &t->event, "INSERT INTO events(run_id,source_id,tick,recorded_at_ms,message) VALUES(?,?,?,?,?)") ||
        !prepare(t, &t->progress, "UPDATE runs SET last_tick=?,missed_writes=?,lost_events=? WHERE id=?")) goto sql_failed;
    if (!telemetry_start_run(t, s, "Untitled experiment", "startup")) goto sql_failed;
    return t;
sql_failed:
    snprintf(error, capacity, "Telemetry initialization failed: %s", sqlite3_errmsg(t->db));
failed:
    telemetry_close(t, NULL); return NULL;
}

/**
 * Release prepared statements, the database connection and ownership lock
 * With a simulation snapshot, attempt final capture and a clean shutdown archive
 * A NULL snapshot skips archival, including during initialization failure
 */
void telemetry_close(Telemetry *t, const Simulation *s) {
    if (!t) return;
    if (s && t->run_id) {
        telemetry_capture(t, s);
        if (exec_sql(t, "BEGIN IMMEDIATE")) { if (end_run(t, "shutdown")) exec_sql(t, "COMMIT"); else exec_sql(t, "ROLLBACK"); }
    }
    sqlite3_finalize(t->sample); sqlite3_finalize(t->passenger); sqlite3_finalize(t->event); sqlite3_finalize(t->progress);
    if (t->db) sqlite3_close(t->db);
#ifdef _WIN32
    if (t->lock != INVALID_HANDLE_VALUE) CloseHandle(t->lock);
#else
    if (t->lock >= 0) close(t->lock);
#endif
    free(t);
}

/**
 * Store feedback for an existing passenger identified by run and passenger ID
 * Validate meaningful UTF-8 text and an optional rating; zero means no rating
 * Returns a typed result and writes the new feedback ID on success
 */
TelemetryResult telemetry_feedback(Telemetry *t, int64_t run_id, int passenger_id, const char *text, int rating, int64_t *feedback_id) {
    if (run_id < 1 || passenger_id < 1 || passenger_id > MAX_PEOPLE || !text || !text[0] || strlen(text) > 2000 ||
        !simulation_valid_text(text, true) || rating < 0 || rating > 5) return TELEMETRY_INVALID;
    bool meaningful = false;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) if (*c > 32) meaningful = true;
    if (!meaningful) return TELEMETRY_INVALID;
    sqlite3_stmt *q = NULL;
    // FEEDBACK: Resolve the composite passenger identity and bind the text as data
    if (!prepare(t, &q, "INSERT INTO feedback(run_id,passenger_id,created_at_ms,submitted_tick,text,rating) SELECT p.run_id,p.passenger_id,?,r.last_tick,?,? FROM passengers p JOIN runs r ON r.id=p.run_id WHERE p.run_id=? AND p.passenger_id=?")) return TELEMETRY_STORAGE_ERROR;
    sqlite3_bind_int64(q, 1, now_ms()); bind_text(q, 2, text);
    if (rating) sqlite3_bind_int(q, 3, rating);
    sqlite3_bind_int64(q, 4, run_id); sqlite3_bind_int(q, 5, passenger_id);
    bool ok = done(q); sqlite3_finalize(q);
    if (!ok) return TELEMETRY_STORAGE_ERROR;
    if (!sqlite3_changes(t->db)) return TELEMETRY_NOT_FOUND;
    *feedback_id = sqlite3_last_insert_rowid(t->db);
    return TELEMETRY_OK;
}

typedef struct { char *data; size_t length, capacity; bool failed; } Output;
/**
 * Append formatted output into a bounded response buffer
 * Latch truncation so the caller cannot return an incomplete JSON or CSV response
 */
static void append(Output *o, const char *format, ...) {
    if (o->failed) return;
    va_list args; va_start(args, format);
    int n = vsnprintf(o->data + o->length, o->capacity - o->length, format, args); va_end(args);
    if (n < 0 || (size_t)n >= o->capacity - o->length) { o->failed = true; return; }
    o->length += (size_t)n;
}
/**
 * Quote one text value for JSON or CSV output
 * Double CSV quotes and prefix formula-like cells for safer spreadsheet imports
 * Escape JSON quotes, backslashes and control bytes without altering UTF-8 text
 */
static void quoted(Output *o, const char *text, bool csv) {
    append(o, "\"");
    if (csv && text[0] && strchr("=+-@\t\r\n", text[0])) append(o, "'");
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (csv) { if (*p == '"') append(o, "\"\""); else append(o, "%c", *p); }
        else if (*p == '"' || *p == '\\') append(o, "\\%c", *p);
        else if (*p < 32) append(o, "\\u%04x", *p);
        else append(o, "%c", *p);
    }
    append(o, "\"");
}

/**
 * Read one allowlisted dataset using bound run, tick and pagination filters
 * Use one extra row to report whether a following page exists
 * Returns a typed result and the number of serialized JSON or CSV bytes
 */
TelemetryResult telemetry_read(Telemetry *t, const char *dataset, TelemetryQuery query, char *buffer, size_t capacity, size_t *length) {
    *length = 0;
    if (query.limit < 1 || query.limit > 200 || query.offset < 0 || query.from_tick < 0 || query.to_tick < query.from_tick || query.passenger_id < 0 || query.passenger_id > MAX_PEOPLE) return TELEMETRY_INVALID;
    // QUERY 1: Resolve the requested experiment and fix an omitted upper tick at its recorded end
    if (query.run_id == 0) query.run_id = t->run_id;
    sqlite3_stmt *q = NULL;
    if (!prepare(t, &q, "SELECT last_tick FROM runs WHERE id=?")) return TELEMETRY_STORAGE_ERROR;
    sqlite3_bind_int64(q, 1, query.run_id);
    int rc = sqlite3_step(q);
    if (rc != SQLITE_ROW) { sqlite3_finalize(q); return rc == SQLITE_DONE ? TELEMETRY_NOT_FOUND : TELEMETRY_STORAGE_ERROR; }
    if (query.to_tick == INT_MAX) query.to_tick = sqlite3_column_int(q, 0);
    sqlite3_finalize(q); q = NULL;
    // QUERY 2: Select only fixed SQL; the dataset name never becomes executable SQL
    const char *sql = NULL;
    bool runs = !strcmp(dataset, "runs"), summary = !strcmp(dataset, "summary");
    if (runs) sql = "SELECT id AS runId,name,started_at_ms AS startedAtMs,ended_at_ms AS endedAtMs,last_tick AS lastTick,end_reason AS endReason,missed_writes AS missedWrites,lost_events AS lostEvents FROM runs ORDER BY id DESC LIMIT ?4 OFFSET ?5";
    else if (summary) sql = "SELECT r.id AS runId,r.name,r.started_at_ms AS startedAtMs,r.ended_at_ms AS endedAtMs,r.last_tick AS lastTick,r.end_reason AS endReason,r.missed_writes AS missedWrites,r.lost_events AS lostEvents,(SELECT count(*) FROM samples WHERE run_id=r.id) AS sampleCount,(SELECT count(*) FROM events WHERE run_id=r.id) AS eventCount,(SELECT count(*) FROM passengers WHERE run_id=r.id) AS passengerCount,(SELECT count(*) FROM passengers WHERE run_id=r.id AND arrived_tick IS NOT NULL) AS completedCount,(SELECT avg((boarded_tick-added_tick)*0.1) FROM passengers WHERE run_id=r.id) AS averageWaitSeconds,(SELECT avg((arrived_tick-boarded_tick)*0.1) FROM passengers WHERE run_id=r.id) AS averageRideSeconds,(SELECT count(*) FROM feedback WHERE run_id=r.id) AS feedbackCount,(SELECT avg(rating) FROM feedback WHERE run_id=r.id) AS averageRating FROM runs r WHERE r.id=?1 LIMIT ?4 OFFSET ?5";
    else if (!strcmp(dataset, "samples")) sql = "SELECT run_id AS runId,tick,captured_at_ms AS capturedAtMs,floor,target,position,state,direction,doors_open AS doorsOpen,stopped,stop_pending AS stopPending,occupancy,waiting,completed,stops,request_mask AS requestMask,guard_doors AS guardDoors,guard_between AS guardBetweenFloors,guard_floor AS guardInvalidFloor,stop_seconds AS secondsStopped FROM samples WHERE run_id=?1 AND tick BETWEEN ?2 AND ?3 ORDER BY tick LIMIT ?4 OFFSET ?5";
    else if (!strcmp(dataset, "passengers")) sql = "SELECT run_id AS runId,passenger_id AS passengerId,name,from_floor AS fromFloor,to_floor AS toFloor,status,vip,added_tick AS addedTick,boarded_tick AS boardedTick,arrived_tick AS arrivedTick,(boarded_tick-added_tick)*0.1 AS waitSeconds,(arrived_tick-boarded_tick)*0.1 AS rideSeconds FROM passengers WHERE run_id=?1 AND added_tick BETWEEN ?2 AND ?3 AND (?6=0 OR passenger_id=?6) ORDER BY passenger_id LIMIT ?4 OFFSET ?5";
    // ANALYSIS: Aggregate the entire selected run in SQLite; future arrivals never count as demand.
    else if (!strcmp(dataset, "analysis")) sql =
        "WITH p AS (SELECT * FROM passengers WHERE run_id=?1),"
        "s AS (SELECT * FROM samples WHERE run_id=?1),"
        "w AS (SELECT (boarded_tick-added_tick)*0.1 AS wait,row_number() OVER(ORDER BY boarded_tick-added_tick) AS n,count(*) OVER() AS total FROM p WHERE boarded_tick IS NOT NULL) "
        "SELECT r.id AS runId,r.name,r.last_tick AS lastTick,r.ended_at_ms AS endedAtMs,r.missed_writes AS missedWrites,r.lost_events AS lostEvents,"
        "(SELECT count(*) FROM s) AS sampleCount,"
        "(SELECT count(*) FROM p) AS passengerCount,"
        "(SELECT count(*) FROM p WHERE status='scheduled') AS scheduledCount,"
        "(SELECT count(*) FROM p WHERE status='cancelled') AS cancelledCount,"
        "(SELECT count(*) FROM p WHERE status='waiting') AS waitingCount,"
        "(SELECT count(*) FROM p WHERE status='riding') AS ridingCount,"
        "(SELECT count(*) FROM p WHERE arrived_tick IS NOT NULL) AS completedCount,"
        "(SELECT count(*) FROM w) AS boardedCount,"
        "(SELECT avg(wait) FROM w) AS averageWaitSeconds,"
        "(SELECT min(wait) FROM w WHERE n>=(total*95+99)/100) AS p95WaitSeconds,"
        "(SELECT max(wait) FROM w) AS maxWaitSeconds,"
        "(SELECT max((r.last_tick-added_tick)*0.1) FROM p WHERE status='waiting') AS oldestWaitingSeconds,"
        "(SELECT avg((arrived_tick-boarded_tick)*0.1) FROM p WHERE arrived_tick IS NOT NULL) AS averageRideSeconds,"
        "(SELECT avg((boarded_tick-added_tick)*0.1) FROM p WHERE vip=1) AS vipWaitSeconds,"
        "(SELECT avg((boarded_tick-added_tick)*0.1) FROM p WHERE vip=0) AS regularWaitSeconds,"
        "(SELECT count(*) FROM p WHERE from_floor=0 AND status!='scheduled' AND added_tick<=r.last_tick) AS lobbyDepartures,"
        "(SELECT count(*) FROM p WHERE to_floor=0 AND status!='scheduled' AND added_tick<=r.last_tick) AS lobbyArrivals,"
        "(SELECT count(*) FROM p WHERE status!='scheduled' AND added_tick<=r.last_tick) AS releasedCount,"
        "(SELECT max(waiting) FROM s) AS peakQueue,"
        "(SELECT avg(occupancy)*100.0/8 FROM s WHERE state LIKE '%UP' OR state LIKE '%DOWN') AS movingLoadPercent,"
        "(SELECT avg(CASE WHEN occupancy=0 THEN 100.0 ELSE 0 END) FROM s WHERE state LIKE '%UP' OR state LIKE '%DOWN') AS emptyTravelPercent,"
        "(SELECT avg(CASE WHEN occupancy=8 THEN 100.0 ELSE 0 END) FROM s WHERE state LIKE '%UP' OR state LIKE '%DOWN') AS fullTravelPercent,"
        "(SELECT sum(stopped)*0.1 FROM s) AS stoppedSeconds,"
        "(SELECT sum(CASE WHEN guard_doors OR guard_between OR guard_floor THEN 1 ELSE 0 END) FROM s) AS guardViolations,"
        "(SELECT count(*) FROM feedback WHERE run_id=r.id) AS feedbackCount,"
        "(SELECT avg(rating) FROM feedback WHERE run_id=r.id) AS averageRating "
        "FROM runs r WHERE r.id=?1 LIMIT ?4 OFFSET ?5";
    else if (!strcmp(dataset, "events")) sql = "SELECT id,run_id AS runId,source_id AS sourceId,tick,recorded_at_ms AS recordedAtMs,message FROM events WHERE run_id=?1 AND tick BETWEEN ?2 AND ?3 ORDER BY tick,id LIMIT ?4 OFFSET ?5";
    else if (!strcmp(dataset, "feedback")) sql = "SELECT f.id,f.run_id AS runId,f.passenger_id AS passengerId,p.name,f.created_at_ms AS createdAtMs,f.submitted_tick AS tick,p.from_floor AS fromFloor,p.to_floor AS toFloor,f.text,f.rating FROM feedback f JOIN passengers p ON p.run_id=f.run_id AND p.passenger_id=f.passenger_id WHERE f.run_id=?1 AND f.submitted_tick BETWEEN ?2 AND ?3 AND (?6=0 OR f.passenger_id=?6) ORDER BY f.submitted_tick,f.id LIMIT ?4 OFFSET ?5";
    if (!sql) return TELEMETRY_NOT_FOUND;
    if (!prepare(t, &q, sql)) return TELEMETRY_STORAGE_ERROR;
    // QUERY 3: Bind filters and request one extra row for reliable hasMore metadata
    sqlite3_bind_int64(q, 1, query.run_id); sqlite3_bind_int(q, 2, query.from_tick); sqlite3_bind_int(q, 3, query.to_tick);
    sqlite3_bind_int(q, 4, query.limit + 1); sqlite3_bind_int(q, 5, query.offset); sqlite3_bind_int(q, 6, query.passenger_id);
    Output out = { buffer, 0, capacity, capacity == 0 };
    int columns = sqlite3_column_count(q), rows = 0;
    if (query.csv) {
        for (int i = 0; i < columns; i++) { if (i) append(&out, ","); quoted(&out, sqlite3_column_name(q, i), true); }
        append(&out, "\r\n");
    } else {
        append(&out, "{\"dataset\":"); quoted(&out, dataset, false);
        append(&out, ",\"runId\":%lld,\"fromTick\":%d,\"toTick\":%d,\"limit\":%d,\"offset\":%d,\"columns\":[", (long long)query.run_id, query.from_tick, query.to_tick, query.limit, query.offset);
        for (int i = 0; i < columns; i++) { if (i) append(&out, ","); quoted(&out, sqlite3_column_name(q, i), false); }
        append(&out, "],\"rows\":[");
    }
    // QUERY 4: Preserve SQL types, represent missing JSON values as null and stop at the page limit
    bool more = false;
    while ((rc = sqlite3_step(q)) == SQLITE_ROW) {
        if (rows == query.limit) { more = true; break; }
        if (!query.csv) append(&out, "%s{", rows ? "," : "");
        for (int i = 0; i < columns; i++) {
            if (i) append(&out, ",");
            if (!query.csv) { quoted(&out, sqlite3_column_name(q, i), false); append(&out, ":"); }
            int type = sqlite3_column_type(q, i);
            if (type == SQLITE_NULL) { if (!query.csv) append(&out, "null"); }
            else if (type == SQLITE_INTEGER) append(&out, "%lld", (long long)sqlite3_column_int64(q, i));
            else if (type == SQLITE_FLOAT) append(&out, "%.6g", sqlite3_column_double(q, i));
            else quoted(&out, (const char *)sqlite3_column_text(q, i), query.csv);
        }
        append(&out, query.csv ? "\r\n" : "}"); rows++;
    }
    sqlite3_finalize(q);
    if (rc != SQLITE_DONE && !more) return TELEMETRY_STORAGE_ERROR;
    if (!query.csv) append(&out, "],\"hasMore\":%s,\"nextOffset\":%d}", more ? "true" : "false", query.offset + rows);
    if (out.failed) return TELEMETRY_STORAGE_ERROR;
    *length = out.length; return TELEMETRY_OK;
}
