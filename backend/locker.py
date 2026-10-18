"""
Zamok — ядро блокировки папок (без HTTP).
Блокировка = перемещение в скрытое хранилище + PBKDF2 хеш пароля + SQLite запись.
Совместимо с C++ версией по PBKDF2 (100k, SHA256, соль 16 байт, формат salt:hash).
"""
import os
import shutil
import hashlib
import uuid
import hmac
from pathlib import Path
from datetime import datetime, timezone

from config import VAULT_DIR, PBKDF2_ITERATIONS, BASE_DIR, FORBIDDEN_PREFIXES, FORBIDDEN_EXACT
from database import (
    init_db,
    add_record,
    get_record,
    list_records,
    delete_record,
)

# ------------------------------------------------------------------
def _ensure_dirs():
    VAULT_DIR.mkdir(parents=True, exist_ok=True)
    # Hide vault (Windows)
    try:
        import ctypes
        ctypes.windll.kernel32.SetFileAttributesW(str(VAULT_DIR), 2)  # FILE_ATTRIBUTE_HIDDEN
    except Exception:
        pass

def is_safe_path(path: str) -> bool:
    """Проверка безопасности пути — не даем запереть системные папки/само хранилище."""
    if not path or len(path) >= 260:
        return False
    try:
        norm = normalize_path(path).lower()
        vault = str(VAULT_DIR.resolve()) if VAULT_DIR.exists() else str(VAULT_DIR)
        vault_lower = os.path.normpath(vault).lower()
        base_lower = os.path.normpath(str(BASE_DIR)).lower()

        if norm == vault_lower or norm.startswith(vault_lower + os.sep):
            return False
        if norm == base_lower:
            return False
        for pref in FORBIDDEN_PREFIXES:
            if norm.startswith(pref):
                return False
        for exact in FORBIDDEN_EXACT:
            if norm == exact:
                return False
        # Must be absolute (C:\ or \\ on Windows; / on POSIX)
        # On Linux/macOS test environment allow POSIX absolute paths
        import sys as _sys
        if _sys.platform == "win32":
            if len(norm) < 3 or norm[1] != ":":
                if not norm.startswith("\\\\"):
                    return False
        else:
            # POSIX: must be absolute
            if not os.path.isabs(path):
                return False
        return True
    except Exception:
        return False

# ------------------------------------------------------------------
def hash_password(password: str, salt: bytes | None = None) -> str:
    if not password or len(password) > 256:
        raise ValueError("Invalid password length")
    if salt is None:
        salt = os.urandom(16)
    dk = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, PBKDF2_ITERATIONS)
    return salt.hex() + ":" + dk.hex()

def verify_password(password: str, stored: str) -> bool:
    try:
        salt_hex, dk_hex = stored.split(":")
        salt = bytes.fromhex(salt_hex)
        expected = bytes.fromhex(dk_hex)
    except Exception:
        return False
    dk = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, PBKDF2_ITERATIONS)
    # Constant-time
    return hmac.compare_digest(dk, expected)

def normalize_path(path: str) -> str:
    # Resolve sympathetically without requiring existence
    return os.path.normpath(os.path.abspath(path))

def is_locked(original_path: str) -> bool:
    init_db()
    norm = normalize_path(original_path)
    # Case-insensitive check on Windows
    rec = get_record(original_path=norm)
    if rec:
        return True
    # Fallback: scan all (for case-insensitive)
    for r in list_records():
        if os.path.normpath(r["original_path"]).lower() == norm.lower():
            return True
    return False

# ------------------------------------------------------------------
def lock_folder(folder_path: str, password: str) -> dict:
    _ensure_dirs()
    init_db()

    if not password or len(password) > 256:
        return {"success": False, "error": "Некорректный пароль (1-256 символов)"}

    folder_path = normalize_path(folder_path)

    if not is_safe_path(folder_path):
        return {"success": False, "error": "Недопустимый путь — нельзя блокировать системные папки или хранилище Zamok"}

    if not os.path.exists(folder_path):
        return {"success": False, "error": "Папка не найдена"}
    if not os.path.isdir(folder_path):
        return {"success": False, "error": "Путь не является папкой"}

    if is_locked(folder_path):
        return {"success": False, "error": "Эта папка уже заблокирована"}

    folder_id = str(uuid.uuid4())
    vault_path = str(VAULT_DIR / folder_id)

    try:
        shutil.move(folder_path, vault_path)
    except Exception as e:
        return {"success": False, "error": "Не удалось переместить папку: " + str(e)}

    # Hide vault entry
    try:
        import ctypes
        ctypes.windll.kernel32.SetFileAttributesW(vault_path, 2)
    except Exception:
        pass

    password_hash = hash_password(password)
    created_at = datetime.now(timezone.utc).isoformat()
    try:
        add_record(folder_id, folder_path, vault_path, password_hash, created_at)
    except Exception as e:
        # Rollback on DB error
        try:
            shutil.move(vault_path, folder_path)
        except Exception:
            pass
        return {"success": False, "error": "Ошибка БД: " + str(e)}

    return {
        "success": True,
        "id": folder_id,
        "original_path": folder_path,
        "vault_path": vault_path,
    }

def unlock_folder(folder_id: str | None = None, original_path: str | None = None, password: str = "") -> dict:
    init_db()
    if original_path is not None:
        original_path = normalize_path(original_path)

    # Try by id first, then by path
    record = None
    if folder_id:
        record = get_record(folder_id=folder_id)
    if not record and original_path:
        record = get_record(original_path=original_path)
        # Case-insensitive fallback
        if not record:
            for r in list_records():
                if os.path.normpath(r["original_path"]).lower() == (original_path or "").lower():
                    record = r
                    break
    if not record:
        return {"success": False, "error": "Запись о заблокированной папке не найдена"}

    if not verify_password(password, record["password_hash"]):
        return {"success": False, "error": "Неверный пароль"}

    vault_path = record["vault_path"]
    target_path = record["original_path"]

    if os.path.exists(target_path):
        return {"success": False, "error": "Папка уже существует по пути: " + target_path}
    if not os.path.exists(vault_path):
        return {"success": False, "error": "Папка не найдена в хранилище (возможно уже разблокирована)"}

    # Ensure parent exists
    parent = os.path.dirname(target_path)
    if parent and not os.path.exists(parent):
        try:
            os.makedirs(parent, exist_ok=True)
        except Exception as e:
            return {"success": False, "error": "Не удалось создать родительскую папку: " + str(e)}

    # Remove hidden before move (best effort)
    try:
        import ctypes
        ctypes.windll.kernel32.SetFileAttributesW(vault_path, 128)  # NORMAL
    except Exception:
        pass

    try:
        shutil.move(vault_path, target_path)
    except Exception as e:
        return {"success": False, "error": "Не удалось восстановить папку: " + str(e)}

    delete_record(record["id"])
    return {"success": True, "original_path": target_path}

def list_locked():
    init_db()
    records = list_records()
    for r in records:
        r["vault_exists"] = os.path.exists(r["vault_path"])
        # Hide hash from external callers unless needed — keep internal
    return records
