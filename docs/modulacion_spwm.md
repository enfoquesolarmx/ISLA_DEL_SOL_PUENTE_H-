# Modulación SPWM: por qué híbrida en el inversor y unipolar en el cargador

Documento de sistema del proyecto **Isla del Sol — Puente H SPWM 48 V → 127 V / 60 Hz**.

Explica las tres formas clásicas de modular un puente H monofásico (bipolar, unipolar e híbrida) y justifica la elección del proyecto:

| Modo de operación | Modulación | Pierna A | Pierna B |
| --- | --- | --- | --- |
| **Inversor** (batería → carga) | **Híbrida** (unipolar con una pierna a frecuencia de línea) | 24 kHz | 60 Hz |
| **Cargador** (red → batería) | **Unipolar** | 24 kHz | 24 kHz |

---

## 1. El puente H y sus tres estados de salida

```
          +48 V
     ┌──────┴──────┐
    QAH           QBH
     │             │
     ├── VS1   VS2 ┤        V_AB = VS1 − VS2
     │   (A)   (B) │        → primario del transformador
    QAL           QBL
     └──────┬──────┘
           GND
```

Con un puente H se pueden aplicar al primario tres voltajes: **+Vdc**, **0** y **−Vdc**. Lo que distingue a cada modulación es **cuáles de estos estados usa** y **cuántos transistores conmutan a alta frecuencia** para lograrlo.

---

## 2. Las tres modulaciones

### 2.1 Bipolar (dos niveles)

- Los MOSFET se conmutan **en diagonal**: QAH + QBL contra QAL + QBH.
- La salida solo toma **+Vdc y −Vdc**; nunca usa el estado cero.
- Una sola señal: se compara la senoidal contra una portadora triangular.
- **Las 4 posiciones conmutan a f_sw.**

### 2.2 Unipolar (tres niveles)

- Cada pierna tiene su propia referencia: la pierna A sigue a +m·sen θ y la pierna B a −m·sen θ, comparadas contra la **misma** portadora.
- La salida toma **+Vdc, 0 y −Vdc**, en escalones de Vdc.
- El rizo de la salida aparece a **2·f_sw** (frecuencia efectiva duplicada).
- **Las 4 posiciones conmutan a f_sw.**

### 2.3 Híbrida (unipolar con pierna de línea)

- **Pierna A** a alta frecuencia (24 kHz); **pierna B** a frecuencia de línea (60 Hz).
- Semiciclo positivo: QBL encendido fijo (VS2 = 0) y la pierna A modula con d = m·sen θ.
- Semiciclo negativo: QBH encendido fijo (VS2 = Vdc) y la pierna A modula con d = 1 − m·|sen θ|.
- La salida toma **+Vdc, 0 y −Vdc**, en escalones de Vdc, con rizo a **f_sw**.
- **Solo 2 posiciones conmutan a f_sw**; las otras 2 conmutan 120 veces por segundo.

Texas Instruments usa este mismo esquema en su diseño de referencia de inversor monofásico TIDM-HV-1PH-DCAC: un par de interruptores a alta frecuencia y el otro a la frecuencia de la red [1].

---

## 3. Comparación

### 3.1 Rizo de corriente en el filtro

Con L como la inductancia de filtro referida al primario (dispersión del transformador + inductor E65 N87), el rizo pico a pico máximo de cada modulación es:

| Modulación | Escalón de voltaje | Frecuencia del rizo | ΔI_pp máximo | Relativo |
| --- | --- | --- | --- | --- |
| Bipolar | 2·Vdc (de +Vdc a −Vdc) | f_sw | Vdc / (2·L·f_sw) | **4** |
| Híbrida | Vdc | f_sw | Vdc / (4·L·f_sw) | **2** |
| Unipolar | Vdc | 2·f_sw | Vdc / (8·L·f_sw) | **1** |

