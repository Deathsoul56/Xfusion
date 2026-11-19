---
title: "ADR-0005: Hilo de emulación y sincronización por audio"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["architecture", "performance", "audio", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0005: Hilo de emulación y sincronización por audio

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobada e implementada el 2026-11-19. Los resultados medidos están en Implementation Notes.

## Context

El emulador corría en un `QTimer` del hilo de la interfaz y la temporización salía del reloj del sistema (acumulador de hasta 4 fotogramas por tick). La hipótesis inicial era que arrastrar la ventana o abrir menús en Windows podía detener los temporizadores y que el audio no marcaba el ritmo.

Antes de implementar se midió el diseño anterior (IMP-001). La hipótesis principal **no se reprodujo**: arrastrando la ventana y abriendo menús durante 30 segundos se mantuvieron 60 fotogramas por segundo. Lo que sí apareció:

- Entre 10 y 15 vacíos de audio en el primer segundo tras abrir un juego (el dispositivo empezaba a reproducir con la cola vacía).
- Una cola de audio estable pero grande, de 4704 a 9408 bytes (27 a 53 ms).
- Mayor consumo de CPU (72,1 s en 10 minutos), por el temporizador de 2 ms y por una copia de imagen nueva en cada fotograma.
- La emulación seguía acoplada al hilo de la interfaz.

## Decision

Se implementó el diseño propuesto, con la justificación corregida a la luz de la medición:

1. `EmuThread` ejecuta `retro_run` en un hilo propio. El hilo también lee el teclado y publica la entrada en botones atómicos.
2. **El ritmo lo marca la cola de audio de SDL3**: el hilo espera mientras la cola tenga más de 1,5 fotogramas de audio. Si el core no produce audio en un fotograma o el dispositivo se detiene, cae al reloj del sistema (`SDL_DelayPrecise`); las esperas por la cola de audio son de 0,5 ms con `SDL_DelayNS`.
3. El dispositivo de audio **arranca en pausa** y se reanuda cuando la cola se ha llenado, para no reproducir sobre un búfer vacío.
4. El video pasa a la interfaz por un buzón de tres búferes (`VideoWidget::submitFrame`), sin crear una imagen nueva por fotograma.
5. Política de acceso al core: cargar, descargar, recargar y leer o escribir SRAM se hacen con el hilo detenido; `reset` pasa por `EmuThread::withCore`. La pausa detiene el hilo y el dispositivo de audio.
6. La medición queda como herramienta permanente: `xfusion.exe --stats` y `scripts/stress-ui.ps1`.

## Consequences

### Positive

- **POS-001**: La emulación queda independiente de la interfaz.
- **POS-002**: El audio no empieza con la cola vacía y la latencia de audio baja.
- **POS-003**: Menos copias de imagen por fotograma y menos CPU.
- **POS-004**: La medición es repetible.

### Negative

- **NEG-001**: Introduce concurrencia: los callbacks de libretro corren en otro hilo y hay que respetar la política de acceso al core (Decision, punto 5).
- **NEG-002**: Si la interfaz está ocupada, la emulación no se frena, pero puede mostrar menos fotogramas de los emulados (el buzón conserva solo el último).
- **NEG-003**: Complica las pruebas y la recarga del core (ADR-0004), que debe hacerse con el hilo detenido.
- **NEG-004**: La emulación va unos 1,5 fotogramas por delante del audio que suena; es un compromiso entre desfase y tolerancia al retraso del planificador (`kTargetQueuedFrames` en `EmuThread.cpp`).

## Alternatives Considered

### Mantener el `QTimer` en el hilo de la interfaz

- **ALT-001**: **Description**: Estado anterior.
- **ALT-002**: **Rejection Reason**: No fallaba en las pruebas de arrastre, pero arrancaba el audio con la cola vacía, gastaba más CPU y dejaba la emulación atada a la interfaz.

### Hilo de emulación con reloj del sistema

- **ALT-003**: **Description**: Hilo propio, pero con la misma temporización por reloj.
- **ALT-004**: **Rejection Reason**: Resuelve el acoplamiento, no la deriva entre el reloj del sistema y el del dispositivo de audio.

## Implementation Notes

- **IMP-001**: Hecho. Medición del diseño anterior y del nuevo con `--stats`, en un solo equipo, con Sonic & Knuckles y el dispositivo de audio predeterminado:

  | Medida | Antes (`QTimer` en la interfaz) | Ahora (hilo + audio) |
  |---|---|---|
  | Fotogramas por segundo, 10 min en reposo | 60,003 y 60,007 (mín. 60, máx. 61) | 60,003 (mín. 59, máx. 61) |
  | Vacíos de audio tras el arranque | 0 | 0 |
  | Vacíos de audio en el primer segundo | 10 y 15 (en las dos corridas que lo registraron) | 0 |
  | Cola de audio (bytes) | 4704 a 9408 | 2940 a 4116 |
  | CPU en 10 minutos | 72,1 s | 50,6 s |
  | Estrés de arrastre de ventana y menús | 30 s: mín. 59, media 60,2 | 10 min: mín. 58, media 60,2, 0 vacíos de audio |

- **IMP-002**: Hecho. La política de acceso al core, pausa y descarga está en Decision, punto 5.
- **IMP-003**: Hecho. Criterio de éxito cumplido de forma literal: 10 minutos arrastrando la ventana y abriendo menús sin vacíos de audio, con 596 muestras, mínimo de 58 fotogramas por segundo y cola mínima de 2940 bytes.
- **IMP-004**: Comprobado en la aplicación real: entrada desde el hilo (Z da el latigazo antes y después de reabrir un juego), pausa (0 fotogramas durante la pausa), reinicio, reabrir un juego, abrir cuatro sistemas distintos en una misma sesión (Master System, Game Gear, 32X, Genesis), guardado de SRAM al cerrar y salida con código 0.
- **IMP-005**: Límites: una sola máquina, un juego en las mediciones largas, un solo dispositivo de audio y solo Windows. No se probó un dispositivo de audio desconectado en caliente (el código cae al reloj del sistema tras 4 fotogramas de espera, sin verificar).
- **IMP-006**: En la primera corrida de 10 minutos del diseño anterior el registro se detuvo a los 357 s sin caída ni evento del sistema. No se repitió en una segunda corrida ni en ninguna del diseño nuevo; queda sin explicar y no se atribuye a ningún diseño.

## References

- **REF-001**: ADR-0004.
- **REF-002**: `src/EmuThread.*`, `src/AudioOutput.*`, `src/VideoWidget.*`, `scripts/stress-ui.ps1`.
- **REF-003**: `frontend_sega_multiemulador.md`, sección 10.
