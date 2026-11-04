# Módulo gate driver de medio puente H con IR2110 + tótem TIP41C/TIP42C — modo trinchera

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Módulo:** gate driver de **medio puente H** (una rama: MOSFET alto + MOSFET bajo), legacy / trinchera, con tótem por canal
**Intercambiable con:** IRS2186, UCC21520ADWR y TLP250 de trinchera (misma interfaz J2/J5, mismo firmware)
**Versión de placa:** V.0.1.NOV.26.J.G
**Estado:** diseño terminado; DRC con 0 errores y 0 elementos sin conectar. Pendiente: fabricación y validación en banco
**Fecha:** 2026-11-04
**Documentos relacionados:** [`docs/identificacion_dt_pin9.md`](../../../docs/identificacion_dt_pin9.md), [`docs/sensado_corriente.md`](../../../docs/sensado_corriente.md), [módulo TLP250](../tlp250-trinchera/README.md), [módulo IRS2186](../irs2186/README.md)

<img width="513" height="923" alt="imagen" src="https://github.com/user-attachments/assets/27c7c135-b067-4162-8e5b-c950183c9330" />

> Valores verificados contra `GATE_DRIVER_IR2110_TOTEM.kicad_pcb` (2026-11-04). Lo marcado *(verificar)* depende de la hoja de datos.

---

## 1. Qué maneja este módulo

Cada módulo maneja **una rama** del puente H: el **MOSFET del lado alto** (HO, referido a VS, alimentado por bootstrap) y el **MOSFET del lado bajo** (LO, referido a COM / GND_K).

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

- **El puente completo usa dos módulos iguales:** rama A (VS1) y rama B (VS2). En la rama B, "H" y "L" de J2 manejan HB y LB, y el pin "VS1" de J5 va a VS2.
- **Inversor (modulación híbrida):** rama A a **24 kHz**, rama B a **60 Hz**. **Cargador (unipolar):** las dos ramas a **24 kHz**.
- **Alcance: de 1 a 5 MOSFET por posición** (sección 6).

### 1.1 Por qué existe: modo trinchera

Igual que el módulo TLP250, este módulo existe para **cuando los drivers modernos no llegan**. El IR2110 (International Rectifier, hoy Infineon) es el driver de medio puente **más fácil de conseguir o rescatar**: está en variadores, soldadoras inverter y UPS viejos, y en casi cualquier tienda de electrónica. Viene en **DIP-14**: se monta en zócalo y se cambia sin cautín. Todo el módulo se arma con partes de tienda local.

| | TLP250 + tótem | **IR2110 + tótem (este)** |
| --- | --- | --- |
| Driver | Optoacoplador DIP-8, uno por canal | DIP-14, un chip para los dos canales |
| Aislamiento de la entrada | Sí (óptico) | Parcial: **VSS y COM separados**, hasta ±5 V |
| Diferencia de retardo entre canales | Hasta **400 ns** | **10–20 ns** |
| Margen dentro de 1 µs | ~150–250 ns con 2 IRF100P219 | **Mucho mayor** |
| Corriente propia | ~2 A | 2 A |
| MOSFET por posición | 1–2 IRF100P219 | **1–5** (por ejemplo 5 × IRF540N) |

---

## 2. Interfaz

### 2.1 J2 — entrada, hacia la tarjeta de control ("J1" de la interfaz)

Conector **Phoenix Contact SPT 1,5/5-H-3,5** (5 posiciones, paso 3.5 mm, de resorte), igual que el módulo IRS2186.

| Terminal | Señal | En este módulo |
| --- | --- | --- |
| 1 | H | HIN del IR2110 (pin 10), 10 kΩ a GND |
| 2 | GND | Tierra lógica = VSS (trenzar con H) |
| 3 | L | LIN del IR2110 (pin 12), 10 kΩ a GND |
| 4 | GND | Tierra lógica = VSS (trenzar con L) |
| **5** | **P9 — ID DT** | **Abierto, sin pista** → el MCU usa **1 µs** |

### 2.2 J5 — salida, hacia la tarjeta del puente H (header de 9 posiciones, 2.54 mm)

