/****************************************
*
* Elevator Lab -- C HTTP Service and Simulation Clock
*
* SPDX-License-Identifier: MIT
*
******************************************/

/* Local HTTP adapter. All simulation state and timing remain in C. SPDX-License-Identifier: MIT */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "simulation.h"
#include "telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET Socket;
#define CLOSE closesocket
#define BAD INVALID_SOCKET
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
typedef int Socket;
#define CLOSE close
#define BAD (-1)
#endif

#define CLIENTS 16
#define INPUT_SIZE 8192
// CLIENT STATE: Track partial reads and writes separately so the server remains responsive
typedef struct {
    Socket socket;
    char input[INPUT_SIZE];
    size_t received, sent, output_size;
    char *output;
    uint64_t deadline;
} Client;
static Client clients[CLIENTS];
static Simulation simulation;
static Telemetry *telemetry;
static const char *database_path = "data/elevator.sqlite3";
static const char *web_root = "web";
static int port = 8099;
static const char *listen_address = "127.0.0.1";
static const char *allowed_hosts[16];
static size_t allowed_host_count;
static const char *public_origin = NULL;
static volatile sig_atomic_t running = 1;

/**
 * Ask the main loop to stop without performing cleanup inside the signal handler
 */
static void stop_server(int sig) { (void)sig; running = 0; }
/**
 * Report listener setup failures with the endpoint and operating-system error.
 * Read the socket error before cleanup can replace it; this function does not close the socket.
 */
static void listener_error(const char *operation) {
#ifdef _WIN32
    fprintf(stderr, "%s %s:%d (socket error %d).\n", operation, listen_address, port, WSAGetLastError());
#else
    fprintf(stderr, "%s %s:%d: %s.\n", operation, listen_address, port, strerror(errno));
#endif
}
/**
 * Return monotonic milliseconds for simulation scheduling and client deadlines
 * Wall-clock changes must not affect the 100 ms runtime tick
 */
static uint64_t milliseconds(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}
/**
 * Configure a socket so a slow client cannot block the simulation loop
 */
static void nonblocking(Socket fd) {
#ifdef _WIN32
    u_long on = 1; ioctlsocket(fd, FIONBIO, &on);
#else
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#endif
}
/**
 * Check whether the most recent socket error permits retrying later
 * Treat interrupted POSIX calls as retryable as well
 */
static bool would_block(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
#endif
}
/**
 * Close one client and release its pending response
 * Reset the slot and mark it available for another connection
 */
static void dispose(Client *c) {
    CLOSE(c->socket); free(c->output); memset(c, 0, sizeof(*c)); c->socket = BAD;
}
/**
 * Queue a complete HTTP response for nonblocking transmission
 * Copy the body, apply content security headers and set a write deadline
 * Dispose the client if response allocation fails
 */
static void respond(Client *c, int status, const char *type, const char *data, size_t length) {
    const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 403 ? "Forbidden" :
        status == 404 ? "Not Found" : status == 409 ? "Conflict" : status == 413 ? "Content Too Large" : status == 503 ? "Service Unavailable" : "Internal Server Error";
    // CONTENT POLICY: SVG exports need embedded styles; application scripts stay same-origin
    const char *policy = !strncmp(type, "image/svg+xml", 13)
        ? "default-src 'none'; style-src 'unsafe-inline'; sandbox; base-uri 'none'; frame-ancestors 'none'"
        : "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'";
    c->output = malloc(length + 1024);
    if (!c->output) { dispose(c); return; }
    int n = snprintf(c->output, 1024,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: %s\r\n\r\n",
        status, reason, type, length, policy);
    memcpy(c->output + n, data, length); c->output_size = (size_t)n + length;
    c->deadline = milliseconds() + 5000;
}
/**
 * Return a bounded JSON error using a server-authored message
 * Messages passed here must be suitable for direct inclusion in this JSON string
 */
static void error(Client *c, int code, const char *message) {
    char data[256];
    int n = snprintf(data, sizeof(data), "{\"error\":\"%s\"}", message);
    respond(c, code, "application/json; charset=utf-8", data, (size_t)n);
}
/* Header names are case insensitive; reject duplicate framing/security headers. */
/**
 * Find a case-insensitive HTTP header and copy its trimmed value
 * Returns 0 if absent, 1 if present, or -1 for duplicates, overflow or malformed input
 */
