# Módulo gate driver de medio puente H con IRS2186

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Módulo:** gate driver de **medio puente H** (una rama: MOSFET alto + MOSFET bajo) con bootstrap. Es el **módulo principal de producción**: bajo costo y 500 ns de tiempo muerto
**Intercambiable con:** UCC21520ADWR y TLP250 de trinchera (misma interfaz J1/J5, mismo firmware)
**Estado:** diseño terminado con P9 en J1. Pendiente: DRC final, fabricación y validación en banco
**Fecha:** 2026-11-03
**Documentos relacionados:** [`docs/identificacion_dt_pin9.md`](../../../docs/identificacion_dt_pin9.md), [`docs/compensacion_adaptativa_tiempo_muerto.md`](../../../docs/compensacion_adaptativa_tiempo_muerto.md), [módulo TLP250](../tlp250-trinchera/README.md)

> Verificar los valores marcados con *(verificar)* contra el esquemático de KiCad antes de fabricar.
<img width="500" height="850" alt="imagen" src="https://github.com/user-attachments/assets/9fd8c481-7021-4d3a-8ff5-21f3c2534462" />
<img width="500" height="850" alt="imagen" src="https://github.com/user-attachments/assets/0e5c1018-f84f-4114-be1e-e99312f8d35b" />


---

## 1. Qué maneja este módulo

Cada módulo maneja **una rama** del puente H: el **MOSFET del lado alto** (HO, referido a VS, alimentado por bootstrap) y el **MOSFET del lado bajo** (LO, referido a COM).

```
            +48 V
              │
     HA ──► [MOSFET alto]          [MOSFET alto] ◄── HB
              │                         │
              ├── VS1 ──► transformador ◄── VS2 ──┤
              │                         │
     LA ──► [MOSFET bajo]          [MOSFET bajo] ◄── LB
              │                         │
             GND ───────────────────── GND
         └── módulo rama A ──┘    └── módulo rama B ──┘
```

- **El puente completo usa dos módulos iguales:** rama A (VS1) y rama B (VS2). En la rama B, "H" y "L" de J1 manejan HB y LB, y el pin "VS1" de J5 va a VS2.
- **Inversor (modulación híbrida):** rama A a **24 kHz**, rama B a **60 Hz**.
- **Cargador (unipolar):** las dos ramas a **24 kHz**.
- **Alcance:** **1 IRF100P219 por posición** con 500 ns. Para 2 MOSFET por posición, el IRS2186 solo ya no cumple 500 ns (ver sección 6).

### Por qué el IRS2186

| Criterio | IRS2186 (este) | UCC21520ADWR | TLP250 + tótem |
| --- | --- | --- | --- |
| Costo y número de componentes | **El menor** ✓ | Medio | El mayor |
| Tiempo muerto | **500 ns** ✓ | 500 ns | 1 µs |
| Corriente de salida | 4 A / 4 A | 4 A / 6 A | ~2 A (más con tótem) |
| Aislamiento entrada / potencia | **No** (lógica referida a COM) | Sí | Sí |
| Reparación a mano | SOIC-8 | SOIC-16 ancho | DIP y TO-220 |

---

## 2. Interfaz

### 2.1 J1 — entrada, hacia la tarjeta de control (bornera de 5 terminales)

| Terminal | Señal | En este módulo |
| --- | --- | --- |
| 1 | H | HIN del IRS2186 (con 10 kΩ a GND) |
| 2 | GND | Tierra lógica = COM (trenzar con H) |
| 3 | L | LIN del IRS2186 (con 10 kΩ a GND) |
| 4 | GND | Tierra lógica = COM (trenzar con L) |
| **5** | **P9 — ID DT** | **Unido a GND** → el MCU usa **500 ns** |

- Serigrafía: "TRENZAR H Y L CON GND", etiquetas H / GND / L / GND / P9. Recomendado agregar **"OPERACION DT IRS2186=500NS"**.
- Las resistencias de 10 kΩ de HIN y LIN a GND mantienen los MOSFET apagados si la tarjeta de control no está conectada o el MCU está en reset.
- Si P9 queda con soldadura fría o cable suelto, el MCU lee 1 → **1 µs** (falla segura: funciona con más armónicos).

### 2.2 J5 — salida, hacia la tarjeta del puente H (header de 9 posiciones, 2.54 mm)

| Pin | Señal | Nota |
| --- | --- | --- |
| 1 | HO | Compuerta lado alto |
| 2 | VS1 | Fuente lado alto (nodo de conmutación) |
| 3 | — | Libre (separación del nodo conmutado) |
| 4 | GND_K | Retorno Kelvin del lado bajo |
| 5 | LO | Compuerta lado bajo |
| 6 | GND | Tierra de potencia |
| 7 | 15 V | Alimentación |
| 8 | — | **Llave mecánica:** sin pad; **quitar el pin 8 del header**; "X" en serigrafía |
| 9 | GND | Reservado (no se lee) |

