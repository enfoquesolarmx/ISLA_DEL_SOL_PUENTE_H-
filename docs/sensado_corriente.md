# Sensado de corriente DC y AC — etapa posterior

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Estado:** definición preliminar para una etapa posterior (después de cerrar los gate drivers). Todos los valores son de partida y se afinan en banco
**Fecha:** 2026-11-03
**Documentos relacionados:** [`compensacion_adaptativa_tiempo_muerto.md`](compensacion_adaptativa_tiempo_muerto.md), [`identificacion_dt_pin9.md`](identificacion_dt_pin9.md)

> Antes de diseñar con los valores de este documento, verificar los datos marcados *(verificar)* contra las hojas de datos.

---

## 1. Por qué dos sensores de corriente

Un solo sensor no ve todo. Cada uno mide una corriente distinta y cuida una parte distinta del sistema:

| Medición | Dónde | Cuida | Para qué |
| --- | --- | --- | --- |
| **Corriente DC** | Bus DC, entre el puente H y la batería (−) | **La batería y el puente** | Protección rápida contra cortos y conducción cruzada; corriente de carga y descarga; potencia y eficiencia; control del cargador |
| **Corriente AC** | En serie con el primario del transformador | **La onda y el transformador** | Signo y valor instantáneo para la compensación de tiempo muerto; lazo de corriente del SPWM; formación de red y flujo bidireccional; detección de componente DC en el primario |

Junto con las dos mediciones de voltaje forman los "sentidos" del inversor:

| Medición | Dónde | Estado |
| --- | --- | --- |
| Voltaje DC | Divisor en el bus | Por definir |
| Voltaje AC | Transformador 120 → 12 V + divisor | **Existe** |
| Corriente DC | Shunt + amplificador en el bus | Existe un INA en el bus; **revisar que sea bidireccional** (sección 3) |
| Corriente AC | Shunt + INA240 en el primario | **Pendiente** |

### Por qué el sensor DC no sirve para el lazo AC

Durante los estados de **rueda libre** (los dos MOSFET bajos o los dos altos encendidos), la corriente del transformador circula **dentro del puente sin pasar por el bus**. El sensor DC lee cero aunque el primario lleve 40 A. Además, la corriente del bus no da el signo instantáneo de la corriente AC. Por eso la compensación de tiempo muerto y el control de la onda necesitan el sensor AC.

### Por qué el sensor AC no reemplaza al DC

El sensor AC está en el dominio HV (flota con VS1), satura en picos de arranque si se dimensiona para precisión y no ve la corriente de la batería en los estados de rueda libre. La **protección contra cortos** debe venir del bus DC, por donde pasa siempre la corriente que entrega la batería.

---

## 2. Corrientes de diseño

| Condición | Lado DC (batería, 48 V) | Lado AC (primario, ~30.5 V RMS) |
| --- | --- | --- |
| Voltaje | 41–63 V (rango de batería de 48 V) | 127 V / 4.16 ≈ **30.5 V RMS** |
| 1000 W, FP 1 | ≈ 1000 / (48 × 0.9) ≈ **23 A promedio** | ≈ 1000 / 30.5 ≈ **33 A RMS**, **~46 A pico** |
| Arranque de motor (2–3 × nominal, segundos) | ~50–70 A | ~90–140 A pico |
| Modo cargador 1 kVA | **Corriente negativa** (entra a la batería) | ~33 A RMS, sentido inverso |

---

## 3. Sensor DC (bus)

### 3.1 Ubicación

```
Batería (+) ──────────────── puente H ───────┐
                                             │
Batería (−) ──[ shunt R_DC ]─────────────────┘   (lado bajo, referido a GND de potencia)
                │      │
                └─ INA ┘──► ADC del MCU
                     └──► comparador de sobrecorriente ──► apagado del PWM por hardware
```

### 3.2 Requisitos

- **Bidireccional:** en modo cargador la corriente del bus cambia de sentido. El amplificador necesita una **referencia a la mitad del voltaje** (por ejemplo 1.65 V con ADC de 3.3 V) para medir los dos sentidos. Si el INA actual solo es unidireccional, cambiarlo.
- **Conexión Kelvin:** dos pistas dedicadas desde los extremos del shunt al amplificador, sin corriente de potencia.
- **Filtro RC de entrada** según la hoja de datos del amplificador.

