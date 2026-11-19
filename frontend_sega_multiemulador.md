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
| PicoDrive | **Core único por ahora** para SG-1000, Master System, Game Gear, Genesis, Sega CD y 32X (ADR-0007) | Probado con 611 de 634 ROMs locales; Sega CD sin probar |
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
(ADR-0001 a ADR-0009; el ADR-0008, kit compartido, quedó sustituido por el
ADR-0009 y diferido. Las de hilos y licencias aún están sin implementar).

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
  comparten nombre, se desambigua (por ejemplo, con un sufijo de hash de la ROM)
  en lugar de sobrescribir.
- El frontend crea las carpetas bajo demanda y nunca escribe saves fuera de la
  carpeta de su sistema.
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
  *(Cubre core ausente y juego rechazado; falta validar BIOS de Sega CD.)*

**Salida:** abrir, jugar y cerrar repetidamente juegos de prueba de Genesis y
Master System sin bloquear la aplicación, filtrar recursos ni perder el control
de la ventana. *(Genesis verificado con una ROM de prueba; falta Master System
y repetir abrir/cerrar en una misma sesión.)*

### Fase 2 — Controles y presentación

- [ ] Definir acciones comunes del frontend y bindings de teclado y gamepad.
- [ ] Añadir asignación y guardado de bindings, con prevención de conflictos.
- [ ] Implementar pantalla completa, relación de aspecto, escalado y volumen.
- [ ] Probar cambios de foco, desconexión de mandos y cambio entre ventanas.
- [ ] Establecer qué opciones de video/audio son globales y cuáles dependen del
  core.

**Salida:** configurar y usar teclado y mando durante sesiones normales, con
ajustes conservados después de reiniciar la aplicación.

### Fase 3 — Persistencia y estados

- [ ] Implementar la jerarquía `saves/<sistema>/` (sección 3.1), con creación
  de carpetas bajo demanda y prevención de colisiones de nombres.
- [ ] Guardar SRAM automáticamente al terminar o descargar el juego y cargarla
  al iniciar, respetando las capacidades del core.
- [ ] Implementar estados y slots si el core ofrece serialización válida.
- [ ] Añadir confirmación o deshacer para evitar reemplazar accidentalmente un
  estado por una carga incorrecta.
- [ ] Mostrar errores de lectura/escritura y no presentar una operación fallida
  como exitosa.

**Salida:** partidas persistentes organizadas por sistema, verificando que dos
juegos con el mismo nombre en sistemas distintos no se sobrescriben; estados
probados en los cores compatibles;
documentar sus límites de compatibilidad y las funciones no soportadas.

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

`scripts\setup-picodrive.ps1` clona PicoDrive en `external\` (ignorado por git),
lo fija al commit `26ecb2b`, lo compila con GCC y copia
`picodrive_libretro.dll` a `<datos>\cores\`.

### 10.2 Qué incluye

- Host libretro propio (`src\LibretroCore.*`): carga el core con `QLibrary`,
  valida la versión de API y los símbolos, y gestiona video (RGB565 y XRGB8888),
  audio, entrada y entorno. Lo específico de cada core está en un perfil
  (`src\CoreProfile.*`, ADR-0007); la ventana usa botones SEGA lógicos.
- Audio con SDL3 (`src\AudioOutput.*`) y video en un widget de Qt con relación de
  aspecto del core (`src\VideoWidget.*`).
- Menús: abrir/cerrar juego, pausa, reinicio y pantalla completa (`F11`).
- Tabla de sistemas única (`src\SystemInfo.*`): carpeta y extensiones de cada
  sistema; la usan el selector de archivos y la ruta de saves.
- Datos en `%APPDATA%\Xfusion\Xfusion\` (configurable con `XFUSION_DATA_DIR`):
  `cores\`, `system\` (BIOS) y `saves\<sistema>\` según la sección 3.1.
- SRAM: se carga al abrir y se guarda al cerrar el juego en
  `saves\<sistema>\<juego>.srm`. PicoDrive no reporta SRAM mientras esté a cero,
  así que no se crean archivos vacíos.

Teclado (Genesis): flechas, `Z`/`X`/`C` = A/B/C, `A`/`S`/`D` = X/Y/Z,
`Enter` = Start, `Retroceso` = Mode.

### 10.3 Limitaciones conocidas

- **Licencia de PicoDrive:** su `COPYING` prohíbe vender las redistribuciones o
  usarlas en un producto o actividad comercial, y exige conservar su aviso. Para
  un proyecto gratuito se puede incluir en los paquetes (ADR-0006). Xfusion usa
  GPL-3.0-or-later (`LICENSE`). Pendiente: `THIRD_PARTY_NOTICES.md`.
  Esto es una nota técnica, no asesoría legal.
- Solo teclado; faltan mandos, bindings configurables y estados.
- No hay detección de colisión de nombres dentro de un mismo sistema, ni
  escritura periódica de SRAM (solo al cerrar; un cierre brusco la pierde).
- Sin biblioteca, opciones del core, gestión de versiones de cores ni
  validación de BIOS de Sega CD.
- Probado con una ROM sintética de Genesis y con la colección local (634 ROMs,
  también en `.zip`; la detección de sistema lee el contenido del zip): 611
  arrancan con video y audio (SG-1000 62, Master System 327, Game Gear 50,
  Genesis 140, 32X 32). 13 zips son formatos fuera de alcance (`.sf7`
  SF-7000 y `.mv` MyVision) y ~10 muestran pantalla negra más de 700
  fotogramas (intros lentas o posibles fallos; pendiente de revisar a mano).
  No probado con Sega CD.
- Al cargar un segundo juego, PicoDrive conservaba estado de la partida anterior y
  el juego leía mal los botones (la A de Sega actuaba al soltar la tecla). Xfusion
  recarga el core completo en cada juego después del primero. Esto cuesta unos
  instantes al abrir; revisar si hay una forma más ligera cuando se gestionen
  los cores (sección 3.2).
- Windows: el teclado se consulta por polling en cada fotograma
  (`GetAsyncKeyState`), y el core recibe un mando de 6 botones.
- `.bin` se asume Genesis.
- La temporización es por reloj del sistema, no sincronizada con el audio.
