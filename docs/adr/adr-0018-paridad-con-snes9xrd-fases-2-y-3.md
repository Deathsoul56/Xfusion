---
title: "ADR-0018: Fases 2 y 3 a la par de snes9xrd (emuladores gemelos)"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["parity", "snes9xrd", "settings", "input", "states", "decision"]
supersedes: "ADR-0015"
superseded_by: ""
---

# ADR-0018: Fases 2 y 3 a la par de snes9xrd (emuladores gemelos)

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20. Sustituye al ADR-0015 (atajos con una sola secuencia) y amplía los ADR-0010, 0011, 0013, 0014, 0016 y 0017. Revisión de `snes9xrd` (rama `dev`, commit `c27a5778`), solo para entender comportamiento; el código es propio (ADR-0009, ver `docs/porting-ledger.md`, filas P-010 a P-018).

## Context

El propietario pidió que Xfusion y `snes9xrd` sean emuladores gemelos, respetando las particularidades de cada sistema, y que las Fases 2 (controles y presentación) y 3 (persistencia y estados) estén a la par. Antes, Xfusion tenía un diálogo con cuatro páginas, un atajo por acción, un solo nivel de deshacer en memoria, diez slots y sin velocidad, rewind, capturas ni ubicaciones de archivos configurables.

## Decision

Lo que se iguala y lo que se adapta a SEGA y a libretro:

