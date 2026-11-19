---
title: "ADR-0012: Ejecutable único con Qt, SDL3 y libc++ estáticos"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["build", "packaging", "static", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0012: Ejecutable único con Qt, SDL3 y libc++ estáticos

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19: el propietario quiere una carpeta limpia, sin decenas de DLL junto al `.exe`.

## Context

Con Qt compartido, `build\` necesitaba unas 25 DLL (Qt, SDL3, libc++, icu, freetype...) y carpetas de plugins para correr sin MSYS2 en el `PATH`.

## Decision

1. En Windows, `XFUSION_STATIC` (activo por defecto) enlaza Qt (paquete MSYS2 `mingw-w64-clang-x86_64-qt6-static`), SDL3 (`SDL3::SDL3-static`) y la runtime de C++ (`-static`). El ejecutable solo importa DLL del sistema.
2. Solo se enlazan los plugins de Qt que se usan (`qt_import_plugins ... NO_DEFAULT`: plataforma Windows y estilo moderno; `offscreen` en las pruebas). Así no entran formatos de imagen ni TLS.
3. Las dependencias de FreeType y HarfBuzz que sus paquetes CMake no declaran (brotli, bz2, graphite2, rpcrt4, usp10) se añaden en `XFUSION_STATIC_LIBS`, y `CMAKE_FIND_LIBRARY_SUFFIXES`/`CMAKE_IMPORT_LIBRARY_SUFFIX` se fijan a `.a` para que no se resuelvan a las bibliotecas de importación de DLL.
4. El core de PicoDrive sigue siendo una DLL externa cargada en ejecución (ADR-0001, ADR-0006); la ABI de libretro es C, así que no depende de la runtime del ejecutable.
5. `-DXFUSION_STATIC=OFF` conserva la variante compartida, con `cmake --install` desplegando Qt y sus DLL.

## Consequences

- Positivas: un solo `xfusion.exe` (~27 MB) que corre sin `PATH` ni DLL auxiliares; carpeta de salida limpia.
- Negativas: MSYS2 necesita ≈2,4 GB más (Qt estático); enlazar tarda más y el exe es mayor; cada biblioteca nueva puede exigir declarar sus dependencias estáticas; las licencias de Qt (LGPL) exigen, con enlace estático, ofrecer los medios para reenlazar: pendiente en `THIRD_PARTY_NOTICES.md` antes de publicar.
- Cambiar de variante exige reconfigurar el directorio de build (no borrar `build\`: guarda saves y BIOS, ADR-0011).
