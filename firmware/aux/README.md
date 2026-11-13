# Firmware del AUX (A2)

**Proyecto:** Isla del Sol: inversor 48 V → 127 V / 60 Hz

**Micro:** AUX (A2) de la [tarjeta de control](../../hardware/control/README.md). Se encarga de la comunicación, la interfaz y la vigilancia. Es el **maestro I2C** del enlace con el CRÍTICO (DC-017).

**Licencia:** MIT ([LICENSE.md](../../LICENSE.md))

| Sketch | Qué hace | Estado |
| --- | --- | --- |
| [`ISLA_AUX_PRUEBA_I2C`](ISLA_AUX_PRUEBA_I2C/ISLA_AUX_PRUEBA_I2C.ino) | Prueba del enlace: PING cada 100 ms, lee la foto de 16 bytes del CRÍTICO, revisa el CRC-8, avisa de enlace establecido, perdido, recuperado o congelado, y libera el bus si se traba | ✅ Validado en banco |

- **Protocolo, resultados y advertencias:** [firmware del CRÍTICO, sección 7.3](../critico/README.md#73-prueba-de-banco-del-enlace-i2c-crítico--aux-validada-2026-11-13).
- **`enlace_protocolo.h` debe ser idéntico** en los dos sketches.
- **Pines del AUX:** [mapa de pines](../../hardware/control/README.md#11-mapa-de-pines). El sketch ya deja D7 (AUX_KILL) en alto y D10 como salida.

> ⚠ **Nunca monitorear con dos laptops a la vez** (lazo de tierra). Ver la advertencia en la sección 7.3.
