# SENSOR_DC_INA_COMPARADOR — módulo de monitoreo y protección de corriente DC

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Módulo:** sensor de corriente del **bus DC**, **bidireccional** (descarga e inyección/carga), con **protección de sobrecorriente por hardware** (comparador de ventana)
**Compatible con:** ATmega328P, STM32, ESP32 y otros MCU de 3.3 V o 5 V, sin cambiar piezas
**Estado:** esquemático y layout terminados. Pendiente: DRC final, serigrafía, fabricación y validación en banco
**Fecha:** 2026-11-04
**Documentos relacionados:** [`docs/sensado_corriente.md`](../../../docs/sensado_corriente.md) (por qué dos sensores, orden de implementación)

> Shunts definidos: **Littelfuse SSA2512LR001FWR (1 mΩ)** en la tarjeta de potencia, con **1, 2 o 3 piezas en paralelo** según el uso (sección 4). La calibración del módulo depende de cuántas se monten.

---

## 1. Qué hace

El módulo se conecta **pegado al shunt** del bus DC de la tarjeta de potencia (R27/R14) y cumple dos funciones:

| Función | Salida | Para qué |
| --- | --- | --- |
| **Medir** la corriente de la batería, en los dos sentidos | **I_OUT**: 0–3.3 V, centro 1.65 V | Potencia, eficiencia, control del cargador (CC/CV), corriente de batería |
| **Proteger** contra sobrecorriente | **FAULT**: colector abierto, activo en bajo | Apaga las señales de los gate drivers por hardware en ~2–3 µs |

```
            TARJETA DE CONTROL (ATmega / STM32 / ESP32)
                  ▲ I_OUT        ▲ FAULT
                  │ (ADC)        │ (pull-up + 74HC08 / BKIN / fault MCPWM)
            ┌─────┴──────────────┴─────┐
            │  J2 (arriba)             │
            │                          │
            │  SENSOR_DC_INA_COMPARADOR│
            │  78L05 · INA240 · LM393  │
            │                          │
            │  J1 (abajo, header)      │
            └─────┬──────────────┬─────┘
                  │ IN+ / IN−    │ 15 V
                  ▼ (Kelvin)     ▼
            TARJETA DE POTENCIA: shunt R27/R14 en el bus DC (lado bajo)
```

- **El shunt se queda en la tarjeta de potencia** (por él pasan 30–100 A). Al módulo solo llega su voltaje (milivoltios) por dos líneas Kelvin.
- **El módulo es intercambiable**, como los gate drivers: si se daña, se cambia sin tocar la tarjeta de potencia, y a futuro puede haber versiones con otros amplificadores en el mismo zócalo.

---

## 2. Compatibilidad con cualquier MCU: los cuatro principios

1. **Alimentación analógica propia y fija:** el módulo toma **15 V** y genera sus **5 V** con un 78L05. Los umbrales de corriente son **absolutos**: no cambian con el MCU.
2. **Salida analógica escalada a 0–3.3 V:** el INA240 da 0–5 V (centro 2.5 V); un divisor fijo lo lleva a **0–3.33 V (centro 1.67 V)**. Sirve para ADC de 3.3 V (centro exacto) y de 5 V (usa 2/3 del rango). El divisor también protege: la salida nunca pasa de 3.33 V.
3. **FAULT de colector abierto:** el módulo solo jala a GND. **La pull-up la pone la tarjeta de control, al voltaje de su MCU.**
4. **El corte de las señales va en la tarjeta de control:** 74HC08 con ATmega (5 V), 74HC08/74LVC08 a 3.3 V, o la entrada *break* interna de un STM32 (BKIN), que corta las salidas del timer por hardware.

| MCU | I_OUT | FAULT |
| --- | --- | --- |
| ATmega328P (5 V) | ADC 0–5 V: ~680 de 1024 cuentas útiles | INT0 + 74HC08 en las 4 señales PWM |
| STM32 (3.3 V) | ADC 0–3.3 V, rango completo | BKIN del timer avanzado (corte interno) |
| ESP32 (3.3 V) | ADC con atenuación de 11 dB (poco lineal) | Entrada de falla del MCPWM |

---

## 3. Interfaz

### 3.1 J1 — abajo, hacia la tarjeta de potencia (header de 2.54 mm, pegado al shunt)