static int header(const char *input, const char *name, char *value, size_t capacity) {
    const char *line = strstr(input, "\r\n");
    int found = 0;
    while (line && line[2] && line[2] != '\r') {
        line += 2;
        const char *end = strstr(line, "\r\n");
        if (!end) return -1;
        size_t n = strlen(name);
        if ((size_t)(end - line) > n && line[n] == ':') {
            size_t i = 0;
            while (i < n && tolower((unsigned char)line[i]) == tolower((unsigned char)name[i])) i++;
            if (i == n) {
                if (found++) return -1;
                const char *start = line + n + 1;
                while (start < end && (*start == ' ' || *start == '\t')) start++;
                while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
                size_t length = (size_t)(end - start);
                if (length >= capacity) return -1;
                memcpy(value, start, length); value[length] = 0;
            }
        }
        line = strstr(line, "\r\n");
    }
    return found;
}
/**
 * Parse unsigned decimal text within the supplied inclusive bounds
 * Reject signs, whitespace, trailing characters and values outside the accepted range
 */
static bool integer(const char *text, int minimum, int maximum, int *value) {
    if (!text || !*text) return false;
    long n = 0;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (!isdigit(*c) || n > maximum) return false;
        n = n * 10 + (*c - '0');
    }
    if (n < minimum || n > maximum) return false;
    *value = (int)n; return true;
}
/**
 * Compare complete host values without case sensitivity
 * Port checks remain part of the strings constructed by the route handler
 */
static bool host_equal(const char *left, const char *right) {
    while (*left && *right) {
        if (tolower((unsigned char)*left++) != tolower((unsigned char)*right++)) return false;
    }
    return *left == *right;
}
/**
 * Decode one hexadecimal digit
 * Returns -1 when the input is not a hexadecimal character
 */
static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/**
 * Decode one form or query value into a bounded NUL-terminated buffer
 * Reject duplicate keys, invalid percent escapes, embedded NULs and truncation
 */
static bool field(const char *body, const char *key, char *value, size_t capacity) {
    size_t k = strlen(key);
    bool found = false;
    for (const char *p = body; *p;) {
        const char *end = strchr(p, '&');
        if (!end) end = p + strlen(p);
        if ((size_t)(end - p) > k && !strncmp(p, key, k) && p[k] == '=') {
            if (found) return false;
            found = true;
            size_t n = 0;
            for (const char *v = p + k + 1; v < end; v++) {
                unsigned char c = (unsigned char)*v;
                if (c == '+') c = ' ';
                else if (c == '%') {
                    if (v + 2 >= end || hex(v[1]) < 0 || hex(v[2]) < 0) return false;
                    c = (unsigned char)(hex(v[1]) * 16 + hex(v[2])); v += 2;
                }
                if (!c || n + 1 >= capacity) return false;
                value[n++] = (char)c;
            }
            value[n] = 0;
        }
        p = *end ? end + 1 : end;
    }
    return found;
}
/**
 * Return the live simulation snapshot with experiment and recorder health metadata
 * The browser receives state from C and never advances the simulation clock
 */
static void state_response(Client *c) {
    static char data[512 * 1024 + 512];
    size_t size = simulation_json(&simulation, data, 512 * 1024);
    if (!size) error(c, 500, "State exceeds response buffer.");
    else {
        int n = snprintf(data + size - 1, sizeof(data) - size + 1,
            ",\"runId\":%lld,\"telemetry\":{\"healthy\":%s,\"missedWrites\":%lu,\"lostEvents\":%lu}}",
            (long long)telemetry_run_id(telemetry), telemetry_healthy(telemetry) ? "true" : "false", telemetry_missed_writes(telemetry), telemetry_lost_events(telemetry));
        respond(c, 200, "application/json; charset=utf-8", data, size - 1 + (size_t)n);
    }
}
/**
 * Check whether an encoded key is present before applying optional defaults
 */
static bool has_field(const char *body, const char *name) {
    size_t length = strlen(name);
    for (const char *p = body; *p;) {
        if (!strncmp(p, name, length) && p[length] == '=') return true;
        p = strchr(p, '&'); if (!p) return false; p++;
    }
    return false;
}
/**
 * Read a bounded integer option or use its fallback only when absent
 * Malformed or duplicate supplied values remain errors rather than defaults
 */
