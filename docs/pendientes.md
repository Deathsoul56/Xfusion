# Ideas y pendientes para más adelante

Lo que se dejó a un lado a propósito, con lo que se sabe de cada cosa, para decidirlo en otra ocasión. No es el plan (`frontend_sega_multiemulador.md`): aquí están los detalles que no se pidieron o se aplazaron.

## Filtros y shaders (ADR-0032)

1. **Filtros que solo tiene la CPU: NTSC (4 variantes), 2xSaI, Super 2xSaI y Super Eagle.** Los shaders de la colección de RetroArch no tienen licencia clara (`no license given`, `license GPL, I think`) y no se copiaron.
   - 2xSaI, Super 2xSaI y Super Eagle: se pueden escribir shaders propios a partir de la descripción del algoritmo (como se hizo con EPX), sin mirar el código con licencia de Snes9x (ADR-0009).
   - NTSC: un shader propio sería una aproximación, no el resultado exacto de `snes_ntsc` (LGPL, ya en `third_party/`). Habría que decidir si merece la pena.
2. **Si un preset integrado falla en tiempo de ejecución** en una GPU concreta, el aviso sale y la imagen se dibuja sin filtro. Lo mejor sería volver solo al filtro de la CPU. El camino de fallo (`renderFailed`, `presetFailed`) está escrito pero no se ha podido provocar.
3. **Un shader detrás de un filtro de GPU.** Hoy, con un shader de GPU o un preset elegido, el filtro se hace en la CPU. Encadenar un preset integrado con uno de usuario exigiría que `PresetRenderer` admita presets concatenados.
4. **Límites del lector de presets de RetroArch:**
   - `mipmap_input` se ignora y `clamp_to_border` se trata como `clamp_to_edge`.
   - `srgb_framebuffer` se ignora: afecta a la mezcla en espacio lineal al escalar (`QRhiTexture::sRGB` lo permitiría; falta probarlo en los cuatro backends).
   - La última pasada siempre va directa al rectángulo de la imagen: sus escalas se ignoran.
   - La compilación de un preset grande se hace en el hilo de la interfaz (hasta unos segundos la primera vez; después sale de la caché). Se podría pasar a un hilo.
   - Un preset editado en disco no se vuelve a leer hasta reiniciar.
5. **Capturas de pantalla con shader.** Las capturas salen del cuadro del núcleo, sin filtros ni shaders. `QRhiWidget::grabFramebuffer()` permitiría capturar lo que se ve.
6. **La GPU no ahorra CPU en una imagen sin filtro** (24-26 % maximizada con las dos rutas). El gasto crece con el tamaño de la ventana también con GPU; no se ha investigado qué lo causa (composición de la ventana por RHI, presentación, vsync).
7. **Probado solo en una máquina** (una GPU, Windows 11): faltan otras GPU (AMD, Intel), un escritorio remoto o una máquina virtual (que deberían caer en el dibujo por software), y varios monitores.

## Ajustes por sistema (ADR-0031)

1. Cada página (Display y Sound) tiene su propio selector de sistema; podría ser uno compartido.
2. Cambiar un valor global no avisa de qué sistemas lo siguen.
3. Otras páginas por sistema: Emulation (velocidad, rewind) y controles.
4. El driver de vídeo es global y pide reiniciar; no se puede cambiar en caliente.

## Interfaz

1. **Cuadro de fps como el de `snes9xrd`:** allí tiene dos líneas (fotogramas por segundo y los dibujados sobre los del juego); aquí solo la primera. El de las teclas pulsadas no se igualó en detalle.
2. Mensajes de la biblioteca: el orden y los anchos de columna siguen a `snes9xrd`, pero en una ventana estrecha los de Size y Serial salen recortados.

## RetroAchievements (Fase 7)

1. **Modo hardcore:** exige la aprobación de RetroAchievements (6 meses de publicación, agente de usuario propio, etc.). Habría que bloquear rebobinado, cámara lenta, avance de fotograma, carga de estados y trucos, y llevar una cola de envíos sin conexión.
2. **Prueba de cargar un juego mientras se espera al inicio de sesión:** necesita una red simulada.
3. Master System y SG-1000 tienen logros comprobados con un juego cada uno, no con la colección.

## Distribución (Fase 6, ADR-0030)

1. El ejecutable **no está firmado** (SmartScreen puede avisar). Tampoco hay instalador.
2. **No hay actualización del propio frontend**: se reemplaza el `.exe` y los datos no se tocan.
3. La ejecución sin administrador se probó simulando una carpeta sin escritura, no en un equipo limpio con una cuenta estándar.
4. La reproducibilidad del paquete se comprobó con esta cadena de herramientas (MSYS2 CLANG64), no entre máquinas distintas.

## Emulación y compatibilidad

1. **BIOS ausente de un `.chd`:** el aviso ya funciona; falta leer el audio de las pistas de un `.chd` (no se hace).
2. **Sega CD en varios discos:** probado con Dracula Unleashed, no una partida entera pasando de un disco a otro.
3. **Regiones:** se pueden forzar por sistema y por juego; la detección automática de juegos sin región en el encabezado sigue siendo del núcleo.
4. **BlastEm** como posible tercer core de Genesis (planteado al inicio, no hecho).
5. Cheats y periféricos (pistolas): solo si el core los expone de forma utilizable.
