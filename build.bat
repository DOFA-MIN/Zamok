@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================
echo  Zamok Folder Locker — Build (C++17)
echo  Без HTTP, только ядро + служба
echo ============================================

:: Поиск vcvarsall.bat — поддерживаем VS 2022 (17) и VS 2026 (18), Community/BuildTools/Professional/Enterprise
set "VC="
for %%V in (18 17) do (
  for %%E in (Community BuildTools Professional Enterprise) do (
    if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
      set "VC=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat"
      goto :found
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
      set "VC=C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat"
      goto :found
    )
  )
)
:found
if "%VC%"=="" (
  echo ERROR: Visual Studio Build Tools не найдены.
  echo Установите Visual Studio 2022 с компонентом "Разработка классических приложений на C++".
  pause
  exit /b 1
)

echo [1/3] Инициализация окружения: "%VC%"
call "%VC%" x64 >nul
if errorlevel 1 (
  echo ERROR: не удалось инициализировать среду VS
  pause
  exit /b 1
)

if exist src\resource.rc (
  echo [2/3] Компиляция ресурсов...
  rc.exe src\resource.rc /fo src\resource.res
  if errorlevel 1 (
    echo WARNING: не удалось скомпилировать ресурсы, продолжаем...
  )
  set "RES=src\resource.res"
) else (
  echo [2/3] Ресурсы не найдены — пропускаем
  set "RES="
)

echo [3/3] Компиляция...
cl.exe /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /W3 /WX- /Zi ^
    src\main.cpp ^
    src\config.cpp ^
    src\locker.cpp ^
    src\dialogs.cpp ^
    src\service.cpp ^
    src\context_menu.cpp ^
    %RES% ^
    /link user32.lib advapi32.lib shell32.lib shlwapi.lib bcrypt.lib ole32.lib comctl32.lib version.lib ^
    /out:Zamok.exe /SUBSYSTEM:WINDOWS /ENTRY:wWinMain

:: Если ENTRY не найден — пробуем консольную точку входа
if not exist Zamok.exe (
  echo Попытка сборки как консольного приложения...
  cl.exe /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /W3 ^
      src\main.cpp ^
      src\config.cpp ^
      src\locker.cpp ^
      src\dialogs.cpp ^
      src\service.cpp ^
      src\context_menu.cpp ^
      %RES% ^
      /link user32.lib advapi32.lib shell32.lib shlwapi.lib bcrypt.lib ole32.lib comctl32.lib version.lib ^
      /out:Zamok.exe /SUBSYSTEM:CONSOLE
)

if exist Zamok.exe (
  echo.
  echo ============================================
  echo  ✓ Сборка успешна: Zamok.exe
  for %%F in (Zamok.exe) do echo    Размер: %%~zF байт
  echo ============================================
) else (
  echo.
  echo  ✗ Сборка провалена. Проверьте вывод выше.
  exit /b 1
)

endlocal
pause
