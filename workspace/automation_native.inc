/* Local Graphene Automation API transport: Unix socket or Windows named pipe. */

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

typedef struct {
#if defined(_WIN32)
    HANDLE pipe;
#else
    int server_fd;
    int client_fd;
    char path[108];
#endif
    char input[65536];
    char output[65536];
    size_t input_len;
    char error[256];
} GrapheneAutomationListener;

size_t graphene_automation_listen(encore_str endpoint) {
    GrapheneAutomationListener *listener =
        (GrapheneAutomationListener *)calloc(1, sizeof(GrapheneAutomationListener));
    if (listener == NULL) return 0;
    char *path = graphene_c_string(endpoint);
    if (path == NULL || path[0] == '\0') {
        snprintf(listener->error, sizeof(listener->error), "Automation endpoint is empty");
        free(path);
        return (size_t)listener;
    }
#if defined(_WIN32)
    listener->pipe = CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT,
        1, 65536, 65536, 0, NULL);
    if (listener->pipe == INVALID_HANDLE_VALUE) {
        listener->pipe = NULL;
        snprintf(listener->error, sizeof(listener->error),
            "Unable to create Automation named pipe (%lu)", (unsigned long)GetLastError());
    }
#else
    listener->server_fd = -1;
    listener->client_fd = -1;
    if (strlen(path) >= sizeof(listener->path)) {
        snprintf(listener->error, sizeof(listener->error), "Automation socket path is too long");
        free(path);
        return (size_t)listener;
    }
    listener->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener->server_fd < 0) {
        snprintf(listener->error, sizeof(listener->error), "Unable to create Automation socket: %s",
            strerror(errno));
        free(path);
        return (size_t)listener;
    }
    int flags = fcntl(listener->server_fd, F_GETFL, 0);
    fcntl(listener->server_fd, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
    strncpy(listener->path, path, sizeof(listener->path) - 1);
    unlink(path);
    if (bind(listener->server_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener->server_fd, 8) != 0) {
        snprintf(listener->error, sizeof(listener->error), "Unable to bind Automation socket: %s",
            strerror(errno));
        close(listener->server_fd);
        listener->server_fd = -1;
    } else {
        chmod(path, 0600);
    }
#endif
    free(path);
    return (size_t)listener;
}

bool graphene_automation_available(size_t handle) {
    GrapheneAutomationListener *listener = (GrapheneAutomationListener *)handle;
    if (listener == NULL) return false;
#if defined(_WIN32)
    return listener->pipe != NULL;
#else
    return listener->server_fd >= 0;
#endif
}

encore_str graphene_automation_error(size_t handle) {
    GrapheneAutomationListener *listener = (GrapheneAutomationListener *)handle;
    return graphene_string(listener == NULL ? "Invalid Automation listener" : listener->error);
}

encore_str graphene_automation_poll(size_t handle) {
    GrapheneAutomationListener *listener = (GrapheneAutomationListener *)handle;
    if (!graphene_automation_available(handle)) return graphene_string("");
#if defined(_WIN32)
    BOOL connected = ConnectNamedPipe(listener->pipe, NULL);
    DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
    if (!connected && connect_error != ERROR_PIPE_CONNECTED && connect_error != ERROR_PIPE_LISTENING &&
        connect_error != ERROR_NO_DATA) {
        snprintf(listener->error, sizeof(listener->error), "Automation pipe connection failed (%lu)",
            (unsigned long)connect_error);
        return graphene_string("");
    }
    DWORD read_count = 0;
    if (!ReadFile(listener->pipe, listener->input + listener->input_len,
            (DWORD)(sizeof(listener->input) - listener->input_len - 1), &read_count, NULL)) {
        DWORD read_error = GetLastError();
        if (read_error != ERROR_NO_DATA && read_error != ERROR_PIPE_LISTENING) {
            DisconnectNamedPipe(listener->pipe);
            listener->input_len = 0;
        }
        return graphene_string("");
    }
    listener->input_len += (size_t)read_count;
#else
    if (listener->client_fd < 0) {
        listener->client_fd = accept(listener->server_fd, NULL, NULL);
        if (listener->client_fd < 0) return graphene_string("");
        int flags = fcntl(listener->client_fd, F_GETFL, 0);
        fcntl(listener->client_fd, F_SETFL, flags | O_NONBLOCK);
    }
    ssize_t read_count = recv(listener->client_fd, listener->input + listener->input_len,
        sizeof(listener->input) - listener->input_len - 1, 0);
    if (read_count == 0) {
        close(listener->client_fd);
        listener->client_fd = -1;
        listener->input_len = 0;
        return graphene_string("");
    }
    if (read_count < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(listener->client_fd);
            listener->client_fd = -1;
            listener->input_len = 0;
        }
        return graphene_string("");
    }
    listener->input_len += (size_t)read_count;
#endif
    listener->input[listener->input_len] = '\0';
    char *newline = memchr(listener->input, '\n', listener->input_len);
    if (newline == NULL) {
        if (listener->input_len + 1 >= sizeof(listener->input)) {
            snprintf(listener->error, sizeof(listener->error), "Automation request exceeds 64 KiB");
            listener->input_len = 0;
        }
        return graphene_string("");
    }
    size_t line_len = (size_t)(newline - listener->input);
    memcpy(listener->output, listener->input, line_len);
    listener->output[line_len] = '\0';
    size_t consumed = line_len + 1;
    memmove(listener->input, listener->input + consumed, listener->input_len - consumed);
    listener->input_len -= consumed;
    return graphene_string(listener->output);
}

