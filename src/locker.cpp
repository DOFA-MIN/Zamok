#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <objbase.h>

#include "locker.h"
#include "config.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <codecvt>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ole32.lib")

static const wchar_t* DB_VERSION = L"1";

// ------------------------------------------------------------------
// UTF-8 helpers (for PBKDF2 compatibility with Python)
// ------------------------------------------------------------------
static std::string wstring_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
    std::string out(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &out[0], sz, NULL, NULL);
    return out;
}

static std::wstring utf8_to_wstring(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring out(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], sz);
    return out;
}

// ------------------------------------------------------------------
// Hex helpers
// ------------------------------------------------------------------
static std::wstring bytes_to_hex_lower(const BYTE* data, size_t len) {
    std::wstring out;
    out.reserve(len * 2);
    const wchar_t* hex = L"0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hex[(data[i] >> 4) & 0xF]);
        out.push_back(hex[data[i] & 0xF]);
    }
    return out;
}

static std::vector<BYTE> hex_to_bytes(const std::wstring& hex) {
    std::vector<BYTE> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        wchar_t b[3] = { hex[i], hex[i+1], 0 };
        out.push_back((BYTE)wcstoul(b, NULL, 16));
    }
    return out;
}

// ------------------------------------------------------------------
// DB escaping: '|' and '\' and '\n' are escaped
// ------------------------------------------------------------------
static std::wstring db_escape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L'|') out += L"\\p";
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') out += L"\\r";
        else out += c;
    }
    return out;
}

static std::wstring db_unescape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t nxt = s[i+1];
            if (nxt == L'\\') { out += L'\\'; i++; }
            else if (nxt == L'p') { out += L'|'; i++; }
            else if (nxt == L'n') { out += L'\n'; i++; }
            else if (nxt == L'r') { out += L'\r'; i++; }
            else { out += s[i]; }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Split by unescaped '|'
// We need to track escape state
static std::vector<std::wstring> db_split_escaped(const std::wstring& line) {
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t c = line[i];
        if (c == L'\\' && i + 1 < line.size()) {
            cur += c;
            cur += line[i+1];
            i++;
        } else if (c == L'|') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    return parts;
}

