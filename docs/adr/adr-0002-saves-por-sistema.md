---
title: "ADR-0002: Saves organizados por sistema"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["data", "saves", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0002: Saves organizados por sistema

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

## Context

Plataformas distintas pueden tener juegos con el mismo nombre (por ejemplo, un título en Master System y en Game Gear). Con una carpeta única, una save sobrescribiría a otra. El usuario pidió explícitamente una jerarquía por sistema.

## Decision

Las saves viven en `saves/<sistema>/`, con carpetas fijas e independientes del idioma: `SG-1000`, `Master System`, `Game Gear`, `Genesis`, `Sega CD`, `32X`. La carpeta la determina el sistema del juego, no el core que lo ejecuta. Aplica a SRAM y, más adelante, a estados. Las carpetas se crean bajo demanda y la raíz `saves/` es configurable.

## Consequences

### Positive

- **POS-001**: Juegos homónimos de sistemas distintos nunca se sobrescriben.
- **POS-002**: Cambiar de core no mueve las saves de un juego.
- **POS-003**: Estructura legible y fácil de respaldar.

### Negative

- **NEG-001**: Dentro de un mismo sistema, dos juegos con igual nombre de archivo todavía chocan. Está pendiente desambiguar por hash del contenido.
- **NEG-002**: Si dos cores usan formatos de SRAM incompatibles, la save compartida puede fallar; hay que avisar antes de reutilizarla.
- **NEG-003**: Depende de que la detección de sistema sea correcta (`.bin` se asume Genesis hoy).

## Alternatives Considered

### Carpeta única con prefijo de sistema en el nombre

- **ALT-001**: **Description**: `saves/<sistema>_<juego>.srm`.
- **ALT-002**: **Rejection Reason**: Menos ordenado y más frágil al renombrar o filtrar.

### Carpeta por core

- **ALT-003**: **Description**: `saves/<core>/<juego>.srm`.
- **ALT-004**: **Rejection Reason**: Cambiar de core o de versión dejaría las saves huérfanas.

## Implementation Notes

- **IMP-001**: La ruta se obtiene con `AppPaths::savesDir(System)`; no se construye a mano.
- **IMP-002**: Las escrituras usan `QSaveFile` (atómicas).
- **IMP-003**: Pendiente: hash de la ROM como sufijo ante colisión y volcado periódico de SRAM.

## References

- **REF-001**: `frontend_sega_multiemulador.md`, sección 3.1.
