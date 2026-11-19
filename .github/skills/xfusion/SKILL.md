---
name: xfusion
description: Convenciones, arquitectura y trampas conocidas del frontend multiemulador SEGA Xfusion (C++20, Qt 6, SDL3, libretro, PicoDrive). Usar al modificar el código de src/, el CMake, el host libretro, las rutas de saves o al probar ROMs. Use when working on libretro host, saves, cores, input, audio/video or ROM compatibility in this repository.
---

# Xfusion

Frontend de escritorio (Windows primero) que aloja cores libretro para SG-1000, Master System, Game Gear, Genesis, Sega CD y 32X. Alcance: hasta la cuarta generación. El plan completo está en `frontend_sega_multiemulador.md`; no duplicar su contenido aquí.

## Reglas del proyecto

- **Saves por sistema:** siempre `saves/<sistema>/<juego>.srm`. La carpeta la decide el sistema del juego, nunca el core. Se obtiene con `AppPaths::savesDir(System)`; no construir rutas de saves a mano.
- **Una sola tabla de sistemas:** `src/SystemInfo.*` define carpeta y extensiones. Cualquier sistema o extensión nueva se añade ahí y en ningún otro lugar.
- **No copiar código de `snes9xrd`** (licencia Snes9x, incompatible con GPL-3). Sus mejoras se portan adaptándolas: entender qué hace el commit y escribir la versión propia. Antes de tomar algo, `scripts/porting-log.ps1 -Blame <archivo>` y anotarlo en `docs/porting-ledger.md` (ADR-0009). `blame` no prueba autoría si el archivo entró en un commit masivo.
- **Nunca versionar** ROMs, BIOS, firmware, `build/` ni `external/`. `Roms/` y las extensiones de ROM están en `.gitignore`. No añadir juegos con copyright como fixtures.
- **Cores = bibliotecas externas** cargadas en tiempo de ejecución, descritas por un perfil de core (ADR-0007). No enlazar un core en el ejecutable ni versionar su binario. Los paquetes de publicación pueden incluir PicoDrive sin modificar, con su `COPYING` y avisos, y no pueden venderse ni usarse comercialmente (ADR-0006).
- Escrituras de saves con `QSaveFile` (atómicas). Un fallo de escritura debe notificarse, no ocultarse.

## Estilo de código

- C++20, 4 espacios, llave de función en línea nueva, miembros con prefijo `m_`, clases en PascalCase, métodos en camelCase.
- Errores del host libretro: `bool` + `QString* error` con mensaje accionable (no excepciones).
- Comentar solo lo que no es obvio (por ejemplo, la razón de una limitación del core).
- Mantener cada archivo con una responsabilidad: host libretro (`LibretroCore`), perfil de core (`CoreProfile`), audio (`AudioOutput`), video (`VideoWidget`), rutas (`AppPaths`), sistemas (`SystemInfo`), ventana (`MainWindow`).
- Lo específico de un core (nombre de biblioteca, opciones `picodrive_*`, mapeo de botones, recarga por juego) vive solo en `CoreProfile`. La ventana habla en `PadButton`, nunca en ids de libretro.
- No dejar código de diagnóstico temporal (variables de entorno, logs ad hoc) en `src/`. La medición permanente es `xfusion.exe --stats`.
- **Hilos:** el core solo lo toca el hilo de emulación (`EmuThread`). Cargar, descargar, recargar el core y leer o escribir SRAM se hacen con el hilo detenido (`m_emu.reset()`); `reset` pasa por `EmuThread::withCore`. El video entra por `VideoWidget::submitFrame` y la entrada por botones atómicos; nada de la interfaz se llama desde el hilo de emulación.

## Compilar y ejecutar (Windows, MSYS2)

