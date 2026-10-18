import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import win32serviceutil
import win32service
import win32event
import servicemanager

from app import main as flask_main
from config import SERVICE_NAME, SERVICE_DISPLAY_NAME, SERVICE_DESCRIPTION


class ZamokService(win32serviceutil.ServiceFramework):
    _svc_name_ = SERVICE_NAME
    _svc_display_name_ = SERVICE_DISPLAY_NAME
    _svc_description_ = SERVICE_DESCRIPTION

    def __init__(self, args):
        win32serviceutil.ServiceFramework.__init__(self, args)
        self.hWaitStop = win32event.CreateEvent(None, 0, 0, None)
        self.flask_thread = None

    def SvcStop(self):
        self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
        win32event.SetEvent(self.hWaitStop)

    def SvcDoRun(self):
        servicemanager.LogMsg(
            servicemanager.EVENTLOG_INFORMATION_TYPE,
            servicemanager.PYS_SERVICE_STARTED,
            (self._svc_name_, ""),
        )
        self.flask_thread = threading.Thread(target=flask_main, daemon=True)
        self.flask_thread.start()
        win32event.WaitForSingleObject(self.hWaitStop, win32event.INFINITE)
        servicemanager.LogInfo("Zamok service stopped")


if __name__ == "__main__":
    win32serviceutil.HandleCommandLine(ZamokService)
