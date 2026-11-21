---
title: "ADR-0023: Sistema por cabecera, corrección manual y 32X"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["recognition", "32x", "library", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0023: Sistema por cabecera, corrección manual y 32X

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20 (Fase 5).

## Context

La extensión no basta para los cartuchos: los juegos de 32X llegan como `.32x`, pero también como `.bin` o `.md` (o dentro de un zip), y un juego de Game Gear puede llamarse `.sms`. En la colección local había 37 juegos de 32X mezclados con los de Genesis. Y cuando la detección falla, el usuario no tenía cómo corregirla.

## Decision

1. **La cabecera corrige a la extensión.** Para un cartucho (suelto o dentro de un zip) se lee la cabecera: un `.bin`, `.md`, `.gen` o `.smd` con `SEGA 32X` es 32X; un `.sms` o `.gg` con `TMR SEGA` es Game Gear o Master System según su byte de máquina. Sin cabecera manda la extensión, como debe ser en SG-1000. Un `.32x` es siempre 32X.
2. **Corrección manual del sistema** (biblioteca, clic derecho > System): *Detect automatically* o uno de los seis sistemas. Se guarda por huella del juego en `library.json` (`system_choices`), así que sigue valiendo si el archivo se mueve o se renombra; la lista muestra "(set)". Al abrir el juego, por la biblioteca o con Open File, la elección manda: decide la carpeta de saves, los mandos y la comprobación del BIOS. Si la elección no es la real, el core lo dirá al cargar.
3. **32X con PicoDrive**, sin BIOS (el core lo trae). Las ROMs de 32X viven en su carpeta, `Roms\32X`, en zip y sin el sufijo "32X" en el nombre; no es una regla de Xfusion, la biblioteca las reconoce vengan de donde vengan.

## Consequences

- Positivas: `Brutal Unleashed.zip` (un `.bin` de 32X dentro de un zip) ya no se lista como Genesis; los 37 juegos de 32X arrancan a 50/60 fps en la aplicación real (cuatro tienen un primer fotograma casi negro tras 9 segundos, probablemente la intro); las pruebas cubren 32X por cabecera, Game Gear/Master System intercambiados y SG-1000 sin cabecera (1 prueba nueva en `disc_tests`, 1 en `library_tests`).
- Negativas: nadie ha jugado un 32X en Xfusion; PicoDrive avisa `wanted to map @02000000` al cargarlos; tres `.32x` (FIFA 96 y los dos Wolfenstein) llevan cabecera de Mega Drive y se identifican por su extensión (un `.bin` con esa cabecera seguiría siendo Genesis); los 13 zips de SF-7000, MyVision y similares se ignoran.
- Pendiente de la Fase 5: gestión de cores (instalación por versión, manifiesto, actualizaciones), juegos multidisco de Sega CD y la corrección manual de la región.