@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Zamok — проверка статуса (без HTTP)...
echo.
python backend\app.py --status
if errorlevel 1 (
  echo.
  echo Попытка запуска с python3...
  python3 backend\app.py --status
)
echo.
echo Для установки: install.bat (как Администратор)
echo Для меню:      python backend\context_menu.py
pause
