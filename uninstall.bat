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

echo Step 1: Remove context menu...
Zamok.exe --uninstall-menu

echo.
echo Step 2: Stop and remove service...
Zamok.exe --uninstall-service

echo.
echo DONE! Zamok Folder Locker removed.
pause