| Pin | Señal | Nota |
| --- | --- | --- |
| 1 | H (HA_OUT) | Emisores del tótem H |
| 2 | VS1 | Referencia del canal H |
| 3 | — | Libre (separación del nodo conmutado) |
| 4 | GK (GND_K) | Referencia Kelvin del canal L = **COM** |
| 5 | L (LA_OUT) | Emisores del tótem L |
| 6 | G (GND) | **Tierra lógica (VSS)**: retorno del 78L05 hacia la tarjeta del puente H |
| 7 | 15 V | Alimentación |
| 8 | **X** | **Llave mecánica:** sin pad; **quitar el pin 8 del header** |
| 9 | NC | Reservado |

### 2.3 Tierras: VSS y COM separados

El IR2110 tiene **dos tierras**: **VSS** (lógica) y **COM** (retorno del lado bajo), con hasta **±5 V** de diferencia permitida.

| Zona | Referencia | Componentes |
| --- | --- | --- |
| Lógica | **GND (VSS)** | J2, R1, R2, VSS (13), SD (11), 78L05, capacitor de entrada y de salida del 78L05, **J5-6** |
| Canal L (potencia) | **GND_K (COM)** | COM (2), colector de Q4 (PNP), C2, C4, C6, C8, Z2 |
| Canal H (HV, flota con VS1) | **VS1** | VS (5), Q2 colector, C3, C5, C9, Z1 |

- **J5-6 a GND lógico** (a diferencia del TLP250, que deja J5-6 sin conectar): el 78L05 toma sus 15 V de J5-7, así que su corriente regresa por el camino corto J5-6. Además, VSS queda bien referido respecto a COM.
- **COM va a GND_K (J5-4):** la corriente de compuerta del canal bajo regresa por GND_K y **no pasa por la tierra lógica**. Es la ventaja del IR2110 sobre el IRS2186.
- GND y GND_K se unen en **un solo punto en la tarjeta del puente H**, nunca dentro del módulo.
- **Lazo de tierra:** igual que con el IRS2186, el GND del cable J2 es un camino más hacia la tarjeta de control. Se controla con **par trenzado H/GND y L/GND** y la unión única de tierras.
- **Opcional:** dos diodos en antiparalelo (1N4148 o BAT54) entre VSS y COM para limitar transitorios a ~0.7 V. No montados en V.0.1.

---

## 3. Circuito

### 3.1 IR2110 (DIP-14)

| Pin | Señal | Conexión |
| --- | --- | --- |
| 1 | LO | R3 (2.2 Ω) → bases del tótem L |
| 2 | COM | **GND_K** |
| 3 | VCC | **15 V**, desacoplo a GND_K |
| 4 | — | NC |
| 5 | VS | **VS1** |
| 6 | VB | Nodo del bootstrap |
| 7 | HO | R5 (2.2 Ω) → bases del tótem H |
| 8 | — | NC |
| 9 | VDD | **5 V** del 78L05 |
| 10 | HIN | J2-1 (H), 10 kΩ a GND |
| 11 | **SD** | **A GND (VSS)**: si queda al aire, el ruido puede apagar las salidas |
| 12 | LIN | J2-3 (L), 10 kΩ a GND |
| 13 | VSS | **GND lógico** |
| 14 | — | NC |

**Datos de la hoja de datos (Infineon IR2110E4):** salida 2 A / 2 A; retardo 120 / 94 ns típ. (150 / 125 ns máx. a 25 °C); diferencia entre canales 10–20 ns; VCC 10–20 V; VDD de VSS + 5 V a VSS + 20 V; VSS–COM ±5 V; UVLO de VCC y VBS ~8.2–8.7 V típ. (hasta ~9.4–9.7 V); consumo del lado alto ≤ 230 µA a 25 °C.

### 3.2 Alimentación lógica: VDD = 5 V con 78L05

