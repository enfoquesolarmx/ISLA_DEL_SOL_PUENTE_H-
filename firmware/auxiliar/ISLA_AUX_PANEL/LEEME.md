# ISLA_AUX_PANEL — panel 16x2 + botonera + enlace I2C con el CRÍTICO (AUX)

## Estado (2026-11-15)

| Qué | Dónde se probó | Resultado |
|---|---|---|
| Pantalla AIP31068 (`LCD_TIPO 1`) | SimulIDE 1.1-SR2 | ✅ Contesta y muestra las pantallas |
| Botonera en A7 (con R42) | SimulIDE 1.1-SR2 | ✅ B1 197, B2 414, B3 613, sin botón 1022 |
| SETUP (B4 sostenido 3 s) | SimulIDE 1.1-SR2 | ✅ Entra con A7 = 814, muestra `SETUP 1/3` |
| Enlace I2C con el CRÍTICO | Banco, Nanos reales | ✅ Ya validado (`firmware/critico/README.md` §7.3) |
| Enlace I2C con el CRÍTICO | SimulIDE 1.1-SR2 | ❌ Falla del simulador (ver abajo) |
| 16x2 con PCF8574 (`LCD_TIPO 0`) | — | ⏳ Falta en físico |
| Botonera física con R42 | — | ⏳ Falta en físico |
| Pantalla 4 con el CRÍTICO real | — | ⏳ Falta en físico |

## La perilla (en `config_panel.h`)

```c
#define LCD_TIPO 1   // 1 = AIP31068 (SimulIDE 1.1 / Grove)   0 = PCF8574 16x2 normal
```

| LCD_TIPO | Pantalla | Dirección | Dónde |
|---|---|---|---|
| 1 | AIP31068 (I2C nativo) | 0x3E | SimulIDE 1.1, Grove LCD |
| 0 | HD44780 + mochila PCF8574 | 0x27 (PCF8574A: 0x3F) | 16x2 de tienda |

Cambia el número, compila y sube; nada más cambia. Cualquier otro valor no compila (`#error`).

## Conexiones

| Pin AUX | Va a |
|---|---|
| D4 | SDA de la pantalla + 4.7 kΩ a 5 V |
| D5 | SCL de la pantalla + 4.7 kΩ a 5 V |
| A7 | BOTON_LCD: **R42 = 10 kΩ a 5 V** (en la tarjeta de control, no en el panel) |
| A4 | A4 del CRÍTICO (SDA del enlace) + 4.7 kΩ a 5 V (R10) |
| A5 | A5 del CRÍTICO (SCL del enlace) + 4.7 kΩ a 5 V (R11) |
| GND | GND del CRÍTICO (una sola tierra) |

**Botonera.** Cada botón une A7 con GND a través de su propia resistencia, en paralelo:

```
5V ──[R42 10 kΩ]──┬── A7 (AUX)
                  ├──[B1]──[2.4 kΩ]──┐
                  ├──[B2]──[6.8 kΩ]──┤
                  ├──[B3]──[15 kΩ]───┤
                  └──[B4]──[39 kΩ]───┴── GND
```

La pantalla va en su propio bus por software (D4/D5). A4/A5 son sólo del enlace con el CRÍTICO, así que una pantalla trabada no tumba el enlace.

## Qué hace

- **Pantallas 1, 2 y 3** de `hardware/panel` §5, con datos de muestra que cambian solos.
  - B2 y B3 pasan de una pantalla a otra.
  - La frecuencia de la pantalla 1 viene del CRÍTICO; sin enlace se muestra `--`.
- **Pantalla 4 (CRITICO)**, con datos reales de la foto: `SPWM ON 60Hz` / `DT 1us  P9 1023`.
  - Sin enlace dice `SIN ENLACE`.
- **SETUP:** se entra con B4 sostenido 3 s.
  - B2 y B3 mueven entre opciones, B4 cambia el valor y B1 sale.
  - Opciones:
    1. la frecuencia (sólo se muestra; se cambia en `config_portadora.h` del CRÍTICO);
    2. la **tecnología de batería** (PLOMO, LFP15, LFP16);
    3. la luz de fondo (sólo PCF8574);
    4. la lectura cruda de A7.
- **Cambio de tecnología de batería (SETUP 2/4):**
  - B4 para escoger; B2 y B3 recorren PLOMO / LFP15 / LFP16. Se ven el corte y el alto de cada una (`C46.4 A58.4`).
  - B4 sostenido 3 s confirma; B1 cancela.
  - El AUX sólo **propone** por I2C (comando `0x02`). El CRÍTICO detiene la SPWM, revisa que el voltaje medido quepa en esa tecnología, la guarda en su EEPROM y vuelve a arrancar si la batería está en límites.
  - La pantalla muestra `ACEPTADO`, `RECHAZADO V/BAT` (el voltaje no cabe: ¿batería equivocada?) o `SIN RESPUESTA` (sin enlace; en SimulIDE siempre sale así).
  - En el banco también se puede cambiar por el Monitor Serie del CRÍTICO: `P0` plomo, `P1` LFP15, `P2` LFP16.
  - Los límites de cada tecnología, con sus fuentes, están en `perfiles_bateria.h` (idéntico en los dos sketches).
