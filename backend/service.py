"""
Zamok Windows Service — без HTTP.
Служба обеспечивает скрытость хранилища и контроль целостности.
Вся логика блокировки выполняется напрямую через locker.py (контекстное меню / диалоги).
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import win32serviceutil
import win32service
import win32event
import servicemanager

from config import SERVICE_NAME, SERVICE_DISPLAY_NAME, SERVICE_DESCRIPTION, VAULT_DIR
import locker  # ensure DB init

class ZamokService(win32serviceutil.ServiceFramework):
    _svc_name_ = SERVICE_NAME
    _svc_display_name_ = SERVICE_DISPLAY_NAME
    _svc_description_ = SERVICE_DESCRIPTION

    def __init__(self, args):
        win32serviceutil.ServiceFramework.__init__(self, args)
        self.hWaitStop = win32event.CreateEvent(None, 0, 0, None)

    def SvcStop(self):
        self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
        win32event.SetEvent(self.hWaitStop)

    def SvcDoRun(self):
        servicemanager.LogMsg(
            servicemanager.EVENTLOG_INFORMATION_TYPE,
            servicemanager.PYS_SERVICE_STARTED,
            (self._svc_name_, ""),
        )
        # Initialize storage
        try:
            locker.init_db()  # via locker._ensure_dirs internally
            VAULT_DIR.mkdir(parents=True, exist_ok=True)
            # Re-apply hidden attribute
            try:
                import ctypes
                ctypes.windll.kernel32.SetFileAttributesW(str(VAULT_DIR), 2)
            except Exception:
                pass
        except Exception as e:
            servicemanager.LogError(f"Zamok init error: {e}")

        # Main loop — keep vault hidden, wait for stop
        # No HTTP server. Service is lightweight idle loop.
        while True:
            rc = win32event.WaitForSingleObject(self.hWaitStop, 5000)  # 5s periodic check
            if rc == win32event.WAIT_OBJECT_0:
                break
            # Periodic maintenance
            try:
                if VAULT_DIR.exists():
                    import ctypes
                    ctypes.windll.kernel32.SetFileAttributesW(str(VAULT_DIR), 2)
            except Exception:
                pass

        servicemanager.LogInfoMsg("Zamok service stopped")

if __name__ == "__main__":
    win32serviceutil.HandleCommandLine(ZamokService)
