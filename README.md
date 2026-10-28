# vengine

Минимальный проект C++23 для Linux x86_64: Clang, Sokol, OpenGL 3.3 Core, Python и Ninja.
Окно 1280×720 очищается каждый кадр. Геометрии и пользовательских шейдеров нет.

## Зависимости

Нужны Python 3.8+, Clang с поддержкой C++23, Ninja, Git, LLD для Release,
заголовки стандартной библиотеки C++, runtime AddressSanitizer/UndefinedBehaviorSanitizer,
библиотеки разработки X11, Xi, Xcursor и OpenGL. Для запуска нужны X11 или XWayland
и драйвер с поддержкой OpenGL 3.3 Core.

Ubuntu/Debian:

```bash
sudo apt install python3 clang lld ninja-build git build-essential libclang-rt-dev libx11-dev libxi-dev libxcursor-dev libgl1-mesa-dev
```

Sokol хранится в `vendor/sokol` как Git submodule. Закреплена ревизия
`0f667484176fa3d4f9377243f37f4cabf6490a55`, поддерживающая OpenGL 3.3.
Современные версии Sokol могут требовать более новую версию OpenGL.
Системные инструменты, стандартная библиотека и графические библиотеки устанавливаются отдельно;
единственная сторонняя исходная зависимость проекта хранится в `vendor/`.

После клонирования:

```bash
git submodule update --init --recursive
```

## Сборка и запуск

```bash
python3 build.py debug
python3 build.py release
python3 build.py run debug
python3 build.py run release
python3 build.py clean
```

Команды можно выполнять из любого рабочего каталога, указав путь к `build.py`.
`run` сначала выполняет инкрементальную сборку. Закрытие окна завершает приложение.
Ошибки инструментов, сборки и запуска возвращают ненулевой код.

Debug: `-O0 -g3 -DDEBUG -Wall -Wextra`, AddressSanitizer и UndefinedBehaviorSanitizer.
Release: `-O3 -DNDEBUG -flto=thin`, LLD, удаление неиспользуемых секций и оптимизация линковки.

Результаты независимы: `build/debug/vengine` и `build/release/vengine`.
Скрипт рекурсивно находит `src/**/*.cpp`, генерирует отдельный `build.ninja` для каждой
конфигурации и запускает Ninja с автоматическим параллелизмом. Clang depfiles отслеживают
заголовки; инкрементальную сборку полностью выполняет Ninja.
Неизменённый манифест не перезаписывается. Без изменений компиляция и линковка не запускаются.
Добавление и удаление исходников обновляет список объектов и вызывает перелинковку.
Старые объектные файлы удалённых исходников не участвуют в линковке; `clean` удаляет их вместе со сборками.
Реализации Sokol компилируются только в `src/sokol_impl.cpp`.
