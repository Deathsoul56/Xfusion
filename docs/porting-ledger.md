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
| snes9xrd (`dev`) | `c27a5778` | 2026-11-20 | Fases 2 y 3: se leyó el comportamiento de ajustes, atajos, controles, estados, rewind y velocidad para adaptarlo (ADR-0018). No se corrió `porting-log.ps1 -Blame`: ninguna fila nueva puede darse por verificada en autoría, y no se copió código |

## De snes9xrd a Xfusion

| ID | Mejora | Origen en snes9xrd | Propiedad | Fase en Xfusion | Qué cambia entre SNES y SEGA | Estado |
|---|---|---|---|---|---|---|
| P-001 | Biblioteca con tiempo jugado, último acceso y caché del escaneo | `528bc22e`, `c46f60b1`; `EmuGameList.cpp` | sin verificar: el archivo entró en el commit masivo `726dbe83` (43 archivos) | 4 | Seis sistemas con una carpeta cada uno, identificación por hash del contenido y archivos `.zip` | pendiente |
| P-002 | Deshacer carga de estado | `9861f44e` | sin verificar | 3 | Estados atados al core y su versión (sección 3.2); no hay garantía entre cores | pendiente |
| P-003 | Modo portátil: configuración junto al ejecutable si es escribible | `be0156cc` | sin verificar | 2 | Xfusion guarda todo junto al ejecutable (ADR-0011), sin `%APPDATA%`; `XFUSION_DATA_DIR` lo sustituye. Queda por decidir qué hacer si la carpeta no es escribible | por evaluar |
| P-004 | Panel de controles: borrar un binding, resaltado de teclado y de ejes, diagrama del mando | `23d96c07`, `09b22555`, `0a8e5777`; `EmuBinding.cpp` | mixto (53,5 % del dueño, 46,5 % BearOso) | 2 | Diagrama de mando de 3 y 6 botones SEGA; bindings sobre `PadButton` y SDL3. No tomar la estructura de `EmuBinding` | adaptado en parte (ADR-0010): tabla de 4 ranuras, captura, borrar, Auto-Assign, diagrama con botones que se iluminan al apretarlos; faltan dos jugadores |
| P-005 | Tema visual por plantilla con tokens por tema | `qt/src/resources/theme.qss` (antes `duckstation.qss`), `EmuTheme`; commit `9861f44e` | externo en origen: `snes9xrd` tomó DuckStation como base (dato del dueño; las primeras versiones tenían archivos `duckstation*`). La licencia actual de DuckStation es CC BY-NC-ND 4.0 (verificado en su `LICENSE` de `master`); no está verificada la de la versión que se tomó | 2 | DuckStation es la referencia de diseño. Xfusion se inspira en su estructura y apariencia (paneles, menús, flujo) con implementación propia; no toma código ni recursos suyos | por evaluar |
| P-006 | RetroAchievements | `c06846a5`, `c93536f8`; `AchievementsPanel.cpp` | probable: `blame` 100 % del dueño, pero el commit de origen tiene 52 archivos | posterior a la v1 | Depende de `rcheevos` (MIT) y de los IDs de consola SEGA; no todos los cores exponen lo necesario | por evaluar |
| P-007 | Grabación AVI | `1db8be3f`; `AviRecorder.cpp` | probable: `blame` 100 % del dueño, commit de origen de 9 archivos | posterior a la v1 | Resolución, relación de aspecto y tasa de fotogramas varían por sistema | por evaluar |
| P-008 | Netplay y rollback | `bd1c9d67`, `e3795fcd` | sin verificar | — | Es específico de SNES: usa los estados del núcleo nativo. Con cores libretro no es trasladable | descartado |
| P-009 | Hilo de emulación y carreras al cargar el primer juego | `dd294b93` ("close emu-thread races"); clase `EmuThread` | sin verificar | 2 (ya implementado en Xfusion, ADR-0005) | El hilo de Xfusion se diseñó sin leer el de `snes9xrd`. Comparar enfoques (sincronización, pausa, cierre) y revisar si hay casos que Xfusion no cubre | por evaluar |
| P-010 | Panel Shortcuts: cuatro enlaces por acción con teclas con modificadores y mandos, Reset y Clear por fila, Reset to Default por columna, Clear All, duplicados resaltados | `ShortcutsPanel.*`, `EmuConfig.cpp`, `EmuBinding.*`, `EmuMainWindow::eventFilter` | sin verificar (solo se leyó el comportamiento; no se corrió `blame`) | 2 | Acciones propias de SEGA (sin SPC, capas, Super Scope); los mismos atajos por defecto salvo Swap Controllers (`Ctrl+6`) y volumen; soltar una tecla termina las acciones de mantener con cualquier modificador | adaptado (ADR-0018) |
| P-011 | Panel Controllers: Controller Ports, Set, Edit, Auto-Assign por columna, etiqueta "raw SDL", iconos, filas Turbo, Swap Controllers 1 and 2 | `ControllerPanel.*`, `BindingPanel.cpp`, `SDLInputManager.cpp` | sin verificar | 2 | Un mando o dos (no ratón, Super Scope ni multitap); mando físico por jugador y tipo de 3 o 6 botones propios de SEGA | adaptado (ADR-0018) |
| P-012 | Control de velocidad: avance rápido (mantener y alternar), Speed Up/Down, frame skip, Frame Advance, métodos de sincronización | `EmuApplication.cpp`, `Snes9xController.cpp`, `EmuCanvas::throttle` | sin verificar | 2 | Con libretro el salto de fotogramas es `retro_run` sin entregar vídeo; la velocidad cambia la frecuencia de reproducción del audio de SDL en vez de una tasa de entrada | adaptado (ADR-0018) |
| P-013 | Rewind con historia de diferencias | `statemanager.cpp`, `Snes9xController::mainLoop` | sin verificar (el núcleo es de Snes9x: no se toca) | 3 | Implementación propia (`RewindBuffer`, XOR con códigos de ceros) sobre `retro_serialize`; sin captura de pantalla en cada instantánea | adaptado (ADR-0018) |
| P-014 | Estados: slot actual 0–999, ranuras rápidas, deshacer en archivo, `.oops`, `.resume`, vista previa con miniaturas (cubre P-002) | `Snapshot`, `StatePreviewDialog.cpp`, `EmuApplication::saveState/loadState` | sin verificar | 3 | Archivos propios (formato `XFST`) con core, versión, huella del juego y miniatura; rechazo de estados ajenos; carpeta por sistema | adaptado (ADR-0018) |
| P-015 | Páginas General, Display, Sound, Emulation y Files y la ventana de ajustes | `*Panel.*`, `EmuSettingsWindow.*`, `EmuConfig.cpp` | sin verificar | 2 | Solo lo que aplica a libretro y SEGA (sin shaders ni backends Vulkan u OpenGL por ahora, sin hacks de SNES); OK guarda de inmediato | adaptado (ADR-0018) |
| P-016 | Mensajes en pantalla apilados y superposiciones (frame rate, iconos, hora, botones pulsados) | `gfx.cpp` (cola de mensajes), `snes9x_imgui.cpp` | sin verificar | 2 | `OsdOverlay` con `QPainter` sobre el vídeo; mismas reglas de cuatro mensajes y dos segundos | adaptado (ADR-0018) |
| P-017 | Capturas de pantalla (F12) | `screenshot.cpp`, `Snes9xController::takeScreenshot` | sin verificar | 3 | PNG del fotograma sin filtros en `screenshots/<sistema>/` | adaptado (ADR-0018) |
| P-018 | Volcado periódico de la SRAM con intervalo configurable | `gfx.cpp` (`AutoSaveDelay`) | sin verificar | 3 | Se guarda también al usar Close Game (allí se pierde); un `.srm` ilegible no se sobrescribe. Cubre X-002 | adaptado (ADR-0016, ADR-0018) |
| P-019 | Filtros de software (22): Simple, Super Eagle, 2xSaI, Super 2xSaI, EPX, HQx, xBRZ, SharpBilinear4x, NTSC | `filter/*` (xBRZ GPL-3, snes_ntsc LGPL-2.1, hqx ISC; el resto con licencia Snes9x), `SoftwareFilter.cpp` | xBRZ y snes_ntsc: externos con licencia propia (copiados con su licencia); hqx: cabecera ISC, adaptación a RGB565 sin verificar; el resto: reescritos | 2 | Los de licencia Snes9x se escribieron de nuevo; Super Eagle y Super 2xSaI son variantes propias; NTSC hecho para el SNES (aproximación en SEGA) | adaptado (ADR-0019) |
| P-020 | Iconos de menús, Settings y tablas de controles; hoja de estilos y 22 temas | `resources/*icons`, `theme.qss`, `EmuTheme.cpp`, `ControllerPanel.cpp` | iconos Material (Apache-2.0) descargados de Google; hoja de estilos y `Theme.cpp` escritos de nuevo; los colores de los 22 temas son los de `snes9xrd` (datos) | 2 | No se copió el QSS ni el código de `EmuTheme`: su autoría dentro de un árbol con licencia Snes9x no está verificada. Los diagramas SNES de los botones se sustituyen por flechas y letras | adaptado (ADR-0020) |
| P-021 | Página Library: estado vacío, lista con búsqueda y orden, carpetas, tiempo jugado y última vez | `LibraryPage.cpp`, `GameListWidget.cpp`, `EmuGameList.cpp`, `FoldersPanel.cpp` | reescritos a partir del comportamiento observado; sin código copiado | 2 | Árbol con licencia Snes9x, autoría sin verificar. Cambia: estadísticas por id de juego, caché de escaneo, duplicados por contenido, pistas de `.cue` excluidas, filtro por sistema | adaptado (ADR-0021) |
| P-022 | Región, título y BIOS de Sega CD reconocidos por contenido | (sin equivalente en `snes9xrd`; referencia de comportamiento: la lectura de cabeceras de los emuladores de Mega Drive) | escrito de nuevo; zlib (licencia zlib) para leer zips | 2 | Primer sector, no extensión; BIOS por contenido y región; cabecera de cartuchos y de zips | nuevo (ADR-0022) |

