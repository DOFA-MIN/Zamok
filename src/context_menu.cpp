#include <windows.h>
#include <shlobj.h>
#include <string>
#include "config.h"

static std::wstring get_exe_path() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path);
}

static bool reg_set_string(HKEY root, const wchar_t* subkey, const wchar_t* valueName, const std::wstring& data) {
    HKEY key;
    LONG res = RegCreateKeyExW(root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (res != ERROR_SUCCESS) return false;
    RegSetValueExW(key, valueName, 0, REG_SZ, (const BYTE*)data.c_str(), (DWORD)((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return true;
}

bool context_menu_install() {
    std::wstring exe = get_exe_path();
    // Commands — quoted exe + args. %1 is replaced by Explorer with folder path.
    std::wstring command_lock    = L"\"" + exe + L"\" \"--lock\" \"%1\"";
    std::wstring command_unlock  = L"\"" + exe + L"\" \"--unlock\" \"%1\"";
    std::wstring command_manager = L"\"" + exe + L"\" \"--manager\"";

    // Icons: use shell32.dll built-in icons
    // Lock = padlock-like, Unlock = key, Manager = list

    // --- Zamok - Запереть ---
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock", NULL, L"Zamok — Запереть");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock", L"Icon", L"shell32.dll,3");
    // Position at top
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock", L"Position", L"Top");
    RegCreateKeyExW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, NULL, NULL);
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock\\command", NULL, command_lock);

    // --- Zamok - Отпереть ---
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock", NULL, L"Zamok — Отпереть");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock", L"Icon", L"shell32.dll,4");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock", L"Position", L"Top");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock\\command", NULL, command_unlock);

    // --- Zamok - Список ---
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager", NULL, L"Zamok — Список");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager", L"Icon", L"shell32.dll,43");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager\\command", NULL, command_manager);

    // Also add for background (when right-clicking inside folder, not on folder)
    // This allows Zamok — Список from any folder background
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\ZamokManager", NULL, L"Zamok — Список");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\ZamokManager", L"Icon", L"shell32.dll,43");
    reg_set_string(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\ZamokManager\\command", NULL, command_manager);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}

bool context_menu_remove() {
    // Directory\shell
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokLock");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokUnlock");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\shell\\ZamokManager");

    // Background
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\ZamokManager\\command");
    RegDeleteKeyW(HKEY_CLASSES_ROOT, L"Directory\\Background\\shell\\ZamokManager");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return true;
}
