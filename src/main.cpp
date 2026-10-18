#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <comdef.h>
#include <shellapi.h>

#include "config.h"
#include "locker.h"
#include "dialogs.h"
#include "service.h"

#pragma comment(lib, "shell32.lib")

extern bool context_menu_install();
extern bool context_menu_remove();

static void print_usage() {
    std::wcout << L"Zamok Folder Locker — защита папок паролем" << std::endl;
    std::wcout << L"Версия 2.0 (без HTTP, только служба + ядро)" << std::endl;
    std::wcout << L"" << std::endl;
    std::wcout << L"Использование:" << std::endl;
    std::wcout << L"  Zamok.exe --install-service     Установить службу Windows" << std::endl;
    std::wcout << L"  Zamok.exe --uninstall-service   Удалить службу" << std::endl;
    std::wcout << L"  Zamok.exe --start-service       Запустить службу" << std::endl;
    std::wcout << L"  Zamok.exe --run-service         Запуск службой (вызывает SCM)" << std::endl;
    std::wcout << L"  Zamok.exe --install-menu        Установить пункты контекстного меню" << std::endl;
    std::wcout << L"  Zamok.exe --uninstall-menu      Удалить пункты контекстного меню" << std::endl;
    std::wcout << L"  Zamok.exe --lock <папка>        Заблокировать папку (с диалогом пароля)" << std::endl;
    std::wcout << L"  Zamok.exe --unlock <папка|id>   Разблокировать папку" << std::endl;
    std::wcout << L"  Zamok.exe --manager             Показать менеджер заблокированных папок" << std::endl;
    std::wcout << L"  Zamok.exe --status              Показать статус хранилища" << std::endl;
    std::wcout << L"  Zamok.exe --help                Показать эту справку" << std::endl;
}

static bool ensure_directories() {
    return locker_init_db();
}

static void show_status() {
    locker_init_db();
    auto items = locker_list_locked();
    std::wcout << L"Zamok Status:" << std::endl;
    std::wcout << L"  Vault: " << get_vault_dir() << std::endl;
    std::wcout << L"  DB: "    << get_db_path()   << std::endl;
    std::wcout << L"  Service: " << (service_is_installed() ? L"установлена" : L"не установлена") << std::endl;
    std::wcout << L"  Заблокировано папок: " << items.size() << std::endl;
    for (auto& f : items) {
        DWORD attr = GetFileAttributesW(f.vault_path.c_str());
        bool exists = (attr != INVALID_FILE_ATTRIBUTES);
        std::wcout << L"    • " << f.original_path
                   << (exists ? L"" : L" [MISSING В ХРАНИЛИЩЕ]")
                   << L"  id=" << f.id << std::endl;
    }
}

static void show_manager_dialog() {
    ensure_directories();
    auto items = locker_list_locked();
    if (items.empty()) {
        dialog_message(NULL, L"Zamok — Менеджер", L"Нет заблокированных папок.", false);
        return;
    }

    // Build detailed message with vault health
    std::wstring msg = L"Заблокированные папки (" + std::to_wstring(items.size()) + L"):\n";
    msg += L"──────────────────────────────────\n";
    for (size_t i = 0; i < items.size(); ++i) {
        auto& f = items[i];
        DWORD attr = GetFileAttributesW(f.vault_path.c_str());
        bool ok = (attr != INVALID_FILE_ATTRIBUTES);
        msg += std::to_wstring(i+1) + L". " + f.original_path + L"\n";
        msg += L"   ID: " + f.id + L"\n";
        msg += L"   " + f.created_at + (ok ? L" ✓" : L" ⚠ нет в хранилище") + L"\n\n";
        // Limit message length to avoid MessageBox overflow
        if (msg.size() > 4000) {
            msg += L"... и еще " + std::to_wstring(items.size() - i - 1) + L" ...";
            break;
        }
    }
    msg += L"\nДля разблокировки: правый клик → Zamok → Разблокировать\nили Zamok.exe --unlock \"<путь>\"";
    dialog_message(NULL, L"Zamok — Менеджер", msg, false);
}