static bool optional_integer(const char *body, const char *key, int minimum, int maximum, int fallback, int *value) {
    if (!has_field(body, key)) { *value = fallback; return true; }
    char text[32];
    return field(body, key, text, sizeof(text)) && integer(text, minimum, maximum, value);
}
/**
 * Parse a bounded set of custom flows with strict integer fields before starting a run.
 * Each semicolon-separated flow is from,to,count,startSeconds,intervalSeconds,vip.
 */
static bool scenario_config(const char *body, ScenarioConfig *config) {
    memset(config, 0, sizeof(*config));
    if (!field(body, "kind", config->kind, sizeof(config->kind)) ||
        !optional_integer(body, "count", 1, MAX_PEOPLE, 300, &config->count) ||
        !optional_integer(body, "duration", 0, 86400, 600, &config->duration) ||
        !optional_integer(body, "seed", 1, INT_MAX, 42, &config->seed)) return false;
    if (!strcmp(config->kind, "custom")) {
        char flows[4096];
        if (!field(body, "flows", flows, sizeof(flows)) || !flows[0]) return false;
        const char *cursor = flows;
        int total = 0;
        while (*cursor) {
            if (config->flow_count == 20) return false;
            ScenarioFlow *flow = &config->flows[config->flow_count++];
            int *values[] = {&flow->from, &flow->to, &flow->count, &flow->start, &flow->interval, &flow->vip};
            for (int i = 0; i < 6; i++) {
                const char *end = cursor;
                while (*end && *end != ',' && *end != ';') end++;
                size_t length = (size_t)(end - cursor);
                char number[16];
                if (!length || length >= sizeof(number)) return false;
                memcpy(number, cursor, length); number[length] = 0;
                if (!integer(number, 0, 86400, values[i])) return false;
                if (i < 5 && *end != ',') return false;
                if (i == 5 && *end && *end != ';') return false;
                cursor = *end ? end + 1 : end;
                if (i == 5 && *end == ';' && !*cursor) return false;
            }
            total += flow->count;
        }
        if (total != config->count) return false;
    }
    return simulation_valid_scenario(config);
}
/**
 * Validate dataset-specific query options and serialize a bounded read response
 * Map recorder validation, missing data and storage failures to HTTP status codes
 */
static void telemetry_response(Client *c, const char *dataset, const char *parameters) {
    /* Reject misspelled/unsupported filters instead of silently returning a different experiment. */
    for (const char *p = parameters; *p;) {
        const char *end = strchr(p, '&'); if (!end) end = p + strlen(p);
        const char *equals = memchr(p, '=', (size_t)(end - p));
        char key[32];
        if (!equals || (size_t)(equals - p) >= sizeof(key)) { error(c, 400, "Malformed telemetry query option."); return; }
        memcpy(key, p, (size_t)(equals - p)); key[equals - p] = 0;
        bool common = !strcmp(key, "limit") || !strcmp(key, "offset") || !strcmp(key, "format");
        bool run = !strcmp(key, "runId") && strcmp(dataset, "runs");
        bool timed = (!strcmp(key, "fromTick") || !strcmp(key, "toTick")) && strcmp(dataset, "runs") && strcmp(dataset, "summary") && strcmp(dataset, "analysis");
        bool passenger = !strcmp(key, "passengerId") && (!strcmp(dataset, "passengers") || !strcmp(dataset, "feedback"));
        if (!common && !run && !timed && !passenger) { error(c, 400, "Unsupported filter for this telemetry dataset."); return; }
        p = *end ? end + 1 : end;
    }
    TelemetryQuery query = { 0, 0, INT_MAX, 100, 0, 0, false };
    int run;
    if (!optional_integer(parameters, "runId", 1, INT_MAX, 0, &run) ||
        !optional_integer(parameters, "fromTick", 0, INT_MAX, 0, &query.from_tick) ||
        !optional_integer(parameters, "toTick", 0, INT_MAX, INT_MAX, &query.to_tick) ||
        !optional_integer(parameters, "limit", 1, 200, 100, &query.limit) ||
        !optional_integer(parameters, "offset", 0, INT_MAX - 200, 0, &query.offset) ||
        !optional_integer(parameters, "passengerId", 1, MAX_PEOPLE, 0, &query.passenger_id)) {
        error(c, 400, "Invalid telemetry filter. Use integer ticks, run/passenger IDs, and a limit of 1-200."); return;
    }
    query.run_id = run;
    char format[16];
    if (has_field(parameters, "format")) {
        if (!field(parameters, "format", format, sizeof(format)) || (strcmp(format, "json") && strcmp(format, "csv"))) {
            error(c, 400, "Expected format=json or format=csv."); return;
        }
        query.csv = !strcmp(format, "csv");
    }
    // QUERY BUFFER: Bound the maximum serialized page rather than allocating from client input
    char *data = malloc(2 * 1024 * 1024);
    if (!data) { error(c, 500, "Allocation failed."); return; }
    size_t length;
    TelemetryResult result = telemetry_read(telemetry, dataset, query, data, 2 * 1024 * 1024, &length);
    if (result == TELEMETRY_OK) respond(c, 200, query.csv ? "text/csv; charset=utf-8" : "application/json; charset=utf-8", data, length);
    else if (result == TELEMETRY_INVALID) error(c, 400, "Invalid telemetry filter or tick range.");
    else if (result == TELEMETRY_NOT_FOUND) error(c, 404, "Experiment or dataset not found.");
    else error(c, 503, "Telemetry database is temporarily unavailable.");
    free(data);
}
/**
 * Resolve an allowlisted generated diagram or stage-note path
 * Accept only constrained basenames and SVG, Mermaid or Markdown extensions
 * Returns false for traversal, unsupported directories or invalid asset names
 */
