# Módulo gate driver TLP250H — modo trinchera (legacy) con tótem TIP41C/TIP42C
<img width="335" height="400" alt="imagen" src="https://github.com/user-attachments/assets/e21339fe-37e3-44d8-814f-c6384ac7cc5f" />
<img width="450" height="827" alt="imagen" src="https://github.com/user-attachments/assets/09583d0f-3638-456e-a11e-980f181438be" />
<img width="527" height="939" alt="imagen" src="https://github.com/user-attachments/assets/71d82b25-b94e-42bb-820c-cf43d3d45a0b" />



**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Módulo:** gate driver de servicio / legacy, intercambiable con IRS2186 y UCC21520ADWR (misma interfaz, mismo firmware)
**Versión de placa:** V.0.1.NOV.26.J.G
**Estado:** diseño terminado (esquemático y layout). Pendiente: DRC final, fabricación y validación en banco
**Fecha:** 2026-11-02 · **Revisión:** 2026-11-03 (diseño final)
**Documentos relacionados:** `identificacion_dt_pin9.md` (P9, llave en posición 8, poka-yoke), `compensacion_adaptativa_tiempo_muerto.md`

---

## 1. Propósito

Este módulo existe para **cuando TI o Infineon no llegan**. Es una opción **educativa y de supervivencia**: demuestra que el puente H puede operar con componentes legacy que se consiguen en cualquier tienda de electrónica o se rescatan de equipos viejos (variadores, UPS, soldadoras inverter, fuentes, amplificadores).

| Criterio | Módulos IRS2186 / UCC21520 | Módulo TLP250 (este) |
| --- | --- | --- |
| Costo y número de componentes | **Menor** ✓ | Mayor |
| Desempeño (DT, THD) | **500 ns, THD ~0.8 %** ✓ | 1 µs, THD ~1.6 % |
| Disponibilidad en zonas aisladas | Baja (distribuidor, importación) | **Alta: tienda local o rescate** ✓ |
| Reparación a mano | SMD fino | **DIP y TO-220, cautín común** ✓ |
| Aislamiento entrada / potencia | IRS: no aislado · UCC: aislado | **Aislado (optoacoplador)** ✓ |
| Valor educativo | Driver integrado ("caja negra") | **Cada etapa visible y medible** ✓ |

**Conclusión de diseño:** para producción se usan IRS2186 o UCC21520. Este módulo no busca eficientar; busca que **siempre exista una alternativa**. La infraestructura que lo soporta (P9 y llave en posición 8) cuesta ~1 USD y queda en todas las tarjetas.

---

## 2. Interfaz

### 2.1 J1 — entrada, hacia la tarjeta de control (bornera de 5 terminales)

| Terminal | Señal | En este módulo |
| --- | --- | --- |
| 1 | H | Ánodo del LED de U1 (vía R3) |
| 2 | GND | Cátodo del LED de U1 (trenzar con H) |
| 3 | L | Ánodo del LED de U2 (vía R4) |
| 4 | GND | Cátodo del LED de U2 (trenzar con L) |
| **5** | **P9 — ID DT** | **Abierto, sin pista** → el MCU usa **1 µs** |

- Serigrafía: **"TRENZAR H Y L CON GND"** y **"OPERACION DT TLP250=1US"**.
- **P9 tiene como vecino un GND (terminal 4).** Cable con **puntas estañadas o terminal**, sin hilos sueltos. Opcional: no instalar el cable P9 (la pull-up de la tarjeta de control lee 1 → 1 µs).
- El GND de J1 es el GND del MCU y **no se une a GND_K ni a la potencia dentro del módulo**: el optoacoplador aísla la entrada.

### 2.2 J5 — salida, hacia la tarjeta del puente H (header de 9 posiciones, 2.54 mm)

