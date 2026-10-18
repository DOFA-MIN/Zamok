#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Service identity
#define ZAMOK_SERVICE_NAME        L"ZamokFolderLocker"
#define ZAMOK_SERVICE_DISPLAY     L"Zamok Folder Locker"
#define ZAMOK_SERVICE_DESC        L"Zamok Folder Locker — защита папок"
#define ZAMOK_VAULT_SUBDIR        L"Zamok\\Vault"
#define ZAMOK_DB_SUBDIR           L"Zamok\\data.db"
#define ZAMOK_CONTEXT_MENU_ROOT   L"Directory\\shell"

// Security
#define ZAMOK_PBKDF2_ITERATIONS   100000
#define ZAMOK_SALT_BYTES          16
#define ZAMOK_HASH_BYTES          32

// ------------------------------------------------------------------
// Inline helpers — defined in header to avoid linker errors
// ------------------------------------------------------------------

inline std::wstring zamok_get_localappdata() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return std::wstring(buf);
    // Fallback: SHGetFolderPath
    wchar_t path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
        return std::wstring(path);
    return L"C:\\Temp";
}

inline std::wstring get_vault_dir() {
    return zamok_get_localappdata() + L"\\" + ZAMOK_VAULT_SUBDIR;
}

inline std::wstring get_db_path() {
    return zamok_get_localappdata() + L"\\" + ZAMOK_DB_SUBDIR;
}

inline std::wstring get_app_dir() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos) p = p.substr(0, pos);
    return p;
}
