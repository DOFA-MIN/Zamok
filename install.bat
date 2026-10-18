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
echo  Zamok Folder Locker - Installation
echo ============================================

echo Step 1: Build...
call build.bat

echo.
echo Step 2: Install Windows service...
Zamok.exe --install-service

echo.
echo Step 3: Start service...
Zamok.exe --start-service

echo.
echo Step 4: Install context menu...
Zamok.exe --install-menu

echo.
echo DONE! Zamok Folder Locker is installed.
echo Right-click any folder to see "Zamok" options.
pause
