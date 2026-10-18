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
echo  Zamok — Удаление
echo ============================================

echo [1/2] Удаление контекстного меню...
Zamok.exe --uninstall-menu

echo.
echo [2/2] Остановка и удаление службы...
Zamok.exe --uninstall-service

echo.
echo ============================================
echo  ✓ Zamok удален.
echo  Хранилище %LOCALAPPDATA%\Zamok осталось — удалите вручную при необходимости.
echo ============================================
pause