// ------------------------------------------------------------------
// Path helpers
// ------------------------------------------------------------------
std::wstring locker_normalize_path(const std::wstring& path) {
    if (path.empty()) return L"";
    wchar_t buf[MAX_PATH] = {0};
    // GetFullPathNameW + normalize
    DWORD len = GetFullPathNameW(path.c_str(), MAX_PATH, buf, NULL);
    if (len == 0 || len >= MAX_PATH) {
        // Fallback: trim trailing slash
        std::wstring p = path;
        while (p.size() > 1 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
        return p;
    }
    std::wstring p(buf);
    // Remove trailing slash (except root like C:\)
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}

bool locker_is_safe_path(const std::wstring& path) {
    if (path.empty()) return false;
    if (path.size() >= MAX_PATH) return false;

    std::wstring norm = locker_normalize_path(path);
    std::wstring lower = norm;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // Must be absolute (C:\... or \\...)
    if (lower.size() < 3 || lower[1] != L':') {
        if (lower.rfind(L"\\\\", 0) != 0) return false;
    }

    // Forbid vault itself and DB location
    std::wstring vault = get_vault_dir();
    std::wstring vaultNorm = locker_normalize_path(vault);
    std::wstring vaultLower = vaultNorm;
    std::transform(vaultLower.begin(), vaultLower.end(), vaultLower.begin(), ::towlower);

    std::wstring dbPath = get_db_path();
    std::wstring dbNorm = locker_normalize_path(dbPath);
    // Actually db is file, get its directory
    size_t pos = dbNorm.find_last_of(L"\\/");
    std::wstring dbDir = (pos != std::wstring::npos) ? dbNorm.substr(0, pos) : dbNorm;
    std::wstring dbDirLower = dbDir;
    std::transform(dbDirLower.begin(), dbDirLower.end(), dbDirLower.begin(), ::towlower);

    // If path is vault or inside vault
    if (lower == vaultLower) return false;
    if (lower.rfind(vaultLower + L"\\", 0) == 0) return false;
    // Same for Zamok base dir (contains DB)
    if (lower == dbDirLower) return false;

    // Forbid Windows system dirs
    const wchar_t* forbidden[] = {
        L"c:\\windows",
        L"c:\\windows\\system32",
        L"c:\\program files",
        L"c:\\program files (x86)",
        L"c:\\programdata",
    };
    for (auto f : forbidden) {
        if (lower == f) return false;
        // Allow subfolders? For safety, block Windows root but allow subfolders of Program Files?
        // We block exact match only for Program Files, but block prefix for Windows
        if (std::wstring(f) == L"c:\\windows" && lower.rfind(f, 0) == 0) return false;
    }

    return true;
}

// ------------------------------------------------------------------
// DB init
// ------------------------------------------------------------------
bool locker_init_db() {
    std::wstring vault = get_vault_dir();
    // Create vault directory (and parent Zamok/)
    // Use SHCreateDirectoryEx for recursive creation
    size_t pos = vault.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring parent = vault.substr(0, pos);
        CreateDirectoryW(parent.c_str(), NULL);
    }
    CreateDirectoryW(vault.c_str(), NULL);
    DWORD attr = GetFileAttributesW(vault.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(vault.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
    }

    std::wstring db = get_db_path();
    // Ensure parent exists
    size_t p2 = db.find_last_of(L"\\/");
    if (p2 != std::wstring::npos) {
        std::wstring parent2 = db.substr(0, p2);
        CreateDirectoryW(parent2.c_str(), NULL);
    }

    // Create DB file with version if not exists
    DWORD dbAttr = GetFileAttributesW(db.c_str());
    if (dbAttr == INVALID_FILE_ATTRIBUTES) {
        // Use UTF-8 BOM? No, use plain UTF-8 via narrow
        std::string dbUtf8 = wstring_to_utf8(db);
        // Write version using narrow fstream with UTF-8 conversion
        // But we need wide path: use _wfopen
        FILE* f = _wfopen(db.c_str(), L"w, ccs=UTF-8");
        if (f) {
            fwprintf(f, L"%ls\n", DB_VERSION);
            fclose(f);
        }
    }
    return true;
}

// ------------------------------------------------------------------
// Password hashing — PBKDF2-HMAC-SHA256 via BCryptDeriveKeyPBKDF2
// Compatible with Python's hashlib.pbkdf2_hmac
// ------------------------------------------------------------------
std::wstring locker_hash_password(const std::wstring& password) {
    if (password.empty()) return L"";

    std::string pwUtf8 = wstring_to_utf8(password);

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    // Try without HMAC flag if failed (PBKDF2 needs HMAC)
    if (status != 0) {
        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
        if (status != 0) return L"";
    }

    BYTE salt[ZAMOK_SALT_BYTES];
    status = BCryptGenRandom(NULL, salt, ZAMOK_SALT_BYTES, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    BYTE dk[ZAMOK_HASH_BYTES] = {0};
    status = BCryptDeriveKeyPBKDF2(hAlg,
        (PUCHAR)pwUtf8.data(), (ULONG)pwUtf8.size(),
        salt, ZAMOK_SALT_BYTES,
        ZAMOK_PBKDF2_ITERATIONS,
        dk, ZAMOK_HASH_BYTES, 0);

    // Clear password copy
    SecureZeroMemory(pwUtf8.data(), pwUtf8.size());

    if (status != 0) {
        // Fallback: manual single SHA256 (should not happen on Win10+)
        BCryptCloseAlgorithmProvider(hAlg, 0);
        SecureZeroMemory(salt, sizeof(salt));
        SecureZeroMemory(dk, sizeof(dk));
        return L"";
    }

    std::wstring saltHex = bytes_to_hex_lower(salt, ZAMOK_SALT_BYTES);
    std::wstring dkHex   = bytes_to_hex_lower(dk, ZAMOK_HASH_BYTES);

    SecureZeroMemory(salt, sizeof(salt));
    SecureZeroMemory(dk, sizeof(dk));
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return saltHex + L":" + dkHex;
}

bool locker_verify_password(const std::wstring& password, const std::wstring& stored_hash) {
    if (password.empty() || stored_hash.empty()) return false;

    size_t colon = stored_hash.find(L':');
    if (colon == std::wstring::npos) return false;

    std::wstring saltHex = stored_hash.substr(0, colon);
    std::wstring dkHex   = stored_hash.substr(colon + 1);

    if (saltHex.size() != ZAMOK_SALT_BYTES * 2) return false;
    if (dkHex.size()   != ZAMOK_HASH_BYTES * 2) return false;

    std::vector<BYTE> salt = hex_to_bytes(saltHex);
    std::vector<BYTE> expected = hex_to_bytes(dkHex);
    if (salt.size() != ZAMOK_SALT_BYTES || expected.size() != ZAMOK_HASH_BYTES) return false;

    std::string pwUtf8 = wstring_to_utf8(password);

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status != 0) {
        status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
        if (status != 0) {
            SecureZeroMemory(pwUtf8.data(), pwUtf8.size());
            return false;
        }
    }

    BYTE dk[ZAMOK_HASH_BYTES] = {0};
    status = BCryptDeriveKeyPBKDF2(hAlg,
        (PUCHAR)pwUtf8.data(), (ULONG)pwUtf8.size(),
        salt.data(), (ULONG)salt.size(),
        ZAMOK_PBKDF2_ITERATIONS,
        dk, ZAMOK_HASH_BYTES, 0);

    SecureZeroMemory(pwUtf8.data(), pwUtf8.size());
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) {
        SecureZeroMemory(dk, sizeof(dk));
        return false;
    }

    // Constant-time compare
    bool ok = true;
    if (expected.size() != ZAMOK_HASH_BYTES) ok = false;
    else {
        BYTE diff = 0;
        for (size_t i = 0; i < ZAMOK_HASH_BYTES; ++i) diff |= dk[i] ^ expected[i];
        ok = (diff == 0);
    }

    SecureZeroMemory(dk, sizeof(dk));
    // Clear expected vector securely
    if (!expected.empty()) SecureZeroMemory(expected.data(), expected.size());

    return ok;
}

