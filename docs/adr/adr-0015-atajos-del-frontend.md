---
title: "ADR-0015: Atajos de teclado del frontend"
status: "Superseded"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["input", "settings", "ui", "decision"]
supersedes: ""
superseded_by: "ADR-0018"
---

# ADR-0015: Atajos de teclado del frontend

## Status

Proposed | Accepted | Rejected | **Superseded** | Deprecated

Sustituido por el ADR-0018 el 2026-11-20: los atajos pasan a cuatro enlaces por acción, con teclas con modificadores y entradas de mando, y se despachan por un filtro de eventos. Lo de abajo describe la primera versión.

Aprobado el 2026-11-19 como parte de la Fase 2 ("atajos de teclado del frontend"; en la Fase 3 se añadieron los de estados).

## Context

Pausa, reinicio y pantalla completa tenían atajos fijos puestos en los menús. El plan pide atajos configurables y la Fase 3 necesita varios más (estados y slots). Los atajos del frontend no son botones del mando: no dependen del sistema ni del jugador.

## Decision

1. **Una tabla de acciones** (`Hotkeys.*`, `allHotkeys()`): identificador estable, texto y secuencia por defecto. Pausa `P`, Reinicio `Ctrl+R`, Pantalla completa `F11`, Volumen − / + `F2` / `F3`, Silencio `F4`, Guardar estado `F5`, Cargar estado `F7`, Slot siguiente / anterior `F6` / `Shift+F6`, Deshacer carga `Shift+F7`. Añadir una acción es añadir una fila y conectar su `QAction`.
2. **Una secuencia por acción** y una acción por secuencia (`HotkeyMap::assign` mueve la secuencia y devuelve la acción que la perdió). Solo teclado: una tecla con o sin modificadores (`QKeySequenceEdit` limitado a una secuencia de una pulsación).
3. **Persistencia** en `config\settings.json` (clave `hotkeys`, texto portable de Qt). Las acciones que falten conservan su valor por defecto (los archivos anteriores siguen funcionando); un texto vacío significa sin atajo; un archivo con una secuencia repetida se repara dejándola en la última acción de la tabla.
4. **`Esc` es fijo:** sale de pantalla completa y no puede ser un atajo.
5. **Una tecla, un significado:** una tecla sola (sin modificadores) no puede ser a la vez botón del mando y atajo. La página Hotkeys rechaza una tecla usada en cualquier binding (de cualquier grupo y jugador) y la página Controllers rechaza una tecla que ya es atajo. Los atajos con modificadores (`Ctrl+R`) no chocan con el mando, porque el mando se lee por estado físico.
6. **Funcionan con el menú oculto:** los `QAction` pertenecen también a la ventana (`addAction`), para no depender de que la barra de menús esté visible: en pantalla completa se oculta. Comprobado en la aplicación real (`F2`, `F3`, `F4` y `F11` con la barra oculta).
7. **Avisos sobre la imagen:** volumen, silencio y estados muestran un aviso breve encima del vídeo (y en la barra de estado), porque la barra de estado también se oculta en pantalla completa. El volumen y el silencio se guardan en cuanto cambian.

## Consequences

- Positivas: un único lugar para todos los atajos; el diálogo evita que una tecla haga dos cosas.
- Negativas: no hay atajos de mando ni combinaciones de varias pulsaciones; los avisos de pantalla son un `QLabel` sencillo, sin cola de mensajes.