| Pin | Señal | En este módulo |
| --- | --- | --- |
| 1 | HA_OUT | Emisores del tótem H |
| 2 | VS1 | Referencia del canal H |
| 3 | — | Libre (separación del nodo conmutado) |
| 4 | GND_K | Referencia Kelvin del canal L |
| 5 | LA_OUT | Emisores del tótem L |
| 6 | GND_PUENTE_H | **Sin uso** en este módulo (la entrada está aislada) |
| 7 | 15 V | Alimentación |
| 8 | — | **Llave mecánica:** sin pad; **quitar el pin 8 del header**; "X" en serigrafía |
| 9 | — | Reservado, abierto |

- **Polaridad:** el TLP250 no invierte (LED encendido = salida alta) y el tótem es seguidor de emisor (no invierte). **El firmware no cambia.**

### 2.3 Dominios de referencia

| Zona | Referencia | Componentes |
| --- | --- | --- |
| Entrada (lado del MCU) | **GND** (de J1) | J1, R1–R4, pines 1–4 de U1 y U2 |
| Canal L (salida) | **GND_K** | U2 pin 5, Q3 colector, C1, C4, C5 |
| Canal H (salida, flota con VS1) | **VS1** | U1 pin 5, Q2 colector, C2, C3, C6, zener, D5/R9 |

GND y GND_K se unen en **un solo punto en la tarjeta del puente H**, nunca dentro del módulo. **Ningún capacitor entre dominios** (en particular, nunca entre VS1 y GND_K: quedaría en paralelo con el MOSFET bajo).

---

## 3. Esquemático por canal

```
            J1-H ──[R3 330 Ω]──► 2 ┐                 VB (Net zener-K) ──┬── C2, C3 (2 × 10 µF) ── VS1
                                   │ TLP250 U1   8 ── VB                 ├── C6 (0.1 µF, pines 8–5) ── VS1
            J1-GND ────────────► 3 ┘             5 ── VS1                └── Z1 18 V (cátodo a VB) ── VS1
                                              6,7 ──[R5 2.2 Ω]──┬── B  Q1 TIP41C  C ── VB
                                                                │          E ──┬── HA_OUT (J5-1)
                                                                └── B  Q2 TIP42C  E ──┘
                                                                           C ── VS1 (J5-2)

  Bootstrap:  15 V ── D5 ES1D ── R9 2.2 Ω 1 W ── VB
```

El canal L es igual, con U2, R4, R6, Q4 (TIP41C, colector a 15 V) y Q3 (TIP42C, colector a GND_K); C1 y C4 (10 µF) y C5 (0.1 µF) entre 15 V y GND_K.

**Por qué seguidor de emisor (colectores a las rieles, emisores a la salida):** no invierte, no satura (no hay tiempo de almacenamiento) y no tiene conducción cruzada propia. El montaje inverso (emisores a las rieles, colectores juntos) invierte la señal, satura los TIP y se come el tiempo muerto.

---

## 4. Etapa TLP250H

### 4.1 Datos de la hoja de datos (Toshiba TLP250H)

| Parámetro | Valor |
| --- | --- |
| Corriente pico de salida | ±2.0 A recomendada, ±2.5 A máximo absoluto |
| Retardo de propagación | 100–500 ns |
| Diferencia entre piezas | ±150 ns |
| Distorsión de ancho de pulso | ≤ 50 ns |
| UVLO | 7.5–9.5 V |
| VCC recomendado | 10–30 V |
| Corriente de alimentación | 3 mA máx. |
| Aislamiento | 5000 Vrms |
| **Requisitos de layout** | 0.1 µF cerámico entre pines 8 y 5, **a menos de 1 cm**; VCC debe subir a **≤ 3 V/µs** |

Pines (DIP-8): 1 NC · 2 ánodo · 3 cátodo · 4 NC · 5 GND (VEE) · 6 salida · 7 salida · 8 VCC. Verificar contra la hoja de datos de la pieza que se compre.

### 4.2 Entrada (LED)

| MCU | R3 = R4 | Corriente del LED |
| --- | --- | --- |
| **ATmega328 a 5 V (diseño actual)** | **330 Ω** | ~10 mA ✓ |
| ESP32 / STM32 a 3.3 V | **150 Ω** | ~11 mA (con 330 Ω quedan ~5 mA, justo en el umbral máximo) |

