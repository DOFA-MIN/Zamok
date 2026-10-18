@echo off
chcp 65001 >nul
cd /d "%~dp0"

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Запрос прав администратора...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ============================================
echo  Zamok (Python) — Удаление
echo ============================================

echo Удаление меню...
python backend\context_menu.py uninstall >nul 2>&1

echo Остановка службы...
python backend\service.py stop >nul 2>&1

echo Удаление службы...
python backend\service.py remove
if errorlevel 1 (
    echo ERROR: не удалось удалить службу. Запустите как Администратор.
    pause
    exit /b 1
)

echo.
echo ✓ Удалено.
pause
