# Sega CD en Xfusion: estado actual

Última actualización: 2026-11-20. Este documento recoge lo que Xfusion sabe hacer hoy con Sega CD / Mega-CD, cómo lo hace y lo que falta. Las decisiones están en el ADR-0022 (reconocimiento), el ADR-0016 (saves) y el ADR-0007 (core). El plan general es `frontend_sega_multiemulador.md`, Fase 5.

## 1. Resumen

| Tema | Estado |
|---|---|
| Core | PicoDrive 2.04 (libretro), el único integrado. Carga el disco y emula el Sega CD. |
| Formatos reconocidos | `.cue` + `.bin`, `.iso` (con audio suelto), `.chd` (sin inspeccionar) |
| Reconocimiento | Por el contenido del primer sector, no por la extensión |
| Región y título | Leídos de la cabecera del disco y mostrados en la biblioteca |
| BIOS | Reconocido por contenido; se instala desde una carpeta con "Find BIOS Files..." |
| Saves | Copia interna del Sega CD en `saves\Sega CD\`; identidad por huella del archivo |
| Probado | 4 discos locales: se reconocen y arrancan; la jugabilidad completa no está verificada |
| Pendiente | Multidisco, región de los `.chd`, jugabilidad por disco, selección manual de core |

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

| Juego | Forma | Región (cabecera) | Producto | Notas |
|---|---|---|---|---|
| Sonic CD | `.iso` 121 MB + 34 `.mp3` | USA (`U`) | MK-4407 -00 | Arranca; muestra la animación del logo |
| Mortal Kombat | `.iso` 31 MB + 20 `.mp3` | Europe (`E`) | T-81025-00 | Llega a la pantalla de licencia de Sega |
| Final Fight CD | `.iso` 23,4 MB + 25 `.mp3` | Europe (`E`) | MK-4410 -00 | Llega a la licencia y luego queda en negro (ver §8). El `.iso` mide 24 561 712 bytes, que no es múltiplo de 2048 |
| Shin Megami Tensei (Japan) | `.cue` + 29 `.bin` | Japan (`J`) | T-044044-00 | Usa el BIOS japonés (versión 1.00) |

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
4. Se carga con PicoDrive, que elige el BIOS por la región del propio disco.

## 5. Biblioteca

La página Library (ADR-0021) lista los discos con su sistema (`Sega CD`), su **región** y su tamaño total (datos más audio). El título interno aparece en el tooltip del nombre; si el disco está incompleto, el nombre sale en amarillo y el tooltip dice qué falta. El filtro **Sega CD** del cuadro "All systems" deja solo los discos. Las estadísticas (tiempo y última vez) se guardan por la huella del archivo.

`File > ROM Information...` con un disco abierto añade: formato, pistas de datos y de audio, problemas, título del juego, producto y región.

## 6. Ajustes

**Options > Files** tiene una fila **Sega CD BIOS** con una marca por región (`Japan ✓  Europe ✓  USA ✗`) y el botón **Find BIOS Files...**. La carpeta del BIOS es la de la fila BIOS (carpeta de datos `system\` o una propia).

## 7. Saves

- La copia de seguridad interna del Sega CD la escribe el propio core en `saves\Sega CD\`, junto a los demás archivos del sistema (ADR-0002). La carpeta se llama `Sega CD`.
- Los estados y las capturas también van en carpetas por sistema (`states\Sega CD\`, `screenshots\Sega CD\`).
- La identidad del juego para los saves es la **huella del archivo** (tamaño más principio y final), no el código de producto. Para un `.cue`, la huella incluye los archivos que cita. Así no se pierden los saves ya existentes y un mismo disco en `.iso` y en `.cue` no comparte saves por casualidad.
- Los archivos que escribe el core no pasan por el índice `saves.json`.
- Estados y rewind con Sega CD **no se han probado**.

## 8. Lo que sabemos que falla o falta

**Sin resolver**

- **Final Fight CD** llega a la pantalla "Produced by or under license from Sega" y después queda en negro (observado hasta unos 45 s, sin pulsar nada). En una sesión anterior el mismo juego se jugó a 50 fotogramas por segundo (PAL). Puede que necesite una pulsación o más tiempo, o que el volcado esté dañado: el `.iso` mide 24 561 712 bytes, y 24 561 712 / 2048 = 11 993,02, así que no es una imagen entera de sectores. Falta compararlo con otra copia o un `.cue` de referencia.
- **Mortal Kombat** y **Sonic CD** arrancan y llegan a las pantallas de inicio; no se ha jugado ninguno.
- **Juegos multidisco** (varios `.cue` o varios `.iso` de un mismo juego): sin soporte. Cada disco se trata como un juego distinto.
- **`.chd`:** no se lee la región ni el título, así que tampoco se avisa de un BIOS ausente; no hay ningún `.chd` local para probar.
- **Un `.bin` suelto de disco sin `.cue`:** se reconoce como Sega CD, pero el core no lo carga; hace falta el `.cue`.
- **Elección del BIOS:** la hace PicoDrive según la región del disco. Xfusion solo comprueba que exista alguno de las regiones que el disco declara; no permite forzar otro.
- **El BIOS no se valida más allá de su cabecera** (no hay comprobación de suma ni de versiones conocidas).

**Pendiente de la Fase 5**

1. Juegos multidisco.
2. Jugabilidad verificada por cada disco local y una tabla de compatibilidad.
3. Selección manual de core y de región/BIOS por juego (hoy el sistema solo se deduce).
4. Región de los `.chd` (requiere leer CHD).
5. Pruebas de estados guardados y rewind con Sega CD.
6. 32X (aún sin pruebas propias).

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
