---
title: "ADR-0031: Ajustes de Display y Sound por sistema"
status: "Accepted"
date: "2026-11-25"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["ajustes", "display", "sound", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0031: Ajustes de Display y Sound por sistema

## Status

Accepted.

## Context

El ADR-0013 dejó el vídeo y el audio como ajustes globales. Siendo un frontend de seis sistemas, el usuario quiere, por ejemplo, un filtro o un volumen distintos para Game Gear que para Genesis, sin perder un valor global para todo lo demás.

## Decision

1. **Qué se puede fijar por sistema:** todo lo de las páginas *Display* (aspecto, escalado, filtro, mensajes, tamaño y esquina) y *Sound* (volúmenes, silencios, dispositivo, tasa y búfer). Lo demás (General, Emulation, Files, controles) sigue siendo global.
2. **Interfaz:** cada una de las dos páginas tiene arriba *Settings for:* con *Global (all systems)* y los seis sistemas. Con un sistema elegido, cada ajuste lleva una casilla *Global*: marcada, el sistema usa el valor global (se ve, pero no se edita); desmarcada, tiene el suyo. En un ajuste de sí o no hay tres estados: encendido, apagado y global. Al desmarcar *Global* el valor propio empieza igual al global. *Restore Defaults* en un sistema lo devuelve a seguir el global.
3. **Datos:** AppSettings::systemOverrides guarda, por sistema, solo lo que se cambia, con las mismas claves y valores que las secciones display y sound del archivo ("systems": {"genesis": {"display": {"osd_size": 40}}}). AppSettings::forSystem() devuelve los ajustes tal como son para un sistema: los globales con esas claves encima, leídos por romJson para que tengan la misma validación (límites, valores desconocidos). Al leer el archivo se descartan los sistemas y las claves que no existen.
4. **Aplicación:** MainWindow::applyView() calcula m_view (los ajustes del sistema del juego abierto, o los globales sin juego) y de él salen la imagen, los mensajes y el audio. Los atajos de volumen y silencio cambian el valor del sistema si éste lo tiene propio y el global si no.
5. **Código:** ScopedPage es la base de DisplayPage y SoundPage: guarda una copia de trabajo de los ajustes mientras la página está abierta y la vuelca al pulsar OK.

## Consequences

- Positivas: ningún valor se duplica en el código (el mismo formato de archivo sirve para los dos usos); pruebas de que un sistema sigue al global, de que los valores propios sobreviven al archivo y de que lo inválido se descarta; comprobado en la aplicación con un tamaño de mensajes distinto para Genesis (40 px) y Game Gear (24 px).
- Negativas: cada página tiene su propio selector de sistema (no se comparten); el cambio de un ajuste global no avisa de qué sistemas lo siguen.
- Sin hacer: más páginas por sistema (Emulation, controles) y el vídeo por GPU (ver el estudio de renderizado).
