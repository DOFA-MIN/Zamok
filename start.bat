@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Starting Zamok in development mode...
python backend\app.py
pause
