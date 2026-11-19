# Ledger de portado entre snes9xrd y Xfusion

Registro de mejoras que pueden viajar de un emulador al otro (ADR-0009). Se
portan **adaptando**: se lee el commit, se entiende qué hace, se decide qué
cambia entre SNES y SEGA y se escribe la versión propia. No se traduce código
línea a línea ni se copian fragmentos con licencia Snes9x.

## Cómo se usa

1. Ver qué hay nuevo en el otro repositorio:

   ```text
   scripts\porting-log.ps1 -Since <hash revisado> -Path qt -Author deathsoul56
   ```

2. Antes de tomar algo de `snes9xrd`, comprobar quién escribió el archivo:

   ```text
   scripts\porting-log.ps1 -Blame qt/src/<archivo>
   ```

   El script muestra el reparto de autoría y el commit de origen. **`blame` dice
   quién hizo el commit, no quién escribió el código.** Si el archivo entró en un
   commit masivo, el script lo avisa y la autoría no es fiable.

3. Añadir o actualizar una fila de la tabla. Una fila no pasa a `adaptado` sin
   anotar qué cambió entre SEGA y SNES.
4. Al cerrar cada fase del plan, revisar el otro repositorio y actualizar el
   último commit revisado.

Estados: `pendiente`, `en curso`, `adaptado`, `descartado`, `por evaluar`.
Propiedad del código: `propio` (verificado), `probable` (blame lo indica pero hay
dudas), `mixto`, `externo`, `sin verificar`.

## Última revisión

| Repositorio revisado | Último commit revisado | Fecha | Alcance |
|---|---|---|---|
| snes9xrd (`dev`) | `c27a5778` | 2026-11-19 | Revisión inicial parcial: solo commits propios de `qt/` |
| Xfusion | — | — | Sin revisar desde snes9xrd |

## De snes9xrd a Xfusion

| ID | Mejora | Origen en snes9xrd | Propiedad | Fase en Xfusion | Qué cambia entre SNES y SEGA | Estado |
|---|---|---|---|---|---|---|
| P-001 | Biblioteca con tiempo jugado, último acceso y caché del escaneo | `528bc22e`, `c46f60b1`; `EmuGameList.cpp` | sin verificar: el archivo entró en el commit masivo `726dbe83` (43 archivos) | 4 | Seis sistemas con una carpeta cada uno, identificación por hash del contenido y archivos `.zip` | pendiente |
| P-002 | Deshacer carga de estado | `9861f44e` | sin verificar | 3 | Estados atados al core y su versión (sección 3.2); no hay garantía entre cores | pendiente |
| P-003 | Modo portátil: configuración junto al ejecutable si es escribible | `be0156cc` | sin verificar | 2 | Xfusion guarda todo junto al ejecutable (ADR-0011), sin `%APPDATA%`; `XFUSION_DATA_DIR` lo sustituye. Queda por decidir qué hacer si la carpeta no es escribible | por evaluar |
| P-004 | Panel de controles: borrar un binding, resaltado de teclado y de ejes, diagrama del mando | `23d96c07`, `09b22555`, `0a8e5777`; `EmuBinding.cpp` | mixto (53,5 % del dueño, 46,5 % BearOso) | 2 | Diagrama de mando de 3 y 6 botones SEGA; bindings sobre `PadButton` y SDL3. No tomar la estructura de `EmuBinding` | adaptado en parte (ADR-0010): tabla de 4 ranuras, captura, borrar, Auto-Assign y diagrama propios; faltan resaltado de ejes en vivo y dos jugadores |
| P-005 | Tema visual por plantilla con tokens por tema | `qt/src/resources/theme.qss` (antes `duckstation.qss`), `EmuTheme`; commit `9861f44e` | externo en origen: `snes9xrd` tomó DuckStation como base (dato del dueño; las primeras versiones tenían archivos `duckstation*`). La licencia actual de DuckStation es CC BY-NC-ND 4.0 (verificado en su `LICENSE` de `master`); no está verificada la de la versión que se tomó | 2 | DuckStation es la referencia de diseño. Xfusion se inspira en su estructura y apariencia (paneles, menús, flujo) con implementación propia; no toma código ni recursos suyos | por evaluar |
| P-006 | RetroAchievements | `c06846a5`, `c93536f8`; `AchievementsPanel.cpp` | probable: `blame` 100 % del dueño, pero el commit de origen tiene 52 archivos | posterior a la v1 | Depende de `rcheevos` (MIT) y de los IDs de consola SEGA; no todos los cores exponen lo necesario | por evaluar |
| P-007 | Grabación AVI | `1db8be3f`; `AviRecorder.cpp` | probable: `blame` 100 % del dueño, commit de origen de 9 archivos | posterior a la v1 | Resolución, relación de aspecto y tasa de fotogramas varían por sistema | por evaluar |
| P-008 | Netplay y rollback | `bd1c9d67`, `e3795fcd` | sin verificar | — | Es específico de SNES: usa los estados del núcleo nativo. Con cores libretro no es trasladable | descartado |
| P-009 | Hilo de emulación y carreras al cargar el primer juego | `dd294b93` ("close emu-thread races"); clase `EmuThread` | sin verificar | 2 (ya implementado en Xfusion, ADR-0005) | El hilo de Xfusion se diseñó sin leer el de `snes9xrd`. Comparar enfoques (sincronización, pausa, cierre) y revisar si hay casos que Xfusion no cubre | por evaluar |

## De Xfusion a snes9xrd

| ID | Mejora | Origen en Xfusion | Propiedad | Qué cambia entre SEGA y SNES | Estado |
|---|---|---|---|---|---|
| X-001 | Leer el teclado por estado físico en cada fotograma, para no perder pulsaciones tras diálogos o cambios de foco | `MainWindow::pollKeyboard` | propio | Comprobar primero si `snes9xrd` tiene ese síntoma; allí las entradas pasan por `SDLInputManager` | por evaluar |
| X-002 | Volcado periódico de SRAM y escritura atómica (`QSaveFile`) | pendiente en Xfusion (Fase 3) | propio | Cuando exista en Xfusion, comparar con `Memory.SaveSRAM` de `snes9xrd` | pendiente |
| X-003 | Ritmo de emulación por la cola de audio, arranque del audio con búfer lleno, y medición con `--stats` y `scripts/stress-ui.ps1` | `EmuThread`, `AudioOutput`, ADR-0005 | propio | Comprobar con sus propias mediciones si `snes9xrd` tiene vacíos de audio al arrancar o una cola de audio de mayor latencia; allí el audio usa cubeb | por evaluar |
