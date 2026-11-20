# Proyecto: frontend multiemulador de SEGA

## 1. Propósito

Crear una aplicación de escritorio nueva, inspirada en la experiencia de uso de
`snes9xrd` y en la sencillez de Kega Fusion, para organizar y jugar títulos de
varios sistemas SEGA mediante cores mantenidos.

El proyecto implementará el frontend y la integración con libretro; no escribirá
emuladores desde cero ni copiará código de `snes9xrd` (licencia Snes9x, no
compatible con GPL-3). Las mejoras de `snes9xrd` se portan adaptándolas y
quedan registradas en `docs/porting-ledger.md` (ADR-0009). La interfaz, los
ajustes y los datos del usuario serán propios.

## 2. Alcance inicial

### Sistemas objetivo

- SG-1000
- Master System
- Game Gear
- Genesis / Mega Drive
- Sega CD / Mega-CD
- 32X

El alcance llega **hasta la cuarta generación** (era Genesis/Mega Drive), sin
sistemas posteriores. Saturn, Dreamcast y generaciones superiores quedan fuera
del proyecto porque ya están bien cubiertos por otros proyectos.

La compatibilidad real dependerá del core, su versión, el juego y los archivos
de firmware requeridos.

### Cores

| Core | Uso | Consideraciones |
|---|---|---|
| PicoDrive | **Core único por ahora** para SG-1000, Master System, Game Gear, Genesis, Sega CD y 32X (ADR-0007) | Probado con 611 de 634 ROMs locales y con un juego de Sega CD |
| Genesis Plus GX | Candidato futuro | Solo si aporta compatibilidad o precisión que PicoDrive no ofrece; requiere perfil, pruebas y revisión de licencia |
| BlastEm | Candidato futuro (Genesis) | Igual que el anterior |

El frontend trata cada core como un componente independiente descrito por un
perfil (ADR-0007). La asignación sistema → core es una tabla de reglas con
selección manual posterior; no se promete que todos los cores admitan las
mismas funciones, y cambiar de core no garantiza compatibilidad de estados.

## 3. Principios y decisiones iniciales

- **Integración:** aplicación host de libretro que carga cores compatibles en
  tiempo de ejecución y sigue el ciclo de vida de libretro.
- **Stack candidato:** C++ y Qt 6 para la aplicación; CMake y Ninja para la
  compilación. SDL3 queda como candidato para entrada y audio, sujeto al
  prototipo de integración.
- **Gráficos:** no fijar todavía el backend. El prototipo debe probar el video
  por software y, si se contempla, el contexto gráfico por hardware que
  solicite un core. Evitar mantener dos propietarios de la ventana o del
  contexto gráfico.
- **Datos:** separar configuración de la aplicación, biblioteca, partidas
  guardadas, estados y firmware en ubicaciones de usuario documentadas.
- **Saves por sistema:** las partidas guardadas se organizan siempre en una
  carpeta por sistema, para que juegos con el mismo nombre en plataformas
  distintas nunca se sobrescriban (ver sección 3.1).
- **Portabilidad:** diseñar primero para Windows, pero evitar acoplar la lógica
  del frontend al cargador de bibliotecas o a rutas específicas de Windows.
- **Licencias y distribución:** proyecto gratuito y de código abierto. Los
  paquetes de publicación incluyen PicoDrive sin modificar y con sus avisos; el
  binario no se versiona y esos paquetes no pueden venderse ni usarse
  comercialmente (ADR-0006). No distribuir BIOS ni firmware con copyright;
  documentar al usuario cómo indicar archivos obtenidos legalmente.
- **Actualización de cores:** los cores son componentes independientes y
  actualizables sin reinstalar la aplicación (ver sección 3.2).

Estas decisiones son una base de trabajo, no un compromiso irreversible. La
integración entre Qt, SDL3 y el renderizado de libretro es el principal punto
técnico que debe validarse antes de construir funciones encima.

Las decisiones de arquitectura se registran una a una en [`docs/adr/`](docs/adr/)
(ADR-0001 a ADR-0019; el ADR-0008, kit compartido, quedó sustituido por el
ADR-0009 y diferido. El ADR-0005, hilo de emulación, está implementado; la
licencia del proyecto ya tiene su `LICENSE`, y el empaquetado con los avisos de
terceros llegará en la Fase 6).

### 3.1 Jerarquía de saves

```text
saves/
├── SG-1000/
├── Master System/
├── Game Gear/
├── Genesis/
├── Sega CD/
└── 32X/
```

Reglas:

- La carpeta del sistema la determina el sistema del juego en la biblioteca, no
  el core que lo ejecute. Un juego de Genesis guarda en `Genesis/` aunque se
  ejecute con Genesis Plus GX o PicoDrive.
- Aplica a SRAM y a estados (`states/` puede seguir la misma estructura por
  sistema). Los nombres de carpeta son fijos y no dependen del idioma de la UI.
- Cada archivo se nombra a partir del juego; si dos juegos del mismo sistema
  comparten nombre, se desambigua con un sufijo de la huella del juego en lugar
  de sobrescribir (implementado, ADR-0016).
