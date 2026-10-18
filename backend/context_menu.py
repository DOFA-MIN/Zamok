import os
import sys
import winreg

from config import BACKEND_DIR, SERVICE_NAME

PYTHON_EXE = sys.executable


def _get_command(script_name, arg="%1"):
    script = BACKEND_DIR / script_name
    return '"{}" "{}" "{}"'.format(PYTHON_EXE, script, arg)


def install_context_menu():
    """Add Zamok items to folder context menu."""
    try:
        # Zamok - Lock
        key_path = r"Directory\shell\ZamokLock"
        key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, key_path)
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, "&Zamok - Lock")
        winreg.SetValueEx(key, "Icon", 0, winreg.REG_SZ, "shell32.dll,3")
        winreg.CloseKey(key)

        cmd_key = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, key_path + r"\command")
        winreg.SetValueEx(cmd_key, "", 0, winreg.REG_SZ, _get_command("lock_dialog.py"))
        winreg.CloseKey(cmd_key)

        # Zamok - Manager (unlock list)
        key_path2 = r"Directory\shell\ZamokManager"
        key2 = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, key_path2)
        winreg.SetValueEx(key2, "", 0, winreg.REG_SZ, "&Zamok - Manager")
        winreg.SetValueEx(key2, "Icon", 0, winreg.REG_SZ, "shell32.dll,4")
        winreg.CloseKey(key2)

        cmd_key2 = winreg.CreateKey(winreg.HKEY_CLASSES_ROOT, key_path2 + r"\command")
        winreg.SetValueEx(cmd_key2, "", 0, winreg.REG_SZ, _get_command("manager.py", ""))
        winreg.CloseKey(cmd_key2)

        return True
    except Exception as e:
        print("Context menu install error:", e)
        return False


def uninstall_context_menu():
    """Remove Zamok items from folder context menu."""
    try:
        winreg.DeleteKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokLock\command")
        winreg.DeleteKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokLock")
    except FileNotFoundError:
        pass
    except Exception as e:
        print("Context menu uninstall error (Lock):", e)

    try:
        winreg.DeleteKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokManager\command")
        winreg.DeleteKey(winreg.HKEY_CLASSES_ROOT, r"Directory\shell\ZamokManager")
    except FileNotFoundError:
        pass
    except Exception as e:
        print("Context menu uninstall error (Manager):", e)

    return True


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "uninstall":
        uninstall_context_menu()
        print("Context menu removed.")
    else:
        install_context_menu()
        print("Context menu installed.")
