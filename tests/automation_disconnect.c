/* clang -std=gnu11 tests/automation_disconnect.c -o target/automation-disconnect */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
typedef struct { const char *value; } encore_str;
static encore_str graphene_string(const char *value) { return (encore_str){value}; }
static char *graphene_c_string(encore_str value) { return strdup(value.value); }
#include "../native/automation/bridge.inc"
int main(void) {
    signal(SIGPIPE, SIG_DFL);
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    GrapheneAutomationListener *listener = calloc(1, sizeof(*listener));
    assert(listener);
    listener->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(listener->server_fd >= 0);
    listener->client_fd = sockets[0];
    close(sockets[1]);
    assert(!graphene_automation_respond((size_t)listener, graphene_string("reply after client timeout")));
    assert(listener->client_fd == -1);
    assert(graphene_automation_available((size_t)listener));
    assert(graphene_automation_destroy((size_t)listener));
    return 0;
}
