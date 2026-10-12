# Comfy Third Person (NeoForge 1.21.1, клиентский мод)

Возможности:
- Alt + колесо мыши: приблизить/отдалить камеру в 3-м лице
- V: сменить плечо (камера через правое/левое плечо/по центру)
- B: сбросить камеру
- F5 больше не показывает вид спереди (отключается в конфиге)
- Настройки: config/comfythirdperson-client.toml

## Сборка
1. Скачай MDK: https://github.com/NeoForgeMDKs/MDK-1.21.1-ModDevGradle (Use this template / Download ZIP)
2. Из MDK возьми папки `gradle/` и файлы `gradlew`, `gradlew.bat`.
3. Положи их в эту папку рядом с build.gradle.
4. Запусти: `./gradlew build` (Windows: `gradlew.bat build`)
5. Готовый jar: build/libs/comfythirdperson-1.0.0.jar -> в папку mods.
Для теста в игре: `./gradlew runClient`.
