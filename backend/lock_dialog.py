import sys
import os
import tkinter as tk
from tkinter import messagebox, simpledialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locker


def main():
    folder_path = sys.argv[1] if len(sys.argv) > 1 else ""
    if not folder_path:
        messagebox.showerror("Zamok", "No folder specified.")
        return 1

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)

    password = simpledialog.askstring(
        "Zamok - Lock Folder",
        "Enter password to lock:\n" + folder_path,
        show="*",
        parent=root,
    )
    root.destroy()

    if password is None:
        return 0

    result = locker.lock_folder(folder_path, password)
    if result["success"]:
        messagebox.showinfo("Zamok", "Folder locked successfully.")
    else:
        messagebox.showerror("Zamok", result.get("error", "Unknown error"))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    sys.exit(main())
