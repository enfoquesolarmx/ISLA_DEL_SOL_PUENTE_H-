# Firmware del CRÍTICO (A1): SPWM, sensado y protecciones

**Proyecto:** Isla del Sol: inversor 48 V → 127 V / 60 Hz, 1000 W, con cargador de 1 kVA

**Micro:** CRÍTICO (A1) de la [tarjeta de control](../../hardware/control/README.md). Va en zócalo de Nano: ATmega328P a 16 MHz o Nano Every (ATmega4809) a 20 MHz ([DC-018](../../docs/decisiones_criticas.md)).

**Licencia:** MIT ([LICENSE.md](../../LICENSE.md))

**Estado:** paso 2, SPWM híbrida en el ATmega328P para **60 Hz o 50 Hz**, verificada en la PC. **Prueba de banco sin potencia:** D12 (EN_HW) queda en 0 y las señales no pasan de la compuerta AND a los gate drivers.

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
| `ISLA_CRITICO.ino` | `setup()` y `loop()`: solo llama a los módulos | ✅ Paso 2: reporte + SPWM con rampa de banco (D12 = 0) |
| `config_portadora.h` | Portadora, muestras, TOP y tiempo muerto en cuentas. Solo constantes, sin registros | ✅ |
| `config_spwm.h` | Pulso mínimo y amplitud máxima | ✅ Valores iniciales, por validar en banco |
| `tabla_seno.h` | Dos cuartos de onda Q16 en flash: 100 muestras (60 Hz) y 120 (50 Hz). **Generado** por `herramientas/genera_tabla_seno.py` | ✅ |
| `spwm.h` + `spwm_328p.cpp` | SPWM híbrida: Timer1, ISR de portadora, pierna B en el cruce por cero, apagado | ✅ 328P |
| `spwm_4809.cpp` | La misma SPWM con TCA0 | Pendiente: hoy da error al compilar, a propósito |
| Arranque suave | Rampa de amplitud con el lazo de voltaje | Pendiente (va con `ac_fb_inversor`) |
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
| Muestras por ciclo / semiciclo / cuarto, 60 Hz | 400 / 200 / 100 | 400 / 200 / 100 | Tabla seno de un cuarto de onda en flash |
| Muestras por ciclo / semiciclo / cuarto, 50 Hz | 480 / 240 / 120 | 480 / 240 / 120 | Ídem |
| Frecuencia de salida, 60 Hz | **60.060 Hz** (+1000 ppm) | **60.096 Hz** (+1600 ppm) | Portadora real / 400 |
| Frecuencia de salida, 50 Hz | **50.050 Hz** (+1000 ppm) | **50.080 Hz** (+1600 ppm) | Portadora real / 480 |
| Tiempo por cuenta | 62.5 ns | 50 ns | 1 / reloj |
| Tiempo muerto 500 ns (P9 a GND) | **8 cuentas** | **10 cuentas** | Redondeo hacia arriba |
| Tiempo muerto 1 µs (P9 abierto) | **16 cuentas** | **20 cuentas** | Redondeo hacia arriba |
| Comparador alto máximo | 317 | 396 | TOP − tiempo muerto legacy |
| Tiempo muerto / periodo | 1.2 % / 2.4 % | 1.2 % / 2.4 % | Igual que en modulación SPWM |
| Pulso mínimo | 8 cuentas (500 ns) | 10 cuentas (500 ns) | `config_spwm.h` |
| Amplitud máxima | **313** cuentas (m = 0.939) | 391 cuentas (m = 0.939) | TOP − DT legacy − pulso mínimo / 2 |

### 3.1 Por qué estos números

- **Portadora sincronizada a 400 × 60 Hz.** Cada ciclo de salida tiene exactamente 400 periodos de portadora: no hay subarmónicos y el cruce por cero cae siempre en el mismo punto.
- **Tabla seno de un cuarto de onda (100 muestras a 60 Hz, 200 B en flash; 120 a 50 Hz).** La senoidal es simétrica: la subida de 0° a 90° se lee tal cual, la bajada de 90° a 180° es la misma tabla leída al revés (`tabla[199 − k]` a 60 Hz, `tabla[239 − k]` a 50 Hz) y el semiciclo negativo usa los mismos valores, porque la polaridad la pone la **pierna B**. Ocupa 200 B en lugar de 800 B, y el espejo cuesta ~3–4 ciclos de 666 por periodo. Va en flash (`PROGMEM`): en RAM, una tabla completa se comería el 40 % de los 2 KB del 328P.
- **Cada muestra se toma al centro de su periodo:** sen((k + 0.5) · 180° / 200), no sen(k · 180° / 200). Así la muestra k y la 199 − k valen lo mismo y el espejo sale exacto. En el borde, el pico queda sin pareja, la onda sale asimétrica y aparece componente par.
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

