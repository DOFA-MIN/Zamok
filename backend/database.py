import sqlite3
from pathlib import Path

from config import DB_PATH, BASE_DIR


def get_connection():
    BASE_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row
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
                "SELECT * FROM locked_folders WHERE id = ?", (folder_id,)
            ).fetchone()
        elif original_path is not None:
            row = conn.execute(
                "SELECT * FROM locked_folders WHERE original_path = ?",
                (original_path,),
            ).fetchone()
        else:
            row = None
        return dict(row) if row else None


def list_records():
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT * FROM locked_folders ORDER BY created_at DESC"
        ).fetchall()
        return [dict(r) for r in rows]


def delete_record(folder_id):
    with get_connection() as conn:
        conn.execute("DELETE FROM locked_folders WHERE id = ?", (folder_id,))
        conn.commit()