static bool evolution_asset(const char *path, char *file, size_t capacity, const char **type) {
    const char *prefix = NULL;
    if (!strncmp(path, "/evolution/diagrams/", sizeof("/evolution/diagrams/") - 1)) prefix = "/evolution/diagrams/";
    else if (!strncmp(path, "/evolution/stages/", sizeof("/evolution/stages/") - 1)) prefix = "/evolution/stages/";
    if (!prefix) return false;
    // ASSET VALIDATION: Check the basename before constructing any local file path
    const char *name = path + strlen(prefix), *extension = strrchr(name, '.');
    if (!extension || extension == name || strlen(name) > 160) return false;
    for (const char *p = name; p < extension; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) return false;
    bool diagram = !strcmp(prefix, "/evolution/diagrams/");
    if (diagram && !strcmp(extension, ".svg")) *type = "image/svg+xml; charset=utf-8";
    else if (diagram && !strcmp(extension, ".mmd")) *type = "text/plain; charset=utf-8";
    else if (!diagram && !strcmp(extension, ".md")) *type = "text/markdown; charset=utf-8";
    else return false;
    return snprintf(file, capacity, "%s", path + 1) < (int)capacity;
}
/**
 * Validate one complete request and dispatch it to the C API or explicit asset routes
 * Check the Host and Origin before commands can mutate the shared simulation
 * Repository source and database files are never exposed by a catch-all file route
 */
