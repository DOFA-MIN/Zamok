"""
Zamok — точка входа без HTTP (для отладки/консоли).

УДАЛЕН Flask HTTP API (см. git history до v1).
Теперь вся работа идет напрямую через locker.py:
  - Служба: backend/service.py (без HTTP)
  - Контекстное меню: backend/context_menu.py
  - Диалоги: backend/lock_dialog.py / unlock_dialog.py / manager.py

Этот файл оставлен для ручного запуска проверки:
  python backend/app.py --status  -> показать статус хранилища
  python backend/app.py --help
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import locker
from config import VAULT_DIR, BASE_DIR
from pathlib import Path

def print_status():
    VAULT_DIR.mkdir(parents=True, exist_ok=True)
    items = locker.list_locked()
    print(f"Zamok — статус (без HTTP)")
    print(f"  Vault: {VAULT_DIR}")
    print(f"  DB:    {BASE_DIR / 'data.db'}")
    print(f"  Заблокировано: {len(items)}")
    for it in items:
        mark = "" if it.get("vault_exists") else " [MISSING]"
        print(f"    • {it['original_path']}{mark}  id={it['id']}")

def main():
    if len(sys.argv) > 1 and sys.argv[1] in ("--status", "status"):
        print_status()
        return
    if len(sys.argv) > 1 and sys.argv[1] in ("--help", "-h", "help"):
        print("Zamok (Python) — без HTTP")
        print("  python backend/app.py --status   показать хранилище")
        print("  python backend/service.py install   установить службу")
        print("  python backend/context_menu.py       установить меню")
        return
    # Default: status
    print_status()
    print("\nHTTP API удален. Используйте службу + контекстное меню.")
    print("См. README.md для инструкций.")

if __name__ == "__main__":
    main()