## 4. SPWM híbrida (`spwm_328p.cpp`)

### 4.1 Qué hace cada pierna

| Semiciclo | Pierna B (60 o 50 Hz, por la ISR) | Pierna A (24 kHz, por el Timer1) | Salida |
| --- | --- | --- | --- |
| **+** | **LB** (D3) encendido | **HA** lleva el pulso de potencia; LA es su complemento | +Vbus × d / TOP |
| **−** | **HB** (D11) encendido | **LA** lleva el pulso de potencia; HA es su complemento | −Vbus × d / TOP |

d = amplitud × |sen θ| (en cuentas). El timer y sus salidas son **los mismos en los dos semiciclos**: solo cambia qué lado de la pierna A lleva el pulso.

| Semiciclo | OCR1A (HA) | OCR1B (LA) |
| --- | --- | --- |
| + | d | d + DT |
| − | TOP − d − DT | TOP − d |

### 4.2 El tiempo muerto lo hace el timer, no el programa

```
TCNT1   TOP ..........    /\          /\
                         /  \        /  \
        OCR1B (LA) ---- /----\------/----\----
        OCR1A (HA) --- /------\----/------\---
        BOTTOM ...... /        \  /        \  /
                                \/          \/
HA: alta mientras TCNT1 < OCR1A (no invertida, centrada en BOTTOM)
LA: alta mientras TCNT1 > OCR1B (invertida,    centrada en TOP)
```

Entre OCR1A y OCR1B hay DT cuentas: en la subida de la rampa, HA se apaga y DT cuentas después prende LA; en la bajada es al revés. **El hueco lo garantiza el hardware en cada flanco**, aunque el programa se atrase o se cuelgue. OCR1A y OCR1B tienen doble búfer: la ISR escribe en BOTTOM los valores del periodo **siguiente**, y tiene 666 ciclos para hacerlo.

### 4.3 Cruce por cero

| Muestra del semiciclo | Pierna A | Pierna B |
| --- | --- | --- |
| 1 … S−2 | Patrón del semiciclo | Fija |
| **S−1** (199 a 60 Hz, 239 a 50 Hz) | Apagada (OCR1A = 0, OCR1B = TOP) | **Se apaga** el interruptor que sale |
| **0** | Apagada | **Se enciende** el interruptor que entra |

- **Tiempo muerto de la pierna B: un periodo completo, 41.6 µs.** Nunca coinciden HB y LB.
- En las muestras 0 y S−1 el seno vale 0.8 % (60 Hz) o 0.65 % (50 Hz), así que el pulso sería menor que el mínimo y no se generaría de todos modos. No se pierde onda.
- **La amplitud solo cambia al inicio de un ciclo** (semiciclo +). Los dos semiciclos de cada ciclo salen iguales y **no aparece CD en el transformador**, aunque el lazo cambie la amplitud a cada rato.

### 4.4 Límites del ciclo de trabajo (`config_spwm.h`)

- **Pulso mínimo de 500 ns.** Un pulso más angosto no se genera: el MOSFET no alcanza a encender completo y se calienta sin entregar energía. El pulso centrado en BOTTOM se arma con dos mitades de periodos vecinos y puede quedar con una sola, por eso el umbral es el ancho **completo** (8 cuentas).
- **Amplitud máxima de 313 cuentas (m = 0.939).** Siempre deja un pulso del lado complementario. **Ese pulso recarga el capacitor de bootstrap** del lado alto en cada periodo: sin él, el lado alto se quedaría sin voltaje de compuerta.

### 4.5 Presupuesto de CPU (medido en simulador, simavr)

| Medición | Ciclos | % de 666 |
| --- | --- | --- |
| ISR, mediana | 169 | 25 % |
| ISR, peor caso | 180 | 27 % |
| Con latencia de entrada y `reti` | ~190 | **~29 %** |

Igual en 60 y 50 Hz. Leer la tabla y los límites de la frecuencia elegida desde variables cuesta ~10 ciclos.

Más de lo que estimó [DC-017](../../docs/decisiones_criticas.md) (80–120 ciclos): casi todo es la multiplicación de 32 bits y los registros que guarda. Sobra margen para el Timer0 de `millis()` y el puerto serie dentro del periodo. **Pendiente:** medir en banco y, si hace falta espacio para el ADC, optimizar la multiplicación.

## 5. Frecuencia de línea: 50 o 60 Hz

El mismo firmware trae las dos frecuencias. Hoy se elige en `config_portadora.h`:

```cpp
constexpr uint8_t FRECUENCIA_LINEA_HZ = LINEA_60_HZ;   // <-- 60 o 50
```