static void route(Client *c, const char *body) {
    // REQUEST 1: Parse the request line before resolving its path or security headers
    char method[12], path[256], protocol[16], host[256], expected[272], origin[272];
    if (sscanf(c->input, "%11s %255s %15s", method, path, protocol) != 3 || strcmp(protocol, "HTTP/1.1")) {
        error(c, 400, "Expected HTTP/1.1."); return;
    }
    char *parameters = strchr(path, '?');
    if (parameters) *parameters++ = 0;
    else parameters = "";
    snprintf(expected, sizeof(expected), "127.0.0.1:%d", port);
    if (header(c->input, "Host", host, sizeof(host)) != 1) { error(c, 403, "Invalid local host."); return; }
    // REQUEST 2: Accept the configured public hostname or an explicitly permitted local endpoint
    bool is_public = public_origin && !strcmp(host, public_origin + 8);
    bool is_allowed = false;
    for (size_t i = 0; i < allowed_host_count; i++) {
        char configured[272];
        snprintf(configured, sizeof(configured), "%s:%d", allowed_hosts[i], port);
        if (host_equal(host, configured)) { is_allowed = true; break; }
    }
    if (!is_public && !is_allowed && strcmp(host, expected)) {
        snprintf(expected, sizeof(expected), "localhost:%d", port);
        if (strcmp(host, expected)) {
            /* Accept only the actual destination IP, never arbitrary Host names. */
            struct sockaddr_in local;
#ifdef _WIN32
            int local_size = sizeof(local);
#else
            socklen_t local_size = sizeof(local);
#endif
            if (getsockname(c->socket, (struct sockaddr *)&local, &local_size)) {
                error(c, 403, "Invalid host."); return;
            }
            snprintf(expected, sizeof(expected), "%s:%d", inet_ntoa(local.sin_addr), port);
            if (strcmp(host, expected)) { error(c, 403, "Use this server's IP address or configured hostname and port."); return; }
        }
    }
    // REQUEST 3: Check browser Origin and content type before dispatching any mutation
    if (!strcmp(method, "POST")) {
        int has_origin = header(c->input, "Origin", origin, sizeof(origin));
        if (is_public) snprintf(expected, sizeof(expected), "%s", public_origin);
        else snprintf(expected, sizeof(expected), "http://%s", host);
        if (has_origin < 0 || (has_origin && strcmp(origin, expected))) {
            error(c, 403, "Cross-origin commands are not accepted."); return;
        }
        char content_type[128];
        if (header(c->input, "Content-Type", content_type, sizeof(content_type)) != 1 ||
            strcmp(content_type, "application/x-www-form-urlencoded")) {
            error(c, 400, "Expected application/x-www-form-urlencoded."); return;
        }
        // WRITE API: Validate a whole passenger group before changing shared live state
        if (!strcmp(path, "/api/scenarios")) {
            ScenarioConfig config;
            if (!scenario_config(body, &config)) {
                error(c, 400, "Invalid scenario. Use a known preset or up to 20 custom flows, 1-1000 total people, distinct floors 0-9, and arrivals within 86400 seconds."); return;
            }
            char name[81];
            snprintf(name, sizeof(name), "%s traffic / %d people / seed %d", config.kind, config.count, config.seed);
            if (!telemetry_start_run(telemetry, &simulation, name, "scenario")) {
                error(c, 503, "Could not archive the current experiment. Scenario was not started."); return;
            }
            simulation_start_scenario(&simulation, &config);
        } else if (!strcmp(path, "/api/people")) {
            char name[128], from[16], to[16], count[16]; int f, t, n;
            if (!field(body, "name", name, sizeof(name)) || !field(body, "from", from, sizeof(from)) ||
                !field(body, "to", to, sizeof(to)) || !field(body, "count", count, sizeof(count)) ||
                !integer(from, 0, 9, &f) || !integer(to, 0, 9, &t) || !integer(count, 1, 8, &n) ||
                !simulation_add_people(&simulation, name, f, t, n)) {
                error(c, 400, "Use a name up to 40 UTF-8 bytes, different floors 0-9, and 1-8 people. Session limit: 1000."); return;
            }
        } else if (!strcmp(path, "/api/feedback")) {
            char run[32], passenger[16], text[2001]; int r, p, rating;
            if (!field(body, "runId", run, sizeof(run)) || !integer(run, 1, INT_MAX, &r) ||
                !field(body, "passengerId", passenger, sizeof(passenger)) || !integer(passenger, 1, MAX_PEOPLE, &p) ||
                !field(body, "text", text, sizeof(text)) || !optional_integer(body, "rating", 1, 5, 0, &rating)) {
                error(c, 400, "Feedback needs runId, passengerId, text up to 2000 UTF-8 bytes, and an optional rating of 1-5."); return;
            }
            // FEEDBACK ORDER: Persist the current passenger before inserting feedback with its foreign key
            if (r == telemetry_run_id(telemetry) && !telemetry_capture(telemetry, &simulation)) {
                error(c, 503, "Telemetry database is temporarily unavailable. Feedback was not saved."); return;
            }
            int64_t id;
            TelemetryResult result = telemetry_feedback(telemetry, r, p, text, rating, &id);
            if (result == TELEMETRY_INVALID) error(c, 400, "Enter meaningful UTF-8 feedback up to 2000 bytes and an optional rating of 1-5.");
            else if (result == TELEMETRY_NOT_FOUND) error(c, 404, "Passenger not found in that experiment.");
            else if (result != TELEMETRY_OK) error(c, 503, "Feedback could not be saved. Please retry.");
            else {
                char json[128]; int n = snprintf(json, sizeof(json), "{\"feedbackId\":%lld,\"runId\":%d,\"passengerId\":%d}", (long long)id, r, p);
                respond(c, 200, "application/json; charset=utf-8", json, (size_t)n);
            }
            return;
        } else if (!strcmp(path, "/api/experiments")) {
            char name[81];
            if (!field(body, "name", name, sizeof(name)) || !name[0] || !simulation_valid_text(name, false)) {
                error(c, 400, "Use an experiment name of 1-80 UTF-8 bytes without control characters."); return;
            }
            // EXPERIMENT BOUNDARY: Archive the old run and obtain a new identity before resetting live state
            if (!telemetry_start_run(telemetry, &simulation, name, "new_experiment")) {
                error(c, 503, "Could not start an experiment. Telemetry database is unavailable."); return;
            }
        } else if (!strcmp(path, "/api/control")) {
            char action[32], floor[16]; int f = -1;
            if (!field(body, "action", action, sizeof(action))) { error(c, 400, "Missing action."); return; }
            if ((!strcmp(action, "request") || !strcmp(action, "speed")) && (!field(body, "floor", floor, sizeof(floor)) || !integer(floor, 0, !strcmp(action, "request") ? 9 : 20, &f))) {
                error(c, 400, "Expected floor 0-9."); return;
            }
            if (!strcmp(action, "reset")) {
                if (!telemetry_start_run(telemetry, &simulation, "Untitled experiment", "reset")) {
                    error(c, 503, "Could not archive this experiment. Telemetry database is unavailable."); return;
                }
            } else if (!simulation_control(&simulation, action, f)) { error(c, 409, "Command is unavailable in the current state."); return; }
        } else { error(c, 404, "Unknown endpoint."); return; }
        telemetry_capture(telemetry, &simulation);
        state_response(c); return;
    }
    // READ API: Route authoritative state and persisted telemetry separately from static documentation
    if (strcmp(method, "GET")) { error(c, 400, "Supported methods: GET and POST."); return; }
    if (!strcmp(path, "/api/state")) { state_response(c); return; }
    if (!strncmp(path, "/api/telemetry/", 15)) { telemetry_response(c, path + 15, parameters); return; }
    // STATIC ASSETS: Serve explicit frontend routes and validated generated documentation only
    const char *file = NULL, *type = NULL;
    char documentation_file[256];
    if (!strcmp(path, "/") || !strcmp(path, "/index.html")) { file = "index.html"; type = "text/html; charset=utf-8"; }
    if (!strcmp(path, "/app.css")) { file = "app.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/app.js")) { file = "app.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/scenarios.js")) { file = "scenarios.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/analysis.js")) { file = "analysis.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/scenarios.css")) { file = "scenarios.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/telemetry.js")) { file = "telemetry.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/telemetry.css")) { file = "telemetry.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/evolution.js")) { file = "evolution.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/evolution.css")) { file = "evolution.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/live-sequence.js")) { file = "live-sequence.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/live-sequence.css")) { file = "live-sequence.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/metriczone.css")) { file = "metriczone.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/evolution/catalog.json")) { file = "evolution/catalog.json"; type = "application/json; charset=utf-8"; }
    if (!file && evolution_asset(path, documentation_file, sizeof(documentation_file), &type)) file = documentation_file;
    if (!strcmp(path, "/assets/agenticai-logo.png")) { file = "assets/agenticai-logo.png"; type = "image/png"; }
    if (!strcmp(path, "/assets/metriczone-logo.png")) { file = "assets/metriczone-logo.png"; type = "image/png"; }
    if (!file) { error(c, 404, "Not found."); return; }
    // FILE READ: Bound both the asset path and file size before allocating a response buffer
    char filename[2048];
    if (snprintf(filename, sizeof(filename), "%s/%s", web_root, file) >= (int)sizeof(filename)) { error(c, 500, "Asset path too long."); return; }
    FILE *fp = fopen(filename, "rb");
    if (!fp) { error(c, 404, "Asset missing. Start from the repository or pass --web-root."); return; }
    fseek(fp, 0, SEEK_END); long length = ftell(fp); rewind(fp);
    if (length < 0 || length > 3000000) { fclose(fp); error(c, 500, "Invalid asset size."); return; }
    char *data = malloc((size_t)length + 1);
    if (!data) { fclose(fp); error(c, 500, "Allocation failed."); return; }
    size_t read = fread(data, 1, (size_t)length, fp); fclose(fp);
    if (read != (size_t)length) error(c, 500, "Asset read failed.");
    else respond(c, 200, type, data, read);
    free(data);
}
/**
 * Accumulate one bounded HTTP request without blocking other clients
 * Reject ambiguous framing, NUL bytes and pipelining before dispatching the body
 */
