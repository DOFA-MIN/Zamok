import os
from pathlib import Path

# Base paths — aligned with C++ version (%LOCALAPPDATA%\Zamok)
BASE_DIR = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "Zamok"
VAULT_DIR = BASE_DIR / "Vault"
DB_PATH = BASE_DIR / "data.db"

# Service identity (matches C++ config.h)
SERVICE_NAME = "ZamokFolderLocker"
SERVICE_DISPLAY_NAME = "Zamok Folder Locker"
SERVICE_DESCRIPTION = "Zamok Folder Locker — защита папок"

PBKDF2_ITERATIONS = 100000  # compatible with C++ (BCryptDeriveKeyPBKDF2)

BACKEND_DIR = Path(__file__).resolve().parent

# Security: forbidden paths (same logic as C++)
FORBIDDEN_PREFIXES = [
    "c:\\windows",
]
FORBIDDEN_EXACT = [
    "c:\\windows",
    "c:\\windows\\system32",
    "c:\\program files",
    "c:\\program files (x86)",
    "c:\\programdata",
]
