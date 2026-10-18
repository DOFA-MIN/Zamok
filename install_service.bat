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
echo  Zamok (Python) — Установка службы
echo  Без HTTP, только ядро + служба
echo ============================================

echo [1/3] Проверка зависимостей...
python -c "import win32serviceutil" >nul 2>&1
if errorlevel 1 (
    echo Установка зависимостей...
    pip install -r backend\requirements.txt
    if errorlevel 1 (
        echo ERROR: не удалось установить зависимости.
        pause
        exit /b 1
    )
)

echo [2/3] Установка контекстного меню...
python backend\context_menu.py
if errorlevel 1 (
    echo WARNING: не удалось установить меню, продолжаем...
)

echo [3/3] Установка и запуск службы...
python backend\service.py install
if errorlevel 1 (
    echo ERROR: не удалось установить службу.
    pause
    exit /b 1
)

python backend\service.py start
if errorlevel 1 (
    echo WARNING: служба установлена, но не запустилась. Проверьте логи.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  ✓ Готово! (Python версия)
echo  Правый клик → Zamok — Запереть / Отпереть / Список
echo  Статус: python backend\app.py --status
echo  Служба: Zamok Folder Locker (без HTTP)
echo ============================================
pause
