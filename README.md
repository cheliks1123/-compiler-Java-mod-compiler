# Mod Builder

Десктоп-программа для Windows (C++ / Dear ImGui) с **двумя компиляторами в одном окне**.
Она запускает сборку в GitHub Actions, ждёт результат и показывает:

- при ошибке: вкладку **Проблемы** (файл, строка, сообщение, детали) и полный **Вывод**;
- при успехе: скачивает результат в папку `downloads/` рядом с программой.

Программа сама ничего не компилирует: код должен уже лежать на GitHub (push), она запускает workflow.

| Вкладка | Что собирает | Что разбирает в ошибках | Результат |
|---|---|---|---|
| **Мод (Java)** | мод для Minecraft (Gradle) | javac, Gradle | `.jar` |
| **C++** | любой проект на CMake | gcc/clang, MSVC (`Cxxxx`), линкеры (`undefined reference`, `LNKxxxx`), ошибки CMake | `.exe`, `.dll`, `.lib` |

У каждой вкладки свои настройки (репозиторий, workflow, ветка, токен), поэтому можно держать две разные цели одновременно.

## Как собрать саму программу

1. Создай репозиторий на GitHub и залей туда эту папку.
2. Открой вкладку **Actions** → **Build ModBuilder** → когда закончится, скачай артефакт `ModBuilder-windows` (`ModBuilder.exe`).

## Как подключить проект

1. Положи шаблон в репозиторий проекта как `.github/workflows/build.yml`:
   - для мода: `docs/mod-build.yml`;
   - для C++: `docs/cpp-build.yml` (нужен `CMakeLists.txt` в корне).
   В workflow обязательно должно быть `on: workflow_dispatch`.
2. На GitHub: Settings → Developer settings → Personal access tokens → Fine-grained tokens.
   Выбери репозиторий проекта, права: **Actions: Read and write**, **Contents: Read**.
3. В программе открой нужную вкладку, заполни владельца, репозиторий, файл workflow (`build.yml`), ветку и токен → «Сохранить» → «Собрать».

Токены хранятся в `%APPDATA%\ModBuilder\config.json` в зашифрованном виде (Windows DPAPI).

## Тесты парсера

На Linux: `cmake -S . -B build && cmake --build build && ./build/logparse_test`
