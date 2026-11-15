# Firmware del AUX (A2)

**Proyecto:** Isla del Sol: inversor 48 V → 127 V / 60 Hz

**Micro:** AUX (A2) de la [tarjeta de control](../../hardware/control/README.md). Se encarga de la comunicación, la interfaz y la vigilancia. Es el **maestro I2C** del enlace con el CRÍTICO (DC-017).

**Licencia:** MIT ([LICENSE.md](../../LICENSE.md))

**Por qué existe el AUX:** un solo ATmega328P no puede con todo el inversor. El SPWM ya ocupa ~29 % del CPU del CRÍTICO y el AVR no tiene prioridades entre interrupciones: el LCD, el CAN y los mensajes en el mismo micro deformarían la onda. Además, 2 KB de RAM no alcanzan para los dos trabajos. Por eso el CRÍTICO **genera la onda y protege**, y el AUX **informa y conversa** ([DC-010](../../docs/decisiones_criticas.md), [DC-017](../../docs/decisiones_criticas.md)). El enlace I2C es lo que los coordina. Justificación completa: [firmware del CRÍTICO, sección 7.3](../critico/README.md#por-qué-dos-microcontroladores-y-un-enlace-entre-ellos).

| Sketch | Qué hace | Estado |
| --- | --- | --- |
| [`ISLA_AUX_PRUEBA_I2C`](ISLA_AUX_PRUEBA_I2C/ISLA_AUX_PRUEBA_I2C.ino) | Prueba del enlace: PING cada 100 ms, lee la foto de 16 bytes del CRÍTICO, revisa el CRC-8, avisa de enlace establecido, perdido, recuperado o congelado, y libera el bus si se traba | ✅ Validado en banco (protocolo v1; ya actualizado a v2) |
| [`ISLA_AUX_PANEL`](ISLA_AUX_PANEL/LEEME.md) | **Panel:** pantalla 16×2 con una perilla `LCD_TIPO` (1 = AIP31068 de SimulIDE, 0 = PCF8574 de tienda) en I2C por software (D4/D5), botonera de 4 botones en A7, pantallas de datos, **SETUP** (B4 sostenido 3 s) con la **tecnología de batería**, enlace con el CRÍTICO como maestro | ✅ Pantalla, botonera, SETUP y batería **validados en SimulIDE**; ⏳ físico pendiente |

- **Protocolo, resultados y advertencias:** [firmware del CRÍTICO, sección 7.3](../critico/README.md#73-prueba-de-banco-del-enlace-i2c-crítico--aux-validada-2026-11-13).
- **`enlace_protocolo.h` y `perfiles_bateria.h` deben ser idénticos** en el CRÍTICO y en el AUX.
- **La carpeta se llama `auxiliar`, no `aux`:** `AUX` es un nombre reservado de Windows y no se puede descomprimir ni clonar.
- **Pines del AUX:** [mapa de pines](../../hardware/control/README.md#11-mapa-de-pines). El sketch ya deja D7 (AUX_KILL) en alto y D10 como salida.

> ⚠ **Nunca monitorear con dos laptops a la vez** (lazo de tierra). Ver la advertencia en la sección 7.3.
