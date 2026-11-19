---
title: "ADR-0006: Licencia del proyecto y distribución de cores"
status: "Proposed"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["licensing", "distribution", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0006: Licencia del proyecto y distribución de cores

## Status

**Proposed** | Accepted | Rejected | Superseded | Deprecated

Requiere decisión del propietario del proyecto. Esto es una nota técnica, no asesoría legal.

## Context

El `COPYING` de PicoDrive 2.04 (verificado en el repositorio clonado) prohíbe vender las redistribuciones y usarlas en un producto o actividad comercial. Es probable que otros cores candidatos (por ejemplo Genesis Plus GX) tengan restricciones similares, pero no se ha verificado. La licencia del propio Xfusion tampoco está elegida. Hasta resolverlo no se puede empaquetar ni publicar un instalador.

## Decision

Propuesta:

1. No empaquetar cores en el instalador ni en el repositorio; el usuario los instala o los descarga en el primer uso (sección 3.2).
2. Revisar y registrar la licencia de cada core y dependencia antes de cada versión.
3. Elegir la licencia de Xfusion después de esa revisión y documentarla en un archivo `LICENSE`.
4. No distribuir BIOS ni firmware con copyright.

## Consequences

### Positive

- **POS-001**: Evita redistribuir código con restricciones de uso comercial.
- **POS-002**: Mantiene abierta la decisión de licencia hasta tener los datos.

### Negative

- **NEG-001**: El primer uso exige un paso extra (instalar o descargar un core).
- **NEG-002**: Depende de la disponibilidad de la fuente de descarga (buildbot de libretro, sin validar).
- **NEG-003**: Si se quiere vender o monetizar Xfusion, estos cores quedan descartados y habrá que revisar alternativas.

## Alternatives Considered

### Empaquetar los cores

- **ALT-001**: **Description**: Incluirlos en el instalador.
- **ALT-002**: **Rejection Reason**: Choca con la restricción de PicoDrive; no resuelta para el resto.

### Elegir licencia ahora sin revisar los cores

- **ALT-003**: **Description**: Fijar la licencia del frontend de inmediato.
- **ALT-004**: **Rejection Reason**: Puede ser incompatible con lo que se decida distribuir después.

## Implementation Notes

- **IMP-001**: Pendiente: verificar la licencia de Genesis Plus GX y BlastEm, y la política de redistribución del buildbot.
- **IMP-002**: Pendiente: añadir `LICENSE` y un archivo `THIRD_PARTY_NOTICES.md`.

## References

- **REF-001**: `external/picodrive/COPYING` (no versionado).
- **REF-002**: ADR-0001.
- **REF-003**: `frontend_sega_multiemulador.md`, secciones 3 y 10.3.