R1 y R2 (10 kΩ) de H y L a GND mantienen los LED apagados si el MCU no está conectado.

### 4.3 Alimentación y bootstrap

| Canal | VCC / VEE | Fuente |
| --- | --- | --- |
| L | 15 V / GND_K | 15 V directo de J5-7 |
| H | VB / VS1 | Bootstrap: 15 V → D5 ES1D → R9 2.2 Ω 1 W → VB, con 2 × 10 µF y zener 18 V |

- **Pendiente de subida:** τ = 2.2 Ω × 20 µF = 44 µs → ~0.34 V/µs ✓ (límite 3 V/µs).
- **Caída en la rama de 60 Hz (8.3 ms encendido):** consumo del TLP (3 mA) más las 10 kΩ de compuerta de la tarjeta del puente H (con 2 MOSFET: 5 kΩ → ~2.8 mA):
  `ΔV = (3 + 2.8) mA × 8.3 ms / 20 µF ≈ 2.4 V` → VB ≈ **11.8 V**, arriba del UVLO máximo de 9.5 V ✓ y de los 6 V a los que se especifica el R_DS(on) del IRF100P219 ✓.
- **X7R y voltaje DC:** un 10 µF / 50 V en 1206 a 15 V puede quedar en ~5–7 µF reales; por eso son **2 × 10 µF**.
- El orden D5 → R9 funciona igual que R → D (circuito de TI) usado en los otros módulos.

### 4.4 Por qué 10 µF + 0.1 µF

| | 10 µF | 0.1 µF |
| --- | --- | --- |
| Función | Depósito de energía: ~480 nC por flanco (ΔV ≈ 48 mV) y los 8.3 ms del bootstrap | Respuesta a los flancos rápidos (~50 ns) |
| Ubicación | Junto al tótem / bootstrap | Pegado a los pines 8 y 5 del TLP (~5 mm) |
| Solo, falla en | Flancos: ruido y caídas en VCC del TLP | Carga total: con 480 nC caería ~4.8 V |

---

## 5. Tótem TIP41C / TIP42C

### 5.1 Cuándo hace falta

Regla: **carga de compuerta total por posición ≤ ~250 nC a 15 V → el TLP250 solo alcanza.** Más de eso → tótem. En esta versión el tótem va **siempre montado**, así el módulo sirve para 1 o 2 MOSFET por posición.

| Carga en cada posición | Qg aprox. a 15 V | ¿TLP solo? | ¿Con tótem? |
| --- | --- | --- | --- |
| 1 × IRF100P219 | ~240 nC | ✓ | ✓ |
| **2 × IRF100P219** | **~480 nC** | ✗ (~600 ns de apagado + 400 ns de desfase = sin margen) | ✓ validar en banco |
| 1–2 × IRF540N | ~100–200 nC | ✓ | ✓ |
| 4 × IRF540N | ~400 nC | ✗ | ✓ validar en banco |

### 5.2 Cálculos

Datos: TIP41C/TIP42C (onsemi): V_CEO 100 V, 6 A continua, **10 A pico**, hFE ≥ 15 a 3 A, **f_T ≥ 3 MHz**, V_BE hasta 2 V a 6 A. IRF100P219: Qg 180 nC típ. / 270 nC máx. a 10 V, R_G interna 1.2 Ω, V_GS(th) 2.2–3.8 V.

| Concepto | 1 MOSFET | 2 MOSFET |
| --- | --- | --- |
| Pico de compuerta (4.7 Ω por MOSFET en la tarjeta del puente H) | ~2.4 A | ~4.8 A (< 10 A ✓) |
| Corriente de base desde el TLP (R5/R6 = 2.2 Ω) | < 0.2 A | ~0.3 A (< 2 A ✓) |
| Compuerta encendida / apagada | ~14.3 V / ~0.7 V | ~14.3 V / ~0.7–1 V |
| Corriente promedio por transistor | — | Qg × f ≈ 12 mA |
| Potencia por transistor a 24 kHz | < 0.05 W | ~0.09 W típ., ~0.13 W máx. |
| Temperatura sin disipador (60 °C/W, 60 °C ambiente) | ~63 °C | ~65–68 °C (límite 150 °C ✓) |
| Margen en 1 µs (ventana útil ~600 ns) | amplio | **~150–250 ns ⚠ validar en banco** |