- El frontend crea las carpetas bajo demanda y nunca escribe saves fuera de la
  carpeta de su sistema. Los estados siguen la misma jerarquía en `states/`.
- La raíz `saves/` se ubica en el directorio de datos del usuario y es
  configurable.
- Si se cambia el core de un juego, la save existente en la carpeta del sistema
  se reutiliza; avisar antes si el formato de SRAM del core es incompatible.

### 3.2 Actualización de cores

Los cores no se compilan dentro del frontend: viven como bibliotecas
independientes en una carpeta `cores/` del directorio de datos del usuario. Así
se pueden actualizar sin recompilar ni reinstalar la aplicación.

**Fuente de actualizaciones (a validar en Fase 0):** el buildbot público de
libretro publica binarios por plataforma y arquitectura, incluyendo
`picodrive`, `genesis_plus_gx` y `blastem`. El frontend consultaría ese índice
y descargaría el archivo del core correspondiente. Confirmar la URL, el formato
y la estabilidad de ese servicio, y las licencias de cada core, antes de
depender de él.

Reglas de diseño:

- **Versiones lado a lado:** cada core se instala en
  `cores/<core>/<versión>/`. Actualizar no borra la versión anterior hasta que
  el usuario lo decida, lo que permite volver atrás si una versión rompe un
  juego.
- **Registro local:** un manifiesto guarda, por core, la versión instalada,
  origen, fecha, hash del binario y la versión activa.
- **Verificación antes de activar:** comprobar integridad (hash) de la descarga
  y que el core cargue y exponga la API libretro esperada. Si falla, conservar
  la versión activa.
- **Actualización manual primero:** "buscar actualizaciones" iniciado por el
  usuario, con la lista de cambios disponible. La comprobación automática y la
  actualización silenciosa son opcionales y desactivadas por defecto. Nunca
  actualizar un core mientras hay un juego en ejecución.
- **Versión fijada por juego:** permitir fijar una versión concreta de un core
  para un juego o sistema cuando una actualización introduce regresiones.
- **Compatibilidad de saves:** la SRAM suele ser portable entre versiones, pero
  los estados no están garantizados. Cada estado guarda core y versión; al
  cargar con otra versión se avisa antes de restaurar y se ofrece usar la
  versión original si sigue instalada. Antes de activar una actualización, el
  frontend puede respaldar `saves/<sistema>/` del sistema afectado.
- **Opciones del core:** al cambiar de versión, conservar las opciones que
  sigan existiendo y descartar o resetear las que el core haya renombrado o
  eliminado, informando al usuario.
- **Sin conexión:** el frontend debe funcionar con los cores ya instalados
  aunque no haya red o el servicio no esté disponible.
- **Distribución:** el paquete de publicación incluye el core (ADR-0006); la
  descarga posterior sirve para actualizar o reemplazar versiones.

## 4. Inspiración en `snes9xrd`

Tomar como referencia su frontend Qt y su flujo de uso, no asumir que las
funciones propias de Snes9x están disponibles en todos los cores.

La referencia de diseño de ambos proyectos es DuckStation (estructura de
paneles, menús y flujo). Es inspiración: su licencia actual (CC BY-NC-ND 4.0)
no permite derivar su código ni sus recursos, así que Xfusion los implementa por
su cuenta.

| Aprendizaje o función | Adaptación al proyecto |
|---|---|
| Biblioteca: escaneo, búsqueda, orden y registro de uso | Diseñar una biblioteca multi-sistema, con rutas configurables y metadatos propios |
| Controles y atajos configurables | Crear un modelo de acciones del frontend, con varios bindings y perfiles por dispositivo |
| Ajustes de pantalla y audio | Separar ajustes globales de opciones específicas del core |
| Estados, selección de slot y deshacer carga | Usar la serialización de libretro solo cuando el core la soporte; validar incompatibilidades entre cores/versiones |
| Rewind | Posponer hasta medir el coste de memoria, rendimiento y serialización por core |
| RetroAchievements y cheats | Funciones posteriores; comprobar soporte, licencias y requisitos particulares antes de comprometerlas |
| Netplay y movies | Fuera del alcance inicial; no trasladar formatos ni asumir interoperabilidad con Snes9x |

Las funciones específicas de SNES (canales SPC700, Super Scope y mouse SNES,
por ejemplo) no se trasladan. Menacer, Justifier, mandos de seis botones y otras
periferias SEGA se implementarán solo después de comprobar el soporte concreto
del core y definir cómo se presentan en la interfaz.

## 5. Funciones del producto

### Primera versión utilizable

- Abrir un juego desde un selector de archivos.
- Ejecutar y cerrar un core de forma controlada, con errores comprensibles.
- Mostrar video y reproducir audio; recibir teclado y mando.
- Pausar, reanudar, reiniciar y alternar pantalla completa.
- Guardar y restaurar SRAM en la ruta de usuario configurada.
- Persistir ajustes básicos y último juego abierto.

### Evolución de la experiencia