- La interfaz J2 **no trae VDD**, y los umbrales de entrada del IR2110 **suben con VDD** (a VDD = 15 V, el "1" lógico pide ≥ 9.5 V). Por eso el módulo genera sus propios **5 V** con un **L78L05 (SOT-89)** desde los 15 V.
- **Capacitores:** 10 µF X7R en la entrada (15 V–GND) y capacitor de salida (5 V–GND, C7) junto al regulador. El 78L05 es un regulador clásico (no LDO): **estable con cerámicos**.
- **MCU de 5 V (ATmega328P):** funciona con margen.
- **MCU de 3.3 V (STM32, ESP32):** con VDD = 5 V el umbral del "1" puede quedar cerca de 3.3 V. **Verificar la gráfica de umbrales contra VDD** de la hoja de datos; si no hay margen, agregar un buffer **74HCT** en las entradas.

### 3.3 Alimentación del lado bajo (VCC)

- 15 V desde J5-7 a VCC (pin 3), con desacoplo a **GND_K**: **C2, C4** (10 µF X7R) y **C6** junto a los pines 3 y 2.
- **Z2: zener de 18 V (1SMA5931B)** entre VCC y GND_K (cátodo a 15 V). Con capacitores 100 % cerámicos, al conectar los 15 V la inductancia del cableado puede provocar un sobretiro de casi el doble; el zener lo recorta antes del máximo de VCC.

### 3.4 Bootstrap (lado alto)

```
15 V ── D5 ES1D ── R9 2.2 Ω 1 W ──┬── VB (pin 6)
                                  ├── C3, C5: 2 × 10 µF X7R ── VS1
                                  └── Z1: zener 18 V (1SMA5931B), cátodo a VB ── VS1
```

- **Z1 con el cátodo a VB.** Si se monta al revés, conduce en directo y pone en corto el bootstrap: el MOSFET alto nunca enciende.
- **Precarga** de 0.5–2 ms con los lados bajos encendidos antes de arrancar el PWM; **ciclo de trabajo máximo ≤ 95 %**.
- Con **3 a 5 MOSFET por posición**, reforzar a **4 × 10 µF** (sección 6).

### 3.5 Tótems (uno por canal)

```
Canal H:  VB ──┬── C9 10 µF ── VS1
               C
  HO ─[R5 2.2 Ω]─B  Q1 TIP41C / D44H11G
               E ──┬── HA_OUT (J5-1)
               E ──┘
  (bases unidas) B  Q2 TIP42C / D45H11G
               C
              VS1

Canal L:  15 V ──┬── C8 10 µF ── GND_K
                 C
  LO ─[R3 2.2 Ω]─B  Q3 TIP41C / D44H11G
                 E ──┬── LA_OUT (J5-5)
                 E ──┘
  (bases unidas) B  Q4 TIP42C / D45H11G
                 C
               GND_K
```

| Elemento | Valor | Por qué |
| --- | --- | --- |
| NPN / PNP | **TIP41C / TIP42C** (trinchera) o **D44H11G / D45H11G** (mejora), mismo footprint TO-220 B-C-E | El IR2110 da solo 2 A; con un tótem maneja de 1 a 5 MOSFET |
| R3, R5 | 2.2 Ω | Limitan el pico que sale del IR2110 hacia las bases |
| C8, C9 | 10 µF X7R entre los colectores de cada par | Entregan el pico de cada flanco con el lazo más corto; protegen al IR2110 del ruido del tótem |
| Seguidor de emisor | Colectores a las rieles, emisores a la salida | No invierte, no satura, sin conducción cruzada propia |

**Regla:** el capacitor de desacoplo de cada tótem va **entre la alimentación del NPN y la referencia del PNP del mismo canal** (VB–VS1, 15 V–GND_K). **Nunca entre canales** (por ejemplo VS1–GND_K: quedaría en paralelo con el MOSFET bajo).

### 3.6 Resistencias de compuerta

Las resistencias de compuerta de cada MOSFET y las de **compuerta a fuente** están en la **tarjeta del puente H**, junto a cada MOSFET.

---

## 4. Tiempo muerto: 1 µs (P9 abierto)

