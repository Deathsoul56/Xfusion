# Cambios

## 0.1.0 (2026-11-25)

Primera versión pública. Frontend multiemulador para los sistemas de SEGA hasta la cuarta generación, gemelo de snes9xrd.

### Sistemas y cores

- SG-1000, Master System, Game Gear, Genesis / Mega Drive, Sega CD y 32X.
- Dos cores libretro incluidos en el paquete: **Genesis Plus GX** (por defecto) y **PicoDrive** (32X). La elección del core se hace por sistema o por juego, y se pueden descargar versiones nuevas desde *Options > Emulation > Manage Cores*.
- Sega CD con `.cue` + `.bin`, `.iso` y `.chd` (libchdr), varios discos con cambio en caliente. Hace falta tu propia BIOS.
- Parches IPS, BPS y UPS sin tocar el juego original.
- Región por juego y por core.

### Interfaz y juego

- Biblioteca de juegos con búsqueda, filtros, tiempo jugado y reconocimiento por contenido.
- Controles configurables (teclado y mando, dos jugadores, 3 o 6 botones, turbo).
- Estados guardados (1000 espacios, deshacer, miniaturas), rewind, avance rápido, cámara lenta, avance fotograma a fotograma, capturas.
- Saves, estados y capturas separados por sistema.
- RetroAchievements en modo casual.

### Imagen y sonido

- Imagen por GPU (Direct3D 11, 12, Vulkan u OpenGL; con respaldo por software), con shaders propios y presets `.slangp` de RetroArch con parámetros en vivo.
- 22 filtros gráficos; HQx, xBRZ, EPX y los escaladores simples se hacen en la GPU.
- Ajustes de imagen y sonido globales o por sistema; imagen ajustada a la ventana, estirada o en escala entera.
- 22 temas de color.

### Distribución

- Portátil: todo junto al ejecutable, sin instalador ni permisos de administrador; si la carpeta no se puede escribir, en `%LOCALAPPDATA%\Xfusion`.
- `xfusion.exe` pesa 32 MB; `xfusion-shaderc.exe` compila los shaders de los presets del usuario.

### Límites conocidos

- Solo probado en Windows y sin firma de código.
- RetroAchievements sin modo hardcore.
- Los estados no se comparten entre cores ni entre los discos de un mismo juego.
- Los discos de Sega CD con pistas `.mp3` suenan sin música con Genesis Plus GX (ver el README).
- Lista completa: `README.md` y `docs/pendientes.md`.