- **A7 por debajo de 102 cuentas** (corto a GND): pantalla FALLA BOTONERA.
- **Monitor Serie a 115200:**
  - qué pantalla se escogió y si contestó ACK;
  - cada botón con su lectura de A7;
  - los cambios del enlace (`>>> ENLACE OK` / `PERDIDO`) y un resumen cada 5 s;
  - si la pantalla se pierde o vuelve (se reinicia sola).

Los voltajes, las corrientes, la batería y la temperatura siguen siendo **de muestra**: el CRÍTICO todavía no los mide.

## Prueba en SimulIDE 1.1

**Hay que dibujar lo que en el hardware real está en otras tarjetas.** SimulIDE no toma en cuenta las pull-up internas del ATmega en I2C.

| Agrega en SimulIDE | Sin ella | En el hardware real |
|---|---|---|
| 10 kΩ de A7 del AUX a 5 V | A7 lee 0: FALLA BOTONERA | R42, tarjeta de control |
| 4.7 kΩ en D4 y D5 a 5 V | La AIP31068 no enciende | El módulo de la pantalla |
| 4.7 kΩ en A4 y A5 a 5 V | El bus no sube | R10/R11, tarjeta de control |
| 10 kΩ de A7 del CRÍTICO a 5 V | P9 FUERA DE RANGO: la SPWM no arranca | R12, tarjeta de control |

**Limitación de SimulIDE 1.1-SR2:** el ATmega328P simulado como esclavo I2C no suelta SDA cuando le toca transmitir.

- El AUX lee la foto en ceros, con `ver` y `timeout` altos y `SDA=0` aun después de los 9 pulsos de recuperación.
- La escritura del maestro al esclavo (PING) sí pasa.
- Con `ISLA_AUX_PRUEBA_I2C`, ya validado en banco, pasa lo mismo. Es del simulador, no del firmware.
- **En SimulIDE sólo se prueban la pantalla y la botonera. El enlace se prueba en hardware real.**

## Prueba en físico (pendiente)

1. **CRÍTICO:** sketch `ISLA_CRITICO` sin cambios.
2. **AUX:** este sketch con `LCD_TIPO 0` (16x2 con mochila PCF8574).
   - Si el serie dice `SIN ACK`, prueba la dirección 0x3F (PCF8574A).
3. **Revisa:**
   - las pantallas 1 a 4;
   - cada botón con su lectura de A7 (~198 / 414 / 614 / 815);
   - SETUP sostenido 3 s;
   - la luz de fondo desde SETUP.
4. **Desconexión:** quita A4.
   - En menos de 300 ms la frecuencia pasa a `--` y la pantalla 4 dice `SIN ENLACE`.
   - Al reconectar, vuelve sola.

## Verificado en PC (simavr)

- Compila sin avisos con `LCD_TIPO` 1 y 0: ~12.8 KB de flash (con Wire).
- Sin CRÍTICO conectado el AUX no se cuelga: hace ~10 intentos por segundo y reporta `sin foto valida`.
- Bytes de D4/D5 decodificados con un esclavo simulado que contesta ACK:

| Pantalla | Dirección | Arranque | Datos |
|---|---|---|---|
| AIP31068 | `7C` | `80 28` ×3, `80 0C`, `80 01`, `80 06`, `80 80` | `40 49`, `40 53`, `40 4C`… ("ISL…") |
| PCF8574 | `4E` | nibble 3 ×3, nibble 2, luego `28 0C 01 06` en mitades con pulso EN; luz (bit 3) encendida | RS = 1 |

## Archivos

| Archivo | Qué hace |
|---|---|
| `config_panel.h` | La perilla `LCD_TIPO`, direcciones y pines |
| `i2c_sw.h/.cpp` | I2C por software en D4/D5 (drenador abierto emulado, 9 pulsos de recuperación) |
| `lcd.h/.cpp` | Una sola interfaz de pantalla; backends AIP31068 y PCF8574 |
| `botonera.h/.cpp` | Escalera en A7: umbrales, antirrebote y sostenido 3 s |
| `enlace_aux.h/.cpp` | Maestro I2C hacia el CRÍTICO (misma lógica que `ISLA_AUX_PRUEBA_I2C`) |
| `enlace_protocolo.h` | Idéntico al del CRÍTICO |
| `ISLA_AUX_PANEL.ino` | Pantallas, menú y reportes |
