---
title: "ADR-0016: Identidad del juego, nombres de save y SRAM"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["saves", "persistence", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0016: Identidad del juego, nombres de save y SRAM

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 como parte de la Fase 3. Concreta las reglas de la sección 3.1 del plan y el ADR-0002 (saves por sistema).

## Context

Hasta ahora el save se llamaba como el archivo del juego (`<nombre>.srm`) dentro de la carpeta del sistema. Dos juegos distintos del mismo sistema con el mismo nombre de archivo (dos hacks, o `Sonic.md` en dos carpetas) compartían save y se sobrescribían; la SRAM solo se guardaba al cerrar, así que un cierre brusco la perdía; y un fallo de lectura o escritura no se trataba con cuidado.

## Decision

1. **Huella del juego** (`GameSaves::fingerprint`): SHA-1 del tamaño más el primer y el último megabyte del archivo (rápido incluso con una imagen de disco). Un `.cue` incluye además los archivos que cita (`FILE "..."`), porque muchas hojas de pista son idénticas palabra por palabra. El zip se trata como un archivo.
2. **Un nombre pertenece a un juego.** Cada carpeta de sistema tiene un índice `saves\<sistema>\saves.json` (nombre → huella, escrito con `QSaveFile`). Orden de decisión: el nombre que ya posee esa huella (así los saves siguen al juego si se renombra o se mueve); si no, el nombre plano si nadie lo tiene; si no, `<nombre> (<8 hex>)`, ampliando la huella si hace falta. La comparación no distingue mayúsculas, como el sistema de archivos de Windows. El nombre se reclama en la primera escritura (SRAM o estado), no al abrir, para no dejar entradas de juegos que no guardan nada.
3. **Compatibilidad hacia atrás:** un `.srm` sin entrada en el índice se considera del juego con ese nombre. Un índice ilegible se conserva como `saves.json.bad`, se avisa y se empieza otro.
4. **Nombres de archivo seguros:** `sanitizedName` sustituye `<>:"/\|?*` y controles, quita puntos y espacios finales, evita nombres de dispositivo de Windows (`CON`, `NUL`, `COM1`...) y limita la longitud.
5. **Carpetas bajo demanda:** `saves\<sistema>\` se crea al abrir un juego de ese sistema (el core también escribe ahí) y `states\<sistema>\` solo al guardar un estado. El sistema lo decide la tabla de `SystemInfo`, no el core.
6. **SRAM:** se lee al abrir y se escribe cada 10 s si cambió y al cerrar. Si el `.srm` no se puede leer, o su tamaño no coincide con el que pide el core, el juego arranca sin él y **no se sobrescribe en esa sesión** (escribir la RAM del core encima destruiría la única copia). PicoDrive informa tamaño 0 mientras la SRAM es todo ceros: una vez que hubo SRAM, un informe vacío se interpreta como "borrada" y se guarda en ceros. Un fallo de escritura se avisa siempre al cerrar y, jugando, la primera vez; se reintenta cada 10 s.
7. **Separación de responsabilidades:** `LibretroCore` solo entrega y recibe bytes de SRAM (`saveRam`, `setSaveRam`); rutas, nombres y archivos viven en `GameSaves` (sin Qt Widgets, probado en `tests\SaveTests.cpp`); la ventana decide cuándo.

## Nota (2026-11-20, ADR-0018)

La carpeta de saves, de estados y de capturas se elige en Settings > Files (datos, carpeta del juego o propia). El intervalo de escritura de la SRAM es ahora un ajuste (30 s por defecto).

## Consequences

- Positivas: dos juegos con el mismo nombre nunca se pisan, en un sistema ni entre sistemas (carpetas distintas); un cierre brusco pierde como mucho 10 s; ningún fallo deja un save destruido en silencio.
- Negativas: un juego modificado en su sitio (parche) tiene otra huella y empieza con save nuevo; la huella ignora la parte central del archivo (dos archivos que solo difieran ahí contarían como el mismo juego); los archivos que escribe el propio core en la carpeta de saves (la copia interna de Sega CD) no pasan por el índice.
- Verificado en la aplicación real con una ROM propia (`scripts\make-test-rom.ps1`): autoguardado con el juego en marcha y recuperación tras matar el proceso, dos juegos con el mismo nombre en un sistema, el mismo nombre en Genesis y Game Gear.
