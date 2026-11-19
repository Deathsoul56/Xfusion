---
title: "ADR-0010: Bindings de entrada por grupo de plataformas"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["input", "settings", "sdl3", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0010: Bindings de entrada por grupo de plataformas

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 (diseño pedido por el propietario; probado con teclado y con un 8BitDo).

## Context

Hasta la Fase 1 las teclas estaban fijas en `MainWindow` y solo existía el pad de Genesis. Se pidió un panel de controles como el de `snes9xrd` (sección "Controller Ports", tabla `Binding #1–#4`, diagrama del mando, Auto-Assign), con una opción padre "plataforma": Genesis, Sega CD y 32X comparten el mismo mando, y Master System, Game Gear y SG-1000 comparten otro.

## Decision

1. **Dos grupos de entrada** (`InputGroup`): `Genesis` (Genesis, Sega CD, 32X; 12 acciones) y `MasterSystem` (SG-1000, Master System, Game Gear; 7 acciones: cruz, Button 1, Button 2, Start/Pause). `SystemInfo` es la única fuente de verdad: cada `SystemDef` declara su grupo y los títulos de la interfaz se derivan de ahí. SG-1000 va en el grupo de Master System porque comparte el mismo pad de 2 botones (supuesto a confirmar).
2. **Un conjunto de bindings por grupo**, con 4 ranuras por acción. Una fuente (tecla, botón, hat o eje) solo puede estar en una acción del grupo: asignarla la mueve y deja la anterior en `None`.
3. **Persistencia** en `<datos>/config/input.json` (JSON versión 1, guardado atómico con `QSaveFile`). Un archivo ausente o ilegible carga los valores por defecto; no se sobrescribe sin que el usuario pulse OK.
4. **Dispositivos con SDL3** (`SDL_INIT_GAMEPAD`): los bindings guardan índices del joystick (botón, hat, eje con signo), no nombres de un mando concreto, y valen para cualquier mando conectado. Auto-Assign usa el mapeo estándar de gamepad de SDL (`SDL_GetGamepadBindings`) y rellena la ranura 1 (botones y hat) y la 3 (stick izquierdo). El teclado se lee por estado físico (`GetAsyncKeyState` en Windows) y se ignora cuando la ventana principal no está activa, para que el diálogo de ajustes no maneje el juego.
5. **Lectura en el hilo de emulación** (`InputManager::poll(grupo, ventanaActiva)`, seguro entre hilos); el botón lógico llega al core como `PadButton` (ADR-0007). El grupo activo lo fija `openGame` según el sistema.
6. **Panel de ajustes** (`SettingsDialog` con una página `ControllersPage`): combo de plataforma, diagrama (`ControllerDiagram`, dibujado con `QPainter`, sin recursos de terceros), tabla de bindings y captura con tiempo límite de 8 s (Esc cancela, clic derecho borra). Trabaja sobre una copia: OK aplica y guarda, Cancelar descarta.
7. **Pruebas** con Qt Test y joysticks virtuales de SDL (`tests/InputTests.cpp`), desactivando los controladores físicos para que un mando real no altere los resultados.
8. **Mando de 3 o 6 botones** (solo grupo Genesis): `InputConfig::padType()`, guardado como `genesis_pad` (3 o 6) en `input.json`; por defecto 6. En 3 botones el panel oculta X, Y, Z y Mode (`InputAction::sixButtonOnly`). El perfil del core indica qué opciones lo seleccionan (`CoreProfile::padTypeKeys` y valores) y `LibretroCore::setPadType` las cambia y avisa con `GET_VARIABLE_UPDATE`, así que el cambio vale en caliente, sin reiniciar el juego. Se aplica al abrir un juego y al pulsar OK.
9. **Diagrama con imágenes** (`resources/controllers/genesis_3button.png`, `genesis_6button.png` y `mastersystem.png`, registradas en `resources.qrc`): zonas sensibles medidas sobre cada imagen. **Los botones se iluminan cuando se aprietan**, no cuando se selecciona su fila: `ControllersPage` consulta cada 30 ms `InputManager::pressedWith(grupo, copia de bindings en edición)` y se lo pasa al diagrama; durante una captura se ilumina el botón que recibirá el binding. El clic en un botón de la imagen selecciona su fila. Lo que no está en la cara del mando se dibuja como etiqueta encima de la imagen, también clicable y con el mismo brillo: MODE (la imagen de 6 botones no lo trae) y "PAUSE (console)" en Master System, cuyo botón de pausa está en la consola y no en el mando.

## Consequences

- Positivas: mismo flujo que `snes9xrd` con la jerarquía de plataformas pedida; añadir un sistema solo requiere asignarle un grupo; la lógica de bindings se prueba sin hardware.
- Negativas: los bindings de mando son índices crudos, así que dos mandos con distinta numeración comparten los mismos valores; por ahora un solo jugador (sin "Controller Ports"); el teclado solo tiene lectura física en Windows.
- Pendientes: dos jugadores, atajos de teclado del frontend, prueba de desconexión de mandos, revisar si 32X y Sega CD necesitan ajustes propios, y confirmar el origen y la licencia de las dos imágenes de mando (ver `THIRD_PARTY_NOTICES.md` pendiente).

## Implementation notes

- En Qt `slots` es una macro: el acceso a las ranuras se llama `bindings()`.
- Diseño inspirado en DuckStation y `snes9xrd` solo como referencia de uso; el código es propio (ADR-0009).
