---
title: "ADR-0020: Iconos, hoja de estilos y temas a la par de snes9xrd"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["ui", "icons", "themes", "licensing", "snes9xrd", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0020: Iconos, hoja de estilos y temas a la par de snes9xrd

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20. Alcance elegido por el propietario: solo la apariencia; las pantallas y funciones que Xfusion aún no tiene (biblioteca de juegos, trucos, juego en red) quedan para fases siguientes.

## Context

La interfaz de Xfusion se veía distinta de la de `snes9xrd`: menús sin iconos y con menos espacio, lista lateral de Settings sin iconos, filas de la tabla de controles sin dibujo y solo siete temas. El árbol `qt/` de `snes9xrd` está bajo la licencia Snes9x (ADR-0009) y su historial no permite saber qué archivos son solo del propietario.

## Decision

1. **Iconos:** los de Material (Apache-2.0), descargados del repositorio de Google con su licencia (`src/resources/icons/`, `THIRD_PARTY_NOTICES.md`). Una sola serie en negro se pinta con el color del texto del tema (`Icons.cpp`, `QSvgRenderer`), así que no hay juegos blanco/negro; `refreshThemedIcons()` los repinta al cambiar de tema. Van en los mismos sitios que en `snes9xrd`: Open File, Reset Game, Close Game, Exit, Pause, Hard Reset, Fullscreen, las páginas del menú Options y la lista lateral de Settings, más teclado y mando en las celdas de las tablas.
2. **Botones del mando:** flechas para la cruceta, un disco con letra (A, B, C, X, Y, Z, M) para los botones, "play" para Start y un rayo para los Turbo. Se dibujan en código; los diagramas del SNES de `snes9xrd` no se copian (licencia Snes9x y no valen para un mando SEGA).
3. **Hoja de estilos:** `theme.qss` es nuevo y se parametriza con los colores del tema (menús, barra, tablas, botones, grupos, cuadros de texto, deslizadores, barras de desplazamiento). Se reproduce el aspecto y las medidas, no el texto de `theme.qss` ni de `EmuTheme.cpp`.
4. **Temas:** 22, con los mismos nombres, el mismo orden y los mismos colores que `snes9xrd` (los colores son datos). "Follow System" elige entre Light y Dark Fusion (Blue), sin volver nunca al estilo nativo.
5. **Ajustes:** la versión del archivo sube a 3; un archivo de versión 2 traduce su índice de tema por nombre (Dark Gray → Dark Fusion (Gray), AMOLED → AMOLED, etc.).
6. **Dependencias:** `Qt6::Svg` y el complemento de formato `QSvgPlugin` (los tics y las flechas del estilo son SVG).

## Consequences

- Positivas: aspecto comparable con `snes9xrd` en oscuro y en claro, comprobado en la aplicación real (menús File y Options, Settings General y Controllers con el tema oscuro y con Baby Pastel). Pruebas: 83 (se añade la migración de temas).
- Negativas: el ejecutable crece un poco por Svg; las medidas se copiaron mirando la ventana, no píxel a píxel; un archivo de ajustes de versión 3 ya no se abre en una versión anterior.
- No incluido: pantalla Library, menús Cheat y Netplay, tamaño y posición inicial de la ventana, botones redondos en las tablas (en Xfusion llenan la celda).