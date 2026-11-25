---
title: "ADR-0032: Estrategia de renderizado por GPU y shaders"
status: "Accepted"
date: "2026-11-25"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["video", "gpu", "shaders", "qrhi", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0032: Estrategia de renderizado por GPU y shaders

## Status

Accepted (2026-11-25). Las Fases A y B están implementadas.

## Context

Hoy la imagen se pinta por software: VideoWidget filtra el cuadro en la CPU (22 filtros) y lo escala con QPainter. Mide en este equipo (Genesis, ventana de 976 x 759, un núcleo): sin filtro 14 %; scanlines 13 %; NTSC 22 %; xBRZ 6x 38 %; HQ4x 55 %. Va bien aquí, pero la carga crece con la resolución de la ventana y los filtros grandes no son viables en una CPU modesta. Además, snes9xrd ofrece shaders (presets *slang* de RetroArch) y el objetivo es ser su gemelo.

Se comparan cuatro caminos:

| Opción | A favor | En contra |
|---|---|---|
| **A. QRhiWidget (Qt RHI)** | Una sola API para D3D11, D3D12, Vulkan y OpenGL; Qt la elige (D3D11 en Windows); los widgets de encima (mensajes, menús) se componen bien; ya está en el Qt estático que usamos; QShaderBaker compila GLSL de estilo Vulkan a todos los destinos | QRhi es una API semi-pública (sin garantía de compatibilidad entre versiones de Qt, aceptable con Qt fijado) |
| B. QOpenGLWidget | Sencillo, conocido | OpenGL en Windows depende del controlador; Qt lo da por secundario; hay que escribir cada shader en GLSL a mano o traducirlo |
| C. Vulkan directo | Máximo control | Mucho código; no integra con widgets; no funciona en equipos sin Vulkan |
| D. SDL_GPU / SDL render | Ya enlazamos SDL3 | La interfaz es de Qt: habría que incrustar una ventana nativa y perder la composición con los widgets |

## Evidence

Se hizo un prototipo desechable (un ejecutable aparte, no en el repositorio) con el Qt estático del proyecto (6.11.2):

1. QRhiWidget arranca con el backend **D3D11**, sube una textura de 320 x 224 por cuadro a 60 fps y la dibuja con un shader de scanlines calculado a la resolución de salida (cada línea de la fuente queda nítida, algo que el filtro de CPU no hace). Un QLabel semitransparente encima (como nuestros mensajes) se ve bien. Consumo: **6 %** de un núcleo, incluido el generador de la imagen de prueba en la CPU.
2. QShaderBaker (Qt ShaderTools, ya empaquetado en MSYS2 y enlazable estáticamente) compiló **sin cambios de fondo los shaders reales de RetroArch** crt-easymode.slang y crt-lottes.slang a SPIR-V, HLSL 5.0 y GLSL 330, con un preproceso de pocas líneas: separar las etapas (#pragma stage), quitar los #pragma parameter y convertir el bloque push_constant en un bloque uniforme (Qt RHI no tiene constantes de empuje). Unos 100 ms por shader.

## Decision

1. **Salida por QRhiWidget** con el backend por defecto de Qt (D3D11 en Windows). El VideoWidget actual queda como alternativa por software cuando la GPU no sirve (escritorio remoto, máquina virtual, controlador roto) y se elige solo, con un aviso, o a mano en *Options > Display > Driver* (hoy fijo en "Qt Software").
2. **Fase A - presentación por GPU, sin shaders de usuario:** textura por cuadro, escalado con el mismo cálculo de displayRect, filtro bilineal o vecino más cercano, escala entera y aspecto como ahora; los filtros de CPU siguen funcionando (se sube la imagen ya filtrada) y se añaden unos pocos **shaders propios** hechos para esto (scanlines, bilineal nítido, rejilla LCD para Game Gear) compilados al construir con qt_add_shaders. Capturas de pantalla: QRhiWidget::grabFramebuffer().
3. **Fase B - presets de usuario:** cargar presets .slangp de una carpeta shaders\ con QShaderBaker en ejecución (preproceso del ejemplo, caché en disco por huella del texto), con varios pasos, parámetros editables y texturas de apoyo. Ni los shaders ni las listas de RetroArch se distribuyen con Xfusion: el usuario los pone (tienen licencias diversas, THIRD_PARTY_NOTICES.md).
4. **No se copia código de Snes9x** (ADR-0009): el formato slang es una especificación pública y el cargador se escribe aquí.

## Fase A implementada (2026-11-25)

- GpuCanvas (QRhiWidget) sube el cuadro como textura BGRA8 y lo dibuja en el rectángulo de displayRect con uno de cinco shaders propios (src/shaders/, compilados al construir con qt_add_shaders): sin shader, scanlines, CRT (scanlines, rejilla y viñeteado), bilineal nítido y rejilla LCD. Los efectos se calculan a la resolución de la pantalla y se desvanecen cuando la imagen se dibuja demasiado pequeña para que se noten.
- VideoWidget::setDriver elige una sola vez, antes de mostrar la ventana: *Options > Display > Driver* (Automático, Qt Software, Direct3D 11, Direct3D 12, Vulkan, OpenGL; hace falta reiniciar). Si el canvas falla (enderFailed) se vuelve al dibujo por software con un mensaje. El driver es global; el shader se puede fijar por sistema (ADR-0031).
- Comprobado con Aladdin: la imagen sale derecha y a su proporción con los cinco shaders y con los cuatro drivers de GPU (D3D11, D3D12, Vulkan, OpenGL) y con software; en pantalla completa con el shader CRT y los fps encima.
- **Medición honesta del consumo de CPU** (un núcleo, Aladdin): ventana de 976 x 759, sin filtro: software 10-14 %, GPU 14-15 %; ventana maximizada: software 24 %, GPU 26 %; con xBRZ 6x maximizada: software 51 %, GPU 57 % (el filtro sigue en la CPU). Es decir, **la ruta de GPU no ahorra CPU** con esta carga: el gasto va con el tamaño de la ventana en las dos y la emulación domina. Lo que gana es la calidad (efectos a la resolución de la pantalla). Para ahorrar CPU habría que llevar los filtros pesados (xBRZ, HQx, NTSC) a la GPU, que queda como posible fase aparte.

## Fase B implementada (2026-11-25)

Presets de RetroArch (`.slangp`) elegidos por el usuario en *Options > Display > GPU Shaders > Preset file*, solo con la GPU.

- **Lectura (SlangPreset, escrita desde la descripción pública del formato):** #reference, shaders, shaderN, filtros, modos de envoltura, alias, escalas por eje (source, viewport, absolute), buffers flotantes, `frame_count_mod`, texturas y parámetros. De cada .slang: #include resueltos, etapas separadas (una etapa puede reabrirse desde un archivo incluido), #pragma parameter leídos y el bloque de push_constant convertido en un bloque uniforme en el siguiente binding libre.
- **Compilación (SlangCompiler):** QShaderBaker a SPIR-V, HLSL 5.0 y GLSL (330/150, o 450/400 si el shader lo necesita, o ninguno de GLSL si usa algo que solo tiene 4.x), con caché en disco (cache\shaders\) por huella del texto y la versión de Qt.
- **Dibujo (PresetRenderer):** cada pasada dibuja en una textura que leen las siguientes y la última, en la ventana dentro del rectángulo de la imagen. Entiende Source, Original, OriginalHistoryN, PassOutputN, PassFeedbackN, alias y texturas del preset, sus tamaños, FrameCount y los parámetros, leyendo los bloques uniformes por la reflexión del shader. La orientación de las texturas intermedias se corrige en los backends con el framebuffer hacia arriba (OpenGL).
- **Parámetros:** *Options > Shader Parameters...* (activo cuando el preset los tiene): un control por parámetro con efecto en vivo; al aceptar se guardan en los ajustes solo los que difieren del preset. El preset y el shader se pueden fijar por sistema (ADR-0031).
- **Carpeta:** `shaders\` junto al programa es donde se sugiere buscar; Xfusion no distribuye shaders ni presets.

**Comprobado:** 460 presets de una copia de `libretro/slang-shaders` (60 + 400 al azar, dos semillas) se leen y compilan los 460, tras arreglar el caso de las etapas reabiertas y los shaders que piden GLSL 4.x (herramienta `preset_scan`). En la aplicación se vieron bien crt-easymode, crt-lottes, crt-hyllian (5 pasadas), hq4x (5), ntsc-adaptive (4) y crt-royale (12 pasadas, con texturas de apoyo y retroalimentación), este último también con Direct3D 12, Vulkan y OpenGL, derecho y con el mismo aspecto. El ejecutable crece unos 6 MB (de 35,1 a 41,3 MB).

**Límites:** el `mipmap_input` se ignora; `clamp_to_border` se trata como `clamp_to_edge`; la última pasada siempre se dibuja directamente en el rectángulo de la imagen (sus escalas se ignoran); el filtro de CPU se aplica antes y su resultado es el `Original`; la compilación de un preset grande se hace al elegirlo y bloquea la ventana (hasta unos segundos la primera vez, desde la caché es inmediata).

## Filtros en la GPU (2026-11-25)

Los filtros más caros de la CPU tienen ahora versión para la GPU, que se usa sola cuando el dibujo es por GPU (*Options > Display > Run the filters on the GPU when they can*, activada por defecto y fijable por sistema):

- **Qué se hace en la GPU:** HQ2x, HQ3x, HQ4x, xBRZ 2x a 6x, EPX, Simple 2x a 4x, Sharp bilinear 4x y Scanlines (este con el shader propio de scanlines). Son presets pequeños de Xfusion (`src/gpu-filters/`) que usan los shaders de xBRZ y HQx de `libretro/slang-shaders` (`third_party/slang-shaders/`, MIT y LGPL, compatibles con la GPL-3.0-or-later; ver su `README.md`), `stock.slang` y `epx.slang` propios, y van dentro del programa como recursos. El paso final es el escalado de la ventana con el filtro bilineal o vecino más cercano de siempre, como con la CPU.
- **Qué sigue en la CPU:** NTSC (los cuatro), 2xSaI, Super 2xSaI y Super Eagle. Los shaders de esa colección no tienen licencia clara (`no license given`, `license GPL, I think`) y no se copian. Con un shader de GPU elegido (el integrado o un preset), el filtro también se hace en la CPU, porque un shader no puede ir detrás de un preset.
- **Consumo medido** (un núcleo, Aladdin, ventana de 976 x 759): HQ4x 86 % con la CPU y 17 % con la GPU; xBRZ 6x 48 % y 14 %; xBRZ 3x 29 % y 16 %; EPX 26 % y 14 %. El resultado se ve igual que el de la CPU a simple vista (HQ4x y xBRZ 6x comparados lado a lado).
- **Pruebas:** `shader_tests` carga y compila todos los presets integrados y comprueba cuáles filtros tienen versión de GPU.
- **Limitación:** si un preset integrado falla en tiempo de ejecución en una GPU concreta, el aviso sale en pantalla y la imagen se dibuja sin filtro (no vuelve sola al de la CPU hasta que se cambie un ajuste).

## Tamaño del ejecutable (2026-11-25)

- `xfusion.exe` pasó de 41,3 MB a 32,0 MB.
- **El compilador de shaders ya no va dentro.** QShaderBaker con glslang y SPIRV-Cross pesaba unos 11 MB. Ahora solo lo llevan `xfusion-shaderc.exe` (13 MB, junto al ejecutable), las pruebas y `preset_scan`. Los shaders de los filtros que vienen con el programa (HQx, xBRZ, EPX, escaladores simples: 0,16 MB) se compilan al construir y van en un recurso (`:/baked/shaders.bin`, clave = `slangCacheKey`). Los de un preset del usuario los compila `xfusion-shaderc compile` la primera vez y quedan en `cache\shaders\`; si falta `xfusion-shaderc.exe`, el preset da un error que lo dice.
- **Sin OpenSSL (-4,5 MB):** el backend Schannel de Qt Network enlazaba `QPasswordDigestor` (de OpenSSL) por una función que Xfusion no usa. `src/PasswordDigestor.cpp` define esas dos funciones sobre `QMessageAuthenticationCode` (vectores de la RFC 6070 en `core_tests`).
- Se probaron `-Os` e `--icf` (solo 0,5 MB) y quitar el plugin JPEG (0,7 MB): este último se dejó porque una cuarta parte de los presets de `slang-shaders` usa texturas JPEG.

## Consequences

- Positivas: filtros CRT/LCD de verdad a la resolución de la pantalla (HQx, xBRZ y EPX también se hacen ya en la GPU; NTSC y las variantes de SaI y Eagle siguen en la CPU); una sola ruta para todos los sistemas; paridad con snes9xrd en shaders; el selector de filtros de CPU y los ajustes por sistema (ADR-0031) siguen valiendo.
- Negativas: el ejecutable crece (glslang y SPIRV-Cross en la Fase B; sin medir en el ejecutable final); QRhi obliga a mantener fijada la versión de Qt; hay que probar el respaldo por software y varios controladores; los presets muy complejos (historial, retroalimentación, LUT) llegan por partes.
- Riesgos a medir en la Fase A: latencia de entrada frente a la ruta actual, comportamiento en pantalla completa y con varios monitores, y el coste de grabFramebuffer().

## Alternatives considered

OpenGL (B) y SDL (D) se descartan por lo de la tabla. Quedarse en software cubre el uso actual, pero deja fuera shaders y CPUs modestas, que es lo que se busca.