// ------------------------------------------------------------------
// List locked folders
// ------------------------------------------------------------------
std::vector<LockedFolder> locker_list_locked() {
    std::vector<LockedFolder> result;
    std::wstring db = get_db_path();

    FILE* f = _wfopen(db.c_str(), L"r, ccs=UTF-8");
    if (!f) return result;

    wchar_t lineBuf[4096];
    bool first = true;
    // Skip BOM if present
    while (fgetws(lineBuf, 4096, f)) {
        std::wstring line(lineBuf);
        // Trim \r\n
        while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r')) line.pop_back();
        if (first) { first = false; continue; } // version line
        if (line.empty()) continue;

        auto parts = db_split_escaped(line);
        if (parts.size() != 5) continue;

        LockedFolder lf;
        lf.id            = db_unescape(parts[0]);
        lf.original_path = db_unescape(parts[1]);
        lf.vault_path    = db_unescape(parts[2]);
        lf.password_hash = db_unescape(parts[3]);
        lf.created_at    = db_unescape(parts[4]);
        result.push_back(lf);
    }
    fclose(f);
    return result;
}

bool locker_is_locked(const std::wstring& folder_path) {
    if (folder_path.empty()) return false;
    std::wstring norm = locker_normalize_path(folder_path);
    auto list = locker_list_locked();
    for (auto& f : list) {
        std::wstring op = locker_normalize_path(f.original_path);
        // Case-insensitive compare on Windows
        std::wstring a = norm, b = op;
        std::transform(a.begin(), a.end(), a.begin(), ::towlower);
        std::transform(b.begin(), b.end(), b.begin(), ::towlower);
        if (a == b) return true;
    }
    return false;
}