### 5.3 Por qué TIP41/42 y no otros

| Opción | Veredicto | Razón |
| --- | --- | --- |
| **TIP41C / TIP42C** | ✓ **Elegido** | Universal, barato, robusto (10 A pico). Lento (f_T 3 MHz) → validar en banco |
| D44H11 / D45H11 | ✓ Mejora directa | **Mismo TO-220 B-C-E**, f_T 40–50 MHz, 20 A pico; ~2 USD c/u |
| 2SD882 / 2SB772 | ⚠ No elegido | Rápido (~100 MHz), 6 A pico, pero TO-126 con pines que cambian por fabricante |
| BD139 / BD140 | ✗ Descartado | 2 A pico (Philips): un par no da corriente para 2 MOSFET |
| FMMT619 / FMMT720 | ✗ No trinchera | Bueno técnicamente (SOT-23), pero no se consigue en tiendas locales; PNP de 4 A pico |
| ZXTC2061E6 | ✗ Descartado | V_CEO 12 V < 15–18 V (el ZXTC2062E6, de 20 V, sí serviría en un módulo no trinchera) |
| TIP102 / TIP107 (Darlington) | ✗ Descartado | Doble V_BE: compuerta "apagada" a ~1.4–2.5 V, pegada al umbral de 2.2 V; además lento |
| IRF540 / IRF640 como buffer | ✗ Descartado | Seguidor de fuente: la compuerta apagada queda a ~4–5 V (no apaga). Como inversor: conducción cruzada propia |
| BC337/327, BC817/807, BC547/557 | ✗ Descartado | ≤ 1 A pico: el tótem sería más lento que el TLP solo |

**Lección:** para un tótem de compuerta importan **velocidad (f_T)** y **poca caída V_BE**, no la ganancia. Un buffer solo ayuda si entrega **más corriente que el driver que lo maneja**.

### 5.4 Resistencias de compuerta a fuente (10 kΩ)

**No van en el módulo**: están en la tarjeta del puente H, junto a cada MOSFET, en su arreglo de resistencias de compuerta. Ahí cubren el módulo sin alimentación, el módulo desconectado con el bus energizado (la compuerta conserva su carga) y el efecto Miller. Si un día se usa el módulo con una tarjeta que no las tenga, hay que agregarlas (HA_OUT → VS1, LA_OUT → GND_K).

---

## 6. Layout (como quedó en V.0.1)

- **Dominio HV** (VB, VS1, HA_OUT, U1 lado de salida, Q1, Q2, C2, C3, C6, zener, D5, R9): **0.6 mm** de separación al resto (regla `HV_a_LV`). Distancia medida entre la pista GND_K y el pad VB del zener: **~1.13 mm** ✓.
- **Sin cobre de GND bajo el cuerpo de los TLP** (barrera del optoacoplador limpia).
- **C5 y C6 (0.1 µF) en B.Cu**, entre los pads 5 y 8 de cada TLP, a ~5 mm ✓.
- **Zener 1SMA5931B en B.Cu**, junto a C2/C3 y a la entrada de VS1 desde J5-2: recorta los picos en el depósito del bootstrap, antes de que VB llegue a los TIP y al TLP.
- **C2/C3 a ~8 mm del colector de Q1** (~11 mm por pista) y pegados a J5-2: lazo de compuerta compacto. **No quedan debajo de los TIP** (sin riesgo de fisura del MLCC).
- **TIP acostados**, sin chocar con J1; cada aleta (colector) da contra el plástico del vecino, nunca contra otra aleta. Sin disipador.
- **HA_OUT** une los pines 3 de Q1 y Q2 por arriba y baja por el borde hasta J5-1, sin cruzar entre las filas de pines.
- **Pistas de salida de 0.5 mm:** suficientes (RMS ~0.3–0.5 A). Lo que importa es el lazo: retorno (VS1 / GND_K) junto a la salida.
- **Opcional:** 100 nF X7R 50 V en B.Cu entre los colectores de cada tótem (Q1-2 ↔ Q2-2 en H; Q4-2 ↔ Q3-2 en L). Montar si en banco el flanco de VGS sale redondeado u oscila.

