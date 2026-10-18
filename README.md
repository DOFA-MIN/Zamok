# Zamok — Folder Locker for Windows 10 (C++)

Application in C++ using Windows API. No browser, no Node.js, no external libraries (only Windows SDK).

## Features
- Right-click any folder → context menu "Zamok" → Lock / Unlock / Manager
- Locked folders moved to hidden vault `%LOCALAPPDATA%\Zamok\Vault`
- Passwords hashed with Windows BCrypt (SHA-256)
- Runs as Windows service `Zamok Folder Locker`
- HTTP API on `http://127.0.0.1:5000/api`

## Build & Install

Requirements:
- Microsoft Visual Studio 2022 (Community/Build Tools) with C++ workload
- Windows 10

1. Open **Developer Command Prompt for VS 2022** (x64)
2. Run: `install.bat` (as Administrator)
   - Compiles `Zamok.exe`
   - Installs and starts the Windows service
   - Registers context menu items

## Usage

Right-click any folder in Explorer:
- **Zamok - Lock** — enter password, folder moves to vault
- **Zamok - Unlock** — enter password, folder returns to original place
- **Zamok - Manager** — show list of locked folders

## Uninstall

Run `uninstall.bat` (as Administrator).

## API

- `GET  /api/status`
- `GET  /api/locked`
- `POST /api/lock`   body: `{"path":"...","password":"..."}`
- `POST /api/unlock` body: `{"id":"...","password":"..."}` or `{"path":"...","password":"..."}`
