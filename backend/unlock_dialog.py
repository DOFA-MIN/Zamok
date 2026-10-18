import sys
import os
import tkinter as tk
from tkinter import messagebox, simpledialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locker

def main():
    folder_path = sys.argv[1] if len(sys.argv) > 1 else ""
    if not folder_path:
        messagebox.showerror("Zamok", "Не указана папка для разблокировки.")
        return 1

    folder_path = os.path.normpath(folder_path)

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    root.lift()

    # Inform if not locked (might be unlocked via ID? but we handle folder path)
    # Don't block — allow attempt (locker will search case-insensitive)

    password = simpledialog.askstring(
        "Zamok — Разблокировка",
        f"Введите пароль для разблокировки:\n{folder_path}",
        show="•",
        parent=root,
    )
    root.destroy()

    if password is None or password == "":
        return 0

    result = locker.unlock_folder(original_path=folder_path, password=password)
    password = ""

    if result["success"]:
        messagebox.showinfo("Zamok", f"✓ Папка разблокирована:\n{result.get('original_path', folder_path)}")
    else:
        messagebox.showerror("Zamok", result.get("error", "Неизвестная ошибка"))
    return 0 if result["success"] else 1

if __name__ == "__main__":
    sys.exit(main())