### 3.3 Dimensionamiento orientativo

| Shunt | Caída a 23 A | Potencia a 23 A | Caída a 70 A (arranque) |
| --- | --- | --- | --- |
| 0.5 mΩ | 11.5 mV | 0.26 W | 35 mV |
| **1 mΩ** | 23 mV | 0.53 W | 70 mV |

Con 1 mΩ y ganancia 20 V/V: ±1.65 V de excursión ≈ **±82 A** de escala completa, suficiente para ver el arranque de motores. Shunt de **2–3 W** (2512, o de barra) por los picos.

### 3.4 Protección por hardware (lo primero que se implementa)

El firmware es demasiado lento para salvar un MOSFET en un corto. La protección debe ser **por hardware**:

- **Comparador rápido** sobre la salida del amplificador, con umbral de ~**60–80 A** *(a definir según los MOSFET y la red de compuerta)*.
- La salida del comparador va a:
  - la **entrada de falla del PWM** del MCU (STM32: *break input* BKIN; ESP32: entrada de *fault* del MCPWM), que apaga las salidas sin intervención del software;
  - o, en ATmega328, a una compuerta lógica que fuerce HIN/LIN a 0.
- **Enclavado (latch):** el PWM queda apagado hasta que el firmware lo rearme y verifique la causa.
- **Tiempo de supresión (blanking) de ~1 µs** en cada flanco para no disparar con el pico normal de recuperación de los diodos.
- **Tiempo total de reacción objetivo: < 2–3 µs.**

> **Regla:** ninguna prueba de banco a plena potencia sin esta protección funcionando.

---

## 4. Sensor AC (primario del transformador)

### 4.1 Ubicación

```
VS1 ──[ shunt R_AC ]── primario del transformador ── VS2
         │      │
         └ INA240 ┘──► ADC del MCU
```

El shunt **flota con VS1**, que salta de 0 a ~48 V a 24 kHz. El amplificador debe tolerar ese modo común y rechazar sus transitorios.

### 4.2 Amplificador: INA240 (Texas Instruments)

| Característica | Valor |
| --- | --- |
| Modo común | **−4 V a 80 V**, independiente de la alimentación |
| Rechazo al PWM | **Mejorado** ("enhanced PWM rejection"), pensado para puentes y motores |
| Arquitectura | Bidireccional, salida de voltaje, *zero-drift* |
| Alimentación | 2.7–5.5 V |
| Ganancias | A1 = 20, A2 = 50, A3 = 100, A4 = 200 V/V *(verificar en SBOS662)* |
| Encapsulado | TSSOP-8 / SOIC-8 |
| Hoja de datos | TI SBOS662 |

### 4.3 Dimensionamiento orientativo

| Shunt | Potencia a 33 A RMS | Caída a 46 A pico | Ganancia | Escala completa (±1.65 V) | Comentario |
| --- | --- | --- | --- | --- | --- |
| 0.5 mΩ | 0.54 W | 23 mV | 50 (A2) | ±66 A | Más precisión en operación normal; satura en arranque de motores |
| **1 mΩ** | 1.1 W | 46 mV | **20 (A1)** | **±82 A** | **Recomendado para empezar:** ve casi todo el arranque |

- Shunt de **3 W** como mínimo (por los arranques), con conexión **Kelvin de 4 terminales**.
- Saturar en un arranque de motor es aceptable: la protección la da el sensor DC; el sensor AC solo necesita precisión en operación normal.
- **Separaciones:** el shunt, el INA240 y sus pistas de entrada están en el **dominio HV** (regla `HV_a_LV`, 0.6 mm). La alimentación y la salida del INA240 están referidas a GND; el chip hace la traslación internamente.

### 4.4 Funciones que habilita

1. **Compensación de tiempo muerto:** `d_cmd = d_ideal + sign(i) · DT · f_sw`, con el signo de este sensor.
2. **Lazo de corriente** del SPWM (control resonante en 60 Hz y armónicos).
3. **Formación de red con flujo bidireccional** (inversor y cargador al mismo tiempo, AC coupling).
4. **Protección del transformador contra saturación:**
   - Si el SPWM tiene asimetría (ramas o retardos distintos), aparece **componente DC** en el primario y el núcleo de hierro se satura poco a poco.
   - El firmware calcula el **promedio de la corriente del primario en cada ciclo de 60 Hz** y lo corrige ajustando ligeramente el ciclo de trabajo.
   - El sensor DC no puede ver este efecto.