// ------------------------------------------------------------------
// Lock folder
// ------------------------------------------------------------------
bool locker_lock_folder(const std::wstring& folder_path, const std::wstring& password, std::wstring& out_id) {
    out_id.clear();

    if (folder_path.empty() || password.empty()) return false;
    if (password.size() > 256) return false; // sanity
    if (!locker_is_safe_path(folder_path)) return false;
    if (locker_is_locked(folder_path)) return false;

    std::wstring normPath = locker_normalize_path(folder_path);

    // Check source exists and is directory
    DWORD attrs = GetFileAttributesW(normPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) return false;

    // Generate GUID id
    GUID guid;
    if (CoCreateGuid(&guid) != S_OK) return false;
    wchar_t guidStr[64] = {0};
    StringFromGUID2(guid, guidStr, 64);
    // Remove braces
    std::wstring id(guidStr);
    if (!id.empty() && id.front() == L'{') id = id.substr(1);
    if (!id.empty() && id.back()  == L'}') id.pop_back();
    out_id = id;

    std::wstring vault = get_vault_dir();
    std::wstring vault_path = vault + L"\\" + out_id;

    // Ensure vault exists
    locker_init_db();

    // Move folder — use MoveFileWithProgress for better error reporting
    if (!MoveFileExW(normPath.c_str(), vault_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
        out_id.clear();
        return false;
    }

    // Hide vault entry
    SetFileAttributesW(vault_path.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);

    std::wstring hash = locker_hash_password(password);
    if (hash.empty()) {
        // Rollback move
        MoveFileExW(vault_path.c_str(), normPath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        out_id.clear();
        return false;
    }

    // Timestamp ISO8601 UTC
    SYSTEMTIME st;
    GetSystemTime(&st);
    wchar_t timeStr[64];
    swprintf_s(timeStr, 64, L"%04u-%02u-%02uT%02u:%02u:%02uZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // Append to DB — open with exclusive lock via _wfsopen + _SH_DENYWR
    std::wstring db = get_db_path();
    FILE* f = _wfsopen(db.c_str(), L"a, ccs=UTF-8", _SH_DENYWR);
    if (!f) {
        // Rollback
        MoveFileExW(vault_path.c_str(), normPath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        out_id.clear();
        // Clear hash securely (wstring)
        if (!hash.empty()) SecureZeroMemory(&hash[0], hash.size() * sizeof(wchar_t));
        return false;
    }

    // Escape fields
    std::wstring esc_id   = db_escape(out_id);
    std::wstring esc_orig = db_escape(normPath);
    std::wstring esc_vault= db_escape(vault_path);
    std::wstring esc_hash = db_escape(hash);
    std::wstring esc_time = db_escape(timeStr);

    fwprintf(f, L"%ls|%ls|%ls|%ls|%ls\n",
        esc_id.c_str(), esc_orig.c_str(), esc_vault.c_str(), esc_hash.c_str(), esc_time.c_str());
    fclose(f);

    // Clear hash from memory
    SecureZeroMemory(&hash[0], hash.size() * sizeof(wchar_t));

    return true;
}

// ------------------------------------------------------------------
// Unlock folder
// ------------------------------------------------------------------
bool locker_unlock_folder(const std::wstring& id_or_path, const std::wstring& password) {
    if (id_or_path.empty() || password.empty()) return false;

    std::wstring normInput = locker_normalize_path(id_or_path);
    std::wstring lowerInput = normInput;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::towlower);

    auto list = locker_list_locked();
    LockedFolder* found = nullptr;
    size_t foundIdx = 0;

    for (size_t i = 0; i < list.size(); ++i) {
        auto& f = list[i];
        std::wstring a = f.id;
        std::wstring b = locker_normalize_path(f.original_path);
        std::wstring al = a, bl = b;
        std::transform(al.begin(), al.end(), al.begin(), ::towlower);
        std::transform(bl.begin(), bl.end(), bl.begin(), ::towlower);
        // id comparison is exact (GUID case-insensitive), path is normalized
        if (al == lowerInput || bl == lowerInput) {
            found = &list[i];
            foundIdx = i;
            break;
        }
        // Also try original (non-normalized) exact match for GUID with braces
        std::wstring idLower = f.id;
        std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::towlower);
        if (idLower == lowerInput) { found = &list[i]; foundIdx = i; break; }
    }

    if (!found) return false;

    if (!locker_verify_password(password, found->password_hash)) return false;

    // Verify vault exists
    DWORD vAttrs = GetFileAttributesW(found->vault_path.c_str());
    if (vAttrs == INVALID_FILE_ATTRIBUTES) return false;
    if (!(vAttrs & FILE_ATTRIBUTE_DIRECTORY)) return false;

    // Target must NOT exist
    DWORD tAttrs = GetFileAttributesW(found->original_path.c_str());
    if (tAttrs != INVALID_FILE_ATTRIBUTES) return false; // already exists

    // Ensure parent directory exists
    std::wstring target = found->original_path;
    size_t pos = target.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring parent = target.substr(0, pos);
        // Create parent chain
        std::wstring cur;
        for (size_t i = 0; i < parent.size(); ++i) {
            cur += parent[i];
            if (parent[i] == L'\\' || i == parent.size()-1) {
                if (cur.size() > 3) CreateDirectoryW(cur.c_str(), NULL);
            }
        }
    }

    // Remove hidden attribute before move
    SetFileAttributesW(found->vault_path.c_str(), FILE_ATTRIBUTE_DIRECTORY);

    if (!MoveFileExW(found->vault_path.c_str(), found->original_path.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
        // Restore hidden
        SetFileAttributesW(found->vault_path.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);
        return false;
    }

    // Rewrite DB without this entry — atomic via temp file
    std::wstring db = get_db_path();
    std::wstring tmp = db + L".tmp";

    FILE* fout = _wfsopen(tmp.c_str(), L"w, ccs=UTF-8", _SH_DENYWR);
    if (!fout) {
        // Rollback move? Best effort
        MoveFileExW(found->original_path.c_str(), found->vault_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        SetFileAttributesW(found->vault_path.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);
        return false;
    }

    fwprintf(fout, L"%ls\n", DB_VERSION);
    for (size_t i = 0; i < list.size(); ++i) {
        if (i == foundIdx) continue;
        auto& f = list[i];
        fwprintf(fout, L"%ls|%ls|%ls|%ls|%ls\n",
            db_escape(f.id).c_str(),
            db_escape(f.original_path).c_str(),
            db_escape(f.vault_path).c_str(),
            db_escape(f.password_hash).c_str(),
            db_escape(f.created_at).c_str());
    }
    fclose(fout);

    // Atomic replace
    // DeleteFileW first if needed (MoveFileEx with REPLACE_EXISTING)
    if (!MoveFileExW(tmp.c_str(), db.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        // Rollback move
        MoveFileExW(found->original_path.c_str(), found->vault_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
        SetFileAttributesW(found->vault_path.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);
        return false;
    }

    // Clear vault path hidden state is already moved

    return true;
}
