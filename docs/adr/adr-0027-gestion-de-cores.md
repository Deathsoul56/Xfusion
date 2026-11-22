---
title: "ADR-0027: Gestión de cores (instalar, actualizar y volver atrás)"
status: "Accepted"
date: "2026-11-22"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["cores", "updates", "buildbot", "manifest", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0027: Gestión de cores (instalar, actualizar y volver atrás)

## Status

Accepted.

## Context

Los cores son bibliotecas externas (ADR-0007, ADR-0024) que hasta ahora se copiaban a mano a `cores\` o con `scripts\setup-*.ps1`. La sección 3.2 del plan pedía instalar por versión, un manifiesto, búsqueda manual de actualizaciones, verificación y vuelta atrás, y dejaba por validar la fuente (el buildbot público de libretro).

## Decision

1. **Fuente validada (2026-11-22):** `https://buildbot.libretro.com/nightly/windows/x86_64/latest/<core>_libretro.dll.zip` responde por HTTPS para `genesis_plus_gx` y `picodrive`, con `Last-Modified`; el zip trae la DLL. El buildbot solo ofrece el último build (no hay carpeta de versiones antiguas), así que «instalar por versión» significa conservar lo que se instaló: cada actualización queda al lado de las anteriores. En Linux la ruta es `linux/x86_64` (sin probar).
2. **Carpetas lado a lado:** cada versión vive en `cores\<core>\<versión>\<biblioteca>`; el nombre de la carpeta sale de la versión que el propio core declara (`retro_get_system_info`: `v1.7.4 4fd32ef`), con los caracteres raros cambiados por `_`. Dos builds distintos con el mismo nombre de versión se distinguen con el inicio del SHA-256.
3. **Manifiesto:** `cores\manifest.json` guarda, por core, la versión activa y, por versión, su SHA-256, ruta, origen, fecha del servidor y fecha de instalación. Una versión activa que no esté en la lista se descarta al leer.
4. **Qué core se carga:** `AppPaths::corePath` usa la versión activa del manifiesto si existe y, si no, la biblioteca suelta de `cores\` (el «archivo local»: lo que ya había y lo que instalan los scripts), de modo que nada de lo anterior deja de funcionar.
5. **Acciones, siempre manuales** (Options > Emulation > Manage Cores...): *Check for Update* (una petición HEAD, sin descargar, compara la fecha del servidor con la de la versión activa), *Install Latest* (descarga, desempaqueta, comprueba el core y lo deja activo), *Use Selected* (vuelta atrás a cualquier versión instalada o al archivo local) y *Remove Selected* (borra una versión que no sea la activa). El cambio vale la próxima vez que se abre un juego.
6. **Verificación:** HTTPS con Schannel; límite de 64 MB; el zip debe contener la biblioteca esperada; antes de instalarla se carga y se comprueba que es un core de libretro con la misma API (`probeCore`); se instala en una carpeta de pruebas y solo después se mueve, así que una descarga mala no toca los cores que funcionan. Si el SHA-256 ya está instalado, solo se activa. No hay firmas del buildbot que comprobar.
7. **Licencias:** Xfusion no incluye ni redistribuye ningún core; los descarga el propio usuario desde el buildbot, con las licencias de cada core (ADR-0006, ADR-0024).

## Consequences

- Positivas: probado en la aplicación real: *Check for Update* devuelve la fecha del servidor y *Install Latest* instala `v1.7.4 4fd32ef` de Genesis Plus GX; un juego abierto después carga la DLL de `cores\genesis_plus_gx\v1.7.4_4fd32ef\`. 7 pruebas nuevas del manifiesto (`core_tests`).
- Negativas: solo existe el último build del servidor (no se pueden pedir versiones viejas que no se instalaron antes); sin comprobación automática ni avisos al arrancar; sin firmas; se añade Qt Network (+~1,5 MB al ejecutable).
- Sin hacer: probar Linux, comprobar actualizaciones al arrancar si el usuario lo pide, borrar de golpe las versiones antiguas, y mostrar las notas de cada build.