### 2.3 Tierras: la entrada comparte la tierra de potencia

En el IRS2186 la lógica (HIN, LIN) va referida a **COM**, que también es el retorno del lado bajo. Por eso **el GND de J1 es la misma tierra de potencia**: el módulo no aísla la tarjeta de control.

Consecuencias y cuidados:

1. **Lazo de tierra:** el GND del cable J1 es un camino más entre la tarjeta de control y la potencia; con dos módulos (rama A y B) ya hay dos. Parte de la corriente de conmutación puede circular por esos cables.
2. **Rebote de tierra:** el di/dt del MOSFET bajo puede mover COM unos cientos de mV respecto al GND del MCU. El IRS2186 tiene umbrales de entrada de ~0.8 V / ~2.5 V *(verificar en la hoja de datos)*; con MCU a 5 V el margen es mayor.
3. **Medidas:**
   - **Par trenzado H/GND y L/GND, lo más corto posible.**
   - **Un solo punto de unión** entre tierra de control y tierra de potencia en la tarjeta del puente H (estrella), cerca de la fuente del MOSFET bajo o del shunt.
   - **Retorno de LO hacia GND_K** (Kelvin) y lógica hacia GND, unidos en un solo punto: la corriente de compuerta no debe pasar por la tierra de las entradas.
4. Si en banco aparecen disparos falsos o ruido en las entradas, los módulos **UCC21520** o **TLP250** lo eliminan por diseño (entrada aislada).

---

## 3. Circuito

### 3.1 IRS2186 (SOIC-8)

| Pin | Señal | Conexión |
| --- | --- | --- |
| 1 | HIN | J1-H, 10 kΩ a GND |
| 2 | LIN | J1-L, 10 kΩ a GND |
| 3 | COM | GND (tierra lógica y retorno del lado bajo) |
| 4 | LO | J5-5 |
| 5 | VCC | 15 V, con desacoplo a COM |
| 6 | VS | J5-2 (VS1) |
| 7 | HO | J5-1 |
| 8 | VB | Nodo del bootstrap |

Verificar contra la hoja de datos de la variante comprada (IRS2186S, SOIC-8).

**Datos de la hoja de datos (Infineon IRS2186):** salida 4 A / 4 A; retardo de propagación ~170 ns típ., 250 ns máx.; diferencia entre canales ≤ 35 ns; VCC 10–20 V; offset VS hasta 600 V *(verificar)*.

### 3.2 Alimentación de 15 V (lado bajo)

- 15 V desde J5-7 a VCC, con desacoplo **10 µF + 1 µF X7R** pegados a los pines 5 y 3, más capacitor electrolítico de respaldo en la línea de 15 V *(verificar valores)*.
- **¿Por qué 15 V y no 12 V?** Más voltaje de compuerta (menor R_DS(on) y más margen de UVLO en el bootstrap de la rama de 60 Hz) dentro del rango de VCC del IRS2186 y del ±20 V de V_GS del MOSFET.

### 3.3 Bootstrap (lado alto)

```
15 V ── R9 2.2 Ω 1 W ── D5 (ES1D / ES1J) ──┬── VB (pin 8)
                                           ├── 2 × 10 µF X7R 50 V 1206 ── VS1
                                           └── Zener 18 V (1SMA5931BT3G), cátodo a VB ── VS1
```

| Componente | Valor | Por qué |
| --- | --- | --- |
| R9 | 2.2 Ω, 1 W, **antes del diodo** | Limita el pico de recarga del bootstrap cuando VS cae a 0 V (circuito de referencia de TI; IR Design Tip DT98-2; patente US5969964, ya expirada) |
| D5 | ES1D (200 V) o **ES1J (600 V)** | Diodo rápido. ES1D limita el bus del módulo a ~120 V; con ES1J el límite lo ponen las separaciones del PCB (≤ 150 V) |
| C_boot | 2 × 10 µF X7R 50 V 1206 | Sostiene VB durante los **8.3 ms** de encendido continuo de la rama de 60 Hz. El X7R pierde capacidad con el voltaje DC: por eso dos |
| Zener | 18 V (1SMA5931BT3G) | Protege VB–VS contra picos por encima del máximo del IRS2186 |

**Reglas de firmware para el bootstrap:**
- **Precarga** de 0.5–2 ms con los lados bajos encendidos antes de arrancar el PWM.
- **Ciclo de trabajo máximo ≤ 95 %** en la rama de 24 kHz, para dejar tiempo de recarga en cada periodo.