- Biblioteca con escaneo de carpetas, búsqueda, filtros por sistema, tiempo de
  juego y último acceso.
- Configuración de mandos y atajos, perfiles y visualización del binding activo.
- Ajustes de relación de aspecto, escalado, sincronización y volumen.
- Estados rápidos, slots y deshacer carga, habilitados según soporte del core.
- Selección de core por juego y opciones del core expuestas por libretro.

### Funciones condicionadas o posteriores

- Sega CD: selección y validación de BIOS por sistema/región; manejo de juegos
  multidisco.
- 32X: integración y pruebas específicas con el core compatible.
- Filtros, shaders, ghosting de Game Gear y overlays.
- Rewind, cheats y RetroAchievements, tras validar soporte y coste.
- Periféricos como pistolas, únicamente si el core y el sistema objetivo los
  exponen de forma utilizable.

## 6. Fases y criterios de salida

### Fase 0 — Alcance y prototipo técnico

- [x] Confirmar el alcance de la primera versión y priorizar Windows como
  plataforma inicial.
- [ ] Seleccionar versiones candidatas de Qt, SDL3 y los cores; registrar sus
  licencias y condiciones de redistribución. *(Parcial: Qt 6.11, SDL 3.4 y
  PicoDrive 2.04 fijado al commit `26ecb2b`; licencias por revisar, ver 10.3.)*
- [x] Probar la carga dinámica de un core en un proceso host mínimo.
- [x] Probar video, audio, entrada, pausa y cierre con una ROM de prueba
  obtenida legalmente. *(Verificado con una ROM propia generada para la prueba;
  falta probar con juegos reales y con mando.)*
- [x] Comprobar si el core elegido solicita renderizado por hardware y cómo se
  integra con Qt; documentar el backend gráfico seleccionado. *(PicoDrive
  entrega video por software en RGB565; el MVP lo dibuja con Qt. No se
  implementa renderizado por hardware.)*
- [x] Acordar estructura de datos del usuario y estrategia de logs y errores.
  *(Ver 10.2; los logs del core van a stderr.)*
- [ ] Validar la fuente de actualización de cores (buildbot de libretro): URL,
  formato, hashes disponibles y licencias de redistribución.

**Salida:** una decisión de arquitectura respaldada por un prototipo ejecutable,
con dependencias y límites documentados. No iniciar la biblioteca ni funciones
avanzadas si el ciclo de vida del core todavía es inestable.

### Fase 1 — Host libretro mínimo

- [x] Crear el esqueleto CMake del frontend y una ventana de aplicación.
- [x] Cargar y descargar un core; validar símbolos y versión de API requerida.
- [x] Implementar el ciclo de vida: inicialización, carga del juego, ejecución,
  cierre y liberación de recursos.
- [x] Conectar callbacks de video, audio, entrada y entorno. *(Entrada solo de
  teclado.)*
- [x] Añadir controles básicos: abrir juego, pausar, reanudar, reiniciar y salir.
- [x] Mostrar errores accionables para core, ROM o firmware ausentes/inválidos.
  *(Cubre core ausente, juego rechazado y BIOS de Sega CD ausente, con la
  carpeta y los nombres esperados; no valida el contenido del BIOS.)*

**Salida:** abrir, jugar y cerrar repetidamente juegos de prueba de Genesis y
Master System sin bloquear la aplicación, filtrar recursos ni perder el control
de la ventana.

**Fase 1 completada (2026-11-19).** Verificado en la aplicación real:

- Cuatro sistemas en una misma sesión (Master System, Game Gear, 32X, Genesis) y
  un disco de Sega CD, con cambio de tamaño de imagen, título y carpeta de saves
  correctos en cada apertura.
- 16 reaperturas seguidas alternando dos juegos: la aplicación siguió
  respondiendo y cerró con código 0. Los handles se estabilizaron (864 a 873
  tras la primera tanda de aperturas) y la memoria privada pasó de 60,3 a
  63,7 MB (unos 0,3 MB por reapertura). Con 16 ciclos no se distingue una fuga
  lenta de una caché; una prueba más larga la descartaría.
- Pausa, reinicio, cierre y reapertura sin bloqueos, y SRAM guardada al cerrar.
- Límites: un solo equipo y Windows; las pruebas de entrada y de estrés se
  describen en las secciones 10.2 y 10.3 y en los ADR-0004 y ADR-0005.

### Fase 2 — Controles y presentación

- [x] Definir acciones comunes del frontend y bindings de teclado y gamepad
  (grupos de plataformas, ADR-0010).
- [x] Añadir asignación y guardado de bindings, con prevención de conflictos
  (`config\input.json`, panel Settings > Controllers, Auto-Assign).
- [x] Segundo jugador (bindings y mando por jugador, ADR-0014) y atajos de
  teclado del frontend configurables (ADR-0015).
- [x] Implementar pantalla completa (`F11`), relación de aspecto, escalado y
  volumen (`Settings > Display` y `Sound`, `config\settings.json`).
