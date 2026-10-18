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

echo Checking Python dependencies...
python -c "import flask, flask_cors, win32serviceutil" >nul 2>&1
if errorlevel 1 (
    echo Installing dependencies...
    pip install -r backend\requirements.txt
    if errorlevel 1 (
        echo ERROR: failed to install dependencies.
        pause
        exit /b 1
    )
)

echo Installing context menu...
python backend\context_menu.py
if errorlevel 1 (
    echo ERROR: failed to install context menu.
    pause
    exit /b 1
)

echo Installing service...
python backend\service.py install
if errorlevel 1 (
    echo ERROR: failed to install service.
    pause
    exit /b 1
)

echo Starting service...
python backend\service.py start
if errorlevel 1 (
    echo ERROR: failed to start service.
    pause
    exit /b 1
)

echo.
echo DONE! Service installed and started.
echo API: http://127.0.0.1:5000/api
echo Right-click any folder to access Zamok - Lock and Zamok - Manager.
pause