- Con 2 A, el IR2110 **sin tótem** apaga un solo IRF100P219 en ~590 ns: ya no cabe en 500 ns.
- Con tótem **TIP41C/TIP42C** el apagado queda en ~350–450 ns; con **D44H11/D45H11**, en ~150–300 ns.
- **Se fija 1 µs (P9 abierto) siempre en este módulo:** es de trinchera, y el técnico puede montar TIP o D44H11 en el mismo footprint. El tiempo muerto no debe depender de qué transistor se monte (eso sería un jumper disfrazado).
- Gracias a la poca diferencia entre canales del IR2110 (10–20 ns), casi toda la ventana de 1 µs queda para el apagado: **margen amplio** incluso con TIP.
- Con 1 µs: 3er armónico ~1.17 % y THD ~1.62 % (híbrida, 24 kHz, 1000 W, sin compensación). La compensación de tiempo muerto (etapa posterior) lo reduce.

---

## 5. Layout (lo que se cuidó)

- **Dominio HV (columna del canal H):** Q1, Q2, C9, la pista Vb, HA_OUT, C3, C5, Z1 y el pin 6 del IR2110. **0.6 mm** al resto (regla `HV_a_LV`). Ninguna pista de GND_K, 15 V o GND la cruza **por la misma capa**; los cruces van por la otra capa y en ángulo.
- **SD (11) a VSS (13):** la pista rodea por debajo del cuerpo sin tocar LIN (12).
- **C6 (VCC–COM)** en B.Cu entre los pines 2 y 3; capacitor de salida del 78L05 junto a VDD.
- **C8 en B.Cu entre los colectores de Q3 (15 V) y Q4 (GND_K)**; **C9 (Vb–VS1)** fuera del camino de otras redes.
- **J5-6 (GND)** llega desde arriba, sin pasar sobre el pad 7 (15 V).
- **TIP acostados**, aletas sin tocarse; sin disipador.

### 5.1 Errores encontrados y corregidos durante el diseño

| Error | Consecuencia | Corrección |
| --- | --- | --- |
| VCC (pin 3) sin conectar | Canal bajo sin alimentación, UVLO sin liberar, bootstrap sin carga | VCC a 15 V |
| SD (pin 11) al aire | El ruido puede apagar las salidas | SD a VSS |
| Colector del PNP del canal L en **VS1** | El tótem L intentaría llevar LA_OUT a 48 V: **conducción cruzada** | Colector del PNP (Q4) a GND_K |
| Zener Z1 **al revés** (cátodo a VS1) | Conduce en directo: corto del bootstrap | Cátodo a VB |
| VS1 pasando entre los pads de R3 (base del tótem L) | ~0.65 mm a un nodo de 48 V a 24 kHz; acoplamiento de ruido | VS1 desviada, R3 movida |
| GND_K pasando entre los pads del capacitor Vb–VS1 | ~0.25 mm a Vb: no cumple 0.6 mm; acoplamiento del dv/dt | Capacitor reubicado (C9) y GND_K por otra ruta |
| Pista GND sobre el pad 7 de J5 (15 V) | Riesgo de puente de soldadura GND–15 V | GND baja directo al pad 6 |
| Serigrafía "P9" junto a J5-9 | Confusión: P9 está en J2 | Cambiada a "NC" |

### 5.2 Serigrafía

| Texto | Cara |
| --- | --- |
| "ATENCION: DT=1US — VALIDAR MOSFET Y SW" | Posterior |
| "IR2110 GD-24KHZ", "GATE DRIVER" | Posterior |
| Etiquetas de J2: H / GND / L / GND / P9 | Posterior |
| "T41C/D44H11G", "T42C/D45H11G" con flechas | Superior |
| Etiquetas de J5: H, VS1, GK, L, G, 15V, NC y "X" en la posición 8 | Superior y posterior |
| "Z18V" con símbolo de orientación en los dos zener | Posterior |
| "USAR SILICON RTV NEUTRO ENTRE TIPs" | Superior |
| "V.0.1.NOV.26.J.G" | Superior |

### 5.3 Nota del DRC

El DRC da **0 errores y 0 elementos sin conectar**, con 3 avisos de "huella no coincide con la librería" en **J5** (pad 8 eliminado) y en los dos **zener** (serigrafía editada). Son esperados.

