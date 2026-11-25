---
title: "ADR-0029: Lectura de discos CHD con libchdr"
status: "Accepted"
date: "2026-11-22"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["chd", "libchdr", "sega-cd", "retroachievements", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0029: Lectura de discos CHD con libchdr

## Status

Accepted.

## Context

Los `.chd` eran una caja negra para Xfusion: solo se comprobaba que empezaran por `MComprHD` y se dejaba todo al core. Eso impedía mostrar la región y el título en la biblioteca, avisar de un BIOS ausente antes de cargar, distinguir un CHD de Sega CD de uno de otra consola y, sobre todo, identificar el disco para RetroAchievements (rcheevos no lee CHD). La Fase 7 estimó el coste en un lector propio y libchdr; el propietario pidió continuar.

## Decision

1. **libchdr** (BSD-3, Romain Tisserand) vive en `third_party/libchdr/`: la rama principal de `rtissera/libchdr` (commit `47d7941`), sin el respaldo micro-flac (se usa `dr_flac`, que viene dentro) y con **zlib y zstd del sistema** (`CHDR_SYSTEM_ZLIB`, `CHDR_SYSTEM_ZSTD`); solo se compila el decodificador de LZMA del SDK (dominio público). Se compila dentro de `xfusion_input`, así que las pruebas también lo ven. A diferencia de la versión que lleva Genesis Plus GX, esta lee también los CHD comprimidos con zstd.
2. **`ChdDisc`** envuelve libchdr: abre la imagen (con `_wfopen` en Windows, para rutas con acentos), lee los metadatos de pistas (`CHT2` y `CHTR`) y devuelve los datos de usuario (2048 bytes) de un sector de una pista, con una caché de un «hunk». Entiende `MODE1`, `MODE1_RAW`, `MODE2_RAW` y las pistas de audio.
3. **Disco:** `readDisc` abre el CHD, cuenta pistas de datos y de audio, lee el primer sector de la primera pista de datos y, si empieza por `SEGADISCSYSTEM`, saca el volumen, el título, el producto y la región como con un `.iso`. Un CHD que libchdr no puede leer se sigue tomando por Sega CD y se deja al core, como antes. La biblioteca subió su versión de escaneo (4) para releer los CHD ya listados.
4. **RetroAchievements:** `AchievementsDisc` instala un lector de CD para rcheevos: un `.chd` se abre con `ChdDisc` y todo lo demás pasa al lector que trae rcheevos. Un manejador que viene del CHD se distingue por un conjunto de punteros, no por su contenido.

## Consequences

- Positivas: probado con los tres `.chd` locales: Dracula Unleashed (discos 1 y 2: título, producto `MK-4420 -00`, USA, 1 pista de datos y 1 de audio) y Final Fight CD (USA, 1 de datos y 25 de audio) se leen bien, y con la cuenta de pruebas RetroAchievements los identifica: Final Fight CD 33 logros (343 puntos) y Dracula Unleashed 35 (200 puntos). Una prueba que lee un CHD real si `XFUSION_TEST_CHD` apunta a uno (no se puede construir un CHD sin `chdman` ni hay discos en el repositorio).
- Negativas: unos 1,2 MB más de fuente de terceros y unos 300 KB más en el ejecutable; la lectura de pistas con pregap virtual o de varias sesiones solo se supuso (no hay discos locales así); un CHD de otro sistema deja de aparecer como Sega CD.
- Sin hacer: leer el audio de las pistas, y usar `ChdDisc` para comprobar el BIOS que pide un `.chd` antes de cargar (ya hay región, falta enlazarlo en `prepareDisc`).
