import sys
import os
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locker


class Manager(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Zamok - Locked Folders")
        self.geometry("700x400")
        self.attributes("-topmost", True)

        self.listbox = tk.Listbox(self, selectmode=tk.SINGLE)
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        btn_frame = tk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=10, pady=5)

        tk.Button(btn_frame, text="Refresh", command=self.refresh).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Unlock Selected", command=self.unlock_selected).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Close", command=self.destroy).pack(side=tk.RIGHT, padx=5)

        self.status = tk.Label(self, text="", anchor=tk.W)
        self.status.pack(fill=tk.X, padx=10, pady=2)

        self.refresh()

    def refresh(self):
        self.listbox.delete(0, tk.END)
        self.items = locker.list_locked()
        for it in self.items:
            mark = "" if it["vault_exists"] else " [MISSING]"
            self.listbox.insert(tk.END, it["original_path"] + mark)
        self.status.config(text="Locked folders: {}".format(len(self.items)))

    def unlock_selected(self):
        sel = self.listbox.curselection()
        if not sel:
            messagebox.showwarning("Zamok", "Select a folder to unlock.")
            return
        idx = sel[0]
        item = self.items[idx]
        folder_path = item["original_path"]

        root = self
        password = simpledialog.askstring(
            "Zamok - Unlock",
            "Enter password to unlock:\n" + folder_path,
            show="*",
            parent=root,
        )
        if password is None:
            return

        result = locker.unlock_folder(folder_id=item["id"], password=password)
        if result["success"]:
            messagebox.showinfo("Zamok", "Folder unlocked successfully.")
            self.refresh()
        else:
            messagebox.showerror("Zamok", result.get("error", "Unknown error"))


if __name__ == "__main__":
    app = Manager()
    app.mainloop()
