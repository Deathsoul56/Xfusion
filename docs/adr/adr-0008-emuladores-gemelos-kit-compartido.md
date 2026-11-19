---
title: "ADR-0008: Emuladores gemelos con un kit de frontend compartido"
status: "Proposed"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["architecture", "reuse", "licensing", "snes9xrd", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0008: Emuladores gemelos con un kit de frontend compartido

## Status

**Proposed** | Accepted | Rejected | Superseded | Deprecated

Pendiente de aprobación. Matiza la frase "No se reutiliza código de `snes9xrd`" del ADR-0001 y de la sección 9 del plan (ver Decision, punto 1).

## Context

Se quiere que `snes9xrd` (SNES) y Xfusion (SEGA) se comporten como emuladores gemelos, al estilo de DuckStation y PCSX2: misma apariencia, mismos menús, mismos ajustes, misma estructura de datos, y que una mejora hecha en uno pueda pasar al otro.

Lo que se verificó en `C:\Users\rafa_\Documents\VScode\snes9xrd` (rama `dev`):

- **Ya tiene un frontend Qt 6 completo** con tema estilo DuckStation (`duckstation.qss`): biblioteca de juegos con tiempo jugado, paneles de ajustes, bindings con SDL, shaders OpenGL/Vulkan, cheats, estados, RetroAchievements, netplay y grabación AVI. Xfusion va a necesitar casi todo eso en las Fases 2 a 4.
- **Los motores son distintos.** `snes9xrd` llama directamente al núcleo nativo (`Snes9xController` → `Memory`, `S9x*`). Xfusion habla con cores libretro. Esa es la verdadera costura entre ambos.
- **Licencia:** `snes9xrd` es un fork de Snes9x y su `LICENSE` es la licencia Snes9x, que incluye "Under no circumstances will commercial rights be given". No es compatible con GPL-3. En el historial de `qt/`, 124 commits son de BearOso (autor original, licencia Snes9x) y 32 del dueño del fork; los del dueño están mezclados en archivos de BearOso.
- **Consecuencia:** copiar código de `snes9xrd/qt` o `snes9xrd/common` a Xfusion (GPL-3) no es posible, ni tampoco copiar de Xfusion a `snes9xrd` sin reglas. Compartir exige una capa con licencia que ambos proyectos puedan usar y con procedencia limpia.
- Las dependencias de `snes9xrd` (SPIRV-Cross, glslang, imgui, stb, VulkanMemoryAllocator, cubeb, SDL3) son todas permisivas y se pueden compartir sin problema.

## Decision

1. **Se comparte código, pero no el de `snes9xrd/qt` ni `snes9xrd/common`.** Lo compartido es un **kit de frontend** nuevo (`emukit`), escrito desde cero para este fin, con licencia permisiva (propuesta: MIT) que permite usarlo desde Xfusion (GPL-3) y desde `snes9xrd` (licencia Snes9x). Ningún archivo con licencia Snes9x entra al kit. El código propio del dueño que vive en archivos de BearOso solo puede pasar al kit si se comprueba con `git blame` que es enteramente suyo.
2. **Tres capas con dependencia en un solo sentido** (producto → kit → dependencias; el kit nunca incluye cabeceras de un producto):

```text
  snes9xrd (SNES)                         Xfusion (SEGA)
  Snes9xBackend (núcleo nativo)           LibretroBackend + CoreProfile
  UI propia: Super Scope, BS-X,           UI propia: 6 sistemas, 32X,
  SPC, netplay, rewind                    BIOS de Sega CD, saves por sistema
          \                                   /
           ----------  emukit (MIT)  ---------
   servicios: DataLayout, Settings, InputBindings, SaveStore, Library,
              AudioOutput, UpdateManager, Logging, ZipReader
   UI:        ventana base, paneles de ajustes, biblioteca, estados, tema, OSD
   video:     presentador (QRhi/OpenGL), escaladores, shaders
   contrato:  IEmuBackend, IVideoSink, IAudioSink, Capabilities
          |
   Qt 6, SDL3 y otras dependencias permisivas
```

3. **El contrato es una interfaz de backend** (`IEmuBackend`) con capacidades declaradas. Cada producto aporta su adaptador: `LibretroBackend` en Xfusion y `Snes9xBackend` (nativo) en `snes9xrd`. Funciones que dependen del motor (rewind, netplay, cheats, estados) se activan según `Capabilities`; no se asume que todos las tienen.
4. **Cómo pasan las mejoras de un lado a otro:**
   - Lo genérico se hace en el kit primero (un cambio, una etiqueta de versión). Cada producto sube su versión fijada del kit cuando quiere. Lo específico de un sistema se queda en el producto.
   - **Regla de dos:** no se mueve código al kit hasta que dos productos lo necesiten, salvo que se diseñe desde el principio como módulo genérico.
   - Un `NullBackend` (imagen y tono de prueba) permite probar la UI y los servicios del kit sin ningún emulador, y se ejecuta en ambos repositorios.
   - Convenciones comunes en el kit (reutilizar antes de escribir, una única fuente de verdad, arreglar la causa raíz), tomadas de `AGENTS.md` de `snes9xrd` y del skill `xfusion`.
5. **Paridad "gemela" medible:** mismo árbol de menús y atajos, mismo esquema de ajustes (versionado y migrable), misma estructura de datos (`saves/<sistema>/`, `states/<sistema>/`, `cores/`), mismo tema y mismos nombres de paneles. Esto sale del kit, no de copiar y pegar.
6. **Extracción por etapas:** primero el kit vive dentro de Xfusion como carpeta `kit/` (objetivo CMake `emukit`, includes `emukit/...`), con una comprobación en CI de que no depende del producto. Cuando esté estable (después de la Fase 2), se separa a su propio repositorio con `git subtree split` (conserva el historial) y los productos lo consumen fijado por versión (submódulo o `FetchContent`). `snes9xrd` lo adopta módulo a módulo, sustituyendo el suyo detrás de un adaptador.

## Consequences

### Positive

- **POS-001**: Una mejora genérica se escribe una sola vez y llega a los dos productos.
- **POS-002**: La licencia queda resuelta de forma limpia: nada con licencia Snes9x entra a Xfusion.
- **POS-003**: El contrato de backend obliga a separar el motor de la interfaz, lo que también mejora Xfusion aislado (ADR-0007).
- **POS-004**: El kit se puede probar sin emulador gracias al `NullBackend`.

### Negative

- **NEG-001**: Hay que escribir de nuevo lo que `snes9xrd` ya tiene (biblioteca, bindings, ajustes) en vez de copiarlo. Es más trabajo inicial.
- **NEG-002**: Una abstracción mal diseñada cuesta más que duplicar. Hay que mantener el kit pequeño y guiado por necesidades reales.
- **NEG-003**: Dos build systems distintos (Qt estático y MSVC/MSYS2 en `snes9xrd`, MSYS2 en Xfusion): el kit debe compilar en ambos.
- **NEG-004**: Adoptar el kit en `snes9xrd` toca archivos derivados de Snes9x y puede generar conflictos al fusionar `upstream`. Conviene adoptar por adaptadores y parches pequeños.
- **NEG-005**: Hay decisiones técnicas por alinear (audio: cubeb en `snes9xrd` y SDL3 en Xfusion; renderizado: sus propios backends frente a `QPainter`).
- **NEG-006**: El mantenimiento recae en una sola persona.

## Alternatives Considered

### Copiar y pegar entre repositorios

- **ALT-001**: **Description**: Llevar código a mano de un proyecto al otro.
- **ALT-002**: **Rejection Reason**: Las copias divergen, no hay una versión de referencia y, en la dirección `snes9xrd` a Xfusion, es incompatible con la licencia.

### Un solo repositorio y una sola aplicación

- **ALT-003**: **Description**: Una aplicación con varios núcleos (SNES nativo y SEGA por libretro).
- **ALT-004**: **Rejection Reason**: Fusiona dos historiales (uno es un fork con `upstream` propio) y dificulta publicar versiones independientes.

### Monorepo con ambos productos y el kit

- **ALT-005**: **Description**: Un solo repositorio que contenga `snes9xrd`, Xfusion y el kit.
- **ALT-006**: **Rejection Reason**: Choca con el fork de Snes9x. Podría reconsiderarse si el fork deja de seguir a `upstream`.

### Cambiar la licencia de snes9xrd

- **ALT-007**: **Description**: Intentar relicenciar para compartir directamente.
- **ALT-008**: **Rejection Reason**: No es posible sin el permiso de los autores originales de Snes9x.

## Implementation Notes

- **IMP-001**: Primer paso en Xfusion: crear `kit/` con CMake y mover sin cambiar comportamiento `AppPaths`, `AudioOutput`, la tabla de sistemas y la lógica de saves; definir `IEmuBackend` y envolver `LibretroCore` como `LibretroBackend`.
- **IMP-002**: Segundo paso: servicios nuevos hechos ya como módulos del kit (`Settings`, `InputBindings` con SDL3, `SaveStore`), que cubren la Fase 2 y la Fase 3.
- **IMP-003**: Añadir el `NullBackend` y una prueba automática de que el kit compila sin cabeceras del producto.
- **IMP-004**: Llevar un registro de procedencia (`kit/PROVENANCE.md`): origen y licencia de cada archivo. Cualquier código de terceros debe tener licencia permisiva compatible.
- **IMP-005**: Antes de adoptar el kit en `snes9xrd`, decidir audio (cubeb o SDL3) y renderizado, y comprobar que compila con su cadena de herramientas.
- **IMP-006**: Pendiente: elegir la licencia exacta del kit (MIT o MPL-2.0) y confirmar la propiedad de las contribuciones del dueño en `snes9xrd/qt` con `git blame`.

## References

- **REF-001**: ADR-0001, ADR-0006, ADR-0007.
- **REF-002**: `snes9xrd/AGENTS.md` (reglas de código y estructura) y `snes9xrd/LICENSE` (licencia Snes9x).
- **REF-003**: `frontend_sega_multiemulador.md`, secciones 4 y 6 (Fases 2 a 4).
