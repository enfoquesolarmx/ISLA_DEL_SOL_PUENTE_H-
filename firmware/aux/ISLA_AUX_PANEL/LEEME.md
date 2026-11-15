# ISLA_AUX_PANEL — prueba de pantalla 16x2 + botonera (AUX)

**Estado: borrador, sin validar en SimulIDE ni en banco.**

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
| A7 | escalera de botones: 10 kΩ a 5 V; cada botón une A7 con GND por su resistencia |
| B1 → 2.4 kΩ · B2 → 6.8 kΩ · B3 → 15 kΩ · B4 → 39 kΩ | |

La pantalla va en su propio bus por software (D4/D5), no en A4/A5: ése es
del enlace con el CRÍTICO.

## Qué hace

- **Pantallas 1, 2 y 3** de `hardware/panel` §5, con datos de muestra que cambian solos. B2 y B3 pasan de una a otra.
- **SETUP:** B4 sostenido 3 s.
  - Dentro de SETUP, B2 y B3 mueven entre opciones, B4 cambia el valor y B1 sale.
  - Opciones: la frecuencia (sólo se muestra; se cambia en `config_portadora.h`), la luz de fondo (sólo PCF8574) y la lectura cruda de A7.
- **Monitor Serie a 115200:**
  - qué pantalla se escogió y si contestó ACK;
  - cada botón con su lectura de A7;
  - si la pantalla se pierde o vuelve (se reinicia sola).
- **A7 por debajo de 102 cuentas** (corto a GND): pantalla de FALLA BOTONERA.

## Verificado en PC (simavr, 2026-11-15)

- Compila sin avisos con LCD_TIPO 1 y 0. Ocupa ~8 KB de flash.
- Bytes decodificados de D4/D5 con un esclavo simulado que contesta ACK:

| Pantalla | Dirección | Arranque | Datos |
|---|---|---|---|
| AIP31068 | `7C` | `80 28` ×3, `80 0C`, `80 01`, `80 06`, `80 80` | `40 49`, `40 53`, `40 4C`… ("ISL…") |
| PCF8574 | `4E` | nibble 3 ×3, nibble 2, luego `28 0C 01 06` en mitades con pulso EN; luz (bit 3) encendida | RS = 1 |

**Falta** validarlo en SimulIDE (AIP31068) y en un 16x2 PCF8574 real.
