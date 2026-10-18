@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo Building Zamok Folder Locker (C++)..."

set VC="C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VC% (
    echo ERROR: Visual Studio Build Tools not found.
    pause
    exit /b 1
)

call %VC% x64

if not exist src mkdir src

echo Compiling resources...
if exist src\resource.rc (
    rc.exe src\resource.rc /fo src\resource.res
)

echo Compiling sources...
cl.exe /nologo /O2 /EHsc /DUNICODE /D_UNICODE ^
    src\main.cpp ^
    src\config.cpp ^
    src\locker.cpp ^
    src\dialogs.cpp ^
    src\service.cpp ^
    src\http_server.cpp ^
    src\context_menu.cpp ^
    /link user32.lib advapi32.lib shell32.lib shlwapi.lib bcrypt.lib ole32.lib comctl32.lib ws2_32.lib ^
    /out:Zamok.exe

if exist Zamok.exe (
    echo.
    echo Build complete: Zamok.exe
) else (
    echo Build failed.
)

pause