| P-023 | Corrección manual del sistema de un juego desde la biblioteca | (sin equivalente en `snes9xrd`, que solo tiene un sistema) | nuevo | 2 | Elección por huella en `library.json`; la cabecera corrige la extensión (32X, Game Gear) | nuevo (ADR-0023) |
## De Xfusion a snes9xrd
| P-024 | Dos cores con reglas por sistema y extracción de zips | (sin equivalente en `snes9xrd`, un solo core) | nuevo | 2 | GPGX por defecto salvo 32X; mp3 de Sega CD a PicoDrive; perfil de GPGX | nuevo (ADR-0024) |

| ID | Mejora | Origen en Xfusion | Propiedad | Qué cambia entre SEGA y SNES | Estado |
|---|---|---|---|---|---|
| X-001 | Leer el teclado por estado físico en cada fotograma, para no perder pulsaciones tras diálogos o cambios de foco | `MainWindow::pollKeyboard` | propio | Comprobar primero si `snes9xrd` tiene ese síntoma; allí las entradas pasan por `SDLInputManager` | por evaluar |
| X-002 | Volcado periódico de SRAM y escritura atómica (`QSaveFile`) | pendiente en Xfusion (Fase 3) | propio | Cuando exista en Xfusion, comparar con `Memory.SaveSRAM` de `snes9xrd` | pendiente |
| X-003 | Ritmo de emulación por la cola de audio, arranque del audio con búfer lleno, y medición con `--stats` y `scripts/stress-ui.ps1` | `EmuThread`, `AudioOutput`, ADR-0005 | propio | Comprobar con sus propias mediciones si `snes9xrd` tiene vacíos de audio al arrancar o una cola de audio de mayor latencia; allí el audio usa cubeb | por evaluar |
