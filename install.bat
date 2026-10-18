@echo off
chcp 65001 >nul
cd /d "%~dp0"

:: Проверка прав администратора
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Запрос прав администратора...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ============================================
echo  Zamok Folder Locker — Установка
echo  (без HTTP, только ядро + служба)
echo ============================================

echo [1/4] Сборка...
call build.bat
if errorlevel 1 (
  echo Ошибка сборки. Прерывание.
  pause
  exit /b 1
)

echo.
echo [2/4] Установка службы Windows...
Zamok.exe --install-service
if errorlevel 1 (
  echo WARNING: не удалось установить службу. Проверьте права.
)

echo.
echo [3/4] Запуск службы...
Zamok.exe --start-service

echo.
echo [4/4] Установка контекстного меню...
Zamok.exe --install-menu

echo.
echo ============================================
echo  ✓ Готово! Zamok установлен.
echo  Правый клик по папке → Zamok — Запереть / Отпереть / Список
echo  Статус: Zamok.exe --status
echo ============================================
pause
