#include "config.h"

inline std::wstring get_app_dir() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p = path;
    size_t pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos) p = p.substr(0, pos);
    return p;
}

inline std::wstring get_vault_dir() {
    wchar_t localAppData[MAX_PATH] = {0};
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    return std::wstring(localAppData) + L"\\" + ZAMOK_VAULT_SUBDIR;
}

inline std::wstring get_db_path() {
    wchar_t localAppData[MAX_PATH] = {0};
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    return std::wstring(localAppData) + L"\\" + ZAMOK_DB_SUBDIR;
}