**Cómo sale cada fórmula** (ciclo de trabajo D, periodo T = 1/f_sw):
- **Bipolar:** ΔI = 2·Vdc·D·(1 − D)·T / L, máximo en D = 0.5 → Vdc / (2·L·f_sw).
- **Híbrida:** la pierna A alterna entre 0 y Vdc: ΔI = Vdc·D·(1 − D)·T / L, máximo en D = 0.5 → Vdc / (4·L·f_sw).
- **Unipolar:** mismo escalón que la híbrida, pero a frecuencia efectiva 2·f_sw → Vdc / (8·L·f_sw).

La duplicación de la frecuencia efectiva en la unipolar es un resultado clásico de los textos de electrónica de potencia [2][3].

**Consecuencia:** con el mismo inductor, la bipolar tiene **el doble de rizo que la híbrida y cuatro veces el de la unipolar**.

### 3.2 Consumo en vacío (clave en un sistema aislado)

En la **bipolar**, aunque la salida promedio sea cero, el puente sigue aplicando ±48 V al filtro a 24 kHz. Circula un rizo grande incluso **sin carga**, lo que significa pérdidas en el cobre, en el núcleo del inductor y en los MOSFET.

En un sistema solar aislado, el inversor pasa **muchas horas encendido con poca o ninguna carga** (de noche, en espera). Cada watt de consumo en vacío sale de la batería durante toda la noche. Las modulaciones de tres niveles (híbrida y unipolar) bajan este consumo porque usan el estado cero y aplican escalones de la mitad del voltaje.

### 3.3 Pérdidas por conmutación

| Modulación | Posiciones a f_sw | Pérdidas de conmutación (relativas) |
| --- | --- | --- |
| Bipolar | 4 | 1 |
| Unipolar | 4 | 1 |
| **Híbrida** | **2** | **≈ 0.5** |

Las pérdidas por conducción son prácticamente iguales en las tres, porque siempre conducen dos MOSFET en serie con la carga. La híbrida **reduce a la mitad las pérdidas de conmutación**, que a 24 kHz con 46 A pico son una parte importante del total.

### 3.4 Distorsión por tiempo muerto (clave para los módulos de trinchera)

Cada pierna que conmuta a alta frecuencia introduce un error de voltaje por el tiempo muerto t_d. En promedio, ese error vale aproximadamente:

ΔV ≈ t_d · f_sw · Vdc · signo(i)  (por pierna)

| | t_d = 500 ns (IRS2186, UCC21520) | t_d = 1 µs (TLP250, IR2110) |
| --- | --- | --- |
| t_d · f_sw a 24 kHz | 1.2 % | 2.4 % |
| Error por pierna (Vdc = 48 V) | ≈ 0.58 V | ≈ 1.15 V |
| **Bipolar / unipolar** (2 piernas a f_sw) | ≈ 1.15 V | **≈ 2.3 V** |
| **Híbrida** (1 pierna a f_sw) | ≈ 0.58 V | **≈ 1.15 V** |

El pico de la fundamental en el primario es de unos 43 V (127 V · √2 / 4.16). Con los módulos de trinchera a 1 µs, la híbrida deja el error en ~2.7 % de ese pico, contra ~5.4 % en la unipolar o la bipolar.

**Esto es lo que hace viables los módulos legacy con 1 µs:** la modulación híbrida reduce a la mitad el costo en distorsión de su tiempo muerto más largo. La pierna B conmuta a 60 Hz, así que su tiempo muerto no aporta un error apreciable.

Este error de voltaje genera armónicos impares; su compensación se trata en [Compensación adaptativa de tiempo muerto](compensacion_adaptativa_tiempo_muerto.md).

### 3.5 Voltaje de modo común

| Modulación | Modo común (VS1 + VS2) / 2 |
| --- | --- |
| Bipolar | Constante en Vdc/2 (ideal) |
| Unipolar | Salta a f_sw entre 0, Vdc/2 y Vdc |
| Híbrida | Salta a f_sw (pierna A) más una onda cuadrada de 60 Hz (pierna B) |

La bipolar es la preferida en **inversores sin transformador** conectados a red, porque el modo común constante evita corrientes de fuga a tierra por la capacitancia de los paneles [4].

