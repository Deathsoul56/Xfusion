---
title: "ADR-0025: Parches IPS, BPS y UPS"
status: "Accepted"
date: "2026-11-21"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["patches", "ips", "bps", "ups", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0025: Parches IPS, BPS y UPS

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-21.

## Context

Ni Genesis Plus GX ni PicoDrive aplican parches (se comprobó en su código): en libretro lo hace el frontend, que parchea el juego en memoria y se lo da al core ya modificado. Xfusion pasaba a los cores la ruta del juego tal cual.

## Decision

1. **Carpeta `patches\` junto al ejecutable** (la raíz de datos, `XFUSION_DATA_DIR` incluido), creada al arrancar. Configurable en Options > Files > Patches con las mismas tres opciones que los demás: carpeta de datos, carpeta del juego (el parche junto a la ROM) o una carpeta propia.
2. **Búsqueda:** un archivo con el nombre del juego (sin extensión; para un zip, el del zip) y extensión `.bps`, `.ups` o `.ips`, en este orden de preferencia, primero en la carpeta del sistema dentro de la de parches (`patches\Genesis\`) y después directamente en ella. Se comparan los nombres sin distinguir mayúsculas.
3. **Formatos** (`Patch.cpp`, escrito desde las especificaciones públicas, sin Qt Widgets y con pruebas): IPS (con repetición, ampliación, recorte y la variante IPS32), BPS (las cuatro acciones) y UPS (en los dos sentidos). BPS y UPS llevan la suma de comprobación del juego y del resultado y se comprueban: un parche para otra versión del juego se rechaza con ese mensaje, en vez de aplicarse mal. IPS no tiene sumas y se aplica tal cual.
4. **El juego original no se toca.** Se parchea en memoria (también el contenido de un zip) y el resultado se escribe a un archivo temporal que se borra al cerrar el juego; el core recibe ese archivo.
5. **Un juego parcheado tiene sus propios saves y estados**: la copia temporal se llama `<juego> [<parche>].<ext>`, así que su huella y su nombre son distintos de los del juego sin parchear. El tiempo jugado de la biblioteca se sigue contando para el juego original.
6. **Si el parche no se puede aplicar** (otra versión, dañado, ilegible) se pregunta si abrir el juego sin parche o cancelar; nunca se abre en silencio sin él. Los discos de Sega CD no se parchean: si hay un parche para un disco, se avisa (un `.chd` está comprimido y un disco son varios archivos) y se pregunta si abrirlo sin él.

## Consequences

- Positivas: funciona con cualquier core, también con zips; comprobado en la aplicación real con una ROM de prueba y un IPS que cambia un color (con el parche la dirección arriba da verde, sin él rojo) y con 11 pruebas automáticas y una de ajustes.
- Negativas: un parche que espera una ROM con cabecera de copiador (`.smd` con 512 bytes) no se aplica a una sin ella (los BPS/UPS lo dicen; un IPS cambiaría bytes equivocados); solo un parche por juego; el tiempo jugado de un juego parcheado no se muestra en su propia fila; no hay lista ni gestión de parches en la interfaz, solo la carpeta.
- Sin hacer: varios parches apilados, parches para discos, elegir entre parches distintos del mismo juego.