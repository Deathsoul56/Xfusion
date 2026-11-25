# Compatibilidad, firmware y limitaciones

Esta es la lista de lo que Xfusion soporta y de lo que se ha comprobado de verdad. Un sistema no se da por soportado
solo porque su core lo anuncie: lo que no se ha probado, está marcado como tal.

## Sistemas y cores

| Sistema | Core por defecto | Formatos | Firmware | Comprobado |
|---|---|---|---|---|
| SG-1000 | Genesis Plus GX | `.sg`, `.sc` (también dentro de `.zip`) | ninguno | 62 juegos se identifican; Bomb Jack arranca, suena y tiene 24 logros |
| Master System | Genesis Plus GX | `.sms` (también `.zip`) | ninguno (opcional: `bios_E.sms`, `bios_U.sms`, `bios_J.sms` en `system\`) | Sonic the Hedgehog: imagen, sonido y logros bien |
| Game Gear | Genesis Plus GX | `.gg` (también `.zip`) | ninguno (opcional: `bios.gg`) | 52 juegos se identifican; Sonic Chaos arranca y tiene 34 logros |
| Genesis / Mega Drive | Genesis Plus GX | `.md`, `.gen`, `.smd`, `.bin` (también `.zip`) | ninguno | Aladdin, Sonic 1 y otros: 60 fps NTSC, 50 fps PAL; SRAM y estados |
| Sega CD / Mega-CD | Genesis Plus GX | `.cue` con `.bin`, `.iso`, `.chd` | **BIOS por región** (ver abajo) | 15 discos locales arrancan sin errores, uno en varios discos |
| 32X | PicoDrive | `.32x`, `.bin`/`.md` con cabecera `SEGA 32X` (también `.zip`) | ninguno | 37 ROMs arrancan a 50 o 60 fps; After Burner Complete con logros |

El core de cada sistema se cambia en *Options > Emulation > Cores* y el de un juego, con clic derecho en la biblioteca.
Los estados guardados no se comparten entre cores.

## Funciones

| Función | Estado |
|---|---|
| Biblioteca con detección por contenido, columnas como `snes9xrd` y ancho automático | Hecho |
| Estados (1000 espacios, deshacer), SRAM, rewind, avance rápido, cámara lenta, avance de fotograma, capturas | Hecho |
| Controles de teclado y mando, dos jugadores, mando de 3 o 6 botones, turbo | Hecho |
| Filtros gráficos de software (22) y temas (22) | Hecho |
| Parches IPS, BPS y UPS (no en discos) | Hecho |
| Gestión de cores: descarga, versiones, volver a una anterior | Hecho; la descarga solo está probada en Windows |
| Región por sistema y por juego | Hecho |
| Sega CD en varios discos | Hecho; probado con Dracula Unleashed, no una partida entera |
| RetroAchievements, modo casual | Hecho, los seis sistemas |
| RetroAchievements, modo hardcore | No: exige la aprobación de RetroAchievements |
| Cheats, shaders de GPU, netplay | No |

## Firmware (BIOS)

Xfusion **no incluye BIOS ni juegos**. Solo el Sega CD necesita BIOS para jugar:

| Archivo | Para discos de |
|---|---|
| `bios_CD_J.bin` | Japón |
| `bios_CD_U.bin` | USA / América |
| `bios_CD_E.bin` | Europa |

Van en la carpeta `system\`. Tus volcados pueden llamarse como quieran: *Options > Files > Find BIOS Files...* los
reconoce por su contenido, elige la versión más nueva de cada región y los copia con el nombre correcto. Si abres un
disco para el que no hay BIOS, Xfusion lo dice antes de cargarlo y ofrece buscarlo. Un disco necesita el BIOS de su
región: el que lleva el encabezado del primer sector (también en los `.chd`).

## Dónde se guardan las cosas

Xfusion es portátil: ajustes, biblioteca, cores, saves y estados van **junto al ejecutable** y la carpeta se puede
mover o borrar entera. Si esa carpeta no se puede escribir (por ejemplo `Program Files`), usa
`%LOCALAPPDATA%\Xfusion`. La variable `XFUSION_DATA_DIR` fuerza otra carpeta. No hace falta ser administrador.

## Requisitos

- Windows 10 o 11 de 64 bits. Solo se ha probado en Windows.
- El ejecutable es uno solo y solo usa DLL del sistema: no hay que instalar nada más.
- El ejecutable **no está firmado**: Windows SmartScreen puede avisar la primera vez («Más información > Ejecutar de todos modos»).

## Limitaciones conocidas

Están al final del [`README.md`](../README.md) (*Limitaciones conocidas*) y, las de Sega CD, en
[`sega-cd.md`](sega-cd.md) (sección 8).
