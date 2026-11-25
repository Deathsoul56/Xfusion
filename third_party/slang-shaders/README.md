# Shaders de terceros para los filtros de la GPU

Copiados sin cambios de `libretro/slang-shaders` (commit `02bf134`, `edge-smoothing/`), para que xBRZ y HQx puedan hacerse en la GPU
(ADR-0032). Van dentro del programa como recursos (`:/gpu-filters/`); los presets que los usan (`src/gpu-filters/*.slangp`) y
`stock.slang` y `epx.slang` son de Xfusion.

| Carpeta | Qué es | Autores y licencia |
|---|---|---|
| `xbrz/` (`2xbrz`, `4xbrz`, `5xbrz`, `6xbrz`, `xbrz-freescale`) | xBRZ | Código de Hyllian (2011/2016) bajo licencia MIT, con ideas y código de xBRZ de Zenju (HqMAME), GNU GPL v3. Las cabeceras de cada archivo llevan los textos completos. |
| `hqx/` (`hq2x`, `hq3x`, `hq4x`, `pass1`, `pass2.inc`, `resources/*.png`) | HQx | (C) 2003 Maxim Stepin, (C) 2010 Cameron Zemek, (C) 2014 Jules Blok, GNU LGPL v2.1 o posterior. El texto está en la cabecera de cada archivo. |

Ambas son compatibles con la GPL-3.0-or-later de Xfusion. No se añade nada de lo que no tiene licencia clara (los shaders de 2xSaI,
Super Eagle y NTSC de esa colección): esos filtros siguen haciéndose en la CPU.
