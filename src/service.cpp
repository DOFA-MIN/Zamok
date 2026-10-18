#include "service.h"
#include "config.h"
#include "locker.h"

#include <iostream>
#include <string>

static SERVICE_STATUS_HANDLE g_status_handle = NULL;
static SERVICE_STATUS g_status = {0};
static HANDLE g_stop_event = NULL;

// ------------------------------------------------------------------
// Service status reporting
// ------------------------------------------------------------------
static void report_status(DWORD state, DWORD wait_hint = 0, DWORD checkpoint = 0) {
    g_status.dwCurrentState = state;
    g_status.dwWaitHint = wait_hint;
    g_status.dwCheckPoint = checkpoint;
    if (state == SERVICE_START_PENDING) g_status.dwControlsAccepted = 0;
    else g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwWin32ExitCode = 0;
    g_status.dwServiceSpecificExitCode = 0;
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    SetServiceStatus(g_status_handle, &g_status);
}

static void WINAPI service_handler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        report_status(SERVICE_STOP_PENDING, 3000, 1);
        if (g_stop_event) SetEvent(g_stop_event);
        break;
    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(g_status_handle, &g_status);
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

    g_stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_stop_event) {
        report_status(SERVICE_STOPPED);
        return;
    }

    // Initialize vault / DB — service ensures directories exist
    locker_init_db();

    report_status(SERVICE_RUNNING);

    // Main service loop — Zamok service is lightweight:
    // monitors vault integrity, waits for stop signal.
    // No HTTP server. All lock/unlock operations are performed
    // by the user-mode Zamok.exe via context menu.
    // Service keeps vault hidden and validates consistency periodically.

    while (WaitForSingleObject(g_stop_event, 5000) == WAIT_TIMEOUT) {
        // Periodic maintenance: ensure vault still hidden,
        // re-apply hidden attribute if user revealed it.
        std::wstring vault = get_vault_dir();
        DWORD attr = GetFileAttributesW(vault.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_HIDDEN)) {
            SetFileAttributesW(vault.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
        }
        // Could add: vault integrity check, orphan entry cleanup
    }

    report_status(SERVICE_STOP_PENDING, 2000, 2);
    if (g_stop_event) {
        CloseHandle(g_stop_event);
        g_stop_event = NULL;
    }
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

    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Service binary path must include --run-service argument and be quoted
    std::wstring binPath = L"\"";
    binPath += exePath;
    binPath += L"\" --run-service";

    SC_HANDLE svc = CreateServiceW(
        scm,
        ZAMOK_SERVICE_NAME,
        ZAMOK_SERVICE_DISPLAY,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        binPath.c_str(),
        NULL, NULL, NULL, NULL, NULL);

    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    // Set description
    SERVICE_DESCRIPTIONW desc = {};
    desc.lpDescription = (LPWSTR)ZAMOK_SERVICE_DESC;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

    // Set failure actions: restart after 60s
    SC_ACTION actions[1] = {};
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 60000;
    SERVICE_FAILURE_ACTIONSW fa = {};
    fa.cActions = 1;
    fa.lpsaActions = actions;
    fa.dwResetPeriod = 86400;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool service_remove() {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;

    SC_HANDLE svc = OpenServiceW(scm, ZAMOK_SERVICE_NAME, DELETE | SERVICE_STOP);
    if (!svc) { CloseServiceHandle(scm); return false; }

    SERVICE_STATUS status = {};
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    // Wait briefly for stop
    for (int i = 0; i < 10; ++i) {
        if (QueryServiceStatus(svc, &status) && status.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(200);
    }
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