- [x] Probar cambios de foco y desconexión de mandos (pruebas `ctest`: entrada
  bloqueada sin foco; mando desenchufado con un botón apretado y reconectado).
  El cambio entre ventanas se comprobó a mano con el diálogo de ajustes.
- [x] Establecer qué opciones de video/audio son globales y cuáles dependen del
  core (ADR-0013: hoy todas son globales; las del core irán con su perfil).

**Salida:** configurar y usar teclado y mando durante sesiones normales, con
ajustes conservados después de reiniciar la aplicación.

**Fase 2 completada (2026-11-19).** Verificado en la aplicación real con una ROM
propia (`scripts\make-test-rom.ps1`) y teclas sintéticas: el jugador 1 (flechas)
y el jugador 2 (`I`) se leen por separado en el core; pausa (`P`), pantalla
completa (`F11`, con el menú oculto), volumen (`F2`/`F3`) y silencio (`F4`)
funcionan y se guardan en `config\settings.json`; las páginas Controllers
(jugador 2) y Hotkeys del diálogo se revisaron a ojo. Límites: solo se probó con
el teclado y con joysticks virtuales de SDL (`ctest`); la asignación de dos mandos
físicos distintos no se ha probado con hardware.

**Ampliación a la par de `snes9xrd` (2026-11-20, ADR-0018).** El propietario pidió
que Xfusion y `snes9xrd` sean gemelos:

- [x] Ventana de ajustes con las mismas páginas (General, Display, Sound,
  Emulation, Controllers, Shortcuts, Files) y menús File, Emulation, View,
  Options, Help.
- [x] Shortcuts con cuatro enlaces por acción (teclas con modificadores y mandos),
  mantener y pulsar, duplicados resaltados, Reset y Clear por fila, Reset to
  Default por columna y Clear All.
- [x] Controllers: Controller Ports, Set, Edit, Auto-Assign por columna, "raw SDL",
  iconos, filas Turbo y Swap Controllers 1 and 2.
- [x] Presentación: mantener proporción, escalado entero, Stretch, bilineal,
  proporción (core, 4:3, píxeles cuadrados), 22 filtros de software como
  `snes9xrd` (Scanlines, Simple, Super Eagle, 2xSaI, Super 2xSaI, EPX, HQx, xBRZ,
  SharpBilinear4x y cuatro NTSC; ADR-0019, `THIRD_PARTY_NOTICES.md`),
  mensajes apilados con superposiciones, siete temas.
- [x] Sonido: volumen normal y de avance rápido, dispositivo, frecuencia, búfer y
  silenciar durante avance rápido o avance de fotogramas.
- [x] Velocidad: avance rápido (10×), Speed Up/Down, frame skip, Frame Advance y
  métodos de sincronización.
- [x] Pausa al perder el foco, al minimizar y mientras hay diálogos de archivo.
- [ ] Pendiente: Achievements (posterior), renderizado por GPU y shaders, ajustes
  por sistema, y probar con dos mandos físicos.

### Fase 3 — Persistencia y estados

- [x] Implementar la jerarquía `saves/<sistema>/` (sección 3.1), con creación
  de carpetas bajo demanda y prevención de colisiones de nombres (`GameSaves`,
  ADR-0016).
- [x] Guardar SRAM automáticamente al terminar o descargar el juego y cargarla
  al iniciar, respetando las capacidades del core (también cada 10 s si cambió).