### 3.4 Red de compuerta

Las resistencias de compuerta de cada MOSFET (arreglo R1/R2/R3) y las **10 kΩ de compuerta a fuente** están en la **tarjeta del puente H**, junto a cada MOSFET. El módulo solo entrega HO y LO.

---

## 4. Tiempo muerto: 500 ns

| Concepto | Valor |
| --- | --- |
| Diferencia de retardo entre canales del IRS2186 | ≤ 35 ns |
| Apagado del IRF100P219 en el peor caso (con la red de compuerta) | ~390 ns |
| **Piso de tiempo muerto** | **~390–425 ns → se usa 500 ns** |

- **Por qué no 1 µs siempre:** con 500 ns el 3er armónico es ~0.58 % y el THD ~0.81 %; con 1 µs, ~1.17 % y ~1.62 % (híbrida, 24 kHz, 1000 W, sin compensación).
- El tiempo muerto se aplica en **ambos flancos**, por firmware. El IRS2186 **no tiene tiempo muerto interno ni interbloqueo**: si el firmware enciende HIN y LIN a la vez, enciende los dos MOSFET.
- El MCU elige 500 ns **solo si P9 de los dos módulos está a GND** (ver `identificacion_dt_pin9.md`).

---

## 5. Layout

- **Dominio HV** (VB, VS1, HO, bootstrap, zener): **0.6 mm** al resto (regla `HV_a_LV`, IPC-2221 B2 para ≤ 150 V):
  ```
  (rule "HV_a_LV" (constraint clearance (min 0.6mm)) (condition "A.NetClass == 'HV' && B.NetClass != 'HV'"))
  ```
- Capacitores de bootstrap y zener **pegados a los pines 8 y 6** del IRS2186; desacoplo de VCC pegado a los pines 5 y 3.
- J1: P9 y las dos GND unidos a la zona de GND. Recomendado: *Thermal reliefs* en esos pads para soldar la bornera a mano sin soldaduras frías.
- J5: sin pad 8 (llave) con "X" en serigrafía.

---

## 6. Dos MOSFET por posición

Con 2 × IRF100P219 en paralelo (~480 nC por flanco), el IRS2186 solo apaga ~1.5 veces más lento: el piso sube a ~590 ns y **500 ns ya no alcanza**. Opciones:

1. Usar el módulo **UCC21520** (6 A de apagado): conserva 500 ns.
2. Agregar al IRS2186 un **tótem rápido** (D44H11/D45H11 o ZXTC2062E6; **no TIP41/42**, demasiado lentos para mejorar un driver de 4 A) o un **PNP de descarga local** por MOSFET. Validar en banco.
3. Marcar el módulo: "IRS2186 · 1 MOSFET/posición".

---

## 7. Validación en banco

1. **Sin MOSFET:** alimentar 15 V, verificar HO y LO con HIN/LIN en alto y bajo. Confirmar que con J1 desconectado las salidas quedan en bajo (10 kΩ).
2. **P9:** confirmar que el MCU reporta **500 ns**. Desconectar el cable P9 → debe reportar **1 µs**.
3. **Bootstrap:** medir VB–VS1 al final de los 8.3 ms de la rama de 60 Hz; debe quedar con margen sobre el UVLO del IRS2186.
4. **Tiempo muerto real:** medir en la compuerta de cada MOSFET el intervalo entre la bajada de LO y la subida de HO (y viceversa) → ≥ 500 ns.
5. **Ruido de tierra:** con carga, medir con el osciloscopio la diferencia entre GND del MCU y COM del módulo durante la conmutación; buscar disparos falsos en HIN/LIN.
6. **Armónicos:** 3er armónico ≈ 0.58 % a 1000 W sin compensación.

---

## 8. Bitácora

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-03 | P9 en J1-5 unido a GND; J5 sin pad 8 con "X"; J5-9 a GND (reservado) | Módulo declara 500 ns ✓ | Serigrafía "OPERACION DT IRS2186=500NS", thermal reliefs en J1, DRC, fabricar |
| | | | |

---

## 9. Referencias

- Infineon, IRS2186(4)(S)PbF datasheet.
- International Rectifier, Design Tip DT98-2, "Bootstrap component selection for control ICs".
- International Rectifier, patente US5969964 (1999, expirada): resistencia en serie con el diodo de bootstrap.
- Texas Instruments, SLUA887A, "Bootstrap circuitry selection for half-bridge configurations".
- Infineon, IRF100P219 datasheet.
- IPC-2221 (separaciones eléctricas).