1. **Ventana de ajustes** con las mismas páginas y el mismo orden: General, Display, Sound, Emulation, Controllers, Shortcuts, Files (Achievements queda para más adelante); Restore Defaults actúa sobre la página visible. Se aplica al pulsar OK (en `snes9xrd` se aplica en vivo y Cancelar revierte; aquí OK guarda de inmediato, no al salir). Menús: File (Recent Files, Save/Load Game Position con bancos, Save/Load with Preview, Save Other, ROM Information), Emulation, View (Set Size 1x–10x), Options, Help.
2. **Atajos (sustituye al ADR-0015).** `ShortcutMap`: 44 acciones, cuatro enlaces por acción mezclando teclas con modificadores exactos y entradas de mando (botón, hat, eje); acciones de mantener (avance rápido, rewind) y de pulsar; los duplicados se resaltan en amarillo y siguen funcionando; Reset por fila, Clear por fila, "Reset to Default" por columna y "Clear All"; `Esc` borra una celda al capturar y sale de pantalla completa en el juego (fijo). Se despachan con un filtro de eventos de la aplicación (solo con la ventana principal activa) y un temporizador de 8 ms para los mandos; soltar una tecla termina las acciones de mantener que empezó, con cualquier modificador. Los valores por defecto son los de `snes9xrd`, con dos excepciones: "Swap Controllers 1 and 2" es `Ctrl+6` (en SEGA `6` es la tecla Z del jugador 2) y el volumen (que allí no tiene atajo) va en `Ctrl+RePág` y `Ctrl+AvPág`, porque los corchetes dependen de la distribución del teclado. Los atajos que dependen de la distribución (`=`, `-`, `\`) se pueden reasignar.
3. **Controles.** Se añaden "Controller Ports" (un mando o dos: con uno el segundo puerto queda vacío para el core, `picodrive_input2 = None`), el combo "Set", los menús "Edit" (borrar el mando actual o todos) y "Auto-Assign" (por columna: teclado por defecto o cada mando conectado), la etiqueta "raw SDL", iconos de tecla o mando en las celdas, filas Turbo (A, B, C, X, Y, Z; Button 1 y 2 en Master System; ciclo de cuatro fotogramas) y "Swap Controllers 1 and 2" (solo de la sesión, se deshace al cerrar el juego). Se mantiene lo propio de Xfusion: el mando físico de cada jugador (automático o por nombre) y el tipo de mando de 3 o 6 botones. "Pause when inactive" y "Allow gamepad input while not focused" se igualan.
4. **Velocidad (`EmuThread`).** Métodos Sound Synchronization (el de siempre), Timer, Timer with Frame Skipping y None; frame rate fijo (0 = el del core), avance rápido con salto de fotogramas (9 por defecto: 10×, el sonido sube de tono con el cambio de frecuencia de SDL o se silencia), Speed Up/Down en pasos del 10 % que cambian la frecuencia de reproducción del audio, frame skip fijo y Frame Advance con el juego en pausa. Un salto de fotogramas solo se ve como `retro_run` sin entregar vídeo (y `GET_AUDIO_VIDEO_ENABLE` avisa al core). Con Sound Synchronization el salto fijo ahorra dibujo pero no acelera.
5. **Rewind (`RewindBuffer`).** Historia de diferencias XOR con códigos de ceros entre instantáneas del core (`retro_serialize`) cada N fotogramas (5 por defecto), con un presupuesto de memoria (10 MB por defecto) donde se descarta lo más antiguo; mantener `Y` retrocede un paso por fotograma mostrado, en silencio, y termina solo al vaciarse la historia. Es lo único que difiere de `snes9xrd` por dentro: allí las instantáneas incluyen una captura de pantalla y su tamaño se redondea a potencia de dos.
6. **Estados.** Slot actual 0–999 con aviso "Current slot: N [used/empty]", diez ranuras rápidas (0–9) que no piden confirmación, ruta `<juego>.NNN.state`, **deshacer en archivo** (`.undo.state`, un nivel, no se renueva al deshacer), **`.oops.state`** automático antes de un reinicio o una carga si pasaron más de cinco minutos sin guardar ni cargar (File > Load Game Position > Oops File), **`.resume.state`** al cerrar el juego si "Save State On Game Close" está activo (se carga al abrirlo), "Ask for confirmation…" opcional, y vista previa: el estado guarda una miniatura PNG (formato 2; el 1 se sigue leyendo) y `Save/Load with Preview` muestra diez ranuras por banco. Se conserva lo propio de Xfusion: rechazo de estados de otro juego o core, aviso si cambia la versión del core y copia `.bak`.
7. **Ubicaciones (Files).** SRAM, estados, capturas y BIOS: carpeta de datos (por sistema, junto al ejecutable), carpeta del juego, o carpeta propia. Capturas F12: PNG del fotograma emulado sin filtros, `<juego>-AAAA-MM-DD-HH-MM-SS.png`, en `screenshots/<sistema>/`.
8. **Presentación.** Mantener proporción, escalado entero, "Stretch image", filtro bilineal, proporción (core, 4:3, píxeles cuadrados), filtros de software (Scanlines, Sharp Bilinear), mensajes en pantalla apilados (cuatro, dos segundos, esquina a elegir, tamaño 8–256 pt) con las superposiciones de frame rate, iconos de pausa / avance / rewind, hora y botones pulsados, volumen aparte para el avance rápido, silenciar durante el avance rápido o el avance de fotogramas, dispositivo y frecuencia de salida y tamaño del búfer, y siete temas de paleta propia (la apariencia de DuckStation no se toma: licencia, P-005).
9. **SRAM:** intervalo configurable (30 s por defecto; 0 = solo al cerrar). Se guarda también al usar File > Close Game, a diferencia de `snes9xrd` (allí el cierre pierde lo no volcado).

Lo que no se porta, a propósito: Netplay y rollback (P-008), canales SPC, Super Scope y ratón SNES, `Grab Mouse` y las capas BG y sprites (PicoDrive no las ofrece), grabación de películas y AVI, trucos y RetroAchievements (posteriores a la v1), y la biblioteca de juegos (Fase 4). Los dos errores que se vieron en `snes9xrd` (nombres de filtro de sonido y de tema desalineados al guardar; `Restore Defaults` de atajos sin refrescar los enlaces) no se repiten.

## Consequences

- Positivas: mismo diseño, mismos atajos por defecto y mismos nombres de archivo y de ajuste donde tiene sentido; las funciones nuevas están cubiertas por 78 pruebas (atajos, ajustes, rewind, filtros, estados, miniaturas, jugadores, turbo, mandos virtuales) y se comprobaron en la aplicación real con la ROM propia.
- Negativas: `AppSettings` crece (unos cuarenta campos y los cuatro enlaces de cada atajo), el archivo de ajustes pasa a la versión 2 (el 1 se lee), `MainWindow` concentra mucha lógica de sesión y los estados de `snes9xrd` y de Xfusion no son intercambiables.
- Sin probar: dos mandos físicos, juegos de 32X y Sega CD con estados o rewind, el volumen de avance rápido por oído, y plataformas distintas de Windows.