| Región (confirma siempre con la placa de la red o del generador) | Frecuencia |
| --- | --- |
| México, Centroamérica, Colombia, Venezuela, Ecuador, Perú, Brasil (la mayor parte), EUA, Canadá | **60 Hz** |
| Casi toda África, Europa, Medio Oriente, Argentina, Chile, Uruguay, Paraguay, Bolivia, gran parte de Asia | **50 Hz** |

### 5.1 Qué cambia y qué no

| | 60 Hz | 50 Hz |
| --- | --- | --- |
| Portadora | 24 kHz | **24 kHz (igual)** |
| TOP, tiempo muerto, pulso mínimo, amplitud máxima | iguales | iguales |
| Muestras por ciclo | 400 | **480** |
| Tabla en flash | 100 muestras (200 B) | 120 muestras (240 B) |
| Salida real (328P) | 60.060 Hz | 50.050 Hz |

**Por qué la portadora no cambia.** Otra opción era bajarla a 20 kHz para conservar 400 muestras y dar 50.000 Hz exactos. A 20 kHz, el rizo de corriente en el filtro de salida sube 20 % y el zumbido queda en el borde de lo audible. Con 24 kHz fijos no cambian el filtro, las pérdidas ni el tiempo muerto: solo la tabla. A cambio, la salida queda 0.1 % arriba, igual que a 60 Hz.

### 5.2 ⚠ Cambiar a 50 Hz no basta con el firmware

- **Transformador:** a 50 Hz el núcleo trabaja con **20 % más flujo** que a 60 Hz (flujo ∝ V / f). Un transformador diseñado solo para 60 Hz puede **saturarse y calentarse**. Debe estar especificado para 50 Hz o "50/60 Hz".
- **Voltaje:** casi todas las redes de 50 Hz son de **220–240 V**, no de 127 V. El transformador de 48 V → 127 V **no sirve**: hace falta otra relación de vueltas, además de relés, capacitores y fusibles de salida para 250 V.

### 5.3 Pendiente

- **Elegir la frecuencia sin reprogramar:** desde el menú SETUP del panel (AUX), guardada en EEPROM y enviada al CRÍTICO por I2C. El CRÍTICO la aplica solo con la SPWM detenida.
- **Con red presente:** medir la frecuencia de la red (`ac_fb_grid`) y negarse a sincronizar si no coincide con la elegida.

## 6. Prueba en la PC (`pruebas/`)

`pruebas/prueba_spwm.cpp` compila **el mismo `spwm_328p.cpp` que va al Nano**, con los registros simulados (`pruebas/avr_simulado/`). Recorre cada periodo cuenta por cuenta con la regla del comparador de la hoja de datos del ATmega328P (modo 8). Verifica:

1. HA y LA nunca encendidos juntos; HB y LB tampoco.
2. Tiempo muerto de la pierna A ≥ DT pedido, en cada flanco.
3. Tiempo muerto de la pierna B ≥ 1 periodo.
4. Ningún pulso de la pierna A menor que el pulso mínimo.
5. Sin CD por ciclo.
6. Fundamental de la salida igual a la amplitud pedida (error < 1 %).
7. Apagado: D12 = 0, las 4 salidas en bajo y el timer detenido.

Se prueba con **60 Hz y 50 Hz**, con DT de 500 ns, de 1 µs y con un valor inválido (debe caer en 1 µs), con amplitudes de 0 a 400 (se recorta a 313). Una frecuencia inválida (55 Hz) debe rechazarse y quedar en 60 Hz.

```
cd firmware/critico
g++ -std=gnu++11 -O2 -D__AVR_ATmega328P__ -DF_CPU=16000000UL \
    -Ipruebas/avr_simulado -IISLA_CRITICO \
    pruebas/prueba_spwm.cpp ISLA_CRITICO/spwm_328p.cpp -o prueba_spwm
./prueba_spwm
```

Resultado actual: **todo correcto** en las dos frecuencias. DT mínimo de 8 / 16 cuentas exactas, pierna B 666 cuentas, CD < 0.0001, error de la fundamental −0.24 % (60 Hz) y −0.23 % (50 Hz) a amplitud máxima. Ese error viene de los pulsos que se quitan cerca del cruce por cero.

> **Correr esta prueba cada vez que alguien toque `spwm_328p.cpp` o `config_spwm.h`.** Ya encontró un error: con el umbral a la mitad salían pulsos de 250 ns.

## 7. Prueba de banco del paso 2

**Sin etapa de potencia. Solo el Nano por USB, o la tarjeta de control sin módulos ni puente.**

