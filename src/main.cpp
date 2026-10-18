#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <comdef.h>

#include "config.h"
#include "locker.h"
#include "dialogs.h"
#include "service.h"

static void print_usage() {
    std::wcout << L"Zamok Folder Locker" << std::endl;
    std::wcout << L"Usage:" << std::endl;
    std::wcout << L"  Zamok.exe --install-service     Install Windows service" << std::endl;
    std::wcout << L"  Zamok.exe --uninstall-service   Remove Windows service" << std::endl;
    std::wcout << L"  Zamok.exe --start-service       Start Windows service" << std::endl;
    std::wcout << L"  Zamok.exe --run-service         Run as Windows service (SCM)" << std::endl;
    std::wcout << L"  Zamok.exe --install-menu        Install context menu items" << std::endl;
    std::wcout << L"  Zamok.exe --uninstall-menu      Remove context menu items" << std::endl;
    std::wcout << L"  Zamok.exe --lock <folder>       Lock a folder" << std::endl;
    std::wcout << L"  Zamok.exe --unlock <folder>     Unlock a folder" << std::endl;
    std::wcout << L"  Zamok.exe --manager             Show locked folders manager" << std::endl;
}

static bool ensure_directories() {
    locker_init_db();
    return true;
}

int wmain(int argc, wchar_t** argv) {
    CoInitialize(NULL);

    if (argc < 2) {
        print_usage();
        CoUninitialize();
        return 1;
    }

    std::wstring cmd = argv[1];

    if (cmd == L"--install-service") {
        if (service_install()) std::wcout << L"Service installed." << std::endl;
        else std::wcout << L"Failed to install service." << std::endl;
    }
    else if (cmd == L"--uninstall-service") {
        if (service_remove()) std::wcout << L"Service removed." << std::endl;
        else std::wcout << L"Failed to remove service." << std::endl;
    }
    else if (cmd == L"--start-service") {
        SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if (scm) {
            SC_HANDLE svc = OpenServiceW(scm, ZAMOK_SERVICE_NAME, SERVICE_START);
            if (svc) {
                StartServiceW(svc, 0, NULL);
                CloseServiceHandle(svc);
            }
            CloseServiceHandle(scm);
        }
    }
    else if (cmd == L"--run-service") {
        service_run(argc, argv);
    }
    else if (cmd == L"--install-menu") {
        if (context_menu_install()) std::wcout << L"Context menu installed." << std::endl;
        else std::wcout << L"Failed to install context menu." << std::endl;
    }
    else if (cmd == L"--uninstall-menu") {
        context_menu_remove();
        std::wcout << L"Context menu removed." << std::endl;
    }
    else if (cmd == L"--lock") {
        if (argc < 3) {
            std::wcout << L"Usage: Zamok.exe --lock <folder>" << std::endl;
            CoUninitialize();
            return 1;
        }
        ensure_directories();
        std::wstring folder = argv[2];

        if (locker_is_locked(folder)) {
            dialog_message(NULL, L"Zamok", L"This folder is already locked.", false);
            CoUninitialize();
            return 0;
        }

        std::wstring password;
        if (!dialog_password(NULL, L"Zamok - Lock Folder", L"Enter password to lock:\n" + folder, password)) {
            CoUninitialize();
            return 0;
        }

        std::wstring id;
        if (locker_lock_folder(folder, password, id)) {
            dialog_message(NULL, L"Zamok", L"Folder locked successfully.", false);
        } else {
            dialog_message(NULL, L"Zamok", L"Failed to lock folder.", true);
        }
    }
    else if (cmd == L"--unlock") {
        if (argc < 3) {
            std::wcout << L"Usage: Zamok.exe --unlock <folder>" << std::endl;
            CoUninitialize();
            return 1;
        }
        ensure_directories();
        std::wstring folder = argv[2];

        std::wstring password;
        if (!dialog_password(NULL, L"Zamok - Unlock Folder", L"Enter password to unlock:\n" + folder, password)) {
            CoUninitialize();
            return 0;
        }

        if (locker_unlock_folder(folder, password)) {
            dialog_message(NULL, L"Zamok", L"Folder unlocked successfully.", false);
        } else {
            dialog_message(NULL, L"Zamok", L"Wrong password or failed to unlock.", true);
        }
    }
    else if (cmd == L"--manager") {
        ensure_directories();
        auto items = locker_list_locked();
        if (items.empty()) {
            dialog_message(NULL, L"Zamok - Manager", L"No locked folders.", false);
        } else {
            std::wstring msg = L"Locked folders:\n\n";
            for (auto& f : items) {
                msg += f.original_path + L"\n";
            }
            dialog_message(NULL, L"Zamok - Manager", msg, false);
        }
    }
    else {
        print_usage();
    }

    CoUninitialize();
    return 0;
}
