---
title: "ADR-0028: RetroAchievements en modo casual"
status: "Accepted"
date: "2026-11-22"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["retroachievements", "rcheevos", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0028: RetroAchievements en modo casual

## Status

Accepted. El análisis de factibilidad está en la Fase 7 de `frontend_sega_multiemulador.md`.

## Context

`snes9xrd` ya integra RetroAchievements con rcheevos. El análisis de la Fase 7 concluyó que en Xfusion es viable en modo casual para los seis sistemas, y que el modo hardcore exige la aprobación de RetroAchievements (al menos 6 meses publicado, user agent propio y bloqueo de rebobinado, cámara lenta, avance de fotograma, carga de estados y trucos). El propietario decidió: hardcore queda para el futuro, los juegos parcheados no tienen logros, y se prueba con una cuenta de pruebas.

## Decision

1. **Solo casual.** `rc_client_set_hardcore_enabled(0)` siempre; la interfaz no ofrece hardcore y lo dice. Un cliente no aprobado usado en hardcore pondría la cuenta del jugador en «Untracked».
2. **rcheevos** (MIT) vive en `third_party/rcheevos/` (copia de la de `snes9xrd`, sin RAIntegration ni cliente externo) y se compila dentro de `xfusion_input`. Con `rc_libretro` se lee la memoria de los cores a través de los mapas que publican (`SET_MEMORY_MAPS`, que `LibretroCore` ahora atiende) y de `retro_get_memory_data`.
3. **Hilos:** `Achievements` guarda un mutex recursivo y toda llamada a `rc_client_t` se hace con él: la hebra de emulación llama a `frame()` tras cada fotograma hacia delante (`EmuThread::setFrameHook`; no mientras se rebobina) y la hebra de la interfaz atiende cuenta, juego, respuestas de red y un temporizador de 500 ms (`rc_client_idle`, que reintenta lo que falló). La red es Qt Network en la hebra de la interfaz; una petición pedida desde la hebra de emulación se pasa con una llamada en cola. Los avisos salen como señal.
4. **Identificación:** un cartucho se identifica con los bytes del archivo (de un zip, el primer juego); un disco con su `.cue`, `.iso` o `.bin` mediante el lector de CD de rcheevos. Un juego **parcheado** no tiene logros (el hash cambia) y un disco **`.chd`** tampoco (rcheevos no lee CHD; falta libchdr), y se avisa en pantalla.
5. **Cuenta:** la contraseña solo viaja hacia el servidor; en `settings.json` queda el token de la sesión, cifrado con AES-128 en modo contador con una clave hecha del identificador de la máquina y el nombre de usuario (`AchievementsToken`). Al arrancar, con los logros activados, se inicia sesión con ese token.
6. **User agent:** `Xfusion/<versión> (<sistema>) <core>/<versión> rc_client/<versión>`, propio y estable, como pide RetroAchievements.
7. **Cambio de disco, activar con el juego abierto y errores (2026-11-22):** al cambiar de disco se avisa con `rc_client_begin_identify_and_change_media`; activar los logros o iniciar sesión con un juego abierto lo ofrece en ese momento; si falla el inicio de sesión automático sale un aviso; los ajustes de marcadores se ocultan (solo valen en hardcore).
8. **Estados y reinicios (actualizado 2026-11-22):** Reset y Hard Reset llaman a `rc_client_reset`. El progreso parcial de los logros se guarda dentro de cada estado (en su encabezado, `StateHeader::achievements`) y se restaura al cargarlo; un estado sin progreso reinicia el cálculo.
9. **Imágenes en los avisos (2026-11-22):** como en `snes9xrd`, el aviso de un logro desbloqueado y el resumen del juego al empezar llevan su insignia (la del logro, la del juego) a la izquierda del texto. La insignia se descarga de la dirección que da el servidor (o la que construye rcheevos a partir del nombre), se guarda en memoria y el aviso sale con ella; si no llega en 8 s sale sin imagen. `OsdOverlay::post` admite un icono. Comprobado con el modo Encore (que vuelve a conceder los logros ya ganados): «Achievement Unlocked: Newbie Collector» con su insignia.
10. **Interfaz:** nueva página *Achievements* en Options (la octava, como en `snes9xrd`) con cuenta, modos, avisos, indicadores y la lista de logros del juego; los avisos salen por el mismo OSD que el resto de mensajes, con la duración elegida.
11. **Sonido, subconjuntos y lista (2026-11-22):** el desbloqueo suena (un acorde de dos notas generado por `scripts/make-achievement-sound.ps1`, obra propia, incrustado como recurso y reproducido con `PlaySound`), el aviso de subconjunto completado sale y la lista de logros muestra las insignias.
12. **Código:** el módulo de `snes9xrd` es entero del propietario (ADR-0009); se adaptó y no se copió tal cual (memoria por libretro, hilos con mutex, OSD de Xfusion).

## Consequences

- Positivas: probado con una cuenta de pruebas: inicio de sesión desde la interfaz, token cifrado en `settings.json`, inicio de sesión automático, e identificación y carga de los logros de Sonic the Hedgehog (35 logros, 300 puntos), After Burner Complete (32X, 22), Sonic CD en `.iso` (100) y Dune en `.cue` (44), a 60 fps. 2 pruebas nuevas de ajustes y del cifrado del token.
- Negativas: sin hardcore ni marcadores con puntos; los `.chd` y los juegos parcheados no tienen logros; el progreso parcial de los logros se pierde al reiniciar (no al cargar un estado); un cambio de disco en caliente no vuelve a identificar el juego; el desbloqueo real se comprobó (2026-11-22): jugando el primer acto de Green Hill Zone de Sonic the Hedgehog con teclas automáticas, el servidor registró «That Was Easy» (3 puntos) y «Newbie Collector» (2 puntos) en la cuenta de pruebas.
- RetroAchievements añade «Warning: Unknown Emulator» como un logro más mientras el cliente no esté aprobado; se muestra como cualquier otro aviso.
- Sin hacer: hardcore (si algún día se aprueba), lector de CHD, guardar el progreso en los estados, lista de marcadores y presencia enriquecida.
