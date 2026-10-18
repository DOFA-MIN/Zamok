#pragma once

#include <windows.h>
#include <string>
#include <vector>

#define ZAMOK_SERVICE_NAME        L"ZamokFolderLocker"
#define ZAMOK_SERVICE_DISPLAY     L"Zamok Folder Locker"
#define ZAMOK_SERVICE_DESC        L"Zamok Folder Locker service"
#define ZAMOK_API_PORT            5000
#define ZAMOK_VAULT_SUBDIR        L"Zamok\\Vault"
#define ZAMOK_DB_SUBDIR           L"Zamok\\data.db"
#define ZAMOK_CONTEXT_MENU_ROOT   L"Directory\\shell"

inline std::wstring get_app_dir();
inline std::wstring get_vault_dir();
inline std::wstring get_db_path();
