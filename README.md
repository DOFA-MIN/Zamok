# Zamok — Folder Locker for Windows 10 (C++ / Python)

**Zamok** — утилита блокировки папок паролем. Без браузера, без HTTP API, только ядро + служба Windows.

- Правый клик по папке → **Zamok — Запереть / Отпереть / Список**
- Папки перемещаются в скрытое хранилище `%LOCALAPPDATA%\Zamok\Vault`
- Пароли хешируются **PBKDF2-HMAC-SHA256 (100 000 итераций, соль 16 байт)** — совместимо между C++ и Python версиями
- Служба Windows `Zamok Folder Locker` следит за целостностью хранилища (перекрывает скрытость, WAL для SQLite в Python)
- **HTTP API полностью удален** в версии 2.0 — нет открытых портов, нет CORS-рисков

## Две реализации

| Реализация | Хранилище | Технологии |
|------------|-----------|------------|
| **C++ (основная)** | Текстовый файл `data.db` с экранированием `\|` + PBKDF2 через `BCryptDeriveKeyPBKDF2` | WinAPI, BCrypt, Windows Service |
| **Python (альтернатива)** | SQLite `data.db` (WAL mode) + PBKDF2 через `hashlib` | pywin32, Tkinter |

Формат хеша одинаковый `salt_hex:hash_hex` (lowercase), путь нормализуется, сравнение case-insensitive.

## Требования

- **C++:** Microsoft Visual Studio 2022 (Community/Build Tools) с Workload "Разработка классических приложений на C++", Windows 10 SDK
- **Python:** Python 3.8+, `pywin32==312` (единственная зависимость), Windows 10

## Установка (C++ — рекомендуется)

1. Открой **Developer Command Prompt for VS 2022 (x64)** или просто запусти от Администратора:
2. `install.bat` (как Администратор)
   - Собирает `Zamok.exe`
   - Устанавливает и запускает службу `Zamok Folder Locker`
   - Регистрирует пункты контекстного меню

Или вручную:
```bat
build.bat
Zamok.exe --install-service
Zamok.exe --start-service
Zamok.exe --install-menu
```

## Установка (Python)

```bat
install_service.bat   # как Администратор — ставит службу + меню
```

Для разработки без службы:
```bat
start.bat             # показывает статус хранилища
python backend\app.py --status
```

## Использование

Правый клик по любой папке в Проводнике:
- **Zamok — Запереть** — ввод пароля, папка перемещается в `%LOCALAPPDATA%\Zamok\Vault`
- **Zamok — Отпереть** — ввод пароля, папка возвращается
- **Zamok — Список** — менеджер заблокированных папок (поиск, разблокировка, открытие хранилища)

Консоль:
```bat
Zamok.exe --lock "C:\MySecret"
Zamok.exe --unlock "C:\MySecret"
Zamok.exe --unlock "{guid-id}"
Zamok.exe --manager
Zamok.exe --status
Zamok.exe --help
```

## Безопасность

- Проверка пути: запрет на `C:\Windows`, `Vault`, `ProgramData` и т.д.
- Экранирование `|` в БД C++ (иначе пути с `|` ломали БД)
- Константное время сравнения паролей (`hmac.compare_digest` / `diff |= `)
- `SecureZeroMemory` для очистки паролей в RAM
- Скрытый атрибут хранилища восстанавливается службой каждые 5 сек
- Атомарная перезапись БД через `.tmp` + `MOVEFILE_REPLACE_EXISTING`
- SQLite WAL + `busy_timeout=5000` против гонок

## Удаление

```bat
uninstall.bat              # C++ версия
uninstall_service.bat      # Python версия
```
Хранилище `%LOCALAPPDATA%\Zamok` сохраняется — удали вручную если нужно.

## Сборка

```bat
build.bat   # автонаходит VS 2022/2026, Community/BuildTools/Enterprise
```

## Changelog v2.0

- **Удален HTTP сервер** (`src/http_server.*`, Flask) — нет открытых портов, нет `INADDR_ANY`
- Исправлен `config.h` (inline теперь в header, линковка починилась)
- Переписан `dialogs.cpp` — рабочий модальный диалог без ресурсов
- `locker.cpp` — PBKDF2 100k, защита путей, экранирование БД, атомарная запись, `SecureZeroMemory`
- `service.cpp` — корректный `binPath` с `--run-service`, описание службы, auto-restart
- `build.bat` — поиск VS 17/18, поддержка всех редакций
- Python: WAL, `COLLATE NOCASE`, `hmac.compare_digest`, улучшен UI менеджера
- `.gitignore` добавлен, `*.obj` удалены из репо

## Лицензия

MIT