bool graphene_automation_respond(size_t handle, encore_str response) {
    GrapheneAutomationListener *listener = (GrapheneAutomationListener *)handle;
    if (!graphene_automation_available(handle)) return false;
    char *text = graphene_c_string(response);
    if (text == NULL) return false;
    size_t length = strlen(text);
    bool ok = false;
#if defined(_WIN32)
    DWORD written = 0;
    ok = WriteFile(listener->pipe, text, (DWORD)length, &written, NULL) && written == (DWORD)length;
    if (ok) ok = WriteFile(listener->pipe, "\n", 1, &written, NULL) && written == 1;
#else
    if (listener->client_fd >= 0) {
        ssize_t first = send(listener->client_fd, text, length, 0);
        ssize_t second = first == (ssize_t)length ? send(listener->client_fd, "\n", 1, 0) : -1;
        ok = first == (ssize_t)length && second == 1;
    }
#endif
    free(text);
    return ok;
}

encore_str graphene_automation_request(encore_str endpoint, encore_str request) {
    char *path = graphene_c_string(endpoint);
    char *text = graphene_c_string(request);
    if (path == NULL || text == NULL) {
        free(path);
        free(text);
        return graphene_string("");
    }
    char response[65536];
    size_t response_len = 0;
#if defined(_WIN32)
    if (!WaitNamedPipeA(path, 5000)) {
        free(path); free(text); return graphene_string("");
    }
    HANDLE pipe = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        free(path); free(text); return graphene_string("");
    }
    DWORD written = 0;
    bool sent = WriteFile(pipe, text, (DWORD)strlen(text), &written, NULL) != 0;
    if (sent) sent = WriteFile(pipe, "\n", 1, &written, NULL) != 0;
    while (sent && response_len + 1 < sizeof(response)) {
        DWORD count = 0;
        if (!ReadFile(pipe, response + response_len,
                (DWORD)(sizeof(response) - response_len - 1), &count, NULL) || count == 0) break;
        response_len += (size_t)count;
        if (memchr(response, '\n', response_len) != NULL) break;
    }
    CloseHandle(pipe);
#else
    int client = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client < 0) {
        free(path); free(text); return graphene_string("");
    }
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
    bool sent = connect(client, (struct sockaddr *)&address, sizeof(address)) == 0;
    size_t length = strlen(text);
    if (sent) sent = send(client, text, length, 0) == (ssize_t)length;
    if (sent) sent = send(client, "\n", 1, 0) == 1;
    while (sent && response_len + 1 < sizeof(response)) {
        ssize_t count = recv(client, response + response_len,
            sizeof(response) - response_len - 1, 0);
        if (count <= 0) break;
        response_len += (size_t)count;
        if (memchr(response, '\n', response_len) != NULL) break;
    }
    close(client);
#endif
    free(path);
    free(text);
    char *newline = memchr(response, '\n', response_len);
    if (newline != NULL) response_len = (size_t)(newline - response);
    response[response_len] = '\0';
    return graphene_string(response);
}

bool graphene_automation_destroy(size_t handle) {
    GrapheneAutomationListener *listener = (GrapheneAutomationListener *)handle;
    if (listener == NULL) return false;
#if defined(_WIN32)
    if (listener->pipe != NULL) {
        DisconnectNamedPipe(listener->pipe);
        CloseHandle(listener->pipe);
    }
#else
    if (listener->client_fd >= 0) close(listener->client_fd);
    if (listener->server_fd >= 0) close(listener->server_fd);
    if (listener->path[0] != '\0') unlink(listener->path);
#endif
    free(listener);
    return true;
}

bool graphene_automation_secure_file(encore_str path_value) {
    char *path = graphene_c_string(path_value);
    if (path == NULL || path[0] == '\0') {
        free(path);
        return false;
    }
#if defined(_WIN32)
    /*
     * The discovery file lives below the current user's profile and inherits its
     * ACL. Named-pipe access is still scoped to the interactive user by the
     * capability token stored in this file.
     */
    DWORD attributes = GetFileAttributesA(path);
    bool ok = attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    bool ok = chmod(path, S_IRUSR | S_IWUSR) == 0;
#endif
    free(path);
    return ok;
}

encore_str graphene_automation_capability_token(void) {
    unsigned char bytes[32];
    bool generated = true;
#if defined(_WIN32)
    size_t offset = 0;
    while (offset < sizeof(bytes)) {
        unsigned int value = 0;
        if (rand_s(&value) != 0) {
            generated = false;
            break;
        }
        size_t remaining = sizeof(bytes) - offset;
        size_t count = remaining < sizeof(value) ? remaining : sizeof(value);
        memcpy(bytes + offset, &value, count);
        offset += count;
    }
#else
    int random_fd = open("/dev/urandom", O_RDONLY);
    if (random_fd < 0) {
        generated = false;
    } else {
        size_t offset = 0;
        while (offset < sizeof(bytes)) {
            ssize_t count = read(random_fd, bytes + offset, sizeof(bytes) - offset);
            if (count <= 0) {
                generated = false;
                break;
            }
            offset += (size_t)count;
        }
        close(random_fd);
    }
#endif
    if (!generated) return graphene_string("");
    static const char hex[] = "0123456789abcdef";
    char token[65];
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        token[index * 2] = hex[bytes[index] >> 4];
        token[index * 2 + 1] = hex[bytes[index] & 0x0f];
    }
    token[64] = '\0';
    return graphene_string(token);
}
