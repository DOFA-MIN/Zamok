#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct LockedFolder {
    std::wstring id;
    std::wstring original_path;
    std::wstring vault_path;
    std::wstring password_hash;
    std::wstring created_at;
};

bool locker_init_db();
std::vector<LockedFolder> locker_list_locked();
bool locker_lock_folder(const std::wstring& folder_path, const std::wstring& password, std::wstring& out_id);
bool locker_unlock_folder(const std::wstring& id_or_path, const std::wstring& password);
bool locker_is_locked(const std::wstring& folder_path);
std::wstring locker_hash_password(const std::wstring& password);
bool locker_verify_password(const std::wstring& password, const std::wstring& stored_hash);
