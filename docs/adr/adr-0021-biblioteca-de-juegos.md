---
title: "ADR-0021: Biblioteca de juegos"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["library", "ui", "persistence", "snes9xrd", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0021: Biblioteca de juegos

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20 (Fase 4 del plan).

## Context

Xfusion abría los juegos solo desde un selector de archivos y mostraba una pantalla negra al arrancar; `snes9xrd` tiene una página Library con buscador, lista de juegos y "Add Game Folder". La Fase 4 pide escaneo sin bloquear, búsqueda, filtros, orden, último acceso y tiempo de juego, sin tocar las ROMs y sin duplicados innecesarios.

## Decision

1. **Qué se ve:** la página Library es lo que se muestra mientras no hay juego abierto (Close Game vuelve a ella). Sin juegos, el estado vacío "Welcome to Xfusion / No games found / Add Game Folder..." como en `snes9xrd`. Con juegos, una tabla (Name, System, Size, Time Played, Last Played, Folder; las columnas se ocultan con el menú de la cabecera), buscador por nombre, filtro por sistema, orden por cualquier columna, Enter o doble clic para jugar y menú contextual (Play, Show in Folder).
2. **Carpetas:** `files.library_folders` en `settings.json`; se gestionan en Options > Files > Library Folders y con el botón del estado vacío. Subcarpetas incluidas.
3. **Escaneo:** en un hilo propio (`scanLibrary`, sin Qt Widgets, con pruebas); se cancela y se repite al cambiar las carpetas. Un archivo alcanzado dos veces (carpetas anidadas o repetidas, enlaces) cuenta una vez; archivos con el mismo contenido (la huella de `GameSaves::fingerprint`) forman una sola entrada con las otras rutas en el tooltip; las pistas que nombra un `.cue` no son juegos (si no, cada `.bin` de un disco saldría como juego de Genesis); una carpeta que no existe se avisa y no detiene el escaneo; un zip sin juego dentro se recuerda como "no es un juego".
4. **Datos:** `library.json` en la carpeta de configuración: estadísticas por id de juego (tiempo, última vez) y la caché del escaneo (ruta, tamaño, fecha, id, sistema) para no abrir de nuevo cada archivo. Las estadísticas se indexan por id, igual que los saves, así que sobreviven a mover o renombrar el juego o a quitarlo de la carpeta un tiempo. Escritura atómica; un archivo ilegible se conserva como `library.json.bad` antes de sustituirlo. Las ROMs solo se leen.
5. **Tiempo de juego:** cuenta el tiempo con el juego en marcha, no en pausa (manual, sin foco, minimizado o con un diálogo abierto); se añade al cerrar el juego y cada minuto, para perder como mucho un minuto si falla. Se cuenta también para juegos abiertos con File > Open File, estén o no en la biblioteca.
6. **Teclas:** con el foco en un campo de texto (el buscador) los atajos del frontend no actúan.
7. **Aspecto:** el de `snes9xrd` (cabecera, buscador, zona punteada, tabla sin rejilla con filas alternas), escrito de nuevo en `theme.qss`; no se copió su código (ADR-0020).

## Consequences

- Positivas: 15 pruebas del escaneo y los datos (más una de ajustes), comprobado en la aplicación real con 660 ROMs: la primera exploración tarda unos segundos en segundo plano y las siguientes salen de la caché; el tiempo y la última vez aparecen tras jugar.
- Negativas: la huella lee el principio y el final de cada archivo nuevo (unos segundos con 660 ROMs en el primer escaneo); no se detectan duplicados con contenido igual pero distinto tamaño de cabecera; el sistema se deduce solo por extensión (un `.bin` de disco sin `.cue` se vería como Genesis).
- Pendiente: corregir a mano el sistema y el core de un juego (Fase 5), región y metadatos del cartucho, escaneo periódico o al detectar cambios en las carpetas.