### Serigrafía (como quedó)

| Texto | Dónde |
| --- | --- |
| "OPERACION DT TLP250=1US" | Cara superior |
| "TRENZAR H Y L CON GND" y etiquetas H / GND / L / GND / P9 | Junto a J1 |
| "TIP41C" / "TIP42C" con flechas de orientación | Junto a cada TO-220 |
| "X" en la posición 8 de J5 | Cara superior y posterior |
| "18V", "1SMA5931B1", "0.1UF", "10UF" | Cara posterior |
| "OJO: USAR SILICON RTV NEUTRO ENTRE TIPs" | Cara posterior |
| "V.0.1.NOV.26.J.G" | Cara posterior |

---

## 7. Lista de materiales (por módulo)

| Ref. | Cant. | Componente | Valor / parte | Nota |
| --- | --- | --- | --- | --- |
| U1, U2 | 2 | Optoacoplador | TLP250H (DIP-8) | Verificar pines en equivalentes |
| Q1, Q4 | 2 | NPN | TIP41C (o D44H11) | TO-220 B-C-E, acostado |
| Q2, Q3 | 2 | PNP | TIP42C (o D45H11) | TO-220 B-C-E, acostado |
| R3, R4 | 2 | Resistencia LED | 330 Ω (5 V) / 150 Ω (3.3 V) | Iguales |
| R1, R2 | 2 | Pull-down de entrada | 10 kΩ | |
| R5, R6 | 2 | Resistencia de base | 2.2 Ω | |
| R9 | 1 | Resistencia bootstrap | 2.2 Ω 1 W | |
| D5 | 1 | Diodo bootstrap | ES1D (o ES1J, como los otros módulos) | SMA |
| Z1 | 1 | Zener | 1SMA5931B (18 V) | B.Cu |
| C2, C3 | 2 | Capacitor | 10 µF X7R 50 V 1206 | VB–VS1 |
| C1, C4 | 2 | Capacitor | 10 µF X7R 50 V 1206 | 15 V–GND_K |
| C5, C6 | 2 | Capacitor | 0.1 µF X7R | B.Cu, pines 8–5 de cada TLP |
| J1 | 1 | Bornera | 5 posiciones | H, GND, L, GND, P9 |
| J5 | 1 | Header macho | 1 × 9, 2.54 mm | **Quitar el pin 8** |
| — | — | Silicón | RTV de curado neutro | Entre cuerpos de los TIP y el PCB |
| (opc.) | 2 | Capacitor | 100 nF X7R 50 V | Entre colectores de cada tótem |

---

## 8. Ensamble

1. Primero los **SMD de B.Cu** (C5, C6, zener), después los de F.Cu y al final los **THT** (TLP, TIP, J1, J5).
2. **Quitar el pin 8 del header J5** antes de soldarlo (el footprint no tiene pad 8).
3. TIP acostados con las patas dobladas a 90°, aletas sin tocarse.
4. Limpiar el flux.
5. **Validar en banco** (sección 9).
6. Al final, **silicón RTV neutro** entre los cuerpos de los TIP y el PCB, y entre transistores vecinos.
   - **No** silicón acético (olor a vinagre): corroe el cobre.
   - **No** silicón caliente: se ablanda a 60–80 °C.
   - No cubrir las patas por completo.

---

## 9. Validación en banco

