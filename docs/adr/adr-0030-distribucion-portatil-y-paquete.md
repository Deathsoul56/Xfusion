---
title: "ADR-0030: Distribución portátil, primer arranque y paquete reproducible"
status: "Accepted"
date: "2026-11-25"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["distribucion", "paquete", "primer-arranque", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0030: Distribución portátil, primer arranque y paquete reproducible

## Status

Accepted.

## Context

La Fase 6 pide una primera distribución: instalación limpia, rutas con caracteres no ASCII, ejecución sin permisos de administrador y un paquete reproducible. Al probar una copia limpia del ejecutable salieron tres problemas: sin cores la aplicación mostraba un error fatal que remitía a scripts de desarrollo y se cerraba; si la carpeta del ejecutable no se podía escribir, no guardaba nada; y la versión no estaba en ningún sitio (el About decía «0.1» y el agente de usuario de RetroAchievements, «0.1.0», cada uno por su lado).

## Decision

1. **Portátil por defecto.** Todo va junto al ejecutable. Si esa carpeta no admite escritura (se prueba creando un archivo), se usa %LOCALAPPDATA%\Xfusion. XFUSION_DATA_DIR sigue mandando sobre ambas. No hay instalador: el paquete es un .zip.
2. **Sin cores no es un error.** La ventana abre igualmente y ofrece *Manage Cores* (también al abrir un juego para el que no hay core). Los cores no se incluyen en el paquete por su licencia (ADR-0006).
3. **Una sola versión.** Sale de project(Xfusion VERSION ...) en CMakeLists.txt: la define XFUSION_VERSION, la usa QApplication::setApplicationVersion y el recurso VERSIONINFO del .exe (generado desde xfusion.rc.in).
4. **Paquete reproducible** con scripts/package.ps1: compila desde cero en uild-release\, pasa las pruebas y crea dist\Xfusion-<versión>-win64.zip (ejecutable, LICENSE, THIRD_PARTY_NOTICES.md, README, docs\ y un LEEME.txt) con su SHA-256. SOURCE_DATE_EPOCH toma la fecha del commit y el zip se escribe con orden y marcas de tiempo fijos.

## Consequences

- Positivas: dos paquetes construidos desde cero con el mismo commit dieron el mismo zip y el mismo xfusion.exe (SHA-256 idéntico). Una copia limpia en una carpeta con ${i}, ${a} y caracteres japoneses arranca, descarga los dos cores, abre un juego de una ruta igual de rara y guarda estados y saves. Con la carpeta sin escritura (ACL denegada) usa el perfil del usuario.
- Negativas: el ejecutable no está firmado (SmartScreen puede avisar); el paquete pesa unos 15 MB; solo se ha probado en Windows y la reproducibilidad es con esta cadena de herramientas (MSYS2 CLANG64 con sus paquetes de ese día), no entre máquinas distintas.
- Sin hacer: firma de código, instalador, comprobación con una cuenta de usuario sin privilegios en un equipo limpio (se simuló con una ACL), y actualizaciones del propio frontend.