**En este proyecto no aplica:** el transformador de 60 Hz da aislamiento galvánico entre la batería y la salida de 127 V. El modo común solo importa como fuente de EMI, por la capacitancia de los nodos VS1 y VS2 hacia los disipadores y el chasis.

Un detalle a favor de la híbrida: en modo inversor, **VS2 solo se mueve a 60 Hz**, así que el disipador de la pierna B (conectado a VS2) es prácticamente silencioso en EMI. El ruido se concentra en la pierna A.

### 3.6 Resumen

| Criterio | Bipolar | Unipolar | Híbrida |
| --- | --- | --- | --- |
| Rizo en el filtro | Alto (4) | **Bajo (1)** | Medio (2) |
| Consumo en vacío | Alto | Bajo | **Bajo** |
| Pérdidas de conmutación | 1 | 1 | **≈ 0.5** |
| Distorsión por tiempo muerto | 2 piernas | 2 piernas | **1 pierna** |
| Modo común | **Constante** | Variable | Variable |
| Reparto térmico entre piernas | Parejo | Parejo | Pierna A más caliente |
| Comportamiento en el cruce por cero | Suave | Sensible | Transición de la pierna B |
| Complejidad del firmware | Baja | Media (2 pares de PWM) | **Baja** (1 par de PWM + 2 salidas lentas) |

---

## 4. Decisión del proyecto

### 4.1 Inversor: híbrida

1. **Eficiencia:** la mitad de pérdidas de conmutación y bajo consumo en vacío, que son las dos cosas que más pesan en un sistema aislado.
2. **Soporte de trinchera:** con TLP250 o IR2110 a 1 µs, solo una pierna aporta distorsión por tiempo muerto (sección 3.4).
3. **Firmware simple:** basta un par complementario de PWM a 24 kHz más dos salidas a 60 Hz. Cabe sin problema en un ATmega328P.
4. **El modo común no importa:** el transformador de 60 Hz da el aislamiento.
5. **Validación externa:** es el mismo esquema que usa el diseño de referencia de TI [1].

**El precio que se acepta:**
- El rizo es el doble que con la unipolar. Lo absorbe el filtro (~2 mH + 4.7 µF MKP X2).
- La pierna A se calienta más que la B. Por eso el banco de desacoplo y el film **C9 están en la pierna A**.

### 4.2 Cargador: unipolar

En modo cargador, el puente trabaja como rectificador activo: toma corriente de la red a través del transformador y la entrega a la batería. Aquí manda la **calidad de la corriente**, no las pérdidas en vacío:

1. **Menor rizo de corriente** (la mitad que la híbrida), con frecuencia efectiva de 48 kHz. Eso da una corriente de red más limpia y un lazo de corriente más fácil de controlar.
2. **Simetría entre semiciclos:** las dos piernas trabajan igual, sin la transición de la pierna B en el cruce por cero.
3. **Las pérdidas de conmutación son mayores**, pero el cargador es de 1 kVA y opera en un régimen controlado.

**Ojo con el cruce por cero:** la literatura reporta que la conmutación unipolar pierde efectividad de control cerca del cruce por cero, lo que aumenta la distorsión. Hay propuestas que conmutan a bipolar en esa zona [5]. Para la versión básica de trinchera basta con verificarlo en banco; la técnica de [5] queda como mejora posterior.

### 4.3 Portadora sincronizada

f_sw = 24 kHz = **400 × 60 Hz**. Cada ciclo de 60 Hz tiene exactamente 400 periodos de portadora, así que:
- la tabla de senos es de 400 muestras (o 200 por semiciclo);
- no aparecen subarmónicos por deriva entre la portadora y la senoidal;
- el cruce por cero cae siempre en el mismo punto de la portadora.

---

## 5. Consecuencias en el hardware y el firmware

