---
title: "ADR-0009: Portar mejoras entre emuladores gemelos por adaptación, con ledger y script"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["process", "reuse", "licensing", "snes9xrd", "decision"]
supersedes: "ADR-0008"
superseded_by: ""
---

# ADR-0009: Portar mejoras entre emuladores gemelos por adaptación, con ledger y script

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19. Sustituye al ADR-0008 (kit compartido), que queda diferido.

## Context

Se quiere que `snes9xrd` (SNES) y Xfusion (SEGA) se sientan como emuladores gemelos y que las mejoras de uno lleguen al otro. El ADR-0008 proponía un kit compartido con licencia MIT. Para un proyecto personal y local, con un solo desarrollador, eso es más infraestructura de la necesaria: hay que diseñar interfaces antes de saber si hacen falta, y la regla de dos del propio ADR-0008 ya decía que no se extrae hasta que dos proyectos necesiten lo mismo.

Datos que siguen siendo válidos (verificados en `snes9xrd`, rama `dev`):

- `snes9xrd` ya tiene un frontend Qt completo, con biblioteca, ajustes, bindings, temas por plantilla (`theme.qss`, antes `duckstation.qss`), shaders, cheats, RetroAchievements, netplay y AVI.
- Su `LICENSE` es la licencia Snes9x ("Under no circumstances will commercial rights be given"), incompatible con GPL-3.
- En el historial de `qt/`, 124 commits son de BearOso y 32 del dueño del fork, mezclados en los mismos archivos.
- Los motores son distintos: nativo en `snes9xrd`, libretro en Xfusion.

## Decision

1. **No se crea un kit compartido por ahora.** Cada emulador conserva su propio código y lo adapta a su consola.
2. **Las mejoras viajan por adaptación consciente**, no por copia: se lee el commit, se entiende qué hace y por qué, se decide qué cambia entre SNES y SEGA, y se escribe la versión propia.
3. **Reglas de licencia al adaptar** (nota técnica, no asesoría legal):
   - De Xfusion a `snes9xrd`: el código de Xfusion es del dueño, que puede reutilizarlo, salvo contribuciones de terceros o código ajeno.
   - De `snes9xrd` a Xfusion: se lee para entender la idea o el comportamiento y se escribe una implementación propia. No se traduce su estructura línea a línea ni se copian fragmentos de BearOso (licencia Snes9x, incompatible con GPL-3).
   - El código de `snes9xrd` que sea enteramente del dueño puede llevarse a Xfusion. La autoría se comprueba archivo por archivo con el script y, si el archivo entró en un commit masivo, no se da por verificada.
4. **Herramientas del proceso:**
   - `docs/porting-ledger.md`: registro de mejoras candidatas, su origen, la propiedad del código, el destino y el estado.
   - `scripts/porting-log.ps1`: lista los commits nuevos de otro repositorio desde una revisión y calcula la autoría por archivo.
5. **El kit queda diferido, no descartado.** Se reabre cuando una misma pieza se haya portado a mano dos o más veces, o cuando el coste de mantener dos copias supere el de abstraerla. En ese caso valen las decisiones del ADR-0008 (licencia MIT, carpeta `kit/` y extracción con `git subtree split`).
6. **Costuras limpias en Xfusion:** se mantiene el código modular (por ejemplo `CoreProfile`, ADR-0007) para que portar y, más adelante, extraer sea barato.

## Consequences

### Positive

- **POS-001**: Cero infraestructura nueva; cada emulador se adapta libremente a su consola.
- **POS-002**: Sin riesgo de licencia por copiar código con licencia Snes9x.
- **POS-003**: El ledger deja constancia de qué se portó, qué se descartó y por qué.
- **POS-004**: Se puede pasar al kit más adelante sin perder nada.

### Negative

- **NEG-001**: Cada mejora se implementa dos veces, a mano.
- **NEG-002**: Depende de disciplina: si no se revisa el otro repositorio, las dos bases divergen.
- **NEG-003**: La paridad visual y de comportamiento ("gemelos") no está garantizada por el código; hay que vigilarla.
- **NEG-004**: Puede haber trabajo duplicado que un kit habría evitado.

## Alternatives Considered

### Kit compartido con licencia MIT (ADR-0008)

- **ALT-001**: **Description**: Capa común escrita desde cero, usada por ambos productos.
- **ALT-002**: **Rejection Reason**: Excesiva para un proyecto personal sin evidencia de duplicación; diferida, no descartada.

### Copiar y pegar entre repositorios

- **ALT-003**: **Description**: Llevar archivos a mano de un proyecto al otro.
- **ALT-004**: **Rejection Reason**: Hacia Xfusion es incompatible con la licencia, y en ambos sentidos hace divergir las bases sin registro.

## Implementation Notes

- **IMP-001**: Antes de portar algo desde `snes9xrd`, ejecutar `scripts/porting-log.ps1 -Blame <archivo>` y anotar la propiedad en el ledger. `blame` indica quién hizo el commit, no quién escribió el código: si el archivo entró en un commit masivo (el script lo avisa), la autoría queda `sin verificar`.
- **IMP-002**: Cada fila del ledger indica qué cambia entre SEGA y SNES (por ejemplo, saves por sistema, motor libretro frente a nativo).
- **IMP-003**: Revisar el otro repositorio al cerrar cada fase del plan y anotar el último commit revisado.
- **IMP-004**: Señal para reabrir el kit: tres filas del ledger sobre la misma pieza (bindings, ajustes, biblioteca, etc.).

## References

- **REF-001**: ADR-0007, ADR-0008.
- **REF-002**: `snes9xrd/AGENTS.md` y `snes9xrd/LICENSE`.
- **REF-003**: `docs/porting-ledger.md`, `scripts/porting-log.ps1`.