> **Cuidado:** si se usa *Herramientas → Actualizar huellas desde la librería*, KiCad **restaura el pad 8 de J5** y se pierde la llave mecánica. Para una versión futura, guardar las huellas modificadas en una librería del proyecto.

---

## 6. Alcance: cantidad de MOSFET por posición

| Por posición | Qg a 15 V | Pico del tótem | Apagado estimado | Con 1 µs |
| --- | --- | --- | --- | --- |
| 1 × IRF100P219 | ~240 nC | ~2.4 A | ~200–300 ns | ✓ amplio |
| 2 × IRF100P219 | ~480 nC | ~4.8 A | ~300–400 ns | ✓ |
| 4 × IRF540N | ~400 nC | ~9 A | ~200–350 ns | ✓ |
| **5 × IRF540N** | ~375–500 nC | ~9 A (6.8 Ω por MOSFET) / ~12 A (4.7 Ω) | ~200–450 ns | ✓ |

- **Picos del tótem:** TIP41C/TIP42C 10 A pico; D44H11/D45H11 20 A. Con TIP y 5 MOSFET usar **6.8 Ω por MOSFET** para quedar en ~9 A.
- **Bootstrap de la rama de 60 Hz (8.3 ms encendido):** con varias resistencias de compuerta a fuente en paralelo, la caída sube.

| Configuración | Caída en 8.3 ms | Compuerta al final | ¿Sirve? |
| --- | --- | --- | --- |
| 1–2 MOSFET, 2 × 10 µF, G-S de 10 kΩ | ~1–2 V | ~12–13 V | ✓ |
| 5 MOSFET, 2 × 10 µF, G-S de 10 kΩ | ~4.8 V | ~8.8 V | ✗ bajo el UVLO |
| 5 MOSFET, **4 × 10 µF**, G-S de **22 kΩ** | ~1.1 V | ~12.5 V | ✓ recomendado |

- **Consumo de 15 V:** Qg × f ≈ 23 mA por canal a 24 kHz con ~960 nC.
- **Paralelo:** mismo número de parte y lote, una resistencia de compuerta por pieza, layout simétrico en la tarjeta del puente H.

---

## 7. Lista de materiales (verificada contra el PCB)

| Ref. | Valor / parte | Footprint | Cara | Función |
| --- | --- | --- | --- | --- |
| U1 | IR2110 | DIP-14 (7.62 mm) | F.Cu | Driver de medio puente (se puede montar en zócalo) |
| U2 | L78L05 | SOT-89-3 | B.Cu | VDD = 5 V |
| Q1 | TIP41C / D44H11G | TO-220-3 | F.Cu | NPN tótem H (colector a Vb) |
| Q2 | TIP42C / D45H11G | TO-220-3 | F.Cu | PNP tótem H (colector a VS1) |
| Q3 | TIP41C / D44H11G | TO-220-3 | F.Cu | NPN tótem L (colector a 15 V) |
| Q4 | TIP42C / D45H11G | TO-220-3 | F.Cu | PNP tótem L (colector a GND_K) |
| R1, R2 | 10 kΩ | 1206 | F.Cu | Pull-down de HIN y LIN |
| R3 | 2.2 Ω | 1206 | F.Cu | Base del tótem L |
| R5 | 2.2 Ω | 1206 | B.Cu | Base del tótem H |
| R9 | 2.2 Ω 1 W | 1210 | B.Cu | Limitador del bootstrap |
| D5 | ES1D | SMA | B.Cu | Diodo de bootstrap |
| 1SMA5931B1 (Z1) | 18 V | SMA | B.Cu | Vb–VS1 (cátodo a Vb) |
| 1SMA5931B2 (Z2) | 18 V | SMA | B.Cu | 15 V–GND_K (cátodo a 15 V) |
| C3, C5 | 10 µF X7R 50 V | **1210** | F.Cu | Bootstrap Vb–VS1 |
| C9 | 10 µF X7R 50 V | **1210** | F.Cu | Tótem H (Vb–VS1) |
| C8 | 10 µF X7R 50 V | **1210** | B.Cu | Tótem L (15 V–GND_K) |
| C2, C4 | 10 µF X7R 50 V | **1210** | B.Cu | VCC–COM |
| C6 | 0.1 µF X7R | 1206 | B.Cu | VCC–COM, junto a los pines 2 y 3 |
| C1 | 10 µF X7R 50 V | **1210** | B.Cu | Entrada del 78L05 (15 V–GND) |
| C7 | 0.1 µF X7R | 1206 | F.Cu | Salida del 78L05 / VDD (5 V–GND) |
| J2 | Phoenix SPT 1,5/5-H-3,5 | 5 pos., 3.5 mm | F.Cu | H, GND, L, GND, P9 (abierto) |
| J5 | Header macho 1 × 9, 2.54 mm | Sin pad 8 | F.Cu | Salida; **quitar el pin 8** |

