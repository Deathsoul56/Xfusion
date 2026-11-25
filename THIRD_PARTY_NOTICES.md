# Avisos de terceros

Esta lista cubre el código de terceros que vive dentro del repositorio. Qt, SDL3 y PicoDrive se enlazan o se
cargan en tiempo de ejecución y tienen su propio apartado pendiente (ver `frontend_sega_multiemulador.md`,
sección 10.3). Esto es una nota técnica, no asesoría legal.

## Filtros gráficos (`third_party/`)

| Componente | Carpeta | Licencia | Origen |
|---|---|---|---|
| xBRZ (2xBRZ a 6xBRZ) | `third_party/xbrz/` | GPL-3.0 (`LICENSE.txt`), © Zenju | Copia del árbol `filter/` de `snes9xrd`, que a su vez la toma del proyecto xBRZ. Solo se cambió `xbrz.h`: se quitó `#include "port.h"` y se definió `alwaysinline`. Xfusion es GPL-3.0-or-later; el binario que contiene xBRZ queda bajo GPL-3.0. |
| snes_ntsc 0.2.2 (filtros NTSC) | `third_party/snes_ntsc/` | LGPL-2.1 (`LICENSE.txt`), © Shay Green (Blargg) | Copia del árbol `filter/` de `snes9xrd`. Solo se cambió `snes_ntsc_config.h`: entrada y salida RGB565. |
| hqx (HQ2x, HQ3x, HQ4x) | `third_party/hqx/` | ISC, según la cabecera del archivo (© Clément Bosch, de FFmpeg; diseño original de Maxim Stepin) | Copia de `filter/hq2x.cpp` de `snes9xrd`, que ya trae una adaptación a RGB565 hecha para Snes9x. Cambios aquí: sin `port.h`, tabla YUV con el orden de canales de Qt y un `hq2x.h` propio. **Pendiente de verificar:** quién escribió esa adaptación a RGB565; la cabecera declara ISC, pero la procedencia de ese cambio no está comprobada. |

## RetroAchievements (`third_party/rcheevos/`)

| Componente | Carpeta | Licencia | Origen |
|---|---|---|---|
| rcheevos (cliente, API, hash y mapa de memoria de libretro) | `third_party/rcheevos/` | MIT (`LICENSE`), c RetroAchievements.org | Copia, sin cambios, del submódulo `external/rcheevos` de `snes9xrd` (commit `2ad0b86`). Se dejaron fuera la integración con RAIntegration y el cliente externo, que un jugador no necesita. Incluye `src/rhash/aes.c`, tiny-AES-c (Unlicense, dominio público), que Xfusion usa para cifrar el token de la sesión. |

## Shaders

| Componente | Licencia | Nota |
|---|---|---|
| Qt Shader Tools (con glslang y SPIRV-Cross) | GPL-3.0 / LGPL-3.0 (Qt), BSD-3 y Apache-2.0 (los compiladores que lleva) | Enlazado estáticamente en xfusion.exe para compilar los presets de usuario. |
| Shaders propios (src/shaders/) | GPL-3.0-or-later como el resto | Cinco shaders hechos para Xfusion. |
| Presets y shaders de RetroArch (slang-shaders) | Varias, por shader | **No se distribuyen**; los pone el usuario. |

## Discos CHD (`third_party/libchdr/`)

| Componente | Carpeta | Licencia | Origen |
|---|---|---|---|
| libchdr | `third_party/libchdr/` | BSD-3 (`LICENSE.txt`), c Romain Tisserand | Copia de `rtissera/libchdr` (commit `47d7941`), sin el respaldo micro-flac ni los directorios `deps/miniz` y `deps/zstd` (se usan zlib y zstd del sistema). Incluye `include/dr_libs/dr_flac.h` (dr_flac, de David Reid: dominio público o MIT-0 a elegir). |
| LZMA SDK (solo el decodificador) | `third_party/libchdr/deps/lzma-26.02/` | Dominio público (`LICENSE`), Igor Pavlov | Copia del árbol de libchdr. |

## Cores (no están en el repositorio)

| Core | Licencia | Origen |
|---|---|---|
| PicoDrive | Propia, no comercial (`COPYING`, ADR-0006) | `scripts/setup-picodrive.ps1`, commit `26ecb2b` |
| Genesis Plus GX | Propia, © Charles MacDonald y Eke-Eke; las redistribuciones no pueden venderse ni usarse comercialmente (`LICENSE.txt` del repositorio) | `scripts/setup-genesis-plus-gx.ps1`, commit `4fd32ef`. Incluye componentes con licencia propia (libchdr, zstd, lzma, zlib, minimp3, tremor) que hay que listar en cada paquete. Esto es una nota técnica, no asesoría legal. |

## Bibliotecas enlazadas

zlib (licencia zlib) se enlaza, a través de Qt, y se usa directamente en `src/ZipRead.cpp` para leer la cabecera de los juegos dentro de un `.zip`.

## Iconos (`src/resources/icons/`)

| Componente | Carpeta | Licencia | Origen |
|---|---|---|---|
| Material Icons (apertura, refrescar, reinicio, salir, pausa, jugar, pantalla completa, ajustes, pantalla, sonido, memoria, mando, teclado, carpeta, marca, flechas, rayo) | `src/resources/icons/` | Apache-2.0 (`LICENSE.txt`), © Google | Descargados del repositorio `google/material-design-icons` (versión `materialicons/24px.svg` de cada icono). Sin cambios, salvo `checkwhite`, `checkblack`, que son `check` con relleno fijo. Los archivos `arrow*` y las letras de los botones del mando son propios (se dibujan en `Icons.cpp`). |

Los iconos de `snes9xrd` no se copiaron: los que no son de Material llevan la licencia Snes9x, y los diagramas de botones del SNES no sirven para un mando SEGA.

Los filtros Simple 2x/3x/4x, Scanlines, Sharp Bilinear 4x, EPX, Super Eagle, 2xSaI y Super 2xSaI están
escritos para Xfusion (`src/VideoFilters.cpp`) a partir de las ideas publicadas de cada algoritmo, sin código de
Snes9x (cuya licencia no es compatible con GPL-3). 2xSaI sigue las reglas de decisión conocidas; Super Eagle y Super
2xSaI son variantes propias, no copias bit a bit de los originales.
