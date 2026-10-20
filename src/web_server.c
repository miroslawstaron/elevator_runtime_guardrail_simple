/* Local HTTP adapter. All simulation state and timing remain in C. SPDX-License-Identifier: MIT */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#ifdef _WIN32
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
typedef int Socket;
#define CLOSE close
#define BAD (-1)
#endif

#define CLIENTS 16
#define INPUT_SIZE 8192
typedef struct {
    Socket socket;
    char input[INPUT_SIZE];
    size_t received, sent, output_size;
    char *output;
    uint64_t deadline;
} Client;
static Client clients[CLIENTS];
static Simulation simulation;
static const char *web_root = "web";
static int port = 8099;
static const char *public_origin = NULL;
static volatile sig_atomic_t running = 1;

static void stop_server(int sig) { (void)sig; running = 0; }
static uint64_t milliseconds(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}
static void nonblocking(Socket fd) {
#ifdef _WIN32
    u_long on = 1; ioctlsocket(fd, FIONBIO, &on);
#else
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#endif
}
static bool would_block(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
#endif
}
static void dispose(Client *c) {
    CLOSE(c->socket); free(c->output); memset(c, 0, sizeof(*c)); c->socket = BAD;
}
static void respond(Client *c, int status, const char *type, const char *data, size_t length) {
    const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 403 ? "Forbidden" :
        status == 404 ? "Not Found" : status == 409 ? "Conflict" : status == 413 ? "Content Too Large" : "Internal Server Error";
    c->output = malloc(length + 1024);
    if (!c->output) { dispose(c); return; }
    int n = snprintf(c->output, 1024,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'\r\n\r\n",
        status, reason, type, length);
    memcpy(c->output + n, data, length); c->output_size = (size_t)n + length;
    c->deadline = milliseconds() + 5000;
}
static void error(Client *c, int code, const char *message) {
    char data[256];
    int n = snprintf(data, sizeof(data), "{\"error\":\"%s\"}", message);
    respond(c, code, "application/json; charset=utf-8", data, (size_t)n);
}
/* Header names are case insensitive; reject duplicate framing/security headers. */
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
static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
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
static void state_response(Client *c) {
    char data[65536];
    size_t size = simulation_json(&simulation, data, sizeof(data));
    if (!size) error(c, 500, "State exceeds response buffer.");
    else respond(c, 200, "application/json; charset=utf-8", data, size);
}
static void route(Client *c, const char *body) {
    char method[12], path[256], protocol[16], host[256], expected[272], origin[272];
    if (sscanf(c->input, "%11s %255s %15s", method, path, protocol) != 3 || strcmp(protocol, "HTTP/1.1")) {
        error(c, 400, "Expected HTTP/1.1."); return;
    }
    snprintf(expected, sizeof(expected), "127.0.0.1:%d", port);
    if (header(c->input, "Host", host, sizeof(host)) != 1) { error(c, 403, "Invalid local host."); return; }
    bool is_public = public_origin && !strcmp(host, public_origin + 8);
    if (!is_public && strcmp(host, expected)) {
        snprintf(expected, sizeof(expected), "localhost:%d", port);
        if (strcmp(host, expected)) { error(c, 403, "Use localhost or 127.0.0.1 with the configured port."); return; }
    }
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
        if (!strcmp(path, "/api/people")) {
            char name[128], from[16], to[16], count[16]; int f, t, n;
            if (!field(body, "name", name, sizeof(name)) || !field(body, "from", from, sizeof(from)) ||
                !field(body, "to", to, sizeof(to)) || !field(body, "count", count, sizeof(count)) ||
                !integer(from, 0, 9, &f) || !integer(to, 0, 9, &t) || !integer(count, 1, 8, &n) ||
                !simulation_add_people(&simulation, name, f, t, n)) {
                error(c, 400, "Use a name up to 40 UTF-8 bytes, different floors 0-9, and 1-8 people. Session limit: 160."); return;
            }
        } else if (!strcmp(path, "/api/control")) {
            char action[32], floor[16]; int f = -1;
            if (!field(body, "action", action, sizeof(action))) { error(c, 400, "Missing action."); return; }
            if (!strcmp(action, "request") && (!field(body, "floor", floor, sizeof(floor)) || !integer(floor, 0, 9, &f))) {
                error(c, 400, "Expected floor 0-9."); return;
            }
            if (!simulation_control(&simulation, action, f)) { error(c, 409, "Command is unavailable in the current state."); return; }
        } else { error(c, 404, "Unknown endpoint."); return; }
        state_response(c); return;
    }
    if (strcmp(method, "GET")) { error(c, 400, "Supported methods: GET and POST."); return; }
    if (!strcmp(path, "/api/state")) { state_response(c); return; }
    const char *file = NULL, *type = NULL;
    if (!strcmp(path, "/") || !strcmp(path, "/index.html")) { file = "index.html"; type = "text/html; charset=utf-8"; }
    if (!strcmp(path, "/app.css")) { file = "app.css"; type = "text/css; charset=utf-8"; }
    if (!strcmp(path, "/app.js")) { file = "app.js"; type = "text/javascript; charset=utf-8"; }
    if (!strcmp(path, "/assets/agenticai-logo.png")) { file = "assets/agenticai-logo.png"; type = "image/png"; }
    if (!file) { error(c, 404, "Not found."); return; }
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
static void read_client(Client *c) {
    int n = recv(c->socket, c->input + c->received, (int)(INPUT_SIZE - 1 - c->received), 0);
    if (n <= 0) { if (!n || !would_block()) dispose(c); return; }
    if (memchr(c->input + c->received, 0, (size_t)n)) { error(c, 400, "NUL bytes are not accepted."); return; }
    c->received += (size_t)n; c->input[c->received] = 0;
    char *end = strstr(c->input, "\r\n\r\n");
    if (!end) { if (c->received == INPUT_SIZE - 1) error(c, 413, "Request too large."); return; }
    char value[64]; int length = 0;
    int has_length = header(c->input, "Content-Length", value, sizeof(value));
    if (has_length < 0 || (has_length && !integer(value, 0, INPUT_SIZE - 1, &length)) ||
        header(c->input, "Transfer-Encoding", value, sizeof(value)) != 0) {
        error(c, 400, "Invalid request framing."); return;
    }
    size_t required = (size_t)(end + 4 - c->input) + (size_t)length;
    if (required >= INPUT_SIZE) { error(c, 413, "Request too large."); return; }
    if (c->received < required) return;
    if (c->received != required) { error(c, 400, "Pipelined requests are not supported."); return; }
    route(c, end + 4);
}
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            if (!integer(argv[++i], 1024, 65535, &port)) { fprintf(stderr, "Invalid port.\n"); return 1; }
        } else if (!strcmp(argv[i], "--web-root") && i + 1 < argc) web_root = argv[++i];
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
        } else { fprintf(stderr, "Usage: elevator_web [--port 8099] [--web-root web] [--public-origin https://hostname]\n"); return 1; }
    }
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    signal(SIGINT, stop_server); signal(SIGTERM, stop_server);
    for (int i = 0; i < CLIENTS; i++) clients[i].socket = BAD;
    Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == BAD) { fprintf(stderr, "Cannot create socket.\n"); return 1; }
    struct sockaddr_in address; memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) || listen(server, CLIENTS)) {
        fprintf(stderr, "Cannot bind 127.0.0.1:%d. Another server may already be running.\n", port); CLOSE(server); return 1;
    }
    nonblocking(server); simulation_init(&simulation);
    printf("Elevator Lab | C runtime\nOpen http://localhost:%d\nAssets: %s\nCtrl+C to stop.\n", port, web_root); fflush(stdout);
    if (public_origin) { printf("Public origin: %s\n", public_origin); fflush(stdout); }
    uint64_t next_tick = milliseconds() + 100;
    while (running) {
        uint64_t now = milliseconds();
        while (now >= next_tick) { simulation_tick(&simulation); next_tick += 100; }
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
    for (int i = 0; i < CLIENTS; i++) if (clients[i].socket != BAD) dispose(&clients[i]);
    CLOSE(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
