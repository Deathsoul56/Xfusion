---
title: "ADR-0013: Ajustes de video y audio globales"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["settings", "video", "audio", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0013: Ajustes de video y audio globales

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aprobado el 2026-11-19 como parte de la Fase 2 (pendiente pedido por el plan: "establecer qué opciones son globales y cuáles dependen del core").

**Sustituido en parte por el ADR-0031 (2026-11-25):** las páginas Display y Sound se pueden fijar por sistema; los valores globales siguen siendo el punto de partida y el valor que sigue cada sistema que no tiene uno propio. El driver de vídeo sigue siendo global.

## Context

El frontend dibuja la imagen y reproduce el audio; el core solo entrega fotogramas y muestras. Las opciones de presentación (aspecto, filtro, escala) y el volumen no dependen de qué core emula el juego.

## Decision

1. **Son globales** y se guardan en `<datos>\config\settings.json` (JSON versión 1, `QSaveFile`): aspecto (`core`, `4:3`, `stretch`), filtro suave, escalado entero, volumen (0–100) y silencio. `AppSettings` los modela y valida (volumen acotado, valores desconocidos vuelven al valor por defecto).
2. **No dependen del core:** la relación de aspecto «núcleo» usa la que reporta el core, pero la elección es del frontend. Las opciones propias de un core (variables libretro, p. ej. el tipo de mando de ADR-0010) viven con el perfil del core o con la configuración de entrada, no aquí.
3. La geometría de la imagen es una función pura, `displayRect` (`DisplayLayout.*`), probada sin ventana. Con escalado entero se usa un múltiplo del alto del fotograma si cabe y, si no, el ajuste normal; en modo estirar se ignora.
4. El volumen se aplica como ganancia lineal del flujo de audio de SDL3 (`AudioOutput::setGain`), sin tocar el hilo de emulación ni el ritmo basado en la cola de audio (ADR-0005).
5. Los cambios se aplican al pulsar OK en `Settings`; Cancelar los descarta. «Restaurar valores» actúa sobre la página visible.

## Consequences

- Un solo archivo y una sola ruta de código para todos los sistemas; añadir un core no cambia estos ajustes.
- Si más adelante se quiere un ajuste por sistema (p. ej. aspecto distinto en Game Gear), habrá que añadir una capa de sobrescritura por sistema sobre estos valores.
- El volumen es lineal (100 % = ganancia 1); no hay curva perceptual.