static void read_client(Client *c) {
    int n = recv(c->socket, c->input + c->received, (int)(INPUT_SIZE - 1 - c->received), 0);
    if (n <= 0) { if (!n || !would_block()) dispose(c); return; }
    if (memchr(c->input + c->received, 0, (size_t)n)) { error(c, 400, "NUL bytes are not accepted."); return; }
    c->received += (size_t)n; c->input[c->received] = 0;
    char *end = strstr(c->input, "\r\n\r\n");
    if (!end) { if (c->received == INPUT_SIZE - 1) error(c, 413, "Request too large."); return; }
    char value[64]; int length = 0;
    // FRAMING: Accept one Content-Length body and reject transfer encoding or duplicates
    int has_length = header(c->input, "Content-Length", value, sizeof(value));
    if (has_length < 0 || (has_length && !integer(value, 0, INPUT_SIZE - 1, &length)) ||
        header(c->input, "Transfer-Encoding", value, sizeof(value)) != 0) {
        error(c, 400, "Invalid request framing."); return;
    }
    size_t required = (size_t)(end + 4 - c->input) + (size_t)length;
    if (required >= INPUT_SIZE) { error(c, 413, "Request too large."); return; }
    // COMPLETENESS: Wait for the full body, but do not accept bytes for a second request
    if (c->received < required) return;
    if (c->received != required) { error(c, 400, "Pipelined requests are not supported."); return; }
    route(c, end + 4);
}
/**
 * Configure the HTTP service and run the independent 10 Hz simulation loop
 * Multiplex bounded clients, capture telemetry and clean up resources on shutdown
 * Returns a nonzero exit status when startup validation or initialization fails
 */
