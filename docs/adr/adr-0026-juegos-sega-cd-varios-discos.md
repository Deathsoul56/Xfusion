---
title: "ADR-0026: Juegos de Sega CD en varios discos"
status: "Accepted"
date: "2026-11-21"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["sega-cd", "multidisc", "libretro", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0026: Juegos de Sega CD en varios discos

## Status

Accepted.

## Context

Algunos juegos de Sega CD vienen en varios discos (Dracula Unleashed, Sherlock Holmes Vol. II, Ground Zero Texas, Prize Fighter, Supreme Warrior, Night Trap, Slam City con 4 discos, Fahrenheit y los dos Ecco japoneses; lista del DAT de Redump). El juego pide cambiar de disco en medio de la partida. Hasta ahora Xfusion trataba cada disco como un juego distinto, así que no se podía cambiar. Genesis Plus GX y PicoDrive implementan la interfaz de control de disco de libretro (`SET_DISK_CONTROL_INTERFACE`): sacar el disco, elegir otro y cerrar la bandeja.

## Decision

1. **Qué discos forman un juego:** los archivos de la misma carpeta con la misma extensión y el mismo nombre salvo una marca `(Disc N)` (nombres de Redump; también `(Disk N)`, `(CD N)` y `(Disc N of M)`), ignorando un título de disco detrás de la marca (`(Disc 2) (Juice)`). `discSetOf` los devuelve en orden de disco; con un solo disco no hay conjunto.
2. **Cómo se cargan:** se abre el disco que el usuario eligió y los demás se añaden detrás con `add_image_index` y `replace_image_index` (como hace RetroArch), sin archivos `.m3u`. Funciona con `.cue`, `.chd` e `.iso`.
3. **Cómo se cambia:** el menú Emulation > Disc (solo visible cuando el juego tiene varios discos) hace `set_eject_state(true)`, `set_image_index(n)` y `set_eject_state(false)` en el hilo del core, muestra `Disc N: <nombre>` y cambia el título de la ventana.
4. **Biblioteca (2026-11-22):** los discos de un juego salen como una entrada, `Juego [N discs]`, que abre el disco 1 (`parseDiscTag`, `groupDiscSets`); el tamaño es el del juego entero.
5. **Saves y estados:** siguen siendo del disco con el que se abrió el juego (cada disco tiene su huella); la RAM de respaldo de Sega CD es compartida por región, así que las partidas guardadas sirven en todos los discos.

## Consequences

- Positivas: probado con Dracula Unleashed (USA): el disco 2 pide `PLEASE INSERT DRACULA UNLEASHED DISK 1`; al elegir el disco 1 en el menú y pulsar un botón el juego continúa desde el disco 1. 1 prueba nueva de la detección de conjuntos.
- Negativas: los estados de partida no se comparten entre los discos de un juego; solo se buscan los discos en la misma carpeta (no en carpetas hermanas) y con el mismo formato.
- Las limitaciones, con su detalle, están en `docs/sega-cd.md` (8) y en el README.
- Sin hacer: un atajo para cambiar de disco, y abrir un juego de varios discos con un `.m3u` propio.
