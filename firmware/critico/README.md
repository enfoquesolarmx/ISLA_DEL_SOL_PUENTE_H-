# Firmware del CRÍTICO (A1): SPWM, sensado y protecciones

**Proyecto:** Isla del Sol: inversor 48 V → 127 V / 60 Hz, 1000 W, con cargador de 1 kVA

**Micro:** CRÍTICO (A1) de la [tarjeta de control](../../hardware/control/README.md). Va en zócalo de Nano: ATmega328P a 16 MHz o Nano Every (ATmega4809) a 20 MHz ([DC-018](../../docs/decisiones_criticas.md)).

**Licencia:** MIT ([LICENSE.md](../../LICENSE.md))

**Estado:** paso 1, variables de la portadora. **Todavía no genera PWM.**

---

## 1. Cómo abrirlo

1. Abre `ISLA_CRITICO/ISLA_CRITICO.ino` en el Arduino IDE. La carpeta debe llamarse igual que el `.ino`, y todos los archivos de la carpeta aparecen como pestañas.
2. Elige la placa en el IDE:
   - **Arduino Nano** (ATmega328P): sin más cambios.
   - **Nano Every** (ATmega4809): con el core **MegaCoreX** a **20 MHz**. El core oficial corre a 16 MHz y el firmware no compila con él, a propósito.
