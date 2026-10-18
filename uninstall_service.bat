@echo off
chcp 65001 >nul
cd /d "%~dp0"

:: Check administrator rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Running as administrator...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ============================================
echo  Zamok Folder Locker - Uninstallation
echo ============================================

echo Removing context menu...
python backend\context_menu.py uninstall >nul 2>&1

echo Stopping service...
python backend\service.py stop >nul 2>&1

echo Removing service...
python backend\service.py remove
if errorlevel 1 (
    echo ERROR: failed to remove service. Run as administrator.
    pause
    exit /b 1
)

echo.
echo DONE! Service removed.
pause
