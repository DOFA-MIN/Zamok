#include "service.h"
#include "config.h"
#include "http_server.h"

#include <iostream>

static SERVICE_STATUS_HANDLE g_status_handle = NULL;
static SERVICE_STATUS g_status = {0};
static HANDLE g_stop_event = NULL;
static HANDLE g_http_thread = NULL;

static void report_status(DWORD state, DWORD wait_hint = 0) {
    g_status.dwCurrentState = state;
    g_status.dwWaitHint = wait_hint;
    if (state == SERVICE_START_PENDING) g_status.dwControlsAccepted = 0;
    else g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_status.dwWin32ExitCode = 0;
    g_status.dwServiceSpecificExitCode = 0;
    g_status.dwCheckPoint = 0;
    SetServiceStatus(g_status_handle, &g_status);
}

static DWORD WINAPI service_worker(LPVOID param) {
    UNREFERENCED_PARAMETER(param);
    http_server_run(ZAMOK_API_PORT, g_stop_event);
    return 0;
}

static void WINAPI service_handler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        report_status(SERVICE_STOP_PENDING);
        SetEvent(g_stop_event);
        break;
    default:
        break;
    }
}

static void WINAPI service_main(DWORD argc, wchar_t** argv) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    g_status_handle = RegisterServiceCtrlHandlerW(ZAMOK_SERVICE_NAME, service_handler);
    if (!g_status_handle) return;

    report_status(SERVICE_START_PENDING, 3000);

    g_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_http_thread = CreateThread(NULL, 0, service_worker, NULL, 0, NULL);

    report_status(SERVICE_RUNNING);

    WaitForSingleObject(g_http_thread, INFINITE);

    report_status(SERVICE_STOPPED);
}

void service_start() {
    SERVICE_TABLE_ENTRYW table[] = {
        { (LPWSTR)ZAMOK_SERVICE_NAME, service_main },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcherW(table);
}

void service_stop() {
    if (g_stop_event) SetEvent(g_stop_event);
}

bool service_install() {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    SC_HANDLE svc = CreateServiceW(
        scm, ZAMOK_SERVICE_NAME, ZAMOK_SERVICE_DISPLAY,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        path, NULL, NULL, NULL, NULL, NULL);

    if (!svc) { CloseServiceHandle(scm); return false; }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool service_remove() {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;

    SC_HANDLE svc = OpenServiceW(scm, ZAMOK_SERVICE_NAME, DELETE | SERVICE_STOP);
    if (!svc) { CloseServiceHandle(scm); return false; }

    SERVICE_STATUS status;
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    DeleteService(svc);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool service_is_installed() {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceW(scm, ZAMOK_SERVICE_NAME, SERVICE_QUERY_STATUS);
    bool installed = (svc != NULL);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return installed;
}

void service_run(int argc, wchar_t** argv) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    service_start();
}