3. Abre el Monitor Serie a **115200** baudios y compara el reporte de arranque con la [tabla de la sección 3](#3-valores-de-la-portadora).

> Si eliges otra placa u otro reloj, **el firmware no compila** y el mensaje dice por qué. Es intencional: un reloj equivocado cambia la frecuencia de salida y el tiempo muerto.

## 2. Organización por módulos

Cada archivo hace una sola cosa y empieza con un bloque que dice qué hace, qué pines usa, a qué tarjeta y conector corresponde y qué timer o interrupción es suyo. Los comentarios no ocupan flash ni RAM: el compilador los quita.

| Archivo | Qué hace | Estado |
| --- | --- | --- |
| `ISLA_CRITICO.ino` | `setup()` y `loop()`: solo llama a los módulos | ✅ Paso 1: salidas seguras + reporte |
| `config_portadora.h` | Portadora, muestras, TOP y tiempo muerto en cuentas. Solo constantes, sin registros | ✅ |
| `spwm.h` + `spwm_328p.cpp` / `spwm_4809.cpp` | Timer de la pierna A, tabla seno, ISR, pierna B en el cruce por cero, arranque suave | Pendiente |
| `dt_p9.h/.cpp` | Lectura de P9 al encender y vigilancia ([identificación por P9](../../docs/identificacion_dt_pin9.md)) | Pendiente |
| `adc_bateria.h/.cpp` | Voltaje y corriente de batería | Pendiente |
| `ac_fb_inversor.h/.cpp` | Retroalimentación del voltaje de salida y lazo | Pendiente |
| `ac_fb_grid.h/.cpp` | Red: presencia, frecuencia, fase y PLL; transferencia | Pendiente |
| `protecciones.h/.cpp` | Fallas y apagado seguro (junto con el [corte por hardware](../../docs/corte_por_hardware.md)) | Pendiente |
| `enlace_i2c.h/.cpp` | Enlace con el AUX: tramas con versión, secuencia y CRC | Pendiente |

**Reglas entre módulos:**
1. **Cada timer tiene un solo dueño.** El timer de la pierna A es de `spwm`.
2. **Las variables que comparte una interrupción van como `volatile`.** Si son de más de 1 byte, se leen con las interrupciones apagadas.
3. **Sin `analogRead()` ni `delay()` en operación:** el ADC lo dispara la portadora.
4. **Los números de diseño van solo en los `config_*.h`.** El código no lleva valores sueltos.
5. **La capa por chip** (registros de timers, ADC e interrupciones) va en archivos `_328p` / `_4809`. La lógica es común (DC-018).

## 3. Valores de la portadora

Las constantes se calculan al compilar a partir de `F_CPU`. El reporte de arranque debe dar exactamente esto:

| Valor | ATmega328P (16 MHz) | ATmega4809 (20 MHz) | De dónde sale |
| --- | --- | --- | --- |
| Portadora objetivo | 24 000 Hz | 24 000 Hz | 400 × 60 Hz ([modulación SPWM](../../docs/modulacion_spwm.md), 4.3) |
| TOP (cuentas) | **333** | **416** | reloj / (2 × 24 kHz), truncado |
| Portadora real | 24 024.02 Hz | 24 038.46 Hz | reloj / (2 × TOP) |
| Ciclos de CPU por periodo | 666 | 832 | Presupuesto de la ISR ([DC-017](../../docs/decisiones_criticas.md)) |
| Muestras por ciclo / semiciclo / cuarto | 400 / 200 / 100 | 400 / 200 / 100 | Tabla seno de un cuarto de onda en flash |
| Frecuencia de salida | **60.060 Hz** (+1000 ppm) | **60.096 Hz** (+1600 ppm) | Portadora real / 400 |
| Tiempo por cuenta | 62.5 ns | 50 ns | 1 / reloj |
| Tiempo muerto 500 ns (P9 a GND) | **8 cuentas** | **10 cuentas** | Redondeo hacia arriba |
| Tiempo muerto 1 µs (P9 abierto) | **16 cuentas** | **20 cuentas** | Redondeo hacia arriba |
| Comparador alto máximo | 317 | 396 | TOP − tiempo muerto legacy |
| Tiempo muerto / periodo | 1.2 % / 2.4 % | 1.2 % / 2.4 % | Igual que en modulación SPWM |

### 3.1 Por qué estos números

- **Portadora sincronizada a 400 × 60 Hz.** Cada ciclo de salida tiene exactamente 400 periodos de portadora: no hay subarmónicos y el cruce por cero cae siempre en el mismo punto.
- **Doble pendiente** (*phase-correct* / *dual-slope*). Los pulsos quedan centrados en el periodo. Con el lado bajo invertido y su comparador DT cuentas más arriba (`OCR1B = OCR1A + DT`), se abre el tiempo muerto **en cada flanco** con el mismo timer.
- **Redondeo hacia arriba en el tiempo muerto.** Nunca queda menos del pedido.
- **Comparador alto limitado a TOP − DT legacy.** El comparador del lado bajo nunca pasa del TOP, con cualquier módulo que se conecte.
- **Salida a 60.06 Hz en modo isla** (0.1 % arriba de 60 Hz). Es el precio de un TOP entero con este reloj. Las cargas no lo notan. En modo red, el PLL de `ac_fb_grid` ajustará el TOP para seguir la frecuencia de la red.

### 3.2 Protecciones al compilar

El firmware **no compila** si pasa algo de esto:
- Chip distinto de 328P o 4809, o reloj distinto de 16 / 20 MHz.
- Portadora fuera de 20–25 kHz, o que no sea múltiplo de 60 Hz.
- Salida más de 0.5 % lejos de 60 Hz.
- Tiempo muerto legacy menor o igual al moderno, o cualquiera de los dos por debajo de su valor en ns.
- Tiempo muerto mayor que el 10 % del semiperiodo.

## 4. Prueba de banco del paso 1

**Sin etapa de potencia. Solo el Nano por USB.**

1. Carga el sketch y abre el Monitor Serie a 115200 baudios. Compara el reporte con la sección 3.
2. Con el multímetro o el osciloscopio, verifica que **D9, D10, D11 y D3 estén en 0 V** desde el encendido. No debe aparecer ningún pulso en el arranque: el firmware fija el nivel antes de configurar el pin como salida.

## 5. Bitácora

| Fecha | Cambio |
| --- | --- |
| 2026-11-12 | Paso 1: `config_portadora.h` (portadora, muestras, TOP, tiempo muerto en cuentas, protecciones al compilar) y `ISLA_CRITICO.ino` con salidas seguras y reporte. Compila en 328P: 4.0 KB de flash, 199 B de RAM. Escrito desde cero (MIT); el borrador antiguo de 4400 líneas solo se usó como referencia de estudio |
