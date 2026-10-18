import os
from pathlib import Path

BASE_DIR = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "Zamok"
VAULT_DIR = BASE_DIR / "Vault"
DB_PATH = BASE_DIR / "data.db"

HOST = "127.0.0.1"
PORT = 5000

SERVICE_NAME = "ZamokFolderLocker"
SERVICE_DISPLAY_NAME = "Zamok Folder Locker"
SERVICE_DESCRIPTION = "Zamok Folder Locker service"

PBKDF2_ITERATIONS = 100000

BACKEND_DIR = Path(__file__).resolve().parent