1. Carga el sketch. El Monitor Serie a 115200 debe mostrar los valores de la sección 3 y **"DT en uso: 16"**: todavía no se lee P9 y se usa el seguro.
2. Cada 10 s debe imprimir **"Ciclos en 10 s: 600"** o 601 (60.06 Hz). Con `FRECUENCIA_LINEA_HZ = LINEA_50_HZ`: **500** o 501 (50.05 Hz).
3. Con osciloscopio de 2 canales, en los pines del Nano:
   - **D9 y D10 a la vez:** pulsos de 24.02 kHz, nunca altos juntos. Con zoom en un flanco, **el hueco entre los dos debe ser ≥ 1 µs**.
   - **D11 y D3 a la vez:** ondas cuadradas de 60.06 Hz (o 50.05 Hz), opuestas, con **~41.6 µs** en que los dos están en bajo en cada cambio.
   - **D9 con disparo en D3:** el ancho de los pulsos de D9 crece y baja como seno durante el semiciclo en que D3 está alto. En el otro semiciclo, D10 hace lo mismo.
4. **D12 debe estar en 0 V todo el tiempo.** Si la tarjeta de control está armada, en la salida de la compuerta AND (74HC08) no debe haber ningún pulso.

> Si cualquiera de estos puntos falla, **no conectes la etapa de potencia**.

## 8. Qué se tomó del borrador antiguo y qué cambió

El borrador de 4400 líneas (sin probar, derivado de un código GPL v3) se usó **solo como referencia de estudio**. Todo el código de esta carpeta está escrito desde cero (MIT).

| Tema | Borrador | Isla del Sol | Por qué |
| --- | --- | --- | --- |
| Timer y modo | Timer1, modo 8, TOP 333 | Igual | Mismo chip y portadora: el resultado es el mismo |
| Tiempo muerto | Lo hace el timer (OCR1B = OCR1A + TM, OC1B invertida). **TM fijo al compilar** (`TIEMPO_MUERTO_SEL`, según la red de compuerta montada) | Mismo método de hardware. **DT elegido al encender por P9** (8 o 16 cuentas); valor inválido → 16 | Un solo firmware para todos los módulos: el técnico cambia el módulo, no el código |
| Bordes del semiciclo | Valores especiales (0/0 = lado bajo fijo, TOP/TOP = lado alto fijo) y una bandera de "medio pulso pendiente" entre periodos | **Nunca** se usa un lado fijo encendido: siempre d con su DT, o la pierna A apagada (0/TOP) | Al pasar de un periodo con un lado fijo encendido al siguiente, los dos flancos pueden caer en la misma cuenta (DT = 0); así no hace falta la bandera. Probado cuenta por cuenta |
| Cruce por cero | Pierna B con ~42.75 µs de tiempo muerto | Igual idea: 2 periodos con la pierna A apagada, pierna B a 1 periodo | Coincide; aquí se verifica en la prueba de la PC |
| Tabla seno | Medio ciclo en **RAM**, calculada al arrancar | **Cuarto de onda en flash**, muestreada al centro del periodo | 0 B de RAM; espejo exacto |
| Amplitud | Se aplica por ciclo | Igual: solo al inicio del semiciclo + | Sin CD en el transformador |
| Pulso mínimo y amplitud máxima | Límite fijo del comparador | Pulso mínimo de 500 ns + amplitud que **siempre** deja pulso de bootstrap | Corregido con la prueba: el pulso de borde puede quedar con una sola mitad |
| Permiso del puente | No existía (el borrador no tiene la compuerta AND) | **D12 = 0 primero** al apagar; el SPWM nunca lo sube | Corte por hardware ([corte por hardware](../../docs/corte_por_hardware.md)) |
| Todo en un archivo | 4400 líneas con `#if` | Un archivo por módulo | Lo pediste así: cada archivo se entiende solo |

## 9. Bitácora

| Fecha | Cambio |
| --- | --- |
| 2026-11-12 | Paso 1: `config_portadora.h` (portadora, muestras, TOP, tiempo muerto en cuentas, protecciones al compilar) y `ISLA_CRITICO.ino` con salidas seguras y reporte. Explicación del cuarto de onda en el código y en la sección 3.1. Compila en 328P: 4.0 KB de flash, 199 B de RAM. Escrito desde cero (MIT); el borrador antiguo de 4400 líneas solo se usó como referencia de estudio |
| 2026-11-12 | **Soporte de 50 Hz y 60 Hz** (`FRECUENCIA_LINEA_HZ`): portadora fija de 24 kHz, 480 muestras a 50 Hz con su propia tabla de cuarto de onda (120). `spwm_preparar(dt, frecuencia)` rechaza valores inválidos y queda en 60 Hz. Advertencias de transformador y voltaje en la sección 5. Prueba en la PC: todo correcto en las dos frecuencias. ISR de 169–180 ciclos. Compila: 5.7 KB de flash, 233 B de RAM |