| Pin | Señal | Nota |
| --- | --- | --- |
| 1 | GND | Blindaje |
| 2 | **IN+ (Kelvin)** | Desde el pad del shunt |
| 3 | **IN− (Kelvin)** | Desde el otro pad; vecino del 2 |
| 4 | GND | Blindaje |
| 5 | **Llave** | **Sin pin ni pad** (asimétrica, no está al centro) |
| 6 | 15 V | Alimentación; el módulo genera sus 5 V |

- Las señales de milivoltios (2 y 3) quedan **entre dos GND** y separadas de los 15 V por la llave.
- En la tarjeta de potencia, la posición 5 del zócalo hembra se **tapa**.
- **El módulo debe ir pegado al shunt**, con header corto y vertical. Nada de cables para IN+ e IN−.

### 3.2 J2 — arriba, hacia la tarjeta de control (Phoenix SPT 1,5/5-H-3,5, igual que los gate drivers)

| Terminal | Señal | Nota |
| --- | --- | --- |
| 1 | **I_OUT** | 0–3.3 V, centro ~1.67 V |
| 2 | GND | Trenzar con I_OUT |
| 3 | **FAULT** | Colector abierto, activo en bajo |
| 4 | GND | Trenzar con FAULT |
| 5 | NC | Libre |

**Referencia de tierra:** I_OUT está referida al GND del módulo, que es el lado bajo del shunt (tierra de potencia). La diferencia de tierra entre tarjetas se suma a la medición; el par trenzado y la unión única de tierras en la tarjeta de potencia la mantienen en unos pocos mV. Para la protección no importa.

---

## 4. Shunts y calibración

### 4.1 La pieza: Littelfuse SSA2512LR001FWR

Los shunts van en la **tarjeta de potencia** (no en el módulo), en el negativo del bus DC, entre GND_IN (batería) y GND (puente).

