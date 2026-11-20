---
title: "ADR-0014: Segundo jugador y asignación de mandos"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["input", "settings", "sdl3", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0014: Segundo jugador y asignación de mandos

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 como parte de la Fase 2 (completa el punto pendiente del ADR-0010: "dos jugadores").

## Context

El ADR-0010 dejó un solo jugador: un conjunto de bindings por grupo de plataforma, válido para cualquier mando conectado. Los sistemas objetivo tienen dos puertos (Genesis, Sega CD, 32X, Master System; Game Gear y SG-1000 usan el primero), y PicoDrive los expone como puertos libretro 0 y 1. Con dos jugadores, "cualquier mando" ya no sirve: el botón 0 del mando A y el del mando B deben ir a jugadores distintos.

## Decision

1. **Bindings por grupo y por jugador** (`InputConfig::bindings(grupo, jugador, botón)`, `kPlayers = 2`). Los botones lógicos (`PadButton`) no cambian; `LibretroCore::setButton(jugador, botón, pulsado)` los envía al puerto del jugador.
2. **Conflictos:** una tecla solo mueve una acción del grupo, sea de uno u otro jugador (el teclado es compartido). Un botón, hat o eje de mando solo puede estar en una acción del mismo jugador, porque cada jugador tiene su propio mando. `assign` devuelve la acción desplazada (jugador y botón) para avisar al usuario.
3. **Cada jugador lee un solo mando** (`InputManager::assignDevices`). Por nombre, si el usuario lo fijó en el combo `Gamepad` (`InputConfig::device(jugador)`, el nombre que informa SDL); en `Automatic`, el siguiente mando conectado que nadie eligió por nombre, en orden de conexión. Dos mandos con el mismo nombre se distinguen por ese orden. Un nombre que no coincide con ningún mando conectado deja al jugador sin mando. Con un único mando, el jugador 1 lo recibe.
4. **Teclado por defecto del jugador 2:** `I`/`J`/`K`/`L` para la cruz y la fila de números para los botones (Genesis: `7`/`8`/`9` = A/B/C, `4`/`5`/`6` = X/Y/Z, `0` Start, `3` Mode; Master System: `7`/`8`, `0`). Ninguna coincide con las del jugador 1 ni con los atajos por defecto (prueba `eachPlayerHasKeyboardDefaultsAndTheyNeverOverlap`).
5. **Formato `config\input.json` versión 2:** `groups.<grupo>.players[0..1]` con las acciones y `devices[0..1]`. La versión 1 se sigue leyendo: sus acciones son las del jugador 1 y el jugador 2 queda con los valores por defecto.
6. **Interfaz:** la página Controllers añade el combo `Player` y el combo `Gamepad`; la tabla, el diagrama iluminado y Auto-Assign actúan sobre el jugador elegido. Auto-Assign usa el mando de ese jugador (sin mando, lo dice). "Restaurar valores" restaura al jugador visible, no al otro ni el tipo de mando de 3/6 botones, que es común a los dos puertos.
7. **Pruebas:** con joysticks virtuales de SDL (cada jugador lee su mando, elegir por nombre aunque se conectara antes, un mando suelto es del jugador 1, versión 1 del archivo) y, en la aplicación real, con una ROM sintética que lee los dos puertos (`scripts\make-test-rom.ps1`): flecha arriba del teclado = rojo (jugador 1) y `I` = verde (jugador 2).

## Nota (2026-11-20, ADR-0018)

Se añaden "Controller Ports" (uno o dos mandos), las filas Turbo, "Swap Controllers 1 and 2", los menús Edit y Auto-Assign por columna y la etiqueta "raw SDL".

## Consequences

- Positivas: dos personas pueden jugar con un teclado, o con dos mandos sin tocar nada; el formato anterior no se pierde.
- Negativas: el reparto automático depende del orden de conexión de SDL; con un dispositivo que SDL trate como mando sin serlo (un receptor, por ejemplo) el usuario tiene que fijar el mando por nombre. Los dos puertos comparten el tipo de mando de 3/6 botones.
- No probado con dos mandos físicos distintos; sí con joysticks virtuales y con el 8BitDo (jugador 1).
