---
title: "ADR-0004: Recargar el core completo al cargar cada juego"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["libretro", "picodrive", "workaround", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0004: Recargar el core completo al cargar cada juego

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aceptada como solución provisional: la causa raíz no está identificada.

## Context

En la aplicación real, al abrir un segundo juego con Abrir (por ejemplo Castlevania - Bloodlines tras Sonic & Knuckles), los botones se leían mal: la A de Sega actuaba al soltar la tecla en vez de al apretarla. Con el primer juego cargado nunca ocurría. Las pruebas con un arnés sin interfaz no reprodujeron el problema, solo la aplicación completa.

Se descartó sin éxito: pedir al core que relea sus variables tras cargar el juego, y consultar el teclado por polling (el registro mostró que las teclas y el estado del core llegaban bien). Pedir un mando de 6 botones es necesario para X/Y/Z, pero tampoco resolvía este caso por sí solo.

## Decision

A partir del segundo juego, `LibretroCore::loadGame` descarga y vuelve a cargar la biblioteca del core (`retro_deinit`, descarga, carga, `retro_init`) antes de cargar el juego. El primer juego usa el core ya inicializado.

## Consequences

### Positive

- **POS-001**: Cada juego parte del mismo estado limpio que el primero, y el error de botones deja de aparecer (confirmado manualmente).
- **POS-002**: Evita arrastrar estado interno del core entre juegos.

### Negative

- **NEG-001**: Abrir un juego tarda un poco más.
- **NEG-002**: Oculta un defecto sin explicarlo; podría haber otros efectos por estado persistente.
- **NEG-003**: Sustituirlo por un reinicio ligero requiere entender la causa en PicoDrive.

## Alternatives Considered

### Reiniciar solo el estado del juego

- **ALT-001**: **Description**: Llamar a `retro_unload_game` y `retro_load_game` sin descargar el core.
- **ALT-002**: **Rejection Reason**: Es el comportamiento anterior, que mostraba el defecto.

### Ejecutar cada juego en un proceso nuevo

- **ALT-003**: **Description**: Aislar el core en un proceso aparte por partida.
- **ALT-004**: **Rejection Reason**: Más robusto, pero mucho más complejo (video, audio y entrada entre procesos). Se reevaluará junto con el aislamiento de cores.

## Implementation Notes

- **IMP-001**: No retirar la recarga sin reproducir el caso: cargar Castlevania - Bloodlines tras otro juego y mantener un botón mientras se aprieta otro.
- **IMP-002**: Pendiente: investigar la causa dentro de PicoDrive (`libretro.c` y estado estático) y, si se encuentra, sustituir la recarga por una solución más ligera.

## References

- **REF-001**: ADR-0001.
- **REF-002**: `frontend_sega_multiemulador.md`, sección 10.3.