- Los **10 µF usan footprint 1210** (con pads de soldadura a mano): aceptan piezas 1210 y 1206.
- Los TIP usan el footprint **TO-220-3_Vertical** y se montan **acostados** con las patas dobladas a 90°.

**Capacitores 100 % cerámicos X7R** (sin electrolíticos): no se secan con el calor del gabinete tropical. Diseñar con la capacidad real a 15 V (−30 a −50 % en 50 V; el 1210 pierde menos que el 1206).

---

## 8. Ensamble

1. SMD de B.Cu, después F.Cu, al final THT (zócalo DIP-14, TIP, borneras, header).
2. **Quitar el pin 8 del header J5.**
3. **Verificar la orientación de Z1 y Z2** (banda = cátodo, hacia VB y hacia 15 V).
4. TIP acostados, patas dobladas a 90°, aletas sin tocarse.
5. Limpiar el flux. Validar en banco.
6. Al final, **silicón RTV de curado neutro** entre los cuerpos de los TIP y el PCB (no acético; no silicón caliente).

---

## 9. Validación en banco

1. **Sin MOSFET:** alimentar 15 V; medir 5 V en VDD; verificar HA_OUT y LA_OUT con HIN/LIN en alto y bajo (no invierte). Con J2 desconectado, salidas en bajo.
2. **SD:** confirmar 0 V en el pin 11.
3. **P9:** el MCU debe reportar **1 µs** antes de habilitar el PWM.
4. **Bootstrap:** VB–VS1 al final de los 8.3 ms de la rama de 60 Hz, con margen sobre el UVLO (~9.7 V máx.).
5. **Retardos y tiempo muerto real** en la compuerta de cada MOSFET, con la carga real (1 a 5 MOSFET). Criterio: VGS < 2 V al menos 200 ns antes de que encienda el MOSFET contrario.
6. **VSS–COM:** medir la diferencia durante la conmutación (debe quedar muy por debajo de ±5 V).
7. **Sobretiro de 15 V al conectar:** con el osciloscopio en VCC al conectar la alimentación; Z2 debe limitarlo.
8. **Térmico:** TIP casi a temperatura ambiente tras 30 min.
9. **Si TIP no cumple el criterio:** cambiar a D44H11G/D45H11G en el mismo footprint.

---

## 10. Bitácora

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-03 | Definición del módulo (IR2110 + tótem, P9 abierto, 78L05, VSS/COM separados, SD a VSS) y primer layout | Corregidos: VCC y SD sin conectar, colector de Q3, orientación de Z1, rutas de VS1 y GND_K | Desacoplo de tótems, serigrafía |
| 2026-11-04 | J5-6 a GND lógico; serigrafía completa; DRC | 0 errores, 0 sin conectar, 3 avisos esperados de huella modificada | Fabricar, validar en banco |
| | | | |

---

## 11. Referencias

- Infineon, IR2110E4 datasheet (High and Low Side Driver).
- onsemi, TIP41/TIP42 datasheet; onsemi, D44H11/D45H11 datasheet.
- STMicroelectronics, L78L05 datasheet.
- International Rectifier, Design Tip DT98-2, "Bootstrap component selection for control ICs".
- Infineon, IRF100P219 datasheet; International Rectifier, IRF540N datasheet.
- IPC-2221 (separaciones eléctricas).