- [x] Implementar estados y slots si el core ofrece serialización válida (10
  slots por juego, `states\<sistema>\`, ADR-0017).
- [x] Añadir confirmación o deshacer para evitar reemplazar accidentalmente un
  estado por una carga incorrecta (deshacer carga, aviso si cambia la versión del
  core, copia `.bak` del estado anterior).
- [x] Mostrar errores de lectura/escritura y no presentar una operación fallida
  como exitosa.

**Salida:** partidas persistentes organizadas por sistema, verificando que dos
juegos con el mismo nombre en sistemas distintos no se sobrescriben; estados
probados en los cores compatibles;
documentar sus límites de compatibilidad y las funciones no soportadas.

**Fase 3 completada (2026-11-19).** Verificado en la aplicación real con la ROM
propia (que guarda un interruptor en SRAM y en RAM de trabajo) y con un juego
real de Game Gear:

- SRAM: no existe el `.srm` justo tras cambiar el estado y aparece a los 12 s con
  el juego en marcha; tras matar el proceso sin cerrarlo, la reapertura conserva
  el cambio. Un cambio que vuelve la SRAM a ceros (PicoDrive entonces informa
  tamaño 0) también se guarda.
- Colisiones: otro juego con el mismo nombre de archivo y contenido distinto
  empezó limpio y guardó en `test-rom (5644e5db).srm` sin tocar `test-rom.srm`;
  el mismo nombre en Genesis y en Game Gear usó `saves\Genesis\` y
  `saves\Game Gear\` (y lo mismo en `states\`).
- Estados: guardar, cargar, deshacer la carga, cambiar de slot, un slot vacío
  no cambia nada, y un estado guardado se restaura en otra sesión. Un estado
  marcado con otra versión del core pide confirmación antes de cargar.
- Límites: solo PicoDrive 2.04 (`26ecb2b`) y un equipo; los estados de 32X y Sega
  CD no se han probado; el aviso por archivo dañado y el bloqueo ante un `.srm`
  ilegible solo se cubren con pruebas unitarias y por lectura de código.

**Ampliación a la par de `snes9xrd` (2026-11-20, ADR-0018).**

- [x] Slot actual 0–999 y ranuras rápidas 0–9, deshacer en archivo, `.oops` y
  `.resume`, vista previa con miniaturas.
- [x] Rewind (10 MB y 5 fotogramas por defecto, mantener `Y`).
- [x] Capturas de pantalla (F12) y Save S-RAM Data.
- [x] Intervalo de la SRAM configurable (30 s) y ubicaciones de saves, estados,
  capturas y BIOS en Settings > Files.
- Verificado en la aplicación real con la ROM propia: guardar, cargar y deshacer
  estado con `Ctrl+S`, `Ctrl+L` y `Ctrl+U`, captura con `F12`, rewind con `Y`,
  avance rápido (600 fotogramas por segundo con `Tab`), pausa (`P`) y pausa al
  perder el foco (0 fotogramas), un solo mando (el segundo jugador deja de
  responder), turbo (alterna a unos 15 Hz), cambio de mandos (`Ctrl+6`) y las
  siete páginas del diálogo. Fuera de la comprobación manual: el avance de
  fotograma (`Frame Advance`) acumula fotogramas pero el primero tras una pausa
  no refleja todavía la entrada en la imagen de la ROM de prueba.

### Fase 4 — Biblioteca y metadatos

- [ ] Escanear rutas configuradas sin bloquear la interfaz.
- [ ] Identificar sistema y core sugerido, permitiendo corregir la selección.
- [ ] Añadir búsqueda, filtros, orden, último acceso y tiempo de juego.
- [ ] Guardar los datos de biblioteca sin modificar ni mover ROMs del usuario.
- [ ] Tratar archivos repetidos, rutas inválidas y juegos retirados del escaneo.

**Salida:** una biblioteca recuperable tras reiniciar, con escaneo repetible y
sin duplicados innecesarios.

### Fase 5 — Cobertura multi-sistema

- [ ] Implementar la gestión de cores (sección 3.2): instalación por versión,
  manifiesto, búsqueda manual de actualizaciones, verificación y reversión.
- [ ] Incorporar SG-1000 y Game Gear con casos de prueba propios.
- [ ] Añadir Sega CD, incluyendo configuración de BIOS y juegos multidisco.
  *(Parcial: un disco `.iso` con pistas `.mp3` funciona con el BIOS copiado a
  mano. Pendiente: instalar y validar BIOS desde Xfusion, juegos multidisco y
  discos `.cue`/`.chd`.)*
- [ ] Añadir 32X mediante PicoDrive u otro core seleccionado.
- [ ] Implementar selección manual de core y reglas de compatibilidad por juego.
- [ ] Revisar regiones y detección automática; permitir corrección manual cuando
  la detección no sea fiable.

**Salida:** matriz de compatibilidad por sistema/core documentada y probada con
juegos de referencia obtenidos legalmente. No declarar un sistema soportado
solo porque el core lo anuncie.

### Fase 6 — Estabilización y primera distribución

- [ ] Probar instalación limpia, actualización, rutas con caracteres no ASCII y
  ejecución sin permisos de administrador.
- [ ] Verificar guardado de configuración, SRAM y estados en rutas de usuario.
- [ ] Comprobar empaquetado, dependencias, avisos de licencia y configuración
  inicial.
- [ ] Publicar lista de cores/funciones soportados, limitaciones conocidas y
  guía para firmware.

**Salida:** paquete reproducible, instrucciones de instalación y lista de
compatibilidad verificadas.

### Después de la primera versión

Evaluar shaders avanzados, rewind, cheats, RetroAchievements y periféricos
especiales de acuerdo con su soporte por core y el interés de los usuarios.
Cada función debe tener criterios de aceptación y pruebas propias antes de
incorporarse al alcance.

## 7. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| Incompatibilidad entre Qt, SDL3 y el contexto gráfico solicitado por un core | Resolverlo con el prototipo de Fase 0 y mantener un único propietario del renderizado |
| Funciones de libretro no soportadas uniformemente | Consultar capacidades del core y habilitar opciones condicionalmente |
| Estados incompatibles al cambiar core o versión | Asociar metadatos de core/versión a cada estado, avisar antes de restaurar y conservar versiones anteriores |
| Una actualización de core introduce regresiones | Instalación lado a lado, reversión, versión fijada por juego y actualización manual por defecto |
| El servicio de descarga de cores cambia o deja de estar disponible | Funcionar sin red con los cores instalados; aislar la fuente tras una interfaz reemplazable |
| BIOS o firmware ausentes, incorrectos o con restricciones de distribución | No incluirlos; validar configuración y explicar cómo proporcionar archivos legales |
| Diferencias regionales, multidisco y mapeo de mandos | Casos de prueba por sistema y opción de corrección manual |
| Licencias distintas entre cores y dependencias | Revisar cada licencia antes de distribuir binarios o código fuente |
| Rewind o shaders consumen recursos o no funcionan en todos los cores | Posponerlos hasta medirlos en hardware y cores representativos |

## 8. Estrategia de pruebas

- Mantener una matriz de sistema, core, versión, formato de juego y funciones
  probadas.
- Usar juegos de prueba obtenidos legalmente; no incluir ROMs, BIOS ni firmware
  protegidos en el repositorio o en los paquetes.
- Probar en cada fase el inicio, pausa, cierre y recuperación ante errores.
- Separar pruebas del host libretro de pruebas de interfaz y de compatibilidad
  por core.
- Registrar pasos de reproducción, core y versión cuando se reporte una falla;
  no atribuir automáticamente al frontend un problema específico del core.

## 9. Fuera de alcance por ahora

- Crear o modificar un core de emulación.
- Copiar código de `snes9xrd` (licencia Snes9x). Sus mejoras se portan
  adaptándolas, con registro en `docs/porting-ledger.md` (ADR-0009). Un kit
  compartido queda diferido.
- Saturn, Dreamcast y cualquier sistema posterior a la cuarta generación
  (descartados, no solo pospuestos).
- Plataformas SEGA no enumeradas en el alcance.
- Netplay, grabación de movies y compatibilidad con formatos propios de Snes9x.
- Distribuir juegos, BIOS o firmware protegidos.

## 10. Estado actual del MVP

### 10.1 Compilar y ejecutar (Windows, MSYS2)

Requisitos: MSYS2 con los entornos CLANG64 y UCRT64, `cmake`, `ninja`, Qt 6
(`mingw-w64-clang-x86_64-qt6-base`) y SDL3 (`mingw-w64-clang-x86_64-sdl3`).

```text
scripts\setup-picodrive.ps1                          # clona, compila e instala el core
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release   # desde el shell CLANG64
ninja -C build
build\xfusion.exe [--core ruta\al\core.dll] [juego]
```

Por defecto en Windows el ejecutable es **uno solo** (ADR-0012): Qt, SDL3 y la
runtime de C++ van enlazados de forma estática y solo importa DLL del sistema
(~27 MB); no hace falta `PATH` ni copiar DLL. Requiere en MSYS2
`mingw-w64-clang-x86_64-qt6-static` (≈2,4 GB) y `mingw-w64-clang-x86_64-libwebp`.
Con `-DXFUSION_STATIC=OFF` se usan las DLL compartidas y
`cmake --install build --prefix <carpeta>` las reúne junto al ejecutable. El
core de PicoDrive sigue siendo una DLL externa (`cores\`).

`scripts\setup-picodrive.ps1` clona PicoDrive en `external\` (ignorado por git),
lo fija al commit `26ecb2b`, lo compila con GCC y copia
`picodrive_libretro.dll` a `build\cores\` (junto al ejecutable).

### 10.2 Qué incluye

- Host libretro propio (`src\LibretroCore.*`): carga el core con `QLibrary`,
  valida la versión de API y los símbolos, y gestiona video (RGB565 y XRGB8888),
  audio, entrada y entorno. Lo específico de cada core está en un perfil
  (`src\CoreProfile.*`, ADR-0007); la ventana usa botones SEGA lógicos.
- Audio con SDL3 (`src\AudioOutput.*`) y video en un widget de Qt con relación de
  aspecto del core (`src\VideoWidget.*`).
- Hilo de emulación (`src\EmuThread.*`, ADR-0005): ejecuta el core, lee el
  teclado y marca el ritmo con la cola de audio. `xfusion.exe --stats` imprime
  cada segundo los fotogramas emulados y el estado del audio;
  `scripts\stress-ui.ps1` arrastra la ventana y abre menús para medirlo.
- Menús (a la par de `snes9xrd`, ADR-0018): File (Open File, Recent Files, Save/Load
  Game Position con bancos de diez ranuras y Oops File, Save/Load with Preview,
  Save Other, ROM Information, Reset Game, Close Game, Exit), Emulation (Pause,
  Pause When Inactive, Frame Advance, Reset, Hard Reset), View (Set Size 1x–10x,
  Fullscreen), Options (General, Display, Sound, Emulation, Controllers, Shortcuts,
  Files) y Help. Los menús muestran el primer enlace de cada atajo. Los atajos
  (Settings > Shortcuts, `config\settings.json`) son los de `snes9xrd`: `Ctrl+O`
  abrir, `P` pausa, `Ctrl+R` reinicio, `F11` pantalla completa (`Esc` la deja y es
  fijo), `F12` captura, `Ctrl+S` / `Ctrl+L` / `Ctrl+U` guardar, cargar y deshacer
  estado, `Ctrl+=` / `Ctrl+-` slot actual, `Shift+F1`…`F10` y `F1`…`F10` guardar y
  cargar las ranuras 0–9, `Tab` avance rápido (mantener), `` ` `` avance rápido
  (alternar), `Y` rewind (mantener), `=` / `-` velocidad, `Ctrl+6` cambiar mandos
  1 y 2, `Ctrl+RePág` / `Ctrl+AvPág` volumen. Los avisos salen apilados sobre la
  imagen, porque la barra de estado se oculta en pantalla completa.
- Ajustes globales (ADR-0013, `config\settings.json`): en `Settings > Display`,
  relación de aspecto (núcleo, 4:3 o estirar), filtro suave y escalado entero;
  en `Sound`, volumen y silencio. Se aplican al pulsar OK.
- Tabla de sistemas única (`src\SystemInfo.*`): carpeta y extensiones de cada
  sistema; la usan el selector de archivos y la ruta de saves.
- Datos junto al ejecutable (`<carpeta del .exe>\`; configurable con `XFUSION_DATA_DIR`):
  `cores\`, `system\` (BIOS), `config\`, `saves\<sistema>\` y
  `states\<sistema>\` según la sección 3.1.
  No se escribe nada en `%APPDATA%`. En desarrollo el ejecutable está en `build\`,
  así que borrar `build\` también borra saves y BIOS: para conservarlos entre
  compilaciones limpias, apuntar `XFUSION_DATA_DIR` a otra carpeta.
- SRAM (`src\GameSaves.*`, ADR-0016): se carga al abrir, se guarda cada 10 s si
  cambió (el hilo de emulación la entrega con el juego en marcha) y al cerrar,
  en `saves\<sistema>\<juego>.srm`. PicoDrive no reporta SRAM mientras esté a
  cero, así que no se crean archivos vacíos. El nombre `<juego>` pertenece al
  contenido que escribió primero (índice `saves\<sistema>\saves.json`); otro
  juego con el mismo nombre recibe `<juego> (<huella>)`. Si el `.srm` no se
  puede leer o su tamaño no coincide con el del core, el juego arranca sin él y
  **no se sobrescribe** durante esa sesión. Un fallo de escritura se avisa (al
  cerrar siempre; jugando, la primera vez, y se reintenta).
- Estados (`src\StateFile.*`, ADR-0017, ADR-0018): slot actual 0–999
  (`<juego>.NNN.state`, ranuras rápidas 0–9), deshacer en archivo
  (`.undo.state`), `.oops.state` automático, `.resume.state` al cerrar el juego
  (opcional), miniaturas y vista previa. Archivos comprimidos con core, versión,
  juego y suma de verificación. Cargar un estado de otro juego o de otro core se
  rechaza; si solo cambió la versión del core se pregunta antes; si el core
  rechaza el estado, se vuelve a donde estaba. Guardar sobre una ranura llena
  deja el anterior como `.bak`.
- Rewind, velocidad y capturas (ADR-0018): rewind con historia de diferencias
  (10 MB y una instantánea cada 5 fotogramas por defecto), avance rápido de 10×,
  Speed Up/Down, frame skip, Frame Advance, y F12 guarda un PNG del fotograma en
  `screenshots\<sistema>\`.
- Mensajes apilados y superposiciones (frame rate, iconos de pausa y avance,
  hora, botones pulsados), siete temas y las ubicaciones de saves, estados,
  capturas y BIOS configurables (Settings > Files).

- Controles configurables (ADR-0010, ADR-0014, ADR-0018): `Options > Controllers...`, página Controllers.
  Dos grupos de plataforma con bindings propios: Genesis / Sega CD / 32X y
  SG-1000 / Master System / Game Gear. **Dos jugadores**: cada grupo tiene los
  bindings del jugador 1 y del jugador 2 (combo `Player`), cuatro ranuras por
  acción, teclado y mandos SDL3, Auto-Assign (usa el mando del jugador elegido)
  y conflictos resueltos moviendo la fuente: una tecla solo mueve una acción
  del grupo, de cualquiera de los dos jugadores; un botón de mando solo una del
  mismo jugador. Cada jugador lee un mando: en `Automatic` se reparten por
  orden de conexión los mandos que nadie eligió por nombre, y el combo
  `Gamepad` permite fijar uno por nombre. Se guarda en `config\input.json`
  (versión 2; la 1 se lee como jugador 1). El grupo Genesis permite elegir mando de 3 o 6
  botones (por defecto 6, vale para los dos jugadores); el cambio se aplica en caliente y en 3 botones se
  ocultan X, Y, Z y Mode. El diagrama usa una imagen por mando y se ilumina el
  botón que se aprieta en ese momento (teclado o mando, según los bindings en
  edición), como en `snes9xrd`; la de Master System incluye una etiqueta
  "PAUSE (console)" porque esa tecla está en la consola. Pruebas: `ctest` (tres
  ejecutables, 67 casos; `input_tests` usa joysticks virtuales); el cambio 3/6 y
  los dos jugadores se comprobaron con una ROM sintética que lee los dos puertos
  (`scripts\make-test-rom.ps1`).

Teclado por defecto (Genesis), jugador 1: flechas, `Z`/`X`/`C` = A/B/C,
`A`/`S`/`D` = X/Y/Z, `Enter` = Start, `Retroceso` = Mode. Jugador 2: `I`/`J`/`K`/`L`
= cruz, `7`/`8`/`9` = A/B/C, `4`/`5`/`6` = X/Y/Z, `0` = Start, `3` = Mode. Master
System: `Z` = Button 1, `X` = Button 2 (jugador 1) y `7`/`8` (jugador 2).

### 10.3 Limitaciones conocidas

- **Licencia de PicoDrive:** su `COPYING` prohíbe vender las redistribuciones o
  usarlas en un producto o actividad comercial, y exige conservar su aviso. Para
  un proyecto gratuito se puede incluir en los paquetes (ADR-0006). Xfusion usa
  GPL-3.0-or-later (`LICENSE`). Pendiente: `THIRD_PARTY_NOTICES.md`.
  Esto es una nota técnica, no asesoría legal.
- Los bindings de mando son índices crudos de SDL y valen para el mando que
  lee ese jugador. Probado con teclado, con un 8BitDo (XInput) y con joysticks
  virtuales para dos jugadores; no se ha probado con dos mandos físicos. SG-1000
  comparte los bindings de Master System (supuesto). Los atajos aceptan teclas con
  modificadores y entradas de mando, pero no combinaciones de mando ni de varias
  teclas sin modificador. `=`, `-` y `\` dependen de la distribución del teclado:
  en teclado español hay que reasignarlos. Un mando solo cuenta en los atajos si
  está en primer plano o si se activó "Allow gamepad input while window is not
  focused".
- **Imágenes de los mandos** (`src\resources\controllers\`, tres): aportadas por el
  propietario; falta confirmar su origen y licencia antes de publicar. Si no
  se pueden redistribuir bajo GPL-3.0, hay que reemplazarlas por dibujos propios.
- Saves y estados: la identidad de un juego es una huella (tamaño más el
  principio y el final del archivo; un `.cue` incluye los archivos que cita). Un
  juego modificado en su sitio (un parche) cuenta como otro juego y empieza
  con su propio save. Un cierre brusco pierde como mucho los últimos 10 s de
  SRAM. Los archivos que escribe el propio core en la carpeta de saves (la copia
  interna de Sega CD) no pasan por este índice. Un estado depende del core y su
  versión; no hay rewind ni estados automáticos. Con el juego en pausa, cargar un
  estado ejecuta un fotograma para redibujar. La raíz `saves\` aún no se puede
  mover por separado de `XFUSION_DATA_DIR`.
- Sin biblioteca, opciones del core ni gestión de versiones de cores. Para Sega
  CD el BIOS se copia a mano (ver abajo); no hay validación del archivo.
- Probado con una ROM sintética de Genesis y con la colección local (634 ROMs,
  también en `.zip`; la detección de sistema lee el contenido del zip): 611
  arrancan con video y audio (SG-1000 62, Master System 327, Game Gear 50,
  Genesis 140, 32X 32). 13 zips son formatos fuera de alcance (`.sf7`
  SF-7000 y `.mv` MyVision) y ~10 muestran pantalla negra más de 700
  fotogramas (intros lentas o posibles fallos; pendiente de revisar a mano).
- **Sega CD** probado con *Final Fight CD* (imagen `.iso` de región EU más 25
  pistas `.mp3` sin `.cue`): arranca el BIOS, carga el disco y se juega a 50
  fotogramas por segundo (PAL), sin vacíos de audio; la copia de seguridad
  interna se guarda en `saves\Sega CD\`. PicoDrive encontró solo las pistas por
  su nombre (`<juego> NN.mp3`). Solo se probó un juego y una región.
- **BIOS de Sega CD:** PicoDrive busca en `system\` un archivo por región del
  disco, elegida por el propio disco: `bios_CD_E.bin` (Europa), `bios_CD_U.bin`
  (EE. UU.) o `bios_CD_J.bin` (Japón); también acepta otros nombres como
  `eu_mcd2_9306`. Por ahora se copian a mano a `system\` junto al ejecutable.
  La carpeta `Sega CD BIOS/` del repositorio está ignorada por git. Si falta,
  Xfusion muestra la carpeta y los nombres esperados.
- Al cargar un segundo juego, PicoDrive conservaba contadores de latencia del
  mando en variables estáticas (`padTHLatency`, `padTLLatency` en `pico/memory.c`)
  y los botones A y B se leían invertidos durante tanto tiempo emulado como duró
  el juego anterior. Xfusion recarga el core completo en cada juego después del
  primero; cuesta unos 7 ms. La causa y la evidencia están en el ADR-0004.
- Windows: el teclado se consulta por polling en cada fotograma
  (`GetAsyncKeyState`) desde el hilo de emulación, y el core recibe un mando de
  6 botones.
- `.bin` se asume Genesis.
- Los diagnósticos de rendimiento se obtienen con `xfusion.exe --stats` y
  `scripts\stress-ui.ps1`.
- Las mediciones de rendimiento se hicieron en un solo equipo, con un solo juego
  y el dispositivo de audio predeterminado; no se probó el audio desconectado en
  caliente ni plataformas distintas de Windows (ADR-0005).
