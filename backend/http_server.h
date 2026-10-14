#pragma once
#include <winsock2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initializes Winsock, binds to 127.0.0.1 on preferred_port (or next available up to +24)
// Returns actual bound port on success, or 0 on failure
int http_server_init(int preferred_port, const char* web_root_override);

// Runs the server accept loop on the calling thread until http_server_stop() is called
void http_server_run(void);

// Stops the accept loop and closes listening socket
void http_server_stop(void);

// Returns currently bound port
int http_server_get_port(void);

#ifdef __cplusplus
}
#endif