```text
scripts\setup-picodrive.ps1                                # clona, fija (26ecb2b), compila e instala el core
# desde un shell CLANG64:
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

- Desde PowerShell: `$env:MSYSTEM='CLANG64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -lc '<comando>'`.
- Para ejecutar `build\xfusion.exe` desde PowerShell, anteponer `C:\msys64\clang64\bin` al `PATH`.
- El core se compila con UCRT64 (GCC); el frontend con CLANG64. Funciona porque la ABI de libretro es C.
- Si el enlazado falla con `Permission denied`, hay un `xfusion.exe` en ejecución: cerrarlo por su PID y reintentar.
- Datos de usuario: `%APPDATA%\Xfusion\Xfusion\` (o `XFUSION_DATA_DIR`): `cores\`, `system\`, `saves\<sistema>\`.

## Trampas conocidas (verificadas)

- **El estado estático de PicoDrive persiste entre juegos.** `padTHLatency` y `padTLLatency` (`pico/memory.c`) no se reinician al cargar un juego mientras el contador de ciclos sí; A y B (línea TL, bit 4) se leen invertidos hasta que el juego nuevo haya corrido tanto como el anterior. `LibretroCore::loadGame` recarga el core completo a partir del segundo juego (`reloadPerGame` del perfil, 7 ms). Causa y evidencia en el ADR-0004. Para comprobarlo antes de tocarlo: cargar un juego, jugar unos segundos, abrir Castlevania - Bloodlines y leer el byte `0x9061` de la RAM en reposo: debe ser `00`, no `50`. Cada core nuevo puede tener su propio estado estático.
- **Mando de 6 botones:** PicoDrive usa por defecto 3 botones y ignora X/Y/Z. Su perfil fuerza `picodrive_input1/2 = "6 button pad"`.
- **Mapeo PadButton → libretro (PicoDrive):** A→Y, B→B, C→A, X→L, Y→X, Z→R, Mode→Select. Teclas: flechas, `Z/X/C` = A/B/C, `A/S/D` = X/Y/Z, `Enter` = Start, `Retroceso` = Mode.
- **SRAM:** PicoDrive devuelve tamaño 0 mientras la SRAM está a cero, así que no se crean `.srm` vacíos. Al cargar (frame 0) devuelve el tamaño real.
- **Zip:** PicoDrive lee zips por sí mismo (`need_fullpath=true`). Xfusion solo inspecciona el zip para detectar el sistema por la extensión de la primera entrada reconocida.
- **Formatos no soportados:** `.sf7` (SF-7000) y `.mv` (MyVision) están fuera de la tabla. `.bin` se asume Genesis (no es fiable; mejor leer la cabecera `SEGA` en 0x100).
- **Sega CD:** el core elige el BIOS por la región del disco y lo busca en `system\` como `bios_CD_E.bin`, `bios_CD_U.bin` o `bios_CD_J.bin` (lista en `CoreProfile::segaCdBiosFiles`). Los BIOS con copyright viven en `Sega CD BIOS/`, ignorada por git, y se copian a mano al directorio de datos. Con `Copy-Item` hay que usar `-LiteralPath`: los nombres llevan `[b]`, que PowerShell toma como comodín. Una región PAL corre a 50 fotogramas por segundo; no es un fallo.
- **Temporización (ADR-0005):** el hilo de emulación marca el ritmo con la cola de audio de SDL3 (objetivo de 1,5 fotogramas de audio) y cae al reloj del sistema si el core no produce audio o no hay dispositivo. El dispositivo de audio arranca pausado hasta que la cola se llena. La entrada también se lee en ese hilo.
- **Entrada (ADR-0010):** los bindings son por grupo de plataforma (`InputGroup` en `SystemInfo`: Genesis/Sega CD/32X y SG-1000/Master System/Game Gear), 4 ranuras por acción, guardados en `<datos>\config\input.json`. Un sistema nuevo solo declara su grupo. `InputManager` lee teclado (`GetAsyncKeyState`) y mandos SDL3 en el hilo de emulación y no entrega nada si la ventana principal no está activa. **`slots` es una macro de Qt**: no usar ese nombre para miembros o métodos.
- **Pruebas de entrada:** `QT_QPA_PLATFORM=offscreen ctest --test-dir build` (18 casos, joysticks virtuales de SDL). El test desactiva los controladores físicos con hints de SDL; sin eso un mando real (p. ej. el 8BitDo como `XInput Controller #1`) altera los recuentos.

## Cómo probar

- Probar siempre con la ventana real: el plugin Qt `offscreen` no cierra con `CloseMainWindow`.
- Para decidir si una ROM "arranca", ejecutar **al menos 700 fotogramas**: muchos juegos (Streets of Rage 3, FIFA 95, EA/Acclaim) muestran negro unos 7 s. Con 240 fotogramas hay falsos "en blanco".
- Criterio mínimo por ROM: vídeo no uniforme y audio no silencioso tras la intro.
- Una ROM sintética de Genesis (código 68000 que pinta un color y escribe un byte en SRAM) sirve como prueba reproducible sin material con copyright.
- Ante un error de entrada, comparar el estado visible del juego con y sin recarga del core antes de culpar al teclado: el problema anterior estaba en el core, no en la lectura del teclado.
- Rendimiento y audio: `xfusion.exe --stats <juego>` imprime cada segundo los fotogramas emulados, los vacíos de audio y la cola de audio. `scripts\stress-ui.ps1 -Rom <juego>` arrastra la ventana y abre menús durante un rato y resume los resultados. Referencia: 60 fotogramas por segundo y cero vacíos de audio tras el arranque.
- Los procesos de prueba se cierran con `Stop-Process -Id <PID>` literal o `$p.Kill()`; no con el nombre.
- Para automatizar la interfaz en Windows: lanzar con `XFUSION_DATA_DIR` apuntando a una carpeta temporal y `C:\msys64\clang64\bin` en `PATH` (sin él aparece "SDL3.dll was not found"). Esc en el diálogo de ajustes lo cierra si no hay una captura activa; el primer clic en una ventana recién abierta puede solo activarla.

## Antes de dar una tarea por terminada

1. Compila sin avisos nuevos (`ninja -C build`).
2. Se probó en la aplicación real, no solo en un arnés, si cambia entrada, audio, video o saves.
3. Si se tocó una ruta o el formato de saves, verificar que dos juegos con igual nombre en distintos sistemas no se sobrescriben.
4. Actualizar `frontend_sega_multiemulador.md` si cambia una limitación o una decisión.
