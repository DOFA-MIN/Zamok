#pragma once

#include <windows.h>

// Runs a simple HTTP server on the given port until stop_event is signaled.
// Endpoints:
//   GET  /api/status
//   GET  /api/locked
//   POST /api/lock   body: {"path":"...","password":"..."}
//   POST /api/unlock body: {"id":"...","password":"..."} or {"path":"...","password":"..."}
void http_server_run(int port, HANDLE stop_event);
