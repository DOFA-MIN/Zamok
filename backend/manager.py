import sys
import os
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locker

class Manager(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Zamok — Заблокированные папки")
        self.geometry("750x450")
        self.minsize(600, 350)
        self.attributes("-topmost", True)

        # Style
        try:
            self.tk.call("tk", "scaling", 1.2)
        except Exception:
            pass

        header = tk.Frame(self, bg="#2c3e50", height=40)
        header.pack(fill=tk.X)
        tk.Label(header, text="🔒 Zamok — Заблокированные папки", bg="#2c3e50", fg="white",
                 font=("Segoe UI", 11, "bold")).pack(side=tk.LEFT, padx=12, pady=8)
        tk.Label(header, text="без HTTP, только ядро + служба", bg="#2c3e50", fg="#bdc3c7",
                 font=("Segoe UI", 8)).pack(side=tk.LEFT)

        # List with scrollbar
        list_frame = tk.Frame(self)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        sb = tk.Scrollbar(list_frame, orient=tk.VERTICAL)
        self.listbox = tk.Listbox(list_frame, selectmode=tk.SINGLE, font=("Consolas", 9),
                                  yscrollcommand=sb.set, activestyle="dotbox")
        sb.config(command=self.listbox.yview)
        self.listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        sb.pack(side=tk.RIGHT, fill=tk.Y)

        # Double-click to unlock
        self.listbox.bind("<Double-Button-1>", lambda e: self.unlock_selected())

        btn_frame = tk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=10, pady=(0, 5))

        tk.Button(btn_frame, text="🔄 Обновить", command=self.refresh, width=14).pack(side=tk.LEFT, padx=3)
        tk.Button(btn_frame, text="🔓 Разблокировать", command=self.unlock_selected,
                  bg="#27ae60", fg="white", activebackground="#1e8449", width=16).pack(side=tk.LEFT, padx=3)
        tk.Button(btn_frame, text="📂 Открыть хранилище", command=self.open_vault).pack(side=tk.LEFT, padx=3)
        tk.Button(btn_frame, text="Закрыть", command=self.destroy, width=10).pack(side=tk.RIGHT, padx=3)

        self.status = tk.Label(self, text="", anchor=tk.W, font=("Segoe UI", 8), fg="#555")
        self.status.pack(fill=tk.X, padx=10, pady=(0, 8))

        self.items = []
        self.refresh()

    def refresh(self):
        self.listbox.delete(0, tk.END)
        self.items = locker.list_locked()
        for it in self.items:
            exists = it.get("vault_exists", True)
            mark = "" if exists else "  ⚠ НЕТ В ХРАНИЛИЩЕ"
            display = f"{it['original_path']}{mark}"
            self.listbox.insert(tk.END, display)
        self.status.config(text=f"Заблокировано папок: {len(self.items)}   |   Двойной клик — разблокировать")

    def unlock_selected(self):
        sel = self.listbox.curselection()
        if not sel:
            messagebox.showwarning("Zamok", "Выберите папку для разблокировки.", parent=self)
            return
        idx = sel[0]
        item = self.items[idx]

        if not item.get("vault_exists", True):
            messagebox.showerror("Zamok", "Папка отсутствует в хранилище.\nВозможно, уже разблокирована или удалена.", parent=self)
            return

        folder_path = item["original_path"]
        password = simpledialog.askstring(
            "Zamok — Разблокировка",
            f"Введите пароль для разблокировки:\n{folder_path}",
            show="•",
            parent=self,
        )
        if password is None or password == "":
            return
        if len(password) > 256:
            messagebox.showerror("Zamok", "Пароль слишком длинный.", parent=self)
            return

        result = locker.unlock_folder(folder_id=item["id"], password=password)
        # Clear password var
        password = ""
        if result["success"]:
            messagebox.showinfo("Zamok", f"✓ Папка разблокирована:\n{result.get('original_path', folder_path)}", parent=self)
            self.refresh()
        else:
            messagebox.showerror("Zamok", result.get("error", "Неизвестная ошибка"), parent=self)

    def open_vault(self):
        from config import VAULT_DIR
        vault = str(VAULT_DIR)
        if os.path.exists(vault):
            try:
                os.startfile(vault)  # Windows only
            except Exception as e:
                messagebox.showinfo("Zamok", f"Хранилище: {vault}\n\n{e}", parent=self)
        else:
            messagebox.showinfo("Zamok", f"Хранилище еще не создано:\n{vault}", parent=self)

if __name__ == "__main__":
    app = Manager()
    app.mainloop()
