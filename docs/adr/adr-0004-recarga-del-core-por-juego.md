---
title: "ADR-0004: Recargar el core completo al cargar cada juego"
status: "Accepted"
date: "2026-11-19"
authors: "Rafa (@raravenam_bch), Copilot"
tags: ["libretro", "picodrive", "decision"]
supersedes: ""
superseded_by: ""
---

# ADR-0004: Recargar el core completo al cargar cada juego

## Status

Proposed | **Accepted** | Rejected | Superseded | Deprecated

Aceptada. La causa raíz está identificada y confirmada (ver Context); la recarga deja de ser un parche provisional.

## Context

En la aplicación real, al abrir un segundo juego con Abrir (por ejemplo Castlevania - Bloodlines tras Sonic & Knuckles), los botones A y B se leían invertidos: la A de Sega actuaba al soltar la tecla en vez de al apretarla, mientras que C y Start funcionaban bien. Con el primer juego nunca ocurría, y el defecto desaparecía solo pasado un rato.

### Causa raíz

`pico/memory.c` de PicoDrive guarda `padTHLatency[]` y `padTLLatency[]` en variables estáticas de archivo. `port_read()` hace esto con cada lectura del mando:

```c
if (CYCLES_GE(SekCyclesDone(), padTLLatency[i]))
  padTLLatency[i] = SekCyclesDone();
else
  in ^= 0x10; // TL
```

El bit 4 (línea TL del puerto) es donde llegan A y B. Al cargar un juego nuevo, `PicoPower()` pone a cero el contador de ciclos (`Pico.t.m68c_cnt`) pero **no** esas dos variables, que conservan el valor del juego anterior. Mientras el juego nuevo no alcance ese valor, `SekCyclesDone()` es menor y cada lectura invierte el bit 4. La inversión dura tanto tiempo emulado como duró el juego anterior, por eso "se arreglaba sola" y por eso con el primer juego no ocurría. Reiniciar con `retro_reset` no la causa: `PicoReset` no reinicia el contador de ciclos.

### Evidencia

Con un arnés sin interfaz, sin estados de guardado (que restauran esas variables y enmascaran el efecto), cargando Sonic & Knuckles y luego Castlevania en el mismo core:

| Duración del primer juego | Byte de entrada en reposo del segundo juego |
|---|---|
| 600 fotogramas | `50` hasta el fotograma 600, `00` desde entonces |
| 1200 fotogramas | `50` hasta el fotograma 1200, `00` desde entonces |
| 2400 fotogramas | `50` al menos hasta el fotograma 1800 |
| cualquiera, con recarga del core | `00` siempre |

Un core compilado en una copia temporal con un parche que reinicia ambas variables en `PicoPower()` y **sin** recarga dio `00` desde el primer instante. Quedó descartado un fallo del teclado o de Xfusion.

Intentos anteriores que no resolvían la causa: pedir a PicoDrive que releyera sus variables tras cargar el juego y consultar el teclado por polling. El mando de 6 botones sigue siendo necesario para X, Y y Z, pero no era la causa.

## Decision

A partir del segundo juego, `LibretroCore::loadGame` descarga y vuelve a cargar la biblioteca del core (`retro_deinit`, descarga, carga, `retro_init`) antes de cargar el juego. El primer juego usa el core ya inicializado. Se activa con el campo `reloadPerGame` del perfil del core (ADR-0007). No se modifica PicoDrive (ADR-0006).

## Consequences

### Positive

- **POS-001**: Cada juego parte del estado limpio que tiene el primero, y no solo respecto a esas dos variables sino a cualquier otro estado estático que el core conserve.
- **POS-002**: El coste es despreciable: la segunda carga midió 7 ms con recarga y 5 a 6 ms sin ella.
- **POS-003**: Se conserva un núcleo sin parches, compatible con las actualizaciones de la sección 3.2 del plan.

### Negative

- **NEG-001**: Cada core nuevo puede tener su propio estado estático; hay que comprobarlo y decidir `reloadPerGame` en su perfil.
- **NEG-002**: La causa está en PicoDrive y sigue sin corregirse allí; otros frontends que mantengan el core cargado entre juegos pueden verla.

## Alternatives Considered

### Parchear PicoDrive

- **ALT-001**: **Description**: Reiniciar `padTHLatency` y `padTLLatency` en `PicoPower()` (probado en una copia temporal: elimina el síntoma).
- **ALT-002**: **Rejection Reason**: Obligaría a distribuir un core modificado con su código fuente completo (ADR-0006) y a mantener el parche en cada actualización. Queda como mejora a proponer a PicoDrive (ver IMP-002).

### Reiniciar solo el estado del juego

- **ALT-003**: **Description**: Llamar a `retro_unload_game` y `retro_load_game` sin descargar el core.
- **ALT-004**: **Rejection Reason**: Es el comportamiento anterior, que mostraba el defecto.

### Ejecutar cada juego en un proceso nuevo

- **ALT-005**: **Description**: Aislar el core en un proceso aparte por partida.
- **ALT-006**: **Rejection Reason**: Resolvería también un cierre brusco del core, pero exige video, audio y entrada entre procesos. Desproporcionado para este caso.

## Implementation Notes

- **IMP-001**: No retirar `reloadPerGame` sin reproducir el caso: cargar Sonic & Knuckles, jugar unos segundos, abrir Castlevania - Bloodlines en el mismo core y leer en la RAM del juego el byte de entrada en reposo (`0x9061`): debe valer `00`, no `50`.
- **IMP-002**: Opcional: proponer a PicoDrive (notaz/picodrive) el reinicio de ambas variables en `PicoPower()`. El parche mínimo añade `io_ports_reset_latency()` en `pico/memory.c`, que pone a cero `padTHLatency` y `padTLLatency`, y la llama desde `PicoPower()`. No se ha enviado nada al proyecto.
- **IMP-003**: Con el hilo de emulación (ADR-0005) la recarga ocurre con el hilo detenido: `closeGame` lo detiene antes de tocar el core y `openGame` lo crea después de cargar el juego. Verificado reabriendo juegos y cambiando de sistema en una misma sesión.

## References

- **REF-001**: ADR-0001, ADR-0005, ADR-0006, ADR-0007.
- **REF-002**: `external/picodrive/pico/memory.c` (`port_read`), `external/picodrive/pico/pico.c` (`PicoPower`, `PicoReset`) en el commit `26ecb2b`.
- **REF-003**: `frontend_sega_multiemulador.md`, sección 10.3.
