# vengine

Минимальный проект C++23 для Linux x86_64: Clang, Sokol, OpenGL 3.3 Core, Python и Ninja.
Окно 1280×720 очищается каждый кадр. Геометрии и пользовательских шейдеров нет.

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
