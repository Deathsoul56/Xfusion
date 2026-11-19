---
title: "ADR-0001: Host libretro propio con cores como bibliotecas externas"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["architecture", "decision", "libretro"]
supersedes: ""
superseded_by: ""
---

# ADR-0001: Host libretro propio con cores como bibliotecas externas

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

## Context

Xfusion quiere cubrir varios sistemas SEGA (hasta la cuarta generación) sin escribir emuladores. Existen cores libretro mantenidos (PicoDrive, Genesis Plus GX, BlastEm). Cada core tiene su licencia y su ritmo de actualización, y la compatibilidad depende de la versión.

## Decision

Xfusion es un host libretro escrito desde cero (C++20, Qt 6, SDL3). Los cores se cargan en tiempo de ejecución con `QLibrary` desde `cores/` en el directorio de datos del usuario. Ningún core se enlaza dentro del ejecutable ni se versiona en el repositorio. No se copia código de `snes9xrd` (ver ADR-0008).

## Consequences

### Positive

- **POS-001**: Los cores se actualizan sin recompilar el frontend (ver sección 3.2 del documento).
- **POS-002**: Un fallo o una licencia problemática de un core no contamina el resto del proyecto.
- **POS-003**: Se puede añadir un core nuevo con reglas por sistema, sin cambiar el host.

### Negative

- **NEG-001**: El host debe tolerar diferencias entre cores (funciones opcionales, formatos de SRAM y estados).
- **NEG-002**: Hay que mantener la compatibilidad con la ABI de libretro y validar versión y símbolos al cargar.
- **NEG-003**: Un bug dentro del core puede tumbar el proceso (ver ADR-0004).

## Alternatives Considered

### Compilar el core dentro del ejecutable

- **ALT-001**: **Description**: Enlazar PicoDrive estáticamente.
- **ALT-002**: **Rejection Reason**: Obliga a recompilar para cada actualización y ata la licencia del core a todo el binario.

### Escribir un emulador propio

- **ALT-003**: **Description**: Implementar la emulación desde cero.
- **ALT-004**: **Rejection Reason**: Fuera de alcance; ya existen cores maduros.

## Implementation Notes

- **IMP-001**: `src/LibretroCore.*` valida `retro_api_version` y todos los símbolos antes de usarlo.
- **IMP-002**: `scripts/setup-picodrive.ps1` clona, fija el commit (`26ecb2b`), compila e instala el core en `cores/`.

## References

- **REF-001**: `frontend_sega_multiemulador.md`, secciones 3 y 3.2.
- **REF-002**: ADR-0004 (recarga del core), ADR-0006 (licencias).
