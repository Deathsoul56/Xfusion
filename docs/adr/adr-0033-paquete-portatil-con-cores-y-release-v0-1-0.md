---
title: "ADR-0033: Paquete portátil con los cores incluidos y publicación v0.1.0"
status: "Accepted"
date: "2026-11-25"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["distribucion", "paquete", "cores", "release", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0033: Paquete portátil con los cores incluidos y publicación v0.1.0

## Status

Accepted. Cambia el punto 2 del ADR-0030 (que dejaba los cores fuera del paquete) y aplica lo que ya decía el ADR-0006.

## Context

El ADR-0030 dejó los cores fuera del paquete y el primer arranque ofrecía descargarlos. El propietario pidió que el paquete sea portátil de verdad: descomprimir en otro equipo y abrir, con todo listo. El ADR-0006 ya aprobó incluir los cores sin modificar, fijados a su commit, con sus licencias y sin vender el paquete ni usarlo comercialmente. Los cores compilados (`genesis_plus_gx_libretro.dll`, `picodrive_libretro.dll`) solo importan DLL del sistema (UCRT y `KERNEL32`), así que no arrastran nada de MSYS2; `xfusion.exe` tampoco (Qt, SDL3 y el runtime van enlazados estáticamente).

## Decision

1. **El zip trae los dos cores en `cores\`**, tal como los compilan `scripts\setup-genesis-plus-gx.ps1` y `scripts\setup-picodrive.ps1` desde el commit que fija cada uno. `scripts\package.ps1` se niega a empaquetar si el clon no está en ese commit, tiene cambios, no está compilado o le falta una licencia.
2. **Licencias y fuentes:** `licenses\cores\<core>\` lleva la licencia de cada core y de sus componentes (libchdr, lzma, zstd, zlib, minimp3, tremor, emu2413, dr_libs, NTSC); `cores\SOURCES.txt` da el repositorio, el commit y el SHA-256 de cada DLL. Los cores no se modifican.
3. **Contenido del zip:** `xfusion.exe`, `xfusion-shaderc.exe`, `cores\`, `licenses\`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `README.md`, `CHANGELOG.md`, `docs\` y `LEEME.txt`. No hay BIOS ni juegos.
4. **Sigue siendo portátil** (ADR-0030): los datos van junto al ejecutable, o en `%LOCALAPPDATA%\Xfusion` si no se puede escribir. Los cores del paquete son el «archivo local» del ADR-0027: sirven mientras no haya una versión activa descargada con *Manage Cores*, que puede sustituirlos.
5. **Publicación:** la v0.1.0 sale como release de GitHub con el zip y su `.sha256`, desde el commit de `main` etiquetado `v0.1.0`. La versión sale de `project(Xfusion VERSION ...)`.

## Consequences

- Positivas: se descomprime y se juega (solo faltan los juegos y, para Sega CD, la BIOS del usuario); el origen de cada core queda trazado.
- Negativas: el zip hereda la restricción no comercial de los cores (NEG-001 del ADR-0006): no se vende ni se monetiza. Pesa más (los cores suman unos 11 MB sin comprimir). Al actualizar un core hay que subir el commit en el script de instalación y publicar otra versión.
- Sin hacer: firma de código, instalador, y probar en un equipo limpio distinto de este (se probó en una carpeta nueva con caracteres no ASCII, sin las variables ni las DLL de MSYS2).
