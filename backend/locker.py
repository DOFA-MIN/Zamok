import os
import shutil
import hashlib
import uuid
from datetime import datetime, timezone

from config import VAULT_DIR, PBKDF2_ITERATIONS
from database import (
    init_db,
    add_record,
    get_record,
    list_records,
    delete_record,
)


def _ensure_dirs():
    VAULT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        import ctypes
        ctypes.windll.kernel32.SetFileAttributesW(str(VAULT_DIR), 2)
    except Exception:
        pass


def hash_password(password, salt=None):
    if salt is None:
        salt = os.urandom(16)
    dk = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, PBKDF2_ITERATIONS)
    return salt.hex() + ":" + dk.hex()


def verify_password(password, stored):
    try:
        salt_hex, dk_hex = stored.split(":")
        salt = bytes.fromhex(salt_hex)
    except Exception:
        return False
    dk = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, PBKDF2_ITERATIONS)
    return dk.hex() == dk_hex


def normalize_path(path):
    return os.path.normpath(os.path.abspath(path))


def is_locked(original_path):
    init_db()
    return get_record(original_path=original_path) is not None


def lock_folder(folder_path, password):
    _ensure_dirs()
    init_db()

    folder_path = normalize_path(folder_path)
    if not os.path.exists(folder_path):
        return {"success": False, "error": "Folder not found"}
    if not os.path.isdir(folder_path):
        return {"success": False, "error": "Path is not a folder"}

    if is_locked(folder_path):
        return {"success": False, "error": "This folder is already locked"}

    folder_id = str(uuid.uuid4())
    vault_path = str(VAULT_DIR / folder_id)

    try:
        shutil.move(folder_path, vault_path)
    except Exception as e:
        return {"success": False, "error": "Failed to move folder: " + str(e)}

    try:
        import ctypes
        ctypes.windll.kernel32.SetFileAttributesW(vault_path, 2)
    except Exception:
        pass

    password_hash = hash_password(password)
    created_at = datetime.now(timezone.utc).isoformat()
    add_record(folder_id, folder_path, vault_path, password_hash, created_at)

    return {
        "success": True,
        "id": folder_id,
        "original_path": folder_path,
        "vault_path": vault_path,
    }


def unlock_folder(folder_id=None, original_path=None, password=""):
    init_db()
    if original_path is not None:
        original_path = normalize_path(original_path)
    record = get_record(folder_id=folder_id, original_path=original_path)
    if not record:
        return {"success": False, "error": "Locked folder record not found"}

    if not verify_password(password, record["password_hash"]):
        return {"success": False, "error": "Wrong password"}

    vault_path = record["vault_path"]
    target_path = record["original_path"]

    if os.path.exists(target_path):
        return {"success": False, "error": "Folder already exists at " + target_path}
    if not os.path.exists(vault_path):
        return {"success": False, "error": "Folder not found in vault (maybe already unlocked)"}

    try:
        shutil.move(vault_path, target_path)
    except Exception as e:
        return {"success": False, "error": "Failed to restore folder: " + str(e)}

    delete_record(record["id"])
    return {"success": True, "original_path": target_path}


def list_locked():
    init_db()
    records = list_records()
    for r in records:
        r["vault_exists"] = os.path.exists(r["vault_path"])
    return records