| Decisión | Dónde se refleja |
| --- | --- |
| **La pierna A es la de alta frecuencia, fija por hardware** | El film C9 y la entrada NEG/POS están junto a la pierna A. El firmware **no debe intercambiar** las piernas: dejarlo como `#define` documentado. |
| La pierna B también conmuta a 24 kHz en modo cargador | Tiene su banco cerámico (2 × 10 µF + 2 × 4.7 µF) junto a C3/C4. Medir el sobretiro en VS2 en modo cargador; si es alto, agregar un snubber RC, no mover C9. |
| La pierna A se calienta más | Dirigir el flujo de aire primero a los capacitores y después a los disipadores de la pierna A. |
| Transición de la pierna B en el cruce por cero | La pierna B también usa el tiempo muerto de P9. En el semiciclo positivo, la pierna A arranca con d ≈ 0; en el negativo, con d ≈ 1. Ver el cruce con el osciloscopio. |

### 5.1 Ciclos de trabajo (referencia para el firmware)

**Inversor (híbrida), con m como índice de modulación y θ = 2π·60·t:**

| Semiciclo | Pierna B | Ciclo de trabajo de la pierna A (QAH) |
| --- | --- | --- |
| Positivo (sen θ ≥ 0) | QBL encendido (VS2 = 0) | d_A = m · sen θ |
| Negativo (sen θ < 0) | QBH encendido (VS2 = Vdc) | d_A = 1 − m · \|sen θ\| |

**Cargador (unipolar), con la misma portadora para las dos piernas:**

| Pierna | Ciclo de trabajo |
| --- | --- |
| A (QAH) | d_A = 0.5 · (1 + m · sen θ) |
| B (QBH) | d_B = 0.5 · (1 − m · sen θ) |

En los dos casos, QAL y QBL son el complemento de su MOSFET alto, con el tiempo muerto que declara P9 (ver [Identificación del tiempo muerto por P9](identificacion_dt_pin9.md)).

---

## 6. Validación en banco

1. **Inversor sin carga:** medir la corriente de entrada en vacío. Es el número que más le importa al usuario de un sistema aislado.
2. **Rizo:** ver la corriente del primario con una sonda o pinza de corriente y confirmar que el rizo está a 24 kHz (híbrida) y a 48 kHz (cargador).
3. **Cruce por cero:** ver V_AB y la corriente en el cruce, primero en modo inversor (transición de la pierna B) y después en modo cargador.
4. **Temperatura:** comparar los disipadores de la pierna A y de la B a 1000 W. La A debe estar más caliente.
5. **THD de la salida:** comparar un módulo de 500 ns contra uno de 1 µs para confirmar el efecto del tiempo muerto (sección 3.4).

---

## 7. Referencias

1. Texas Instruments, *Grid Connected Inverter Reference Design* (TIDM-HV-1PH-DCAC), guía de diseño TIDUB21D, rev. D, marzo 2020. <https://www.ti.com/lit/pdf/tidub21> · Página del diseño: <https://www.ti.com/tool/TIDM-HV-1PH-DCAC>
2. N. Mohan, T. M. Undeland, W. P. Robbins, *Power Electronics: Converters, Applications, and Design*, 3.ª ed., Wiley, 2003. Capítulo 8, inversores conmutados de CD a CA: PWM bipolar y unipolar en el puente H monofásico.
3. D. G. Holmes, T. A. Lipo, *Pulse Width Modulation for Power Converters: Principles and Practice*, IEEE Press / Wiley, 2003.
4. R. Teodorescu, M. Liserre, P. Rodríguez, *Grid Converters for Photovoltaic and Wind Power Systems*, Wiley / IEEE Press, 2011. Inversores fotovoltaicos sin transformador y corriente de fuga por modo común.
5. M. A. Bolaños-Navarrete, J. D. Bastidas-Rodríguez, G. Osorio, "A Hybrid Commutation Technique for Reducing Zero-Crossing Distortion in a Sliding Mode Controller for Single-Phase Grid-Tied Full-Bridge Inverters", *Energies*, vol. 17, n.º 15, 3671, 2024. DOI: [10.3390/en17153671](https://doi.org/10.3390/en17153671)

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz del repositorio).*
