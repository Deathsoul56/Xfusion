---
title: "ADR-0007: PicoDrive como core inicial y perfiles de core intercambiables"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["architecture", "cores", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0007: PicoDrive como core inicial y perfiles de core intercambiables

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

La decisión de usar solo PicoDrive está aprobada. El diseño de perfiles está aceptado pero sin implementar.

## Context

El documento planteaba Genesis Plus GX como core principal, pero solo PicoDrive está integrado. PicoDrive cubre los seis sistemas objetivo (en la colección de prueba arrancan 611 de 634 ROMs de SG-1000, Master System, Game Gear, Genesis y 32X; Sega CD no se ha probado). El usuario decidió usar PicoDrive y pidió que cambiar de core después no sea difícil.

Hoy las particularidades de PicoDrive están repartidas por el código:

- `AppPaths::defaultCorePath()` fija `picodrive_libretro.dll`.
- `LibretroCore::handleEnvironment` responde a `picodrive_input1/2` con `"6 button pad"` (desde el ADR-0010 el valor depende del tipo de mando elegido; 6 por defecto).
- `LibretroCore::loadGame` recarga el core entero a partir del segundo juego (ADR-0004).
- `MainWindow` asume el mapeo libretro → botón Genesis de PicoDrive.
- La detección de sistema (`SystemInfo`) y los nombres de las opciones del core no son genéricos.

## Decision

1. Se usa **solo PicoDrive** para todos los sistemas.
2. Se introduce un **perfil de core** (`CoreProfile`) como única fuente de las particularidades de cada core: nombre de la biblioteca, opciones que se fuerzan (el tipo de mando ya no es fijo: lo elige el usuario y el perfil solo indica las claves, ver ADR-0010), si necesita recarga por juego, mapeo de botones lógicos a identificadores libretro, y sistemas y extensiones que admite.
3. El resto del frontend habla en **acciones del frontend** (botones SEGA lógicos A, B, C, X, Y, Z, Start, Mode), no en identificadores de libretro. El perfil traduce.
4. La asignación sistema → core es una **tabla de reglas** (datos), con selección manual posterior; no vive en el código de la interfaz.
5. Todo core nuevo exige: su perfil, una prueba de arranque con la colección de ROMs de ejemplo, la revisión de licencia (ADR-0006) y una nota de compatibilidad de saves y estados.

## Consequences

### Positive

- **POS-001**: Cambiar o añadir un core es en su mayor parte añadir un perfil y probarlo.
- **POS-002**: Las particularidades dejan de estar dispersas y quedan documentadas en un solo sitio.
- **POS-003**: La interfaz y los mandos configurables (Fase 2) no dependen de ningún core.

### Negative

- **NEG-001**: Un core distinto **no** es un reemplazo directo: los estados de guardado casi nunca son compatibles entre cores, la SRAM puede variar de formato, los nombres de opciones cambian y cada core tiene sus errores propios.
- **NEG-002**: Más indirección (perfil y traducción de botones) para un solo core.
- **NEG-003**: Cada core aprobado necesita pruebas propias, lo que cuesta tiempo.

## Alternatives Considered

### Mantener las particularidades de PicoDrive en el código

- **ALT-001**: **Description**: Estado actual.
- **ALT-002**: **Rejection Reason**: Cambiar de core obligaría a tocar el host, la ventana y las rutas.

### Abstracción completa para cualquier core libretro

- **ALT-003**: **Description**: Soportar todas las funciones opcionales de libretro (opciones del core, hardware, subsistemas).
- **ALT-004**: **Rejection Reason**: Excesivo para el alcance. Se implementa lo que PicoDrive y un segundo core candidato necesiten.

## Implementation Notes

- **IMP-001**: Hecho: `src/CoreProfile.*` reúne el nombre de la biblioteca, las opciones forzadas, la recarga por juego y el mapeo de botones de PicoDrive. `LibretroCore` recibe el perfil y `AppPaths` lo usa para la ruta del core. Verificado: los 12 botones llegan al core (incluidos X/Y/Z tras cargar un segundo juego) y Z sigue activando la A de Sega antes y después de reabrir un juego.
- **IMP-002**: Hecho: `MainWindow` usa una sola tabla de teclas → `PadButton` (botones SEGA lógicos); ya no hay identificadores libretro fuera del perfil. Pendiente: bindings configurables y mando (Fase 2).
- **IMP-003**: Pendiente: un segundo perfil (por ejemplo Genesis Plus GX) como prueba de la abstracción; no hace falta integrarlo todavía.
- **IMP-004**: Las saves quedan por sistema, no por core (ADR-0002), por lo que el cambio de core conserva la SRAM.
- **IMP-005**: Pendiente: tabla de reglas sistema → core como datos; hoy `defaultCoreProfile()` devuelve siempre PicoDrive.

## References

- **REF-001**: ADR-0001, ADR-0002, ADR-0004, ADR-0006.
- **REF-002**: `frontend_sega_multiemulador.md`, secciones 2 y 5.