int main(int argc, char **argv) {
    // STARTUP 1: Validate listen settings, public origin and explicit hostname aliases
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            if (!integer(argv[++i], 1024, 65535, &port)) { fprintf(stderr, "Invalid port.\n"); return 1; }
        } else if (!strcmp(argv[i], "--listen-address") && i + 1 < argc) listen_address = argv[++i];
        else if (!strcmp(argv[i], "--allowed-host") && i + 1 < argc) {
            const char *name = argv[++i];
            size_t length = strlen(name);
            if (!length || length > 253 || allowed_host_count == 16) {
                fprintf(stderr, "Expected up to 16 allowed hostnames, each 1-253 characters.\n"); return 1;
            }
            for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
                if (!isalnum(*p) && *p != '-' && *p != '.') {
                    fprintf(stderr, "Allowed host must be a hostname without scheme, path, wildcard, or port.\n"); return 1;
                }
            }
            allowed_hosts[allowed_host_count++] = name;
        }
        else if (!strcmp(argv[i], "--web-root") && i + 1 < argc) web_root = argv[++i];
        else if (!strcmp(argv[i], "--public-origin") && i + 1 < argc) {
            public_origin = argv[++i];
            size_t length = strlen(public_origin);
            if (length < 9 || length > 253 || strncmp(public_origin, "https://", 8)) {
                fprintf(stderr, "Public origin must be https://hostname without a path or port.\n"); return 1;
            }
            for (const unsigned char *p = (const unsigned char *)public_origin + 8; *p; p++) {
                if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '.')) {
                    fprintf(stderr, "Invalid public hostname.\n"); return 1;
                }
            }
        } else if (!strcmp(argv[i], "--database") && i + 1 < argc) database_path = argv[++i];
        else { fprintf(stderr, "Usage: elevator_web [--port 8099] [--listen-address 127.0.0.1] [--allowed-host hostname] [--web-root web] [--public-origin https://hostname] [--database data/elevator.sqlite3]\n"); return 1; }
    }
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    signal(SIGINT, stop_server); signal(SIGTERM, stop_server);
    for (int i = 0; i < CLIENTS; i++) clients[i].socket = BAD;
    // STARTUP 2: Bind the requested IPv4 endpoint before opening the persistent recorder
    Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == BAD) { fprintf(stderr, "Cannot create socket.\n"); return 1; }
#ifndef _WIN32
    // POSIX RESTART: Reuse addresses left in TIME_WAIT by this server's closed connections.
    // Do not enable SO_REUSEPORT: an existing listener must still own the endpoint exclusively.
    int reuse_address = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address))) {
        listener_error("Cannot configure SO_REUSEADDR for"); CLOSE(server); return 1;
    }