int wmain(int argc, wchar_t** argv) {
    // Enable DPI awareness for dialogs
    SetProcessDPIAware();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (argc < 2) {
        print_usage();
        CoUninitialize();
        return 0;
    }

    std::wstring cmd = argv[1];
    // Normalize --help variants
    if (cmd == L"--help" || cmd == L"-h" || cmd == L"/?" || cmd == L"/help") {
        print_usage();
        CoUninitialize();
        return 0;
    }

    if (cmd == L"--install-service") {
        if (!IsUserAnAdmin()) {
            std::wcout << L"Ошибка: запустите от имени Администратора." << std::endl;
            CoUninitialize();
            return 1;
        }
        if (service_install()) std::wcout << L"✓ Служба установлена." << std::endl;
        else std::wcout << L"✗ Не удалось установить службу. Код: " << GetLastError() << std::endl;
    }
    else if (cmd == L"--uninstall-service") {
        if (!IsUserAnAdmin()) {
            std::wcout << L"Ошибка: запустите от имени Администратора." << std::endl;
            CoUninitialize();
            return 1;
        }
        if (service_remove()) std::wcout << L"✓ Служба удалена." << std::endl;
        else std::wcout << L"✗ Не удалось удалить службу. Код: " << GetLastError() << std::endl;
    }
    else if (cmd == L"--start-service") {
        SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if (!scm) {
            std::wcout << L"Не удалось открыть SCM: " << GetLastError() << std::endl;
        } else {
            SC_HANDLE svc = OpenServiceW(scm, ZAMOK_SERVICE_NAME, SERVICE_START);
            if (!svc) {
                std::wcout << L"Служба не найдена. Установите: --install-service" << std::endl;
            } else {
                if (StartServiceW(svc, 0, NULL)) std::wcout << L"✓ Служба запущена." << std::endl;
                else {
                    DWORD err = GetLastError();
                    if (err == ERROR_SERVICE_ALREADY_RUNNING) std::wcout << L"Служба уже запущена." << std::endl;
                    else std::wcout << L"Не удалось запустить: " << err << std::endl;
                }
                CloseServiceHandle(svc);
            }
            CloseServiceHandle(scm);
        }
    }
    else if (cmd == L"--run-service") {
        service_run(argc, argv);
    }
    else if (cmd == L"--install-menu") {
        if (!IsUserAnAdmin()) {
            std::wcout << L"Ошибка: запустите от имени Администратора." << std::endl;
            CoUninitialize();
            return 1;
        }
        if (context_menu_install()) std::wcout << L"✓ Контекстное меню установлено." << std::endl;
        else std::wcout << L"✗ Ошибка установки меню." << std::endl;
    }
    else if (cmd == L"--uninstall-menu") {
        if (!IsUserAnAdmin()) {
            std::wcout << L"Предупреждение: для удаления меню нужны права администратора." << std::endl;
        }
        context_menu_remove();
        std::wcout << L"✓ Контекстное меню удалено." << std::endl;
    }
    else if (cmd == L"--lock") {
        if (argc < 3) {
            std::wcout << L"Использование: Zamok.exe --lock \"<путь к папке>\"" << std::endl;
            CoUninitialize();
            return 1;
        }
        ensure_directories();
        std::wstring folder = argv[2];
        // Support drag-and-drop with quotes already stripped by shell

        if (!locker_is_safe_path(folder)) {
            dialog_message(NULL, L"Zamok", L"Недопустимый путь. Нельзя блокировать системные папки или само хранилище.", true);
            CoUninitialize();
            return 1;
        }
        if (locker_is_locked(folder)) {
            dialog_message(NULL, L"Zamok", L"Эта папка уже заблокирована.", false);
            CoUninitialize();
            return 0;
        }

        std::wstring password;
        if (!dialog_password(NULL, L"Zamok — Блокировка", L"Введите пароль для блокировки:\n" + locker_normalize_path(folder), password)) {
            CoUninitialize();
            return 0; // cancelled
        }

        std::wstring id;
        if (locker_lock_folder(folder, password, id)) {
            // Clear password from memory
            SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
            dialog_message(NULL, L"Zamok", L"✓ Папка успешно заблокирована.\n\nОна перемещена в защищенное хранилище.\nДля разблокировки используйте контекстное меню или --unlock.", false);
        } else {
            SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
            dialog_message(NULL, L"Zamok", L"✗ Не удалось заблокировать папку.\n\nПроверьте что папка существует и не является системной.", true);
            CoUninitialize();
            return 1;
        }
    }
    else if (cmd == L"--unlock") {
        if (argc < 3) {
            std::wcout << L"Использование: Zamok.exe --unlock \"<путь или ID>\"" << std::endl;
            CoUninitialize();
            return 1;
        }
        ensure_directories();
        std::wstring target = argv[2];

        // Check if target is known locked
        std::wstring norm = locker_normalize_path(target);
        bool known = locker_is_locked(target);
        // Also allow unlocking by ID — locker_unlock_folder handles both
        // If not known and not an ID format, warn
        if (!known) {
            // Check if it's an ID ( GUID )
            auto items = locker_list_locked();
            bool isId = false;
            for (auto& it : items) if (it.id == target) { isId = true; break; }
            if (!isId) {
                // Still allow attempt — locker_unlock_folder will fail gracefully
            }
        }

        std::wstring password;
        if (!dialog_password(NULL, L"Zamok — Разблокировка", L"Введите пароль для разблокировки:\n" + target, password)) {
            CoUninitialize();
            return 0;
        }

        if (locker_unlock_folder(target, password)) {
            SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
            dialog_message(NULL, L"Zamok", L"✓ Папка успешно разблокирована.", false);
        } else {
            SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
            dialog_message(NULL, L"Zamok", L"✗ Неверный пароль или папка не найдена.\n\nПроверьте путь/ID и пароль.", true);
            CoUninitialize();
            return 1;
        }
    }
    else if (cmd == L"--manager") {
        show_manager_dialog();
    }
    else if (cmd == L"--status") {
        show_status();
    }
    else {
        // Unknown — maybe user dragged folder onto exe without flag
        // Treat first arg as folder path to lock (Explorer fallback)
        std::wstring maybePath = cmd;
        DWORD attrs = GetFileAttributesW(maybePath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // It's a folder — implicit lock
            ensure_directories();
            if (locker_is_locked(maybePath)) {
                dialog_message(NULL, L"Zamok", L"Эта папка уже заблокирована.", false);
            } else {
                std::wstring password;
                if (dialog_password(NULL, L"Zamok — Блокировка", L"Введите пароль для блокировки:\n" + locker_normalize_path(maybePath), password)) {
                    std::wstring id;
                    if (locker_lock_folder(maybePath, password, id)) {
                        SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
                        dialog_message(NULL, L"Zamok", L"✓ Папка заблокирована.", false);
                    } else {
                        SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));
                        dialog_message(NULL, L"Zamok", L"✗ Ошибка блокировки.", true);
                    }
                }
            }
        } else {
            std::wcout << L"Неизвестная команда: " << cmd << std::endl;
            print_usage();
            CoUninitialize();
            return 1;
        }
    }

    CoUninitialize();
    return 0;
}
