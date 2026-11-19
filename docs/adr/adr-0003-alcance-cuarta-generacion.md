---
title: "ADR-0003: Alcance hasta la cuarta generación"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch)"
tags: ["scope", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0003: Alcance hasta la cuarta generación

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

## Context

SEGA tiene sistemas de muchas generaciones. Saturn y Dreamcast ya están bien cubiertos por otros proyectos, y cada generación añade requisitos distintos de rendimiento, gráficos y firmware.

## Decision

Xfusion soporta SG-1000, Master System, Game Gear, Genesis / Mega Drive, Sega CD y 32X. Saturn, Dreamcast y cualquier sistema posterior a la cuarta generación quedan descartados, no pospuestos.

## Consequences

### Positive

- **POS-001**: Un conjunto de requisitos acotado: video por software, audio simple y poca exigencia de hardware.
- **POS-002**: El diseño no necesita contextos gráficos por hardware ni emulación de alto coste.

### Negative

- **NEG-001**: Los usuarios que quieran Saturn o Dreamcast necesitan otra herramienta.
- **NEG-002**: Los formatos fuera de la tabla (`.sf7` SF-7000, `.mv` MyVision) quedan sin soporte salvo decisión expresa.

## Alternatives Considered

### Incluir Saturn y Dreamcast

- **ALT-001**: **Description**: Ampliar a todas las consolas SEGA.
- **ALT-002**: **Rejection Reason**: Duplica el esfuerzo de proyectos existentes y rompe la simplicidad del frontend.

## Implementation Notes

- **IMP-001**: La tabla única de sistemas está en `src/SystemInfo.*`; añadir un sistema es una decisión de alcance, no solo una línea de código.

## References

- **REF-001**: `frontend_sega_multiemulador.md`, secciones 2 y 9.