#endif
    struct sockaddr_in address; memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, listen_address, &address.sin_addr) != 1) {
        fprintf(stderr, "Listen address must be an IPv4 address.\n"); CLOSE(server); return 1;
    }
    if (bind(server, (struct sockaddr *)&address, sizeof(address))) {
        listener_error("Cannot bind"); CLOSE(server); return 1;
    }
    if (listen(server, CLIENTS)) {
        listener_error("Cannot listen on"); CLOSE(server); return 1;
    }
    nonblocking(server); simulation_init(&simulation);
    if (!strcmp(database_path, "data/elevator.sqlite3")) {
#ifdef _WIN32
        _mkdir("data");
#else
        mkdir("data", 0700);
#endif
    }
    char database_error[256];
    telemetry = telemetry_open(database_path, &simulation, database_error, sizeof(database_error));
    if (!telemetry) { fprintf(stderr, "%s\n", database_error); CLOSE(server); return 1; }
    printf("Elevator Lab | C runtime\nOpen http://localhost:%d\nAssets: %s\nCtrl+C to stop.\n", port, web_root); fflush(stdout);
    printf("Listening: %s:%d\n", listen_address, port); fflush(stdout);
    for (size_t i = 0; i < allowed_host_count; i++) printf("Allowed host: %s:%d\n", allowed_hosts[i], port);
    fflush(stdout);
    if (public_origin) { printf("Public origin: %s\n", public_origin); fflush(stdout); }
    printf("Telemetry: %s (SQLite, 10 Hz)\n", database_path); fflush(stdout);
    // RUNTIME 1: Schedule 100 ms ticks independently of browser requests
    uint64_t next_tick = milliseconds() + 100;
    while (running) {
        uint64_t now = milliseconds();
        // CLOCK: Catch up elapsed ticks and retry unhealthy recording at a reduced rate
        while (now >= next_tick) {
            // SPEED: Keep 100 ms simulation steps and persist each one at accelerated wall time.
            for (int step = 0; step < simulation.speed; step++) {
                simulation_tick(&simulation);
                if (telemetry_healthy(telemetry) || simulation.tick % 10 == 0) telemetry_capture(telemetry, &simulation);
            }
            next_tick += 100;
        }
        // RUNTIME 2: Poll readable/writable clients and expire slow connections
        fd_set reads, writes; FD_ZERO(&reads); FD_ZERO(&writes); FD_SET(server, &reads);
        Socket highest = server;
        for (int i = 0; i < CLIENTS; i++) {
            Client *c = &clients[i];
            if (c->socket == BAD) continue;
            if (now > c->deadline) { dispose(c); continue; }
            if (c->output) FD_SET(c->socket, &writes); else FD_SET(c->socket, &reads);
            if (c->socket > highest) highest = c->socket;
        }
        struct timeval timeout = { 0, 20000 };
        int ready = select((int)highest + 1, &reads, &writes, NULL, &timeout);
        if (ready < 0) continue;
        // RUNTIME 3: Admit connections only while a bounded client slot is available
        if (FD_ISSET(server, &reads)) {
            Socket fd = accept(server, NULL, NULL);
            if (fd != BAD) {
                int slot = -1;
#ifndef _WIN32
                if (fd < FD_SETSIZE)
#endif
                for (int i = 0; i < CLIENTS; i++) if (clients[i].socket == BAD) { slot = i; break; }
                if (slot < 0) CLOSE(fd);
                else { nonblocking(fd); clients[slot].socket = fd; clients[slot].deadline = milliseconds() + 3000; }
            }
        }
        for (int i = 0; i < CLIENTS; i++) {
            Client *c = &clients[i]; if (c->socket == BAD) continue;
            if (FD_ISSET(c->socket, &reads)) read_client(c);
            if (c->socket != BAD && c->output && FD_ISSET(c->socket, &writes)) {
                size_t left = c->output_size - c->sent;
                int n = send(c->socket, c->output + c->sent, (int)(left > 65536 ? 65536 : left), 0);
                if (n > 0) { c->sent += (size_t)n; if (c->sent == c->output_size) dispose(c); }
                else if (!n || !would_block()) dispose(c);
            }
        }
    }
    // SHUTDOWN: Release all clients and attempt a final telemetry archive before exiting
    for (int i = 0; i < CLIENTS; i++) if (clients[i].socket != BAD) dispose(&clients[i]);
    CLOSE(server);
    telemetry_close(telemetry, &simulation);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
