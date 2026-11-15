# ISLA_AUX_PANEL — panel 16x2 + botonera + enlace I2C con el CRÍTICO (AUX)

**Estado:**
- Pantalla AIP31068, botonera y SETUP validados en SimulIDE 1.1-SR2.
- Falta validar en físico: 16x2 con PCF8574 (`LCD_TIPO 0`) y botonera real.
- El enlace I2C con el CRÍTICO ya está validado en banco (ver `firmware/critico/README.md` §7.3).

## La perilla (en `config_panel.h`)

```c
#define LCD_TIPO 1   // 1 = AIP31068 (SimulIDE 1.1 / Grove)   0 = PCF8574 16x2 normal
```

| LCD_TIPO | Pantalla | Dirección | Dónde |
|---|---|---|---|
| 1 | AIP31068 (I2C nativo) | 0x3E | SimulIDE 1.1, Grove LCD |
| 0 | HD44780 + mochila PCF8574 | 0x27 (PCF8574A: 0x3F) | 16x2 de tienda |

Cualquier otro valor no compila (`#error`).

## Conexiones (iguales en SimulIDE y en el Nano)

| Pin AUX | Va a |
|---|---|
| D4 | SDA de la pantalla + 4.7 kΩ a 5 V |
| D5 | SCL de la pantalla + 4.7 kΩ a 5 V |
| A7 | escalera de botones: **R42 = 10 kΩ a 5 V** (en la tarjeta de control, no en el panel); cada botón une A7 con GND por su resistencia |
| B1 → 2.4 kΩ · B2 → 6.8 kΩ · B3 → 15 kΩ · B4 → 39 kΩ | |

| A4 | A4 del CRÍTICO (SDA del enlace) + 4.7 kΩ a 5 V |
| A5 | A5 del CRÍTICO (SCL del enlace) + 4.7 kΩ a 5 V |
| GND | GND del CRÍTICO (una sola tierra) |

La pantalla va en su propio bus por software (D4/D5); A4/A5 son sólo del
enlace con el CRÍTICO.

**En SimulIDE hay que dibujar lo que en el hardware real ya está en otras tarjetas:**
- R42 de 10 kΩ de A7 a 5 V. Sin ella, A7 lee 0 y sale FALLA BOTONERA.
- 4.7 kΩ a 5 V en D4 y D5. Sin ellas, la pantalla AIP31068 no enciende.
- 4.7 kΩ a 5 V en A4 y A5 del enlace (R10/R11 de la tarjeta de control).

SimulIDE no toma en cuenta las pull-up internas del ATmega en I2C.

Validado en SimulIDE 1.1-SR2 (2026-11-15):
- AIP31068 contesta y muestra la pantalla 1.
- Botonera: B1 197, B2 414, B3 613; sin botón 1022.
- B4 sostenido 3 s: entra a SETUP (A7 = 814); muestra `SETUP 1/3` / `Frec --Hz script`.

## Prueba de enlace en SimulIDE 1.1 (dos Arduino Nano)

1. **Nano CRÍTICO:** carga `hex/ISLA_CRITICO.hex` (sketch `ISLA_CRITICO` del repositorio, sin cambios). Es el esclavo 0x20.
2. **Nano AUX:** carga `hex/ISLA_AUX_PANEL_AIP31068.hex` (`LCD_TIPO 1`). Es el maestro.
3. **Conecta:** A4↔A4, A5↔A5 y GND↔GND, con 4.7 kΩ de A4 y de A5 a 5 V. Pantalla AIP31068 en D4/D5 del AUX, y botonera en A7 del AUX.
4. **A7 del CRÍTICO (P9):**
   - Sin nada conectado, P9 sale FUERA DE RANGO y la SPWM no arranca, pero el enlace sí funciona. La pantalla 4 dice `SPWM STOP P9`.
   - Con 10 kΩ de A7 a 5 V: 1023 cuentas, 1 µs, SPWM en ON.
   - Para 500 ns: 10 kΩ a 5 V y 6.67 kΩ (10 k ∥ 20 k) a GND, que dan unas 409 cuentas.
5. **Qué ver:**
   - En la pantalla 4 (B3 tres veces): `SPWM ON 60Hz` / `DT 1us  P9 1023`.
   - En la pantalla 1 (`INV 127V 60.0Hz`), la frecuencia viene del CRÍTICO.
6. **Desconexión:** desconecta A4 o borra el cable. En menos de 300 ms la frecuencia pasa a `--` y la pantalla 4 dice `SIN ENLACE`. Al reconectar, vuelve sola. El serie del AUX imprime `>>> ENLACE PERDIDO` y luego `>>> ENLACE OK`.

Los voltajes, las corrientes, la batería y la temperatura siguen siendo **de muestra**, porque el CRÍTICO todavía no los mide.

## Qué hace

- **Pantallas 1, 2 y 3** de `hardware/panel` §5, con datos de muestra que cambian solos, y **pantalla 4 (CRITICO)** con datos reales del enlace. B2 y B3 pasan de una a otra.
- **SETUP:** B4 sostenido 3 s.
  - Dentro de SETUP, B2 y B3 mueven entre opciones, B4 cambia el valor y B1 sale.
  - Opciones: la frecuencia (sólo se muestra; se cambia en `config_portadora.h`), la luz de fondo (sólo PCF8574) y la lectura cruda de A7.
- **Monitor Serie a 115200:**
  - qué pantalla se escogió y si contestó ACK;
  - cada botón con su lectura de A7;
  - si la pantalla se pierde o vuelve (se reinicia sola).
- **A7 por debajo de 102 cuentas** (corto a GND): pantalla de FALLA BOTONERA.

## Verificado en PC (simavr, 2026-11-15)

- Compila sin avisos con LCD_TIPO 1 y 0. Ocupa ~12.8 KB de flash (con Wire).
- Sin CRÍTICO conectado, el AUX no se cuelga: hace ~10 intentos de enlace por segundo y reporta `sin foto valida`.
- Bytes decodificados de D4/D5 con un esclavo simulado que contesta ACK:

| Pantalla | Dirección | Arranque | Datos |
|---|---|---|---|
| AIP31068 | `7C` | `80 28` ×3, `80 0C`, `80 01`, `80 06`, `80 80` | `40 49`, `40 53`, `40 4C`… ("ISL…") |
| PCF8574 | `4E` | nibble 3 ×3, nibble 2, luego `28 0C 01 06` en mitades con pulso EN; luz (bit 3) encendida | RS = 1 |

**Limitación de SimulIDE 1.1-SR2:** el ATmega328P simulado como esclavo I2C no suelta SDA cuando le toca transmitir.
- Resultado: el AUX lee la foto en ceros, con `ver` y `timeout` altos y `SDA=0` aun después de los 9 pulsos de recuperación.
- La escritura (PING) sí pasa.
- Con el sketch `ISLA_AUX_PRUEBA_I2C`, ya validado en banco, pasa lo mismo, así que es del simulador.
- El enlace se valida sólo en hardware real.

**Falta en físico:**
- 16x2 con PCF8574 (`LCD_TIPO 0`);
- botonera con R42;
- pantalla 4 con el CRÍTICO real.
