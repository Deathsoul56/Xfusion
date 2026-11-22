---
title: "ADR-0024: Dos cores, Genesis Plus GX y PicoDrive"
status: "Accepted"
date: "2026-11-20"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["cores", "genesis-plus-gx", "picodrive", "decision"]
supersedes: "ADR-0007 (decisión 1: solo PicoDrive)"
superseded_by: ""
---

# ADR-0024: Dos cores, Genesis Plus GX y PicoDrive

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-20. Sustituye la decisión de "solo PicoDrive" del ADR-0007; el diseño de perfiles de ese ADR sigue vigente y ahora se usa.

## Context

El propietario eligió dos cores: **Genesis Plus GX** (GPGX) para SG-1000, Master System, Game Gear, Genesis y Sega CD (más precisión, FM del Master System, mejor Sega CD con CHD y RAM de respaldo, opciones propias de Game Gear) y **PicoDrive** para 32X, que solo él emula. El código tenía un único core cargado al arrancar.

## Decision

1. **Reglas** (`CoreRules`, datos y pruebas): el core por defecto de cada sistema es GPGX salvo el 32X, que es de PicoDrive. En Options > Emulation > Cores el usuario puede elegir otro por sistema entre los que lo soportan (`emulation.cores` en `settings.json`); un core que no soporta el sistema se ignora.
2. **Sin excepciones por contenido** (cambiado el 2026-11-21): Sega CD va siempre a Genesis Plus GX, como pidió el propietario. Como ese core no reproduce audio `.mp3` (solo WAV y OGG), un disco con pistas `.mp3` se abre igualmente y se avisa en pantalla y en la barra de estado de cuántas pistas no sonarán; para oírlas hay que fijar PicoDrive para Sega CD en Options > Emulation > Cores (y entonces sus `.mp3` deben ser MPEG-1 a 44,1 kHz) o convertirlas a OGG o WAV.
3. **Cambio de core al abrir el juego** (`LibretroCore::useCore`): se descarga el core actual y se carga el otro; el resto del frontend no cambia. Si el core elegido no está instalado se usa otro que sirva para el sistema, con aviso; si ninguno, un mensaje con la carpeta. `--core <ruta>` fuerza un solo core para todo (pruebas).
4. **Perfiles** (`CoreProfile`): GPGX elige el mando con `retro_set_controller_port_device` (mando de 3 o 6 botones, puerto 2 vacío con un solo mando; solo en la familia Genesis) y no con una opción; el mapa de botones libretro es el mismo que el de PicoDrive; no necesita recarga por juego.
5. **Zips:** GPGX no abre zips (libretro pide al frontend que los descomprima). Xfusion extrae el juego a un archivo temporal con su nombre y extensión (un `.sc` pasa a `.sg`) mientras está cargado; PicoDrive sigue leyendo el zip.
6. **RAM de respaldo:** GPGX informa solo de lo que el juego ha escrito (rellena con 0xFF), así que un `.srm` más corto que el máximo es normal: no se trata como un archivo de otro emulador ni se guarda copia `.orig`, y un juego que borra su save se escribe con 0xFF.
7. **Sega CD con GPGX:** necesita los mismos BIOS (`bios_CD_J/E/U.bin`, ADR-0022); la copia de seguridad interna es un archivo por región (`scd_J.brm`, `scd_U.brm`, `scd_E.brm`) que escribe el propio core en `saves\Sega CD\`, compartido por los juegos, como en la consola. Las copias `<juego>.brm` de PicoDrive no se migran.
8. **Obtención y licencia:** el core se compila con `scripts/setup-genesis-plus-gx.ps1` (commit `4fd32ef`, en `external/`, no versionado). La licencia de GPGX prohíbe vender las redistribuciones y su uso comercial, la misma condición que PicoDrive (ADR-0006); se aplica el mismo tratamiento en `THIRD_PARTY_NOTICES.md`.
9. **Core por juego (2026-11-21):** además del core por sistema, el menú contextual de la biblioteca (clic derecho > Core) fija el core de un juego concreto; se guarda por huella en `library.json` (`core_choices`) y manda sobre el del sistema. Solo se acepta un core que sepa jugar el sistema del juego; si no está instalado se usa otro (la regla de la decisión 3).
10. **Opciones de core (2026-11-21):** cada perfil lista las opciones que el usuario puede cambiar (`CoreProfile::userOptions`: clave, texto, valores y valor por defecto): región, FM del Master System, pantalla extendida y fantasma de LCD de Game Gear, bordes, filtro NTSC del core y sin límite de sprites (GPGX); región y sin límite de sprites (PicoDrive). Se editan en Options > Emulation > Core Options y se guardan en `emulation.core_options` de `settings.json` solo si difieren del valor por defecto; al leer el archivo se descarta lo que el perfil no lista. Llegan al core al abrir un juego (`LibretroCore::setUserOptions`, antes de `retro_load_game`); no se aplican en caliente porque el hilo de emulación lee el mapa de opciones. Probado: región PAL con GPGX corre a 50 fps y NTSC-U a 60.

## Consequences

- Positivas: los seis sistemas funcionan con el core elegido; probados en la aplicación real con un juego de cada sistema (Genesis, Master System, Game Gear y SG-1000 con GPGX; 32X con PicoDrive; un disco `.cue` con GPGX y uno `.iso` con audio `.mp3` con PicoDrive); mando (cruceta y B), guardado de SRAM de 2 bytes y estados con la ROM de pruebas bajo GPGX; 5 pruebas nuevas de reglas y de ajustes.
- Negativas: los estados no se comparten entre cores (la cabecera del estado lo detecta y avisa, pero guardar con el otro core sobrescribe el estado del slot); los saves de SRAM de PicoDrive sirven en GPGX pero los `.brm` de Sega CD no; se mantienen dos cores y dos perfiles; los tres discos locales con `.mp3` abren con Genesis Plus GX pero sin música hasta que se conviertan a OGG o WAV o se elija PicoDrive para Sega CD.
- Sin hacer: opciones de core que no estén en la lista (y aplicarlas en caliente), un `.sc` suelto con GPGX (solo se renombra dentro de un zip), pistolas de luz, comprobar `.chd` con GPGX, y empaquetar los dos cores en una distribución.