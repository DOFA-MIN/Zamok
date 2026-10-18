#include "locker.h"
#include "config.h"

#include <shlwapi.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>
#include <direct.h>
#include <sys/stat.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")

static const wchar_t* DB_FORMAT_VERSION = L"1";

bool locker_init_db() {
    std::wstring vault = get_vault_dir();
    CreateDirectoryW(vault.c_str(), NULL);
    DWORD attr = GetFileAttributesW(vault.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        SetFileAttributesW(vault.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);

    std::wstring db = get_db_path();
    std::wifstream fin(db);
    if (!fin.good()) {
        std::wofstream fout(db);
        fout << DB_FORMAT_VERSION << std::endl;
        fout.close();
    }
    return true;
}

std::wstring locker_hash_password(const std::wstring& password) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD cbHash = 0, cbData = 0;
    std::wstring result;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status != 0) return result;

    BYTE salt[16];
    status = BCryptGenRandom(NULL, salt, 16, 0);
    if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return result; }

    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, salt, 16, 0);
    if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return result; }

    int pwlen = (int)password.size();
    std::vector<BYTE> pwBytes(pwlen * 2);
    for (int i = 0; i < pwlen; i++) {
        pwBytes[i*2] = (BYTE)(password[i] & 0xFF);
        pwBytes[i*2+1] = (BYTE)((password[i] >> 8) & 0xFF);
    }

    status = BCryptHashData(hHash, pwBytes.data(), (ULONG)pwBytes.size(), 0);
    if (status != 0) { BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0); return result; }

    BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    std::vector<BYTE> hash(cbHash);
    BCryptFinishHash(hHash, hash.data(), cbHash, 0);

    wchar_t hex[128];
    for (DWORD i = 0; i < cbHash; i++)
        swprintf_s(hex + i*2, 128 - i*2, L"%02X", hash[i]);

    // salt(hex):hash(hex)
    wchar_t saltHex[64];
    for (int i = 0; i < 16; i++)
        swprintf_s(saltHex + i*2, 64 - i*2, L"%02X", salt[i]);

    result = saltHex;
    result += L":";
    result += hex;

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

