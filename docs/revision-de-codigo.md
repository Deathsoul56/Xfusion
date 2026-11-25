# Revisión de código (2026-11-25)

Qué se miró, qué se encontró y qué se hizo. No cambia el comportamiento: las 170 pruebas pasan y los seis sistemas abren, guardan y cargan un estado y cierran sin dejar el proceso vivo.

## Cómo se miró

- Compilación con `-Wall -Wextra -Wpedantic -Wshadow -Wunused -Wdeprecated -Wold-style-cast -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough`.
- `clang-tidy` sobre `src/` con las comprobaciones de rendimiento, de código redundante, de modernización y de errores comunes (`performance-*`, `readability-redundant-*`, `modernize-*`, `bugprone-*`, `misc-unused-*`).
- Búsqueda de funciones declaradas y nunca llamadas, de `#include` que sobran (a mano y quitando cada uno para ver si compila), de recursos sin uso, de archivos fuera de CMake y de texto desfasado en la documentación.

## Resultado

- **Avisos propios del compilador:** 3 (todos eran de constantes de rcheevos o de una función sin usar). Sin API deprecada de Qt. Los de `third_party/` se dejan: es código ajeno sin tocar.
- **Código muerto quitado:** `Achievements::gameLoaded`, `LibretroCore::discIndex` y `scaled()` de `PresetRenderer`.
- **Includes que sobraban:** unos 24 de Qt y de la biblioteca estándar, y 53 de cabeceras propias en los cuatro archivos de `MainWindow`.
- **Rendimiento:**
  - `PresetRenderer` comparaba con expresiones regulares el nombre de cada textura y de cada miembro de los bloques de uniformes en cada fotograma. Ahora se clasifican una vez al cargar el preset y el fotograma solo lee una tabla; también reutiliza el búfer de uniformes y reserva los vectores. El resultado visual es idéntico (`crt-royale` y `hq4x` sin ninguna diferencia de píxeles).
  - Se probó cambiar la espera de la cola de audio de `EmuThread` (de sondear cada 0,5 ms a dormir en proporción al exceso) y **no cambió el consumo** (mediana 13,9 % frente a 14,1 %), así que se dejó como estaba.
- **Estructura:** `MainWindow.cpp` (1943 líneas) se partió por las secciones que ya tenía: `MainWindow.cpp` (ventana, menús, atajos, sesión), `MainWindowGames.cpp` (abrir y cerrar juegos, parches, discos, saves), `MainWindowStates.cpp` (estados guardados) y `MainWindowView.cpp` (ajustes, imagen, audio, shaders, logros, pausa).
- **Pequeñas correcciones:** `wide ? 5 : 5` en `Patch.cpp`, `count()` por `contains()`, conversiones explícitas de `qsizetype` a `int`, casts redundantes.
- **Documentación:** las notas de los ADR-0011, 0013 y 0019 dicen en qué las cambiaron los ADR-0030, 0031 y 0032; el plan describe los ajustes por sistema; `.gitignore` incluye `build-lint/` y `build-dbg/`.

## Lo que se miró y se dejó como está

- `MainWindow.h` sigue siendo una clase grande: dividirla en varias (por ejemplo una para las discos o la de los estados) sacaría lógica de la ventana pero es un cambio de diseño, no de limpieza.
- Los iconos `check`, `arrowup` y `arrowdown` de `resources.qrc` parecen sin uso, pero la hoja de estilos los pide con `@SHADE@` (blanco o negro): se usan.
- `scripts/porting-log.ps1` y `scripts/stress-ui.ps1` están referidos en la documentación: se mantienen.
- `FormAlign.h` no figura en CMake porque solo tiene cabecera: no hace falta.
- `third_party/`: `rcheevos` ya va sin el kit del editor; el resto está recortado a lo que se usa.

## Tamaño del ejecutable

`xfusion.exe` bajó de 41,3 a 32,0 MB: sin el compilador de shaders (aparte, en `xfusion-shaderc.exe`) y sin OpenSSL. Detalle en el ADR-0032.
