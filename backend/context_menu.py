import os
import sys
import winreg
from config import BACKEND_DIR

PYTHON_EXE = sys.executable

def _get_command(script_name, arg="%1"):
    script = BACKEND_DIR / script_name
    # Quote correctly — supports paths with spaces
    if arg:
        return f'"{PYTHON_EXE}" "{script}" "{arg}"'
    else:
        return f'"{PYTHON_EXE}" "{script}"'

def _set_key(path, default_value, icon=None, position="Top"):
    key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, path)
    winreg.SetValueEx(key, "", 0, winreg.REG_SZ, default_value)
    if icon:
        winreg.SetValueEx(key, "Icon", 0, winreg.REG_SZ, icon)
    if position:
        winreg.SetValueEx(key, "Position", 0, winreg.REG_SZ, position)
    winreg.CloseKey(key)

def install_context_menu():
    """Устанавливает пункты Zamok в контекстное меню папок (без HTTP)."""
    try:
        # Lock
        _set_key(r"Directory\shell\ZamokLock", "Zamok — Запереть", "shell32.dll,3")
        key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokLock\command")
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, _get_command("lock_dialog.py"))
        winreg.CloseKey(key)

        # Unlock (отдельный пункт — удобнее чем искать в менеджере)
        _set_key(r"Directory\shell\ZamokUnlock", "Zamok — Отпереть", "shell32.dll,4")
        key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokUnlock\command")
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, _get_command("unlock_dialog.py"))
        winreg.CloseKey(key)

        # Manager
        _set_key(r"Directory\shell\ZamokManager", "Zamok — Список", "shell32.dll,43")
        key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokManager\command")
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, _get_command("manager.py", ""))
        winreg.CloseKey(key)

        # Background (внутри папки)
        _set_key(r"Directory\Background\shell\ZamokManager", "Zamok — Список", "shell32.dll,43")
        key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, r"Directory\Background\shell\ZamokManager\command")
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, _get_command("manager.py", ""))
        winreg.CloseKey(key)

        # Notify Explorer
        try:
            import ctypes
            ctypes.windll.shell32.SHChangeNotify(0x08000000, 0x0000, None, None)  # SHCNE_ASSOCCHANGED
        except Exception:
            pass

        return True
    except Exception as e:
        print("Context menu install error:", e)
        return False

def uninstall_context_menu():
    for path in [
        r"Directory\shell\ZamokLock\command", r"Directory\shell\ZamokLock",
        r"Directory\shell\ZamokUnlock\command", r"Directory\shell\ZamokUnlock",
        r"Directory\shell\ZamokManager\command", r"Directory\shell\ZamokManager",
        r"Directory\Background\shell\ZamokManager\command", r"Directory\Background\shell\ZamokManager",
    ]:
        try:
            winreg.DeleteKey(winreg.HKEY_CLASSES_ROOT, path)
        except FileNotFoundError:
            pass
        except Exception as e:
            print(f"Uninstall error {path}:", e)

    try:
        import ctypes
        ctypes.windll.shell32.SHChangeNotify(0x08000000, 0x0000, None, None)
    except Exception:
        pass
    return True

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "uninstall":
        uninstall_context_menu()
        print("Context menu removed.")
    else:
        ok = install_context_menu()
        print("Context menu installed." if ok else "Failed to install.")