bool locker_verify_password(const std::wstring& password, const std::wstring& stored_hash) {
    size_t colon = stored_hash.find(L":");
    if (colon == std::wstring::npos) return false;

    std::wstring salt_hex = stored_hash.substr(0, colon);
    std::wstring hash_hex = stored_hash.substr(colon + 1);

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD cbData = 0;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status != 0) return false;

    BYTE salt[16];
    for (int i = 0; i < 16; i++) {
        wchar_t byteStr[3] = { salt_hex[i*2], salt_hex[i*2+1], 0 };
        salt[i] = (BYTE)wcstoul(byteStr, NULL, 16);
    }

    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, salt, 16, 0);
    if (status != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    int pwlen = (int)password.size();
    std::vector<BYTE> pwBytes(pwlen * 2);
    for (int i = 0; i < pwlen; i++) {
        pwBytes[i*2] = (BYTE)(password[i] & 0xFF);
        pwBytes[i*2+1] = (BYTE)((password[i] >> 8) & 0xFF);
    }

    status = BCryptHashData(hHash, pwBytes.data(), (ULONG)pwBytes.size(), 0);
    if (status != 0) { BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0); return false; }

    DWORD hashLen = (DWORD)(hash_hex.size() / 2);
    std::vector<BYTE> expected(hashLen);
    for (DWORD i = 0; i < hashLen; i++) {
        wchar_t byteStr[3] = { hash_hex[i*2], hash_hex[i*2+1], 0 };
        expected[i] = (BYTE)wcstoul(byteStr, NULL, 16);
    }

    std::vector<BYTE> actual(hashLen);
    DWORD cbActual = hashLen;
    BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PBYTE)&cbActual, sizeof(DWORD), &cbData, 0);
    BCryptFinishHash(hHash, actual.data(), hashLen, 0);

    bool ok = (actual.size() == expected.size());
    if (ok) {
        for (size_t i = 0; i < actual.size(); i++) {
            if (actual[i] != expected[i]) { ok = false; break; }
        }
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

std::vector<LockedFolder> locker_list_locked() {
    std::vector<LockedFolder> result;
    std::wstring db = get_db_path();
    std::wifstream fin(db);
    if (!fin.good()) return result;

    std::wstring line;
    bool first = true;
    while (std::getline(fin, line)) {
        if (first) { first = false; continue; }
        if (line.empty()) continue;

        LockedFolder f;
        size_t p1 = line.find(L'|');
        size_t p2 = line.find(L'|', p1 + 1);
        size_t p3 = line.find(L'|', p2 + 1);
        size_t p4 = line.find(L'|', p3 + 1);
        if (p1 == std::wstring::npos || p2 == std::wstring::npos || p3 == std::wstring::npos || p4 == std::wstring::npos) continue;

        f.id = line.substr(0, p1);
        f.original_path = line.substr(p1 + 1, p2 - p1 - 1);
        f.vault_path = line.substr(p2 + 1, p3 - p2 - 1);
        f.password_hash = line.substr(p3 + 1, p4 - p3 - 1);
        f.created_at = line.substr(p4 + 1);
        result.push_back(f);
    }
    return result;
}

bool locker_is_locked(const std::wstring& folder_path) {
    auto list = locker_list_locked();
    std::wstring norm = folder_path;
    // normalize trailing slash
    if (!norm.empty() && norm.back() == L'\\') norm.pop_back();
    for (auto& f : list) {
        std::wstring op = f.original_path;
        if (!op.empty() && op.back() == L'\\') op.pop_back();
        if (op == norm) return true;
    }
    return false;
}

bool locker_lock_folder(const std::wstring& folder_path, const std::wstring& password, std::wstring& out_id) {
    if (locker_is_locked(folder_path)) return false;

    // generate id
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t guidStr[64];
    StringFromGUID2(guid, guidStr, 64);
    out_id = guidStr;

    std::wstring vault = get_vault_dir();
    std::wstring vault_path = vault + L"\\" + out_id;

    if (!MoveFileExW(folder_path.c_str(), vault_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        return false;

    SetFileAttributesW(vault_path.c_str(), FILE_ATTRIBUTE_HIDDEN);

    std::wstring hash = locker_hash_password(password);

    SYSTEMTIME st;
    GetSystemTime(&st);
    wchar_t timeStr[64];
    swprintf_s(timeStr, 64, L"%04u-%02u-%02uT%02u:%02u:%02uZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::wstring db = get_db_path();
    std::wofstream fout(db, std::ios::app);
    fout << out_id << L"|" << folder_path << L"|" << vault_path << L"|" << hash << L"|" << timeStr << std::endl;
    fout.close();

    return true;
}

bool locker_unlock_folder(const std::wstring& id_or_path, const std::wstring& password) {
    auto list = locker_list_locked();
    LockedFolder* found = NULL;
    for (auto& f : list) {
        if (f.id == id_or_path || f.original_path == id_or_path) {
            found = &f;
            break;
        }
    }
    if (!found) return false;

    if (!locker_verify_password(password, found->password_hash)) return false;

    if (GetFileAttributesW(found->vault_path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    if (!MoveFileExW(found->vault_path.c_str(), found->original_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        return false;

    // rewrite db without this entry
    std::wstring db = get_db_path();
    std::wofstream fout(db);
    fout << DB_FORMAT_VERSION << std::endl;
    for (auto& f : list) {
        if (f.id == found->id) continue;
        fout << f.id << L"|" << f.original_path << L"|" << f.vault_path << L"|" << f.password_hash << L"|" << f.created_at << std::endl;
    }
    fout.close();

    return true;
}
