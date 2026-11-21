---
title: "ADR-0022: Reconocimiento de juegos de Sega CD por su contenido"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["sega-cd", "library", "bios", "recognition", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0022: Reconocimiento de juegos de Sega CD por su contenido

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20 (Fase 5, parte de Sega CD y regiones).

## Context

Hasta ahora el sistema de un archivo se deducía de su extensión, y eso no basta para los discos. Con los cuatro discos locales (`Roms/CD`) se vio que hay tres formas de distribuir un juego de Sega CD:

| Juego | Forma | Datos |
|---|---|---|
| Sonic CD, Mortal Kombat, Final Fight CD | un `.iso` más pistas `.mp3` numeradas ("Sonic CD 02.mp3", ...) | sectores de 2048 bytes; "SEGADISCSYSTEM" en el byte 0 |
| Shin Megami Tensei (Japan) | un `.cue` más 29 `.bin` (uno por pista) | sectores de 2352 bytes; sincronía y "SEGADISCSYSTEM" en el byte 16 |
| (sin ejemplo local) | un `.chd` | comprimido; no se puede mirar dentro sin una biblioteca |

Problemas encontrados: un `.bin` es a la vez un cartucho de Genesis y una pista de disco (la regla por extensión los mezclaba); un `.iso` puede ser de otra consola; el `.cue` es el juego y los `.bin` solo sus partes; cada disco exige el BIOS de su región y ese BIOS tiene que llamarse `bios_CD_J/E/U.bin`, mientras que los volcados traen cualquier nombre. La cabecera del primer sector (la misma de un cartucho, en el 0x100 de los datos) da título, producto y región.

## Decision

1. **Se identifica por el contenido, en capas.** La extensión solo propone candidatos (`cue`, `iso`, `bin`, `chd`). `systemForFile` lee entonces el primer sector: con "SEGADISCSYSTEM" (en 0, 16 o 24 según el formato) es Sega CD; un `.bin` que no lo tiene es un cartucho de Genesis; un `.iso` o un `.cue` que no lo tienen no son de Sega CD y la biblioteca los ignora. Un `.chd` se acepta por su marca `MComprHD` (no se inspecciona).
2. **El `.cue` es el juego.** Sus pistas (`FILE ...`) no son juegos; se cuentan las pistas de datos y de audio y se avisa de cada archivo que el cue nombra y no existe. Para un `.iso`, el audio son los archivos `<nombre> NN.mp3|wav|ogg|flac` (NN desde 02) de su carpeta.
3. **Metadatos de la cabecera**, para todos los sistemas: título, producto y región (letras J/U/E o dígito hexadecimal, como lo hacen los emuladores de referencia; la letra gana a la cifra). Para Master System y Game Gear, el nibble del byte 0x7FFF y el código BCD. Los zip se leen por dentro (zlib; primer archivo con extensión de juego). La biblioteca muestra la región y la lista el título en el tooltip; el tamaño de un disco es el de todos sus archivos.
4. **El BIOS se reconoce por contenido, no por nombre** (`Bios.cpp`): 128 KB, cabecera Mega-CD con "BOOT ROM" y una sola región. `Find BIOS Files...` (Options > Files, y el aviso al abrir un disco) copia los que faltan como `bios_CD_J/E/U.bin`; el más nuevo si hay varios; nunca sustituye uno válido y guarda como `.old` un archivo que estorbe.
5. **Antes de cargar un disco** se comprueba que haya un BIOS para alguna de sus regiones; si no, se dice cuál falta y dónde va, con el botón para buscarlos. Las pistas que faltan se avisan sin impedir el juego.
6. **La identidad para saves no cambia** (huella del archivo, ADR-0016): usar el código de producto habría dejado huérfanos los saves existentes y un `.cue` y un `.iso` del mismo juego tendrían los mismos saves por casualidad.

## Consequences

- Positivas: los cuatro discos locales se reconocen como Sega CD con su región (Europe, Europe, Japan, USA) y título; un `.bin` suelto de disco ya no se lista como Genesis; el BIOS del usuario, con nombres de catálogo, se reconoce y se instala; 510 de las 625 ROMs locales muestran región. 16 pruebas nuevas (`disc_tests`) y 1 de biblioteca.
- Negativas: los 115 juegos sin región son zips sin cabecera legible, SG-1000 y volcados raros; un `.chd` no se puede comprobar (ni su región, así que no se avisa de un BIOS ausente); no se valida el contenido del BIOS más allá de la cabecera; un `.bin` de disco sin `.cue` se reconoce pero el core no lo carga (hace falta el `.cue`).
- Observado y sin resolver: los tres `.iso` arrancan y llegan a la pantalla "Produced by or under license from Sega"; Sonic CD continúa con su animación, Final Fight CD se queda en negro después y Mortal Kombat no se verificó más allá. El `.iso` de Final Fight CD mide 24 561 712 bytes, que no es múltiplo de 2048, así que puede estar mal volcado; sin un `.cue` de referencia no se pudo comprobar. Pendiente de la Fase 5: juegos multidisco y validación de la jugabilidad por disco.