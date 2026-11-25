---
title: "ADR-0019: Filtros gráficos de software a la par de snes9xrd"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["video", "filters", "licensing", "snes9xrd", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0019: Filtros gráficos de software a la par de snes9xrd

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20. Completa la página Display del ADR-0018.

**Ampliado por el ADR-0032 (2026-11-25):** con el dibujo por GPU, HQx, xBRZ, EPX y los escaladores simples se hacen en la tarjeta gráfica (la CPU conserva NTSC, 2xSaI, Super 2xSaI y Super Eagle y es la ruta del driver de software).

## Context

`snes9xrd` ofrece 22 filtros de software (Scanlines, Simple, Super Eagle, 2xSaI, Super 2xSaI, EPX, HQ, xBRZ, SharpBilinear4x y cuatro NTSC). Xfusion tenía solo Scanlines y Sharp Bilinear. Los filtros de `snes9xrd` mezclan licencias: xBRZ es GPL-3, snes_ntsc es LGPL-2.1, el de hqx declara ISC y los demás (2xSaI, EPX, Super Eagle, blit, sharpbilinear_flexible) llevan la licencia Snes9x, incompatible con GPL-3 (ADR-0009).

## Decision

1. **La misma lista y los mismos nombres** (`allVideoFilters()`), 22 entradas, elegibles en Settings > Display > Software Filters y guardadas con una clave estable en `settings.json` (`sharp_bilinear` sigue siendo el nombre de SharpBilinear4x).
2. **De terceros, con su licencia, en `third_party/`** (ver `THIRD_PARTY_NOTICES.md`): xBRZ (GPL-3, de 2x a 6x), snes_ntsc (LGPL-2.1, Composite, RF, S-Video y RGB) y hqx (HQ2x, HQ3x y HQ4x, con RGB565 y el orden de canales de Qt). El resultado enlazado con xBRZ queda bajo GPL-3.0, la licencia ya elegida para Xfusion.
3. **Escritos para Xfusion**, sin tocar código con licencia Snes9x: Simple 2x/3x/4x, Scanlines, Sharp Bilinear 4x, EPX, Super Eagle, 2xSaI y Super 2xSaI. 2xSaI sigue las reglas de decisión del algoritmo publicado; **Super Eagle y Super 2xSaI son variantes propias** (Eagle con esquinas suavizadas; 2xSaI con mezclas 3 a 1), no copias bit a bit de los originales, y así se dice en la interfaz y en los avisos.
4. **Dónde se aplican:** en el hilo de la interfaz, al recibir cada fotograma (`VideoWidget`); el hilo de emulación no los paga. La disposición (escalado entero, proporción) usa el tamaño original del fotograma, y las capturas de pantalla y las miniaturas de los estados usan el fotograma sin filtrar.
5. **NTSC:** se usa la biblioteca tal cual (hecha para el SNES, con su ritmo de píxel); en Mega Drive y Master System es una aproximación. No se ofrece la opción de líneas de exploración de los NTSC.

## Consequences

- Positivas: misma lista que `snes9xrd`, probada con cuatro pruebas (lista y claves, tamaño y color de cada escalador, diagonales, NTSC) y comprobada en la aplicación real (2xSaI, HQ3x, 4xBRZ y NTSC Composite con un juego de Genesis; 60 fotogramas por segundo emulados con cada filtro en Game Gear).
- Negativas: los filtros grandes (xBRZ 5x y 6x, HQ4x) cuestan CPU en la interfaz con pantallas de 320×224 y no se midió en equipos lentos; el proyecto enlaza ahora código GPL-3.0 sin la excepción para Snes9x de xBRZ; la adaptación a RGB565 de hqx tiene autoría sin verificar (ver `THIRD_PARTY_NOTICES.md`).
- Limitaciones: Super Eagle y Super 2xSaI no son idénticos a los originales; no hay shaders por GPU (siguen fuera del alcance).
