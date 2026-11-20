---
title: "ADR-0017: Estados guardados, slots y deshacer"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["states", "persistence", "libretro", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0017: Estados guardados, slots y deshacer

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 como parte de la Fase 3.

## Context

libretro ofrece `retro_serialize_size`, `retro_serialize` y `retro_unserialize`, pero lo que producen solo vale para el mismo core, versión y juego, y un estado dañado o ajeno puede dejar el juego roto. El plan pide estados con slots, aviso al cambiar de versión del core y deshacer la carga.

## Decision

1. **Soporte condicionado:** los tres símbolos se resuelven sin hacerlos obligatorios; `LibretroCore::supportsStates()` es verdadero con un juego cargado y tamaño de estado mayor que cero. Se consulta una vez al abrir el juego (antes de arrancar el hilo de emulación) y desactiva el menú si no hay soporte. Todas las operaciones con el core corren dentro de `EmuThread::withCore`, también con el juego en pausa.
2. **Diez slots por juego** en `states\<sistema>\<juego>.slotN.state`, con el mismo nombre que decide `GameSaves` (ADR-0016). Menú `State` y atajos (ADR-0015). El slot actual vale para guardar y cargar; cambiarlo muestra la fecha del estado o "empty".
3. **Formato propio** (`StateFile`): `XFST`, versión de formato, cabecera JSON (core, nombre y versión del core, huella del juego, sistema, fecha, tamaño y SHA-1 del estado sin comprimir) y el estado comprimido con zlib. Se escribe con `QSaveFile`. Un archivo truncado, con otro formato, de una versión de formato más nueva o con el estado dañado se rechaza diciendo por qué. Guardar sobre un slot lleno conserva el anterior como `.bak`.
4. **Antes de cargar:** el estado de otro juego (otra huella) o de otro core se rechaza. Si solo cambia la versión del core, se pregunta ("no hay garantía entre versiones"; por defecto No).
5. **Deshacer la carga:** cada carga toma antes una instantánea en memoria (un nivel, `Shift+F7`). Si el core rechaza el estado, se restaura esa instantánea y se dice; si tampoco se puede, se avisa de que hay que reiniciar. Un estado nunca se presenta como cargado o guardado si la operación falló.
6. **Con el juego en pausa**, tras cargar o deshacer se ejecuta un fotograma para redibujar la imagen restaurada.

## Nota (2026-11-20, ADR-0018)

Los slots pasan a ser 0–999 con la ruta `<juego>.NNN.state` (diez ranuras rápidas), el deshacer es un archivo (`.undo.state`) y se añaden `.oops.state`, `.resume.state`, miniaturas (formato 2 de `StateFile`) y la vista previa. Lo demás de esta decisión sigue igual.

## Consequences

- Positivas: ningún estado ajeno se carga en silencio; un estado malo se puede deshacer; los fallos de lectura, escritura y compresión se muestran.
- Negativas: no hay rewind ni estados automáticos; el deshacer es de un solo nivel y solo en memoria (se pierde al cerrar el juego); los estados de otra versión del core se pueden forzar bajo responsabilidad del usuario.
- Probado en la aplicación real con PicoDrive 2.04 (`26ecb2b`) y Genesis (ROM propia) y con un juego de Game Gear; sin probar con 32X ni Sega CD.
