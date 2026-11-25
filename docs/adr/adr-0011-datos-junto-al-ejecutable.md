---
title: "ADR-0011: Datos de usuario junto al ejecutable"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["paths", "saves", "portable", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0011: Datos de usuario junto al ejecutable

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 por pedido del propietario: "la carpeta de las saves debe ser la misma del .exe, no tiremos nada a roaming".

**Ajustado por el ADR-0030 (2026-11-25):** si la carpeta del ejecutable no se puede escribir (por ejemplo `Program Files`), se usa `%LOCALAPPDATA%\Xfusion`; `XFUSION_DATA_DIR` sigue mandando sobre las dos.

## Context

Hasta ahora `AppPaths::dataRoot()` usaba `%APPDATA%\Xfusion\Xfusion\` (`QStandardPaths::AppDataLocation`). Las saves, los BIOS, los cores y la configuración quedaban fuera de la carpeta del programa y no viajaban con ella.

## Decision

1. La raíz de datos es la carpeta del ejecutable (`QCoreApplication::applicationDirPath()`). Dentro: `cores\`, `system\` (BIOS), `config\` y `saves\<sistema>\` (ADR-0002).
2. Xfusion no escribe nada en `%APPDATA%`.
3. `XFUSION_DATA_DIR` sigue sustituyendo la raíz (pruebas y desarrollo).

## Consequences

- Positivas: una carpeta autocontenida y movible; los saves quedan a la vista.
- Negativas: si el programa se instala en una carpeta sin permiso de escritura (p. ej. `Program Files`) fallarán las saves y la configuración; por ahora se distribuye como carpeta portátil. En desarrollo el exe está en `build\`, así que borrar `build\` borra saves y BIOS (usar `XFUSION_DATA_DIR` para conservarlos).
- Los datos que ya existían en `%APPDATA%\Xfusion\Xfusion\` no se migran solos; se copian a mano.
