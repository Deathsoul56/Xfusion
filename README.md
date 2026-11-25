# Xfusion
*Multi-system SEGA frontend, ~~"sex edition"~~*

Frontend multiemulador para los sistemas de SEGA hasta la cuarta generación, hecho con C++20, Qt 6 y SDL3, que ejecuta
**cores libretro**. Es el gemelo de [snes9xrd](https://github.com/Deathsoul56/snes9xrd): la misma interfaz, los mismos
atajos y los mismos ajustes, para un hardware distinto.

| Sistema | Formatos |
|---|---|
| SG-1000 | `.sg`, `.sc` |
| Master System | `.sms` |
| Game Gear | `.gg` |
| Genesis / Mega Drive | `.md`, `.gen`, `.smd`, `.bin` |
| Sega CD / Mega-CD | `.cue` + `.bin`, `.iso`, `.chd` |
| 32X | `.32x` (y `.bin`/`.md` con cabecera de 32X) |

Los juegos también pueden estar en `.zip`. Saturn, Dreamcast y los sistemas posteriores quedan fuera del proyecto.

## Qué trae

- **Biblioteca de juegos** con búsqueda (escribe directamente en la lista), filtro por sistema, región, tamaño, tiempo
  jugado y última vez; `F5` vuelve a escanear. Los sistemas y las regiones se reconocen por el contenido del archivo, no
  por su extensión, y puedes corregir el sistema de un juego con clic derecho > System.
- **Controles** configurables: cuatro bindings por botón, teclado y mando, dos jugadores, mando de 3 o 6 botones,
  turbo, auto-asignar.
- **Estados guardados** con 1000 espacios, deshacer, vista previa con miniaturas; **rewind**, avance rápido (`Tab` mientras
  lo mantienes), cámara lenta, avance de un fotograma, capturas de pantalla.
- **22 filtros gráficos** de software (xBRZ, HQx, 2xSaI, EPX, NTSC, scanlines...) y 22 temas de color.
- **Parches IPS, BPS y UPS** aplicados al abrir el juego, sin tocar el original.
- **RetroAchievements** (modo casual): inicia sesión con tu cuenta en *Options > Achievements*, con avisos de logros en pantalla y la lista de logros del juego. Los seis sistemas tienen logros en RetroAchievements.
- **Saves por sistema**: la SRAM y los estados de cada juego viven en carpetas separadas, y no se pisan aunque dos
  sistemas tengan juegos con el mismo nombre.
- **Todo junto al ejecutable**: la carpeta del programa se puede mover o borrar como un solo bloque.

## Cores: cuál juega cada sistema

Xfusion usa dos cores y elige uno al abrir cada juego:

| Sistema | Core por defecto | Por qué |
|---|---|---|
| SG-1000, Master System, Game Gear, Genesis, Sega CD | **Genesis Plus GX** | El más preciso; FM del Master System; buen Sega CD con CHD y RAM de respaldo |
| 32X | **PicoDrive** | Es el único core libretro razonable para 32X |

**Es una decisión del usuario.** En *Options > Emulation > Cores* puedes elegir, sistema por sistema, qué core juega
cada uno entre los que lo soportan, y en la biblioteca, con clic derecho > *Core*, el de un juego concreto (que manda
sobre el del sistema); el cambio vale la próxima vez que abras un juego. Las opciones propias de cada core (región, pantalla extendida de\nGame Gear, bordes, FM del Master System, filtros...) están en *Options > Emulation > Core Options*. Si el core elegido no está
instalado, Xfusion usa otro que sirva y te lo dice. `--core ruta\al\core.dll` fuerza un solo core para todo (útil
para pruebas).

> **Sega CD y el audio `.mp3`.** Genesis Plus GX reproduce el audio de un disco desde pistas **WAV u OGG** y desde
> imágenes `.chd`, pero **no lee pistas `.mp3`**. Muchas copias de Sega CD en `.iso` traen la música como archivos
> `Juego 02.mp3`, `Juego 03.mp3`...; con el core por defecto esos discos se abren y funcionan, **pero sin música**, y
> Xfusion te avisa de cuántas pistas no sonarán. Para oírlas tienes tres caminos:
>
> 1. **Convertir las pistas a OGG o WAV** con el mismo nombre (`Juego 02.ogg`, `Juego 03.ogg`...); así suenan con
>    Genesis Plus GX.
> 2. **Elegir PicoDrive para Sega CD** en *Options > Emulation > Cores*; PicoDrive sí reproduce `.mp3`, pero solo
>    MPEG-1 a 44,1 kHz (un `.mp3` de 22,05 kHz se salta y desplaza el número de las pistas siguientes; Xfusion también
>    avisa de eso). Ojo: el cambio vale para todos tus discos.
> 3. **Usar una imagen `.chd`** o un `.cue` con `.bin`, con los que Genesis Plus GX suena sin conversión.
>
> Los detalles están en [`docs/sega-cd.md`](docs/sega-cd.md).

Los estados guardados no se comparten entre cores: cada estado recuerda con qué core se hizo y Xfusion avisa si
intentas cargarlo con otro.

## Empezar

Descomprime el `.zip` en una carpeta cualquiera y abre `xfusion.exe`: no hay instalador ni hace falta ser administrador. Windows 10 u 11 de 64 bits. El ejecutable no está firmado, así que SmartScreen puede avisar la primera vez (*Más información > Ejecutar de todos modos*). Sin cores instalados, la primera vez Xfusion lo dice y ofrece abrir *Manage Cores*. El resumen de qué soporta cada sistema, el firmware y lo comprobado está en [`docs/compatibilidad.md`](docs/compatibilidad.md).

1. **Cores.** Desde *Options > Emulation > Manage Cores...* puedes descargar la última versión de cada core del buildbot de
   libretro; se guardan por versión en `cores\<core>\<versión>\` y puedes volver a una anterior o borrarla. También sirve
   colocar `genesis_plus_gx_libretro.dll` y `picodrive_libretro.dll` en `cores\` junto al ejecutable: los scripts `scripts\setup-genesis-plus-gx.ps1` y `scripts\setup-picodrive.ps1` los clonan, compilan e
   instalan (ver *Compilar*).
2. **Juegos.** Abre uno con *File > Open File...* o añade tus carpetas en *Options > Files > Library Folders* (o con el
   botón *Add Game Folder...* de la pantalla de bienvenida). Se buscan también las subcarpetas.
3. **BIOS de Sega CD.** Cada región de disco necesita su BIOS (`bios_CD_J.bin`, `bios_CD_E.bin`, `bios_CD_U.bin`) en
   `system\`. Tus volcados pueden llamarse como quieran: *Options > Files > Find BIOS Files...* los reconoce por su
   contenido y copia los que faltan con el nombre correcto. Xfusion no incluye BIOS ni juegos.
4. **Controles.** *Options > Controllers*: por defecto la cruceta son las flechas, A/B/C son Z/X/C, X/Y/Z son A/S/D y
   Start es Enter. Los atajos del frontend están en *Options > Shortcuts*.

### Juegos de Sega CD en varios discos

Pon los discos en la misma carpeta con el nombre de Redump (`Juego (USA) (Disc 1).chd`, `Juego (USA) (Disc 2).chd`, también `.cue` o `.iso`). Al abrir uno, el menú *Emulation > Disc* cambia de disco mientras juegas, como abrir la bandeja de la consola (disco siguiente o anterior, también con atajos que puedes asignar en *Options > Shortcuts*, o un disco de cualquier archivo con *Insert Disc From File...*). Los discos de un mismo juego deben tener el mismo formato (más en *Limitaciones conocidas*).

## Parches

Pon un archivo `.bps`, `.ups` o `.ips` con el nombre del juego en la carpeta `patches\` (o en
`patches\<sistema>\`, por ejemplo `patches\Genesis\`) y se aplica al abrirlo. El juego original no se modifica y un
juego parcheado tiene sus propios saves y estados. BPS y UPS comprueban que el parche sea para esa versión del juego;
si no se puede aplicar, Xfusion pregunta antes de abrirlo sin él. La carpeta se cambia en *Options > Files > Patches*,
que también permite buscar el parche junto a cada juego. Los discos de Sega CD no se parchean.

## Carpetas

Por defecto todo vive junto al ejecutable (o en la carpeta de la variable de entorno `XFUSION_DATA_DIR`); si esa carpeta no se puede escribir (por ejemplo `Program Files`), en `%LOCALAPPDATA%\Xfusion`:

```text
xfusion.exe
cores\        los cores libretro
system\       BIOS
config\       ajustes, controles, biblioteca
saves\<sistema>\   SRAM y copias de seguridad de los juegos
states\<sistema>\  estados guardados
screenshots\<sistema>\
patches\      parches IPS, BPS y UPS
```

SRAM, estados, capturas, BIOS y parches se pueden mover a otra carpeta, o junto a cada juego, en
*Options > Files*.

## Limitaciones conocidas

- **Sega CD en varios discos** (menú *Emulation > Disc*):
  - Los estados guardados son del disco con el que abriste el juego; no se comparten entre discos. La RAM de respaldo sí es común.
  - La biblioteca los junta en una entrada (`Juego [2 discs]`) que abre el disco 1.
  - Solo se emparejan discos de la misma carpeta, con la misma extensión y el nombre de Redump (`(Disc N)`); hasta 4 discos.
  - Solo se ha probado con Dracula Unleashed (el cambio del disco 2 al 1); no se ha jugado una partida entera pasando de un disco a otro.
- **Discos de Sega CD con pistas `.mp3`:** sin música con Genesis Plus GX (ver arriba).
- **Parches:** los discos de Sega CD no se parchean.
- **Estados guardados:** no se comparten entre cores ni entre los discos de un mismo juego.
- **Opciones de core:** llegan al core cuando abres un juego, no en caliente.
- **Región:** se fuerza para todo un core (Options > Emulation > Core Options) o para un juego (clic derecho en la biblioteca > Region, que manda sobre lo anterior); la región que se lee del disco (también de los `.chd`) solo sirve para avisar de un BIOS ausente y para la biblioteca.
- **Mensaje de inicio:** la suma de comprobación solo se calcula para Mega Drive y 32X.
- **Gestión de cores:** el buildbot de libretro solo ofrece el último build, así que solo se pueden tener las versiones que instalaste antes; la búsqueda de actualizaciones es manual, no hay firmas que comprobar (solo HTTPS y el SHA-256 que se guarda) y la descarga solo está probada en Windows.
- **RetroAchievements:** solo modo casual (el hardcore exige que RetroAchievements apruebe el emulador y queda para el futuro); los juegos parcheados no tienen logros; el progreso parcial de un logro se pierde al reiniciar (al cargar un estado se recupera). El análisis está en la Fase 7 de `frontend_sega_multiemulador.md`.
- **Plataformas:** solo se ha probado en Windows.
## Compilar

Windows con [MSYS2](https://www.msys2.org/): entornos CLANG64 y UCRT64, `cmake`, `ninja`, Qt 6 estático
(`mingw-w64-clang-x86_64-qt6-static`), SDL3 (`mingw-w64-clang-x86_64-sdl3`) y `mingw-w64-clang-x86_64-libwebp`.

```powershell
scripts\setup-picodrive.ps1          # clona, compila e instala PicoDrive en build\cores
scripts\setup-genesis-plus-gx.ps1    # lo mismo con Genesis Plus GX
```

```bash
# desde un shell MSYS2 CLANG64
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
ninja -C build
build/xfusion.exe [--core ruta/al/core.dll] [juego]
```

El ejecutable es **uno solo**: Qt, SDL3 y la runtime de C++ van enlazados de forma estática y solo importa DLL del
sistema. Con `-DXFUSION_STATIC=OFF` se usan las DLL compartidas y `cmake --install build --prefix <carpeta>` las reúne
junto al ejecutable. Los cores siguen siendo DLL externas.

Para hacer el paquete (compila desde cero, pasa las pruebas y deja `dist\Xfusion-<versión>-win64.zip` con su SHA-256): `scripts\package.ps1`.

Pruebas (no necesitan juegos ni cores):

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
.\build\input_tests.exe; .\build\settings_tests.exe; .\build\save_tests.exe
.\build\library_tests.exe; .\build\disc_tests.exe; .\build\patch_tests.exe
```

`scripts\make-test-rom.ps1` genera una ROM propia de Genesis para comprobar el frontend sin juegos con copyright.

## Documentación

- [`frontend_sega_multiemulador.md`](frontend_sega_multiemulador.md): plan del proyecto, fases y estado.
- [`docs/compatibilidad.md`](docs/compatibilidad.md): sistemas, formatos, firmware y qué está comprobado.
- [`docs/sega-cd.md`](docs/sega-cd.md): Sega CD en detalle (formatos, BIOS, audio, saves, limitaciones).
- [`docs/adr/`](docs/adr): las decisiones de diseño, una por archivo.
- [`docs/porting-ledger.md`](docs/porting-ledger.md): qué se tomó de snes9xrd y cómo.

## Licencia y avisos

Xfusion se publica bajo **GPL-3.0-or-later** (ver [`LICENSE`](LICENSE)). El código de terceros que lleva dentro, con sus
licencias, está en [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Los cores son programas aparte, con licencia propia, y **no están en este repositorio**. Tanto PicoDrive como Genesis
Plus GX prohíben vender sus redistribuciones o usarlas en un producto comercial: quien reparta Xfusion con ellos
hereda esa condición. Xfusion no incluye ni distribuye juegos, BIOS ni firmware. Esto es una nota técnica, no
asesoría legal.
