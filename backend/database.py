import sqlite3
from pathlib import Path
from config import DB_PATH, BASE_DIR

def get_connection():
    BASE_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH), timeout=10, check_same_thread=False,
                           isolation_level=None)  # autocommit off? explicitly commit
    conn.row_factory = sqlite3.Row
    # WAL mode for concurrency safety + foreign keys
    try:
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute("PRAGMA synchronous=NORMAL;")
        conn.execute("PRAGMA busy_timeout=5000;")
    except Exception:
        pass
    return conn

def init_db():
    with get_connection() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS locked_folders (
                id            TEXT PRIMARY KEY,
                original_path TEXT NOT NULL UNIQUE,
                vault_path    TEXT NOT NULL,
                password_hash TEXT NOT NULL,
                created_at    TEXT NOT NULL
            )
            """
        )
        # Index for fast lookup
        conn.execute("CREATE INDEX IF NOT EXISTS idx_original_path ON locked_folders(original_path)")
        conn.commit()

def add_record(folder_id, original_path, vault_path, password_hash, created_at):
    with get_connection() as conn:
        conn.execute(
            "INSERT INTO locked_folders "
            "(id, original_path, vault_path, password_hash, created_at) "
            "VALUES (?, ?, ?, ?, ?)",
            (folder_id, original_path, vault_path, password_hash, created_at),
        )
        conn.commit()

def get_record(folder_id=None, original_path=None):
    with get_connection() as conn:
        if folder_id is not None:
            row = conn.execute(
                "SELECT * FROM locked_folders WHERE id = ? COLLATE NOCASE", (folder_id,)
            ).fetchone()
            if row:
                return dict(row)
            # Fallback: try as original_path case-insensitive (for unlock by path passed as id)
            row = conn.execute(
                "SELECT * FROM locked_folders WHERE original_path = ? COLLATE NOCASE", (folder_id,)
            ).fetchone()
            if row:
                return dict(row)
        if original_path is not None:
            row = conn.execute(
                "SELECT * FROM locked_folders WHERE original_path = ? COLLATE NOCASE",
                (original_path,),
            ).fetchone()
            if row:
                return dict(row)
            # Lowercase fallback for already normalized
            row = conn.execute(
                "SELECT * FROM locked_folders WHERE lower(original_path) = lower(?)",
                (original_path,),
            ).fetchone()
        return dict(row) if row else None

def list_records():
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT * FROM locked_folders ORDER BY created_at DESC"
        ).fetchall()
        return [dict(r) for r in rows]

def delete_record(folder_id):
    with get_connection() as conn:
        conn.execute("DELETE FROM locked_folders WHERE id = ? COLLATE NOCASE", (folder_id,))
        conn.commit()