### 4.5 Alternativas

| Sensor | Ventaja | Desventaja |
| --- | --- | --- |
| **Hall aislado (por ejemplo ACS758)** | Aislado, sin problema de modo común | Más caro, más ruido, deriva con la temperatura |
| **Transformador de corriente en el secundario (127 V)** | Aislado, barato, de trinchera | No mide DC (no sirve para la protección contra saturación) y tiene un pequeño desfase |
| **Shunt bajo cada MOSFET bajo** | Referido a GND_K, amplificador sencillo | Solo mide cuando conduce el MOSFET bajo: hay que reconstruir la corriente |

---

## 5. Acondicionamiento y muestreo

- **Referencia común:** las salidas de los dos amplificadores centradas en **VREF/2** (1.65 V con ADC de 3.3 V).
- **Filtro antialias:** RC con corte de ~**10–20 kHz** para la corriente AC (deja pasar la información del lazo y atenúa los flancos). Para el sensor DC del ADC, más filtrado; el comparador de protección toma la señal **antes** del filtro lento.
- **Muestreo sincronizado con el PWM:** a la **mitad del pulso** (en el centro del periodo con PWM centrado), lejos de los flancos de conmutación.
- **Número entero de muestras por ciclo de 60 Hz** (por ejemplo 100 o 400) para el Goertzel y el promedio del componente DC.
- **Protección del pin del ADC:** resistencia serie y diodos de enclavamiento.
- **MCU:** el ATmega328 no alcanza para un lazo de corriente con muestreo sincronizado a 24 kHz. Usar **STM32G4** (ADC sincronizado con el timer, entrada BKIN) o **ESP32** (MCPWM con entrada de falla).

---

## 6. Calibración y validación en banco

1. **Offset en cero:** sin corriente, registrar la lectura de cada sensor y restarla en firmware.
2. **Ganancia:** comparar contra un multímetro (DC) y una pinza amperimétrica de referencia (AC) en 3 o 4 puntos.
3. **Signo de la corriente AC:** confirmar que la convención física coincida con la del firmware (por ejemplo: positivo = sale de VS1 hacia el transformador). Un signo invertido hace que la compensación de tiempo muerto **empeore** el 3er armónico.
4. **Protección:** con bus reducido (12–24 V) y fuente con limitación de corriente, provocar una sobrecorriente controlada y medir con el osciloscopio el tiempo desde el disparo del comparador hasta el apagado de las compuertas (objetivo < 2–3 µs).
5. **Componente DC del primario:** medir el promedio por ciclo con carga resistiva; anotar el valor sin corrección como referencia.
6. **Ruido de conmutación:** ver la salida del INA240 con el osciloscopio; verificar que el muestreo cae lejos de los flancos.

---

## 7. Orden de implementación

| Paso | Qué | Depende de |
| --- | --- | --- |
| 1 | Sensor DC bidireccional + **comparador de protección por hardware** | — |
| 2 | Sensor AC con INA240 en el primario | Paso 1 (para probar con protección) |
| 3 | Acondicionamiento y muestreo sincronizado | Pasos 1 y 2; MCU con ADC sincronizado |
| 4 | Calibración en banco | Paso 3 |
| 5 | Compensación de tiempo muerto (etapa 2 de su hoja de ruta) | Paso 4 |
| 6 | Lazo de voltaje y corriente (control resonante) | Paso 5 |
| 7 | Formación de red con AC coupling y control por frecuencia | Paso 6 |

---

## 8. Bitácora

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-03 | Definición preliminar de los dos sensores (DC y AC) | Documento creado | Terminar los gate drivers; después, paso 1 |
| | | | |

---

## 9. Referencias

- Texas Instruments, **INA240** datasheet (SBOS662): −4 V a 80 V, bidireccional, rechazo mejorado al PWM.
- N. Mohan, T. Undeland y W. Robbins, *Power Electronics: Converters, Applications and Design*: medición y control de corriente en inversores.
- D. G. Holmes y T. A. Lipo, *Pulse Width Modulation for Power Converters*: muestreo sincronizado y control de corriente.
- SMA, *Off-Grid System — Frequency Shift Power Control*: referencia para la formación de red con AC coupling.
