---
title: "ADR-0006: Licencia del proyecto y distribución de cores"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["licensing", "distribution", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0006: Licencia del proyecto y distribución de cores

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobada el 2026-11-19 (reemplaza la propuesta inicial de no empaquetar cores). Esto es una nota técnica, no asesoría legal.

## Context

Xfusion es un proyecto de código abierto, totalmente gratuito. El `COPYING` de PicoDrive 2.04 (verificado en el repositorio clonado) permite redistribuir y usar el código con estas condiciones:

- Las redistribuciones no pueden venderse ni usarse en un producto o actividad comercial.
- Las redistribuciones modificadas deben incluir el código fuente completo.
- Toda redistribución debe reproducir el aviso de copyright y el descargo de responsabilidad.

Un proyecto gratuito y abierto cumple la primera condición. PicoDrive incluye además componentes con licencia propia (`pico/cd/libchdr`, `pico/sound/emu2413`, `platform/common/dr_libs`, `cpu/cyclone`) que hay que listar.

## Decision

1. **Se puede incluir el core en los paquetes de publicación (release)**, sin modificarlo y fijado al commit `26ecb2b`. Va dentro del zip o instalador, en `cores/`, y sigue cargándose en tiempo de ejecución como biblioteca (ADR-0001).
2. **El binario del core no se versiona en el repositorio.** El repositorio contiene el script que lo clona, fija y compila; los releases se generan con ese script.
3. Cada paquete incluye el `COPYING` de PicoDrive, su aviso de copyright y un archivo `THIRD_PARTY_NOTICES.md` con las licencias de sus componentes.
4. Si algún día se modifica el core, el paquete debe incluir el código fuente completo modificado.
5. Los paquetes con el core **no pueden venderse ni usarse en actividad comercial**. Hay que evitar tienda de pago, publicidad en el paquete o cualquier monetización que dependa de él. Las donaciones voluntarias son una zona gris: no activarlas sin revisarlo.
6. No se distribuyen BIOS ni firmware con copyright.
7. El código de Xfusion se publica bajo **GPL-3.0-or-later** (archivo `LICENSE`). Es independiente de la licencia de PicoDrive: el core es una biblioteca aparte cargada en tiempo de ejecución a través de la API libretro y conserva la suya.

## Consequences

### Positive

- **POS-001**: Experiencia de usuario directa: descargar, descomprimir y jugar.
- **POS-002**: El repositorio queda limpio de binarios y el core sigue siendo reemplazable.
- **POS-003**: Se cumplen las condiciones de PicoDrive para un proyecto gratuito.

### Negative

- **NEG-001**: El paquete con el core queda atado a la restricción no comercial; quien lo redistribuya hereda esa restricción.
- **NEG-002**: Si Xfusion quisiera monetizarse en el futuro, habría que dejar de incluir PicoDrive o cambiar de core.
- **NEG-003**: Hay que mantener el archivo de avisos y actualizarlo en cada versión del core.
- **NEG-004**: La licencia no comercial de PicoDrive no cumple la definición de código abierto de la OSI. Xfusion como proyecto sí puede serlo, pero el paquete combinado no.

## Alternatives Considered

### Descargar el core en el primer uso

- **ALT-001**: **Description**: El paquete solo trae el frontend; el core se descarga después.
- **ALT-002**: **Rejection Reason**: Más fricción para el usuario y depende de una fuente de descarga sin validar. Sigue siendo la opción de respaldo y la base de las actualizaciones (sección 3.2).

### Versionar el binario en el repositorio

- **ALT-003**: **Description**: Subir `picodrive_libretro.dll` a git.
- **ALT-004**: **Rejection Reason**: Binarios en el historial y mezcla de licencias con el código fuente de Xfusion.

## Implementation Notes

- **IMP-001**: Hecho: `LICENSE` con el texto oficial de GPL-3.0. Pendiente: confirmar los avisos de Qt (LGPL, enlace dinámico) y SDL3 al empaquetar, y añadir el encabezado de licencia a los archivos fuente si se desea.
- **IMP-002**: Pendiente: crear `THIRD_PARTY_NOTICES.md` y el empaquetado (Fase 6).
- **IMP-003**: Pendiente: verificar las licencias de cada componente de PicoDrive antes de la primera publicación.
- **IMP-004**: Si se incorpora otro core (ADR-0007), repetir esta revisión para él.

## References

- **REF-001**: `external/picodrive/COPYING` (no versionado).
- **REF-002**: ADR-0001, ADR-0007.
- **REF-003**: `frontend_sega_multiemulador.md`, secciones 3 y 10.3.