| Dato | Valor |
| --- | --- |
| Fabricante / parte | **Littelfuse SSA2512LR001FWR** (serie SSA, lámina metálica) |
| Mouser | **576-SSA2512LR001FWR** *(verificar el número y las existencias en mouser.com)* |
| Resistencia | **1 mΩ ±1 %** |
| Potencia | **6 W a 70 °C**, 4 W a 100 °C (con disipación adecuada por el cobre) |
| TCR | ±100 ppm/°C |
| Tamaño | 2512: L 6.35 × W 3.02 mm; terminales A = 1.0 mm; altura máx. 1.10 mm |
| Alternativas de la misma serie | SSA3921LR001F (7 W, ±50 ppm/°C) · SSA5931LR001F (10 W, ±50 ppm/°C), con huella distinta |
| Hoja de datos | [Littelfuse SSA Series](https://www.mouser.com/datasheet/2/240/media-3323775.pdf) |

**Huella KiCad:** `SSA2512_Littelfuse_Kelvin` (librería del proyecto), según el patrón recomendado por Littelfuse:

| Dato | Valor |
| --- | --- |
| Pads | 1.60 × 3.40 mm (esquinas redondeadas), en x = ±2.60 mm |
| Separación entre bordes internos | 3.60 mm |
| Guía Kelvin (capa Fab) | Marca en el **centro del borde interno** de cada pad: de ahí salen IN+ e IN− |
| Modelo 3D | `${KIPRJMOD}/3d/SSA2512_Littelfuse.step` (6.35 × 3.02 × 1.10 mm, simplificado) |

> La huella 2512 estándar de KiCad (`R_2512_6332Metric`) tiene pads más angostos y más separados (~4.4 mm). Para esta pieza se usa la de Littelfuse: el punto Kelvin queda justo donde empieza el elemento resistivo.

### 4.2 ¿Cuántas piezas instalar? (1, 2 o 3)

La tarjeta de potencia tiene **3 posiciones** de shunt en paralelo. Se montan **1, 2 o 3** según el uso:

| | **1 pieza** | **2 piezas** | **3 piezas** |
| --- | --- | --- | --- |
| **Uso recomendado** | **Pruebas en banco** | **Operación normal (1000 W)** | Reserva: arranques de motor fuertes o más potencia |
| Resistencia efectiva | **1 mΩ** | **0.5 mΩ** | **0.33 mΩ** |
| Sensibilidad en I_OUT | 13.3 mV/A | 6.7 mV/A | 4.4 mV/A |
| Rango de medición del INA240 (antes de saturar) | ±122 A | ±245 A | ±367 A (teórico) |
| **Límite de protección con R6 = 2.7 kΩ** | **±34 A** | **±68 A** | **±102 A** |
| R6 para ±68 A | — (no alcanza: usar 2 piezas) | **2.7 kΩ** (actual) | **1.6 kΩ** |
| Potencia por pieza a 1000 W (~30 A RMS en el bus) | ~0.9 W | ~0.23 W | ~0.10 W |
| Potencia por pieza en el límite de disparo | ~1.2 W | ~1.2 W | ~1.2 W (con R6 = 2.7 kΩ) |
| Resolución con STM32 (12 bits, 3.3 V) | ~0.06 A por cuenta | ~0.12 A | ~0.18 A |
| Resolución con ATmega328P (10 bits, 5 V) | ~0.37 A por cuenta | ~0.73 A | ~1.1 A |
| `R_SHUNT_OHM` en firmware | `0.001f` | `0.0005f` | `0.000333f` |
| Etiqueta CAL del módulo | "CAL: 1 mΩ / ±34 A" | "CAL: 0.5 mΩ / ±68 A" | "CAL: 0.33 mΩ / ±102 A" (o ±68 A con R6 = 1.6 kΩ) |

**Cómo leer la tabla:**

- **1 pieza (pruebas):** la protección dispara antes (±34 A), lo que hace más seguras las primeras pruebas del puente, y la medición es el doble de sensible. **No permite arrancar motores grandes**: un arranque de 50–100 A dispara la protección, como debe ser en pruebas.
- **2 piezas (operación):** la configuración de diseño. Deja margen para la operación a 1000 W (~23 A promedio, ~30 A RMS) y para arranques moderados.
- **3 piezas:** más margen térmico y de arranque. Si se quiere conservar el límite de ±68 A, cambiar **R6 a 1.6 kΩ** en el módulo; con R6 = 2.7 kΩ el límite sube a ±102 A.
- **Potencia por pieza:** en todas las configuraciones queda muy por debajo de los 6 W. En el disparo, cada pieza lleva ~34 A (~1.2 W) por unos microsegundos.

### 4.3 Reglas de montaje

1. **La posición con las pistas Kelvin se monta SIEMPRE.** El INA240 mide en esa pieza. Si esa posición se deja vacía, el INA240 mediría la caída en el cobre entre pads vacíos y la lectura sería incorrecta. Marcarla en la serigrafía: **"KELVIN · MONTAR SIEMPRE"**.
2. **Las posiciones vacías se marcan DNP** en el esquemático (salen así en la lista de materiales).
3. **Las tres posiciones con la misma huella** (`SSA2512_Littelfuse_Kelvin`) y layout simétrico: así la corriente se reparte igual entre las piezas montadas.
4. **Disipación:** planos de cobre anchos en los dos pads, varias vías térmicas hacia la otra cara y, de preferencia, cobre de 2 oz.
5. **Lejos de los barrenos de montaje** (o con separador/arandela de buena superficie): la flexión de la placa al apretar el tornillo puede fisurar las soldaduras.
6. **Serigrafía del valor:** escribir **"1mΩ 6W"** o **"1mR 6W"**, nunca "1M" (se lee como 1 megaohm).
7. **Al cambiar el número de piezas:** actualizar `R_SHUNT_OHM` en el firmware y la etiqueta **CAL** del módulo; ajustar R6 solo si se quiere conservar un límite fijo.
8. **Calibrar en banco** después de cualquier cambio: con una corriente conocida (pinza de referencia), ajustar `R_SHUNT_OHM`. La calibración absorbe la tolerancia, las soldaduras y el reparto entre piezas.

### 4.4 Fórmulas

| Magnitud | Fórmula |
| --- | --- |
| Salida del INA240 (INA_OUT) | V = 2.5 V + I × R_shunt × 20 |
| Salida al MCU (I_OUT) | V = 1.667 V + I × R_shunt × 13.33 |
| Corriente desde la lectura | **I = (V_I_OUT − 1.667 V) / (R_shunt × 13.33)** |
| Ventana del comparador | ΔV = 2.5 V × R6 / (R5 + R6 + R7), con R5 = R7 = 3.6 kΩ (R6 = 2.7 kΩ → 0.682 V) |
| Límite de protección | **I_límite = ΔV / (R_shunt × 20)** |
| R6 para un límite dado | R6 = ΔV × (R5 + R7) / (2.5 V − ΔV), con ΔV = I_límite × R_shunt × 20 |

### 4.5 Límite del LM393

Con 5 V, las entradas del LM393 trabajan bien hasta ~3.5 V (VCC − 1.5 V). **V_ALTO = 2.5 V + ΔV no debe pasar de ~3.5 V** → ΔV ≤ ~1 V. Para un límite mayor, montar más piezas de shunt, no abrir más la ventana. La referencia del INA240 (VS/2) y los umbrales salen de los mismos 5 V: si el 78L05 varía ±5 %, todo se mueve junto y el límite casi no cambia.

---

## 5. Circuito

### 5.1 Alimentación

| Ref. | Valor | Función |
| --- | --- | --- |
| U1 | L78L05 (SOT-89) | 15 V → 5 V |
| C15 | 1 µF X7R | Entrada del 78L05 |
| C14 | 1 µF X7R | Salida del 78L05 |
| C2 | 0.1 µF X7R | Pegado al pin 5 (VS) del INA240 |
| C5 | 0.1 µF X7R | Pegado a los pines 8 y 4 del LM393 (en B.Cu, bajo el DIP) |

### 5.2 Medición (INA240A1, ganancia 20, bidireccional)

```
IN+ (Kelvin) ──[R3 0 Ω]──┬──── pin 2 (IN+)
                         C3 10 nF (filtro diferencial; DNP mientras R3 = R4 = 0 Ω)
IN− (Kelvin) ──[R4 0 Ω]──┴──── pin 3 (IN−)

pin 5 (VS) = 5 V · pin 4 (GND) = GND
pin 6 (REF2) = 5 V · pin 7 (REF1) = GND   → referencia = VS/2 = 2.5 V

pin 8 ── INA_OUT ──[R1 1 kΩ]──┬──────────── I_OUT (J2-1)
                              ├──[R2 2 kΩ]── GND      (divisor × 0.667)
                              └──[C1 10 nF]─ GND      (filtro, corte ~24 kHz)
```

- **Filtro de entrada opcional:** si en banco hay ruido, montar **R3 = R4 = 10 Ω** y **C3 = 10 nF** (C0G/NP0 de preferencia). Constante de tiempo 0.2 µs: limpia los picos sin retrasar la protección. **No subir R3/R4 por encima de ~10 Ω** (error de ganancia; ver SBOS662). Mantener (R3 + R4) × C3 ≤ ~0.5 µs.
- **Impedancia de salida de I_OUT:** ~667 Ω con 10 nF; adecuada para el ADC del ATmega (≤ 10 kΩ) y para el muestreo rápido de un STM32.

### 5.3 Protección (LM393 en ventana)

```
INA_OUT ──[R8 1 kΩ]──┬── INA_RC        (blanking: τ = 1 µs)
                    [C4 1 nF]
                     │
                    GND

5 V ──[R5 3.6 kΩ]──┬── V_ALTO ≈ 3.18 V ──[C7 1 nF]── GND
                [R6 2.7 kΩ]
                   ├── V_BAJO ≈ 1.82 V ──[C6 1 nF]── GND
                [R7 3.6 kΩ]
                   │
                  GND

U3A (descarga): IN+ (3) = V_ALTO, IN− (2) = INA_RC → OUT (1) ─┐
                                                              ├── FAULT (J2-3), colector abierto
U3B (carga):    IN+ (5) = INA_RC, IN− (6) = V_BAJO → OUT (7) ─┘
VCC (8) = 5 V · GND (4) = GND
```

| Estado | INA_RC | FAULT |
| --- | --- | --- |
| Corriente normal | Entre 1.82 y 3.18 V | Alto (por la pull-up de la tarjeta de control) |
| Sobrecorriente de **descarga** | > V_ALTO | **Bajo** (U3A) |
| Sobrecorriente de **carga** | < V_BAJO | **Bajo** (U3B) |

- **El comparador toma INA_OUT (pin 8), no I_OUT:** I_OUT está filtrada (~7 µs) y dividida; la protección llegaría tarde.
- **Sin pull-up ni LED en el módulo:** la pull-up (10 kΩ al VCC del MCU) y el LED de falla van en la tarjeta de control.
- **Sin histéresis:** el enclavado lo hace el firmware al primer disparo.

### 5.4 Tiempo de respuesta de la protección

| Etapa | Tiempo aprox. |
| --- | --- |
| *Blanking* (R8/C4) | ~1 µs |
| LM393 | ~0.3–1.3 µs (según la sobreexcitación) |
| 74HC08 (tarjeta de control) | ~20 ns |
| Gate driver + descarga de compuerta | ~0.1–0.3 µs |
| **Total** | **~2–3 µs** |

---

## 6. Uso en la tarjeta de control

### 6.1 Corte por hardware (ATmega328P)

```
5 V ──[10 kΩ]──┬── FAULT (desde J2-3)
               ├── pines 2, 5, 10 y 13 del 74HC08 (entradas B)
               └── INT0 del ATmega

ATmega HA ──┬── 74HC08 1A → 1Y → J2-H del módulo A     (y así LA, HB, LB)
          [10 kΩ] a GND (pull-down: evita pulsos falsos durante el reset del MCU)
```

- **74HC08 y no CD4081 ni 74LS08:** debe dar ~10 mA al LED de un módulo TLP250.
- Con **STM32**, FAULT entra a **BKIN** y no hace falta el 74HC08.

### 6.2 Firmware

```c
// Conversión de la lectura (ADC de 10 bits, Vref = 5 V en ATmega)
#define R_SHUNT_OHM   0.0005f       // 1 pieza: 0.001f · 2 piezas: 0.0005f · 3 piezas: 0.000333f
#define V_CENTRO      1.667f        // calibrar: lectura con corriente cero
#define GANANCIA_OUT  13.33f        // 20 × 0.667

float corriente_dc(uint16_t adc){
    float v = adc * (5.0f / 1023.0f);
    return (v - V_CENTRO) / (R_SHUNT_OHM * GANANCIA_OUT);   // A; positivo = descarga
}

// Falla: el hardware ya cortó las señales; el firmware enclava
ISR(INT0_vect){
    pwm_deshabilitar();             // detener el timer y llevar las salidas a 0
    estado_falla = FALLA_SOBRECORRIENTE;
}
// Rearme: solo manual o tras verificar la causa, nunca automático en bucle
```

- **Calibrar V_CENTRO** al arrancar, con el PWM apagado (corriente cero), y guardarlo.
- **Signo:** positivo = la batería entrega (inversor); negativo = la batería recibe (cargador). Confirmarlo en banco: depende de cómo esté orientado el shunt.

---

## 7. Layout (lo que se cuidó)

- **J1 abajo, pegado al shunt; J2 arriba**, hacia la tarjeta de control.
- **IN+ e IN− como par:** corren juntos y **encimados en capas opuestas** (F.Cu y B.Cu) desde J1 hasta R3/R4; área de lazo casi nula. C3 pegado a los pines 2 y 3 del INA240.
- **Área sin cobre** alrededor del corredor de IN+/IN−; J1-1 y J1-4 unidos a GND por fuera del par.
- **INA_OUT corto** hasta R8; el LM393 junto al INA240; divisor de umbrales y C6/C7 junto al LM393.
- **C5 bajo el LM393** (B.Cu, entre las filas del DIP); C2 junto al pin 5 del INA240.
- **Vías de cosido** de GND en los bordes, uniendo los rellenos de las dos caras. Al quitar el pad de la llave en J1 se perdió una unión entre caras; las vías la reemplazan.
- **DRC:** los avisos de "huella no coincide con la librería" en J1 (pad de llave eliminado) son esperados. Si se usa *Actualizar huellas desde la librería*, **se restaura el pad 5 y se pierde la llave**.

### 7.1 Serigrafía pendiente

| Texto | Dónde |
| --- | --- |
| "SENSOR_DC_INA_COMPARADOR" y versión | Cualquier cara |
| J2: I_OUT · GND · FAULT · GND · NC, y "TRENZAR I_OUT Y FAULT CON GND" | Junto a J2 |
| J1: GND · IN+ · IN− · GND · **X** · 15V | Junto a J1 |
| "I_OUT 0–3.3V · centro 1.65V" y "FAULT OC" | Visible |
| **"CAL: ___ mΩ / ±___ A"** según las piezas montadas (sección 4.2) | Visible |
| "VCC 15V → 5V interno" | Junto al 78L05 |
| Valores de los componentes | Junto a cada uno |

---

## 8. Lista de materiales

| Ref. | Valor / parte | Función |
| --- | --- | --- |
| U1 | L78L05 (SOT-89) | Regulador de 5 V |
| U2 | **INA240A1PW** (TSSOP-8, ganancia 20) | Amplificador de corriente bidireccional |
| U3 | **LM393** (DIP-8) | Comparador doble (ventana) |
| R1 | 1 kΩ | Divisor de salida |
| R2 | 2 kΩ | Divisor de salida |
| R3, R4 | 0 Ω (10 Ω si se monta el filtro) | Entrada del INA240 |
| R5, R7 | 3.6 kΩ | Divisor de umbrales |
| R6 | 2.7 kΩ | Divisor de umbrales (define la ventana) |
| R8 | 1 kΩ | *Blanking* |
| C1 | 10 nF | Filtro de I_OUT |
| C2, C5 | 0.1 µF X7R | Desacoplo del INA240 y del LM393 |
| C3 | 10 nF C0G (DNP) | Filtro diferencial de entrada |
| C4 | 1 nF | *Blanking* |
| C6, C7 | 1 nF | Estabilizan V_BAJO y V_ALTO |
| C14, C15 | 1 µF X7R | Salida y entrada del 78L05 |
| J1 | Header macho 1 × 6, 2.54 mm, **sin pin 5** | A la tarjeta de potencia |
| J2 | Phoenix SPT 1,5/5-H-3,5 | A la tarjeta de control |
| *(tarjeta de potencia)* | **Littelfuse SSA2512LR001FWR**, 1 a 3 piezas | Shunts del bus DC (sección 4) |

---

## 9. Validación en banco

1. **Alimentación:** 15 V en J1-6 → 5 V en el 78L05, en el INA240 y en el LM393.
2. **Corriente cero:** I_OUT ≈ 1.67 V; INA_OUT ≈ 2.5 V; FAULT en alto (con pull-up externa). Guardar V_CENTRO.
3. **Ganancia y signo:** con una fuente con limitación de corriente y una carga, pasar 5, 10 y 20 A por el shunt; comparar contra una pinza o un multímetro de referencia. Repetir en sentido inverso (modo cargador).
4. **Umbrales sin potencia:** inyectar voltaje en INA_OUT (desconectando el INA240 o con una fuente a través de R8) y confirmar que FAULT cae en ~3.18 V y ~1.82 V.
5. **Disparo real a bajo voltaje:** con bus reducido (12–24 V) y limitación de corriente, provocar una sobrecorriente controlada; medir con el osciloscopio desde el cruce del umbral hasta el apagado de las compuertas (objetivo < 2–3 µs).
6. **Ruido de conmutación:** con el inversor operando, ver I_OUT e INA_RC en el osciloscopio; si hay picos que acercan INA_RC a los umbrales, montar el filtro de entrada (R3 = R4 = 10 Ω, C3 = 10 nF).
7. **Prueba de falla segura:** desconectar J2 con el equipo encendido a bajo voltaje y verificar el comportamiento del firmware (ver nota en `docs/sensado_corriente.md` sobre la lógica de FAULT).

---

## 10. Bitácora

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-04 | Definición del módulo (INA240 + LM393 en ventana), interfaz J1/J2 compatible con cualquier MCU, esquemático y layout | Etapas de medición y protección validadas en esquemático | Serigrafía, DRC final, fabricar |
| 2026-11-04 | Shunts definidos: Littelfuse SSA2512LR001FWR (1 mΩ, 6 W), huella `SSA2512_Littelfuse_Kelvin` y modelo 3D; configuraciones de 1, 2 y 3 piezas | 1 pieza para pruebas (±34 A), 2 para operación (±68 A) | Validar en banco con 1 pieza |
| | | | |

---

## 11. Referencias

- Texas Instruments, **INA240** datasheet (SBOS662): −4 V a 80 V, bidireccional, rechazo mejorado al PWM.
- Texas Instruments / onsemi, **LM393** datasheet: rango de modo común de entrada (VCC − 1.5 V), salida de colector abierto.
- STMicroelectronics, **L78L05** datasheet.
- Texas Instruments / Nexperia, **74HC08** datasheet (para la tarjeta de control).
- Documento del proyecto: [`docs/sensado_corriente.md`](../../../docs/sensado_corriente.md).