1. **Sin MOSFET:** alimentar y medir VCC de cada TLP y la salida con la entrada en alto y en bajo. Confirmar que no invierte.
2. **Corriente de LED:** medir la caída en R3/R4 → ~10 mA.
3. **P9:** confirmar que el MCU reporta **1 µs** antes de habilitar el PWM.
4. **Aislamiento:** con el módulo desconectado, confirmar continuidad nula entre J1-GND y GND_K/VS1.
5. **Retardos:** medir los retardos de encendido y apagado de cada canal con carga (1 o 2 MOSFET). Anotar la diferencia entre canales.
6. **Criterio de aceptación (2 MOSFET):** VGS por debajo de **2 V al menos 200 ns antes** de que encienda el MOSFET contrario, en el peor caso.
7. **Si no cumple:** cambiar TIP41C/TIP42C por D44H11/D45H11 (mismo footprint).
8. **Bootstrap en la rama de 60 Hz:** medir VB–VS1 al final de los 8.3 ms → debe quedar > ~11 V.
9. **Térmico:** tras 30 min a plena potencia, los TIP deben estar casi a temperatura ambiente.
10. **Armónicos:** comparar el 3er armónico con la tabla de `identificacion_dt_pin9.md` (1 µs → ~1.17 %).

---

## 10. Errores encontrados y corregidos durante el diseño (registro educativo)

| Error | Consecuencia | Corrección |
| --- | --- | --- |
| Tótem armado como inversor (PNP arriba, NPN abajo, colectores juntos) | Señal invertida, TIP saturados (tiempo de almacenamiento de µs), conducción cruzada en el tótem | NPN arriba y PNP abajo, **emisores** juntos a la salida |
| LED con 1 kΩ | ~1.7 mA: el TLP puede no encender | 330 Ω (5 V) / 150 Ω (3.3 V), iguales en ambos canales |
| Pines 8 y 5 del TLP sin conectar | TLP sin alimentación | U1: 8 → VB, 5 → VS1 · U2: 8 → 15 V, 5 → GND_K |
| Faltaban resistencias de base, 0.1 µF, zener y etiquetas HO/LO | Sobrecorriente en el TLP, ruido, picos en VB, salida desconectada | Agregados |
| Relleno de GND bajo el TLP y alrededor del dominio HV | Se anula la barrera del optoacoplador y la separación de 0.6 mm | Sin cobre de GND bajo los TLP ni en la zona HV |
| Pista de GND entre los pads de D5 | GND pegado a un nodo que salta ~63 V a 24 kHz | Se aisló J1-GND de la potencia (J5-6 sin uso) y desapareció esa pista |
| C1/C4 a GND en vez de GND_K | El pico de compuerta regresa por el shunt y el plano de GND | C1/C4 a GND_K |
| TIP parados, chocando con J1 y con aletas casi juntas | Choque mecánico; corto VB–GND_K si las aletas se tocan | TIP acostados, aletas contra plástico |
| Propuesta confusa de un capacitor "entre VS y GND_K" | Capacitor en paralelo con el MOSFET bajo | Desacoplo siempre dentro del mismo canal: VB–VS1 y 15 V–GND_K |

---

## 11. Bitácora

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-02 | Evaluación de buffers y definición del módulo | Elegido TLP250H + tótem TIP41C/TIP42C; regla de 250 nC | Esquemático y layout |
| 2026-11-03 | Esquemático y layout terminados, versión V.0.1.NOV.26.J.G | P9 en J1 abierto; J5 sin pad 8 con "X"; J1-GND aislado (J5-6 sin uso); C5/C6 y zener en B.Cu; TIP acostados; serigrafía completa | DRC final, fabricar, validar en banco |
| | | | |

---

## 12. Referencias

- Toshiba, TLP250H datasheet, Rev. 2.0 (2013).
- onsemi, TIP41/TIP42 datasheet.
- onsemi, D44H11/D45H11 datasheet.
- Philips, BD135/137/139 datasheet (1999).
- Diodes Inc., FMMT619, FMMT717–FMMT723, ZXTC2061E6 / ZXTC2062E6.
- Infineon, IRF100P219 datasheet; International Rectifier, IRF540N datasheet.
- IPC-2221 (ancho de pistas y separaciones).
