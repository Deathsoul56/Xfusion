---
title: "ADR-0005: Hilo de emulación y sincronización por audio"
status: "Proposed"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["architecture", "performance", "audio", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0005: Hilo de emulación y sincronización por audio

## Status

**Proposed** | Accepted | Rejected | Superseded | Deprecated

Propuesta pendiente de aprobación; no hay implementación.

## Context

Hoy el emulador corre en un `QTimer` del hilo de la interfaz y la temporización sale del reloj del sistema (acumulador de hasta 4 fotogramas por tick). En una sesión de prueba la mayoría de los segundos emularon 60 fotogramas, con algunos segundos aislados de 30 a 55 al interactuar con menús. Arrastrar la ventana o abrir menús modales en Windows puede detener los temporizadores. El audio no marca el ritmo, así que pueden aparecer cortes o deriva.

## Decision

Propuesta: ejecutar `retro_run` en un hilo dedicado, con el audio de SDL3 como reloj maestro (ajuste dinámico de la cola). La entrada se publica como una instantánea atómica para el hilo de emulación, y los fotogramas pasan a la interfaz mediante un búfer reutilizable. Antes de implementarlo hay que medir y validar el diseño con un prototipo.

## Consequences

### Positive

- **POS-001**: La emulación no se interrumpe por la interfaz.
- **POS-002**: Audio y video más estables y sin deriva.
- **POS-003**: Menos copias de imagen por fotograma.

### Negative

- **NEG-001**: Introduce concurrencia: los callbacks de libretro pasan a otro hilo y hay que sincronizar entrada, video, pausa y descarga del core.
- **NEG-002**: Complica las pruebas y la recarga del core (ADR-0004).
- **NEG-003**: Más difícil de cambiar después de añadir mandos, biblioteca y estados, por eso conviene decidirlo antes.

## Alternatives Considered

### Mantener el `QTimer` en el hilo de la interfaz

- **ALT-001**: **Description**: Estado actual.
- **ALT-002**: **Rejection Reason**: Sensible a bloqueos de la interfaz; el audio no gobierna el tiempo.

### Hilo de emulación con reloj del sistema

- **ALT-003**: **Description**: Hilo propio, pero con la misma temporización por reloj.
- **ALT-004**: **Rejection Reason**: Resuelve los bloqueos, no la deriva con el audio.

## Implementation Notes

- **IMP-001**: Medir antes: fotogramas por segundo, tamaño de cola de audio y duración de `retro_run` bajo carga de la interfaz.
- **IMP-002**: Definir la política de pausa y de descarga del core desde el hilo de emulación.
- **IMP-003**: Criterio de éxito: sin cortes de audio durante una sesión de 10 minutos moviendo la ventana y abriendo menús.

## References

- **REF-001**: ADR-0004.
- **REF-002**: `frontend_sega_multiemulador.md`, sección 10.3 (temporización).
