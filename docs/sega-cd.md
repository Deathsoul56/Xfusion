# Sega CD en Xfusion: estado actual

Última actualización: 2026-11-20. Este documento recoge lo que Xfusion sabe hacer hoy con Sega CD / Mega-CD, cómo lo hace y lo que falta. Las decisiones están en el ADR-0022 (reconocimiento), el ADR-0016 (saves) y el ADR-0007 (core). El plan general es `frontend_sega_multiemulador.md`, Fase 5.

## 1. Resumen

| Tema | Estado |
|---|---|
| Core | Genesis Plus GX (ADR-0024); PicoDrive solo si se elige en Options > Emulation > Cores |
| Formatos reconocidos | `.cue` + `.bin`, `.iso` (con audio suelto), `.chd` (sin inspeccionar) |
| Reconocimiento | Por el contenido del primer sector, no por la extensión |
| Región y título | Leídos de la cabecera del disco y mostrados en la biblioteca |
| BIOS | Reconocido por contenido; se instala desde una carpeta con "Find BIOS Files..." |
| Saves | Copia interna del Sega CD en `saves\Sega CD\`; identidad por huella del archivo |
| Probado | 4 discos locales: se reconocen y arrancan; la jugabilidad completa no está verificada |
| Pendiente | Región de los `.chd`, jugabilidad por disco, selección manual de core |

## 2. Cómo es un juego de Sega CD

Un juego tiene una pista de datos (el programa) y, casi siempre, pistas de audio (música). Los volcados las reparten de tres maneras:

1. **`.cue` + `.bin`.** El `.cue` es una hoja de texto con una línea `FILE` por archivo y una `TRACK` por pista; cada `.bin` contiene una o más pistas en sectores de 2352 bytes (datos más corrección de errores). *El `.cue` es el juego; los `.bin` son sus partes.* Ejemplo local: *Shin Megami Tensei (Japan)*, con 29 `.bin`.
2. **`.iso` + audio suelto.** El `.iso` tiene solo la pista de datos, en sectores de 2048 bytes. La música va aparte como `<nombre> NN.mp3` (o `.wav`, `.ogg`, `.flac`) con NN desde 02. Ejemplos locales: *Sonic CD*, *Mortal Kombat*, *Final Fight CD*. PicoDrive encuentra el audio por ese nombre, sin `.cue`.
3. **`.chd`.** Imagen comprimida de MAME. Xfusion la acepta por su marca interna `MComprHD` pero no puede leer su contenido (ni región ni título).

### La marca del disco

El primer sector de datos de cualquier disco de Sega CD empieza con el texto `SEGADISCSYSTEM`. Dónde está depende del formato:

| Formato | Dónde está la marca |
|---|---|
| `.iso`, 2048 bytes por sector | byte 0 |
| `.bin` en modo 1, 2352 bytes por sector | byte 16 (tras 12 de sincronía y 4 de cabecera) |
| `.bin` en modo 2 forma 1 | byte 24 |

Después, en el desplazamiento 0x100 de los datos, hay una cabecera de 256 bytes idéntica a la de un cartucho de Mega Drive:

| Desplazamiento | Contenido |
|---|---|
| 0x00 | Tipo de sistema (`SEGA MEGA DRIVE`, `SEGA GENESIS`) |
| 0x10 | Copyright y fecha |
| 0x20 | Título nacional (48 bytes) |
| 0x50 | Título internacional (48 bytes) |
| 0x80 | Tipo y código de producto (`GM MK-4407 -00`) |
| 0xF0 | Región (16 bytes) |

El nombre del volumen está en el byte 0x10 del primer sector (por ejemplo `SONICCD`).

### Región

El campo de región usa dos notaciones. Las cabeceras antiguas escriben letras (`J` Japón, `U` América, `E` Europa; pueden ir varias, como `JUE`). Las nuevas escriben un dígito hexadecimal con bits: 1 Japón, 2 Asia, 4 América, 8 Europa. Xfusion aplica la regla de los emuladores de referencia: **si hay letras J/U/E, mandan ellas** (la `E` es Europa, no el 14 hexadecimal); si no, se lee el primer dígito; un campo que empieza por `SEGA` no dice nada.

## 3. Lo que tenemos de prueba

Los discos están en `Roms\CD\` (ignorada por git; no se distribuyen).

| Juego | Forma | Región (cabecera) | Notas |
|---|---|---|---|
| Brutal Paws of Fury | `.iso` + 14 `.mp3` | USA | Arranca (logo de GameTek); sin música con Genesis Plus GX (`.mp3`) |
| Dune | `.cue` + 2 `.bin` | USA | Arranca y recorre las pantallas de Virgin y Cryo |
| Earthworm Jim - Special Edition | `.cue` + 28 `.bin` | USA | Arranca (licencia de Sega) |
| Eternal Champions | `.iso` + 19 `.mp3` | USA | Arranca hasta el aviso de la RAM de respaldo; sin música (`.mp3`) |
| FIFA International Soccer | `.cue` + 46 `.bin` | USA | Arranca hasta la intro con el futbolín |
| Final Fight CD | `.chd` | sin leer | Arranca y suena (60 fps) |
| Lunar - The Silver Star | `.cue` + 52 `.bin` | USA | Arranca (Working Designs) |
| Mortal Kombat | `.iso` + 20 `.mp3` | Europe | 50 fps (PAL); sin música (`.mp3`) |
| Prince of Persia | `.cue` + 25 `.bin` | Japan (la cabecera; el nombre dice USA) | Arranca hasta la intro |
| Robo Aleste | `.cue` + 17 `.bin` | USA | Arranca y recorre la intro (Tengen) |
| Samurai Shodown | `.iso` + 29 `.mp3` | USA | Arranca hasta el título; sin música (`.mp3`) |
| Shin Megami Tensei (Japan) | `.cue` + 29 `.bin` | Japan | Usa el BIOS japonés (versión 1.00) |
| Sonic CD | `.iso` + 34 `.mp3` | USA | Arranca; sin música (`.mp3`) |
| The Secret of Monkey Island (RE) | `.cue` + 27 `.bin` | USA | Arranca; con el parche de `build\patches` no se aplica (los discos no se parchean) |
| Vay | `.cue` + 22 `.bin` | USA | Arranca (licencia de Sega) |

Prueba del 2026-11-21 con Genesis Plus GX: los 15 discos arrancan sin ningún error ni aviso, corren a 60 fotogramas por segundo (50 el europeo) sin cortes de sonido durante 25 s, y la biblioteca los reconoce a todos con su región y su título. Guardar y cargar estado funciona con Dune. Los cuatro `.iso` con `.mp3` no suenan por lo explicado en 4.6 y 4.7.

Los BIOS de prueba están en `Sega CD BIOS\` (también ignorada por git):

| Archivo | Versión | Región |
|---|---|---|
| Sega Mega-CD Model 1 BIOS v1.00s (1991)(Sega)(Jp).bin | 1.00S | Japón |
| Sega Mega-CD Model 2 BIOS v2.00w (1993)(Sega)(Eu)[b].bin | 2.00W | Europa |
| Sega Sega CD Model 2 BIOS v2.11x (1993)(Sega)(US)[b].bin | 2.11X | EE. UU. |

## 4. Cómo los reconoce Xfusion

Todo está en `src/DiscInfo.*`, `src/GameHeader.*`, `src/Bios.*` y `src/SystemInfo.cpp`, sin Qt Widgets y con pruebas (`tests/DiscTests.cpp`).

### 4.1 Qué sistema es un archivo

La extensión solo propone un candidato. Para `.cue`, `.iso`, `.bin` y `.chd`, `systemForFile` lee el primer sector:

| Archivo | Resultado |
|---|---|
| `.iso` o `.bin` con `SEGADISCSYSTEM` | Sega CD |
| `.bin` sin la marca | Genesis (cartucho) |
| `.iso` sin la marca | No es de Sega CD; la biblioteca lo ignora |
| `.cue` cuya pista de datos tiene la marca | Sega CD |
| `.cue` de otra consola, sin pista de datos o con la pista ausente | No es de Sega CD |
| `.chd` con marca `MComprHD` | Sega CD (sin inspeccionar) |

### 4.2 El `.cue` y sus pistas

`readDisc` lee las líneas `FILE` y `TRACK`: cuenta pistas de datos y de audio, suma el tamaño de todos los archivos del disco y **avisa de cada archivo que el cue nombra y no existe** ("Missing track file: ..."). Si falta la pista de datos, el juego no se reconoce y el mensaje de error del diálogo de apertura lo explica. Si faltan solo pistas de audio, el juego se abre (sin esa música) tras un aviso.

La biblioteca no lista los `.bin` que un `.cue` nombra: son partes del juego, no juegos.

### 4.3 El audio de un `.iso`

Se buscan en su carpeta los archivos `<nombre del iso> NN.mp3|wav|ogg|flac` con NN ≥ 02 (el 01 es la pista de datos). Se cuentan y su tamaño se suma al del juego.

### 4.4 El BIOS

El core pide los archivos `bios_CD_J.bin`, `bios_CD_E.bin` y `bios_CD_U.bin` en la carpeta `system\`, pero los volcados circulan con cualquier nombre. Xfusion reconoce un BIOS por su contenido (`identifyBios`):

- Mide exactamente 128 KB.
- Su cabecera (en 0x100) tiene el título nacional con `BOOT ROM`.
- Su campo de región tiene una sola región (J, E o U).

La versión sale del final del título (`1.00S`, `2.00W`, `2.11X`).

`installBios` copia de una carpeta los BIOS de las regiones que faltan en `system\`, con el nombre que quiere el core. Si hay varios de una región, elige la versión más alta. **Nunca sustituye un BIOS válido**; un archivo que estorbe con el mismo nombre pasa a `.old`. `checkBios` comprueba que cada `bios_CD_X.bin` existe y que su región coincide con su nombre (un BIOS europeo llamado `bios_CD_U.bin` no cuenta como estadounidense).

### 4.5 Qué pasa al abrir un disco

1. Se reconoce el sistema (§4.1).
2. `prepareDisc` lee el disco: si faltan pistas, avisa.
3. Con la región del disco, comprueba que haya un BIOS para alguna de sus regiones. Si no, un cuadro explica cuál falta (`bios_CD_E.bin`...), en qué carpeta va, y ofrece **Find BIOS Files...** para elegir una carpeta con los volcados. Un `.chd` o una cabecera sin región se saltan esta comprobación: decide el core.
4. Se elige el core (§4.6) y se carga; el core elige el BIOS por la región del propio disco.

### 4.6 Qué core juega el disco

**Genesis Plus GX** siempre, salvo que el usuario elija PicoDrive para Sega CD en Options > Emulation > Cores. Genesis Plus GX es el más maduro para Sega CD (CHD, un `.cue` con `.bin`) pero **no reproduce pistas `.mp3`**, solo WAV y OGG: con un disco como los tres `.iso` locales se abre y se avisa de cuántas pistas `.mp3` no sonarán. Para oírlas: convertir las pistas a OGG o WAV con el mismo nombre (`<nombre> NN.ogg`), o elegir PicoDrive para Sega CD (que sí reproduce `.mp3`, si son MPEG-1 a 44,1 kHz, §4.7).

### 4.7 Audio `.mp3` que ningún core reproduce

PicoDrive solo reproduce `.mp3` MPEG-1 Layer III a **44,1 kHz**. Si el primer fotograma de una pista es de otro tipo, la salta, y las pistas siguientes **se desplazan un número**: el juego pide la pista 5 y suena otra, o ninguna. Es lo que le pasa al `Final Fight CD.iso` local: sus 25 `.mp3` son MPEG-2 a 22,05 kHz (los de Sonic CD y Mortal Kombat son MPEG-1 a 44,1 kHz y suenan). El registro de PicoDrive lo delata: `track 5: mp3 bitrate 0` y la pista 5 pasa a ser `... 06.mp3`. Genesis Plus GX tampoco los reproduce (solo WAV y OGG). Con PicoDrive, Xfusion lo detecta al abrir el disco (`DiscInfo::unplayableMp3`) y avisa con los nombres. Soluciones: convertir las pistas a MPEG-1 a 44,1 kHz (`ffmpeg -i "in.mp3" -ar 44100 "out.mp3"`), convertirlas a OGG o WAV (que sí lee Genesis Plus GX), o usar una imagen `.chd` (con la que Final Fight CD suena).

### 4.8 Juegos en varios discos

Xfusion junta los discos de un juego por el nombre (`Juego (USA) (Disc 1).chd` y `Juego (USA) (Disc 2).chd` en la misma carpeta, con el mismo formato) y, al abrir cualquiera, deja los demás dentro del core. El menú **Emulation > Disc** (aparece con cualquier disco) cambia de disco mientras corre el juego: es lo que hace el jugador al abrir la bandeja. Ofrece los discos del juego, *Switch to Next/Previous Disc* (también atajos de teclado, sin tecla por defecto: Options > Shortcuts) e *Insert Disc From File...* para meter cualquier otro disco, sea cual sea su nombre, así que los discos que no se emparejan por el nombre también se pueden usar. El diseño se adaptó de cómo lo hace DuckStation (ADR-0026). Probado con Dracula Unleashed (USA): abierto el disco 2, el juego pide el disco 1; se elige el disco 1 en el menú, se pulsa un botón y el juego continúa. ADR-0026.

Juegos de Sega CD con varios discos (DAT de Redump): Dracula Unleashed (2), Sherlock Holmes - Consulting Detective Vol. II (2), Ground Zero Texas (2), Prize Fighter (2), Supreme Warrior (2), Night Trap (2), Slam City with Scottie Pippen (4), Fahrenheit (2: Sega CD y 32X) y Ecco the Dolphin CD japonés (2, son dos juegos).

## 5. Biblioteca

La página Library (ADR-0021) lista los discos con su sistema (`Sega CD`), su **región** y su tamaño total (datos más audio). El título interno aparece en el tooltip del nombre; si el disco está incompleto, el nombre sale en amarillo y el tooltip dice qué falta. El filtro **Sega CD** del cuadro "All systems" deja solo los discos. Las estadísticas (tiempo y última vez) se guardan por la huella del archivo.

`File > ROM Information...` con un disco abierto añade: formato, pistas de datos y de audio, problemas, título del juego, producto y región.

## 6. Ajustes

**Options > Files** tiene una fila **Sega CD BIOS** con una marca por región (`Japan ✓  Europe ✓  USA ✗`) y el botón **Find BIOS Files...**. La carpeta del BIOS es la de la fila BIOS (carpeta de datos `system\` o una propia).

## 7. Saves

- La copia de seguridad interna del Sega CD la escribe el propio core en `saves\Sega CD\` (ADR-0002). **Genesis Plus GX** usa un archivo por región, compartido por todos los juegos como en la consola (`scd_J.brm`, `scd_U.brm`, `scd_E.brm`); **PicoDrive** uno por juego (`<juego>.brm`). Los dos formatos no se migran de uno a otro.
- Los estados y las capturas también van en carpetas por sistema (`states\Sega CD\`, `screenshots\Sega CD\`).
- La identidad del juego para los saves es la **huella del archivo** (tamaño más principio y final), no el código de producto. Para un `.cue`, la huella incluye los archivos que cita. Así no se pierden los saves ya existentes y un mismo disco en `.iso` y en `.cue` no comparte saves por casualidad.
- Los archivos que escribe el core no pasan por el índice `saves.json`.
- Estados y rewind con Sega CD **no se han probado**.

## 8. Lo que sabemos que falla o falta

**Sin resolver**

- **Discos .iso con .mp3** (Brutal Paws of Fury, Eternal Champions, Mortal Kombat, Samurai Shodown, Sonic CD): se abren y corren, pero Genesis Plus GX no reproduce .mp3, así que no suena la música (4.6). Con PicoDrive suenan si los .mp3 son MPEG-1 a 44,1 kHz (4.7).
- **Juegos multidisco (4.8), limitaciones conocidas:**
  - **Estados guardados:** son del disco con el que se abrió el juego. Un estado hecho con el disco 1 no se carga desde el disco 2, y el estado de reanudar al cerrar tampoco se comparte. La RAM de respaldo sí es común (un archivo por región).
  - **Biblioteca:** los discos de un juego salen como una sola entrada (`Juego (USA) [2 discs]`, la del disco 1, con el tamaño total y los demás discos en la ayuda emergente); si dos archivos llevan el mismo número de disco, el segundo queda aparte.
  - **Qué discos se encuentran:** solo los que están en la misma carpeta, con la misma extensión y con el nombre de Redump (`(Disc N)`, `(Disk N)`, `(CD N)`); si el disco 1 es `.chd` y el 2 es `.cue`, no se emparejan, ni los que están en carpetas hermanas o con otro estilo de nombre.
  - **Hasta 4 discos:** es el máximo de Genesis Plus GX (Slam City with Scottie Pippen llega justo). PicoDrive no se ha probado con varios discos.
  - **Atajos de teclado:** *Switch to Next/Previous Disc* existen en Options > Shortcuts pero sin tecla asignada; el menú solo existe si el core ofrece el control de disco.
  - **Probado a medias:** con Dracula Unleashed se comprobó el cambio del disco 2 al 1 pedido por el juego (arranca de nuevo desde el disco 1). No se ha jugado una partida entera pasando del disco 1 al 2 ni se ha probado ningún otro juego multidisco.
  - **Otros discos no emparejados:** Ecco the Dolphin CD japonés son dos juegos distintos en dos discos, y Fahrenheit trae un disco de Sega CD y otro de 32X; no son una partida de dos discos.
- **`.chd`:** se leen con libchdr (ADR-0029): región, título, producto y pistas se ven en la biblioteca y los logros funcionan. Falta usar esa región para avisar de un BIOS ausente antes de cargar. Los tres `.chd` locales (Final Fight CD y Dracula Unleashed) arrancan y suenan con Genesis Plus GX.
- **Un `.bin` suelto de disco sin `.cue`:** se reconoce como Sega CD, pero el core no lo carga; hace falta el `.cue`.
- **Elección del BIOS:** la hace el core (Genesis Plus GX o PicoDrive) según la región del disco. Xfusion solo comprueba que exista alguno de las regiones que el disco declara; no permite forzar otro.
- **El BIOS no se valida más allá de su cabecera** (no hay comprobación de suma ni de versiones conocidas).

**Pendiente de la Fase 5**

1. Avisar de un BIOS ausente antes de cargar un `.chd` (la región ya se lee).
2. Una tabla de compatibilidad con la jugabilidad verificada por cada disco local (hoy solo se comprobó que arrancan).
3. Selección de región y de BIOS por juego (el core elige el BIOS; la región solo se fuerza para todo un core).
4. Compartir los estados entre los discos de un juego.

## 9. Cómo probarlo

```powershell
# Pruebas automáticas del reconocimiento (discos sintéticos, cabeceras, BIOS, zips)
$env:QT_QPA_PLATFORM='offscreen'; .\build\disc_tests.exe
.\build\library_tests.exe

# Un disco concreto, con una carpeta de datos propia y los BIOS ya instalados
$env:XFUSION_DATA_DIR = "$env:TEMP\xf-test"
.\build\xfusion.exe "Roms\CD\Sonic CD\Sonic CD.iso"
```

Si falta el BIOS, el cuadro de aviso ofrece **Find BIOS Files...**; apúntalo a `Sega CD BIOS\` y se instalan los tres.

Para la biblioteca: en `settings.json` de la carpeta de datos, `"files": {"library_folders": ["<ruta a Roms>"]}`; los discos salen con la región y el filtro **Sega CD** los separa.

## 10. Licencia y distribución

Xfusion no incluye BIOS ni juegos de Sega CD (ADR-0006): el usuario los aporta. PicoDrive impide la redistribución comercial. Las carpetas `Roms\` y `Sega CD BIOS\` están ignoradas por git.
