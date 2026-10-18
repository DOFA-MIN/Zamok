#include <windows.h>
#include <shlobj.h>
#include <string>
#include <iostream>
#include "config.h"

static std::wstring get_exe_path() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
}

bool context_menu_install() {
    std::wstring exe = get_exe_path();
    // Escape backslashes for registry
    std::wstring command_lock = L"\"" + exe + L"\" \"--lock\" \"%1\"";
    std::wstring command_unlock = L"\"" + exe + L"\" \"--unlock\" \"%1\"";
    std::wstring command_manager = L"\"" + exe + L"\" \"--manager\"";

    HKEY key;
    LONG res;

    // Zamok Lock
    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)L"&Zamok \u0417\u0430\u043c\u043e\u043a (\u0417\u0430\u043c\u043a\u0430\u0442\u044c)", (wcslen(L"&Zamok \u0417\u0430\u043c\u043e\u043a (\u0417\u0430\u043c\u043a\u0430\u0442\u044c)") + 1) * sizeof(wchar_t));
        RegSetValueExW(key, L"Icon", 0, REG_SZ, (const BYTE*)L"shell32.dll,3", (wcslen(L"shell32.dll,3") + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)command_lock.c_str(), (command_lock.size() + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    // Zamok Unlock
    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)L"&Zamok \u0420\u0430\u0437\u0430\u043c\u043a\u0430\u0442\u044c", (wcslen(L"&Zamok \u0420\u0430\u0437\u0430\u043c\u043a\u0430\u0442\u044c") + 1) * sizeof(wchar_t));
        RegSetValueExW(key, L"Icon", 0, REG_SZ, (const BYTE*)L"shell32.dll,4", (wcslen(L"shell32.dll,4") + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)command_unlock.c_str(), (command_unlock.size() + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    // Zamok Manager
    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)L"&Zamok \u0421\u043f\u0438\u0441\u043e\u043a", (wcslen(L"&Zamok \u0421\u043f\u0438\u0441\u043e\u043a") + 1) * sizeof(wchar_t));
        RegSetValueExW(key, L"Icon", 0, REG_SZ, (const BYTE*)L"shell32.dll,4", (wcslen(L"shell32.dll,4") + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    res = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS) {
        RegSetValueExW(key, L"", 0, REG_SZ, (const BYTE*)command_manager.c_str(), (command_manager.size() + 1) * sizeof(wchar_t));
        RegCloseKey(key);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

bool context_menu_remove() {
    // Remove ZamokLock
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock");

    // Remove ZamokUnlock
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock");

    // Remove ZamokManager
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}
