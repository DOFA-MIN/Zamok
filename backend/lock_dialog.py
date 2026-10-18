import sys
import os
import tkinter as tk
from tkinter import messagebox, simpledialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locker

def main():
    folder_path = sys.argv[1] if len(sys.argv) > 1 else ""
    if not folder_path:
        messagebox.showerror("Zamok", "Не указана папка.\n\nИспользуйте правый клик по папке → Zamok — Запереть")
        return 1

    # Normalize for display
    folder_path = os.path.normpath(folder_path)

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    root.lift()
    root.focus_force()

    # Safety check before asking password
    if not locker.is_safe_path(folder_path):
        messagebox.showerror("Zamok", f"Недопустимый путь:\n{folder_path}\n\nНельзя блокировать системные папки или хранилище Zamok.", parent=root)
        root.destroy()
        return 1

    if locker.is_locked(folder_path):
        messagebox.showinfo("Zamok", f"Эта папка уже заблокирована:\n{folder_path}", parent=root)
        root.destroy()
        return 0

    password = simpledialog.askstring(
        "Zamok — Блокировка",
        f"Введите пароль для блокировки:\n{folder_path}\n\nПароль будет захеширован PBKDF2 (100k итераций).",
        show="•",
        parent=root,
    )
    root.destroy()

    if password is None or password == "":
        return 0  # cancelled

    if len(password) < 3:
        messagebox.showwarning("Zamok", "Пароль слишком короткий (минимум 3 символа).", parent=None)
        return 1

    result = locker.lock_folder(folder_path, password)
    # Clear password
    password = ""

    if result["success"]:
        messagebox.showinfo("Zamok", f"✓ Папка заблокирована:\n{folder_path}\n\nID: {result['id']}\nХранилище: {result['vault_path']}")
    else:
        messagebox.showerror("Zamok", result.get("error", "Неизвестная ошибка"))
    return 0 if result["success"] else 1

if __name__ == "__main__":
    sys.exit(main())
