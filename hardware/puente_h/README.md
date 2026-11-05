# Tarjeta de potencia: Puente H SPWM 48 V

**Isla del Sol — Puente H, prototipo V0.1** · Archivo: `PUENTE_H_INV48V_REV_C_2026-10_GATE_CENTRO` · Estado: **diseño terminado, pendiente de fabricar y validar en banco**

> ⚠️ **Seguridad:** esta tarjeta maneja hasta ~35 A en el lado de 48 V, y el transformador entrega **127 V AC, que pueden ser mortales**. Una batería de 48 V entrega cientos de amperes en un corto. Arma y prueba primero con una fuente limitada en corriente y con voltaje reducido (sección 12).

Es la tarjeta principal del inversor/cargador: contiene los cuatro MOSFET del puente H, el banco de capacitores del bus, los shunts para medir corriente y los conectores para los módulos universales (gate drivers, sensor de corriente y buck de 15 V). La lógica de control vive en otra tarjeta.

---
<img width="916" height="766" alt="imagen" src="https://github.com/user-attachments/assets/5b75ac33-6a59-4065-8a0b-40a90492ccdb" />

## 1. Características

| Parámetro | Valor |
| --- | --- |
| Entrada | Batería de 48 V (bus de ~42 a ~60 V) |
| Potencia | 1000 W como inversor; 1 kVA como cargador |
| Salida | Primario del transformador de 60 Hz (rango compatible 24–28 V / 120 V, sección 9) |
| Topología | Puente H completo: dos piernas, un MOSFET alto y uno bajo en cada una |
| MOSFET | 4 × Infineon **IRF100P219** (100 V, 1.4 mΩ típico), TO-247 vertical |
| Disipadores | 4 × Fischer **SK 129 50.8 STS** (5.3 °C/W), soldados a la tarjeta |
| Modulación (inversor) | **Híbrida:** pierna A a 24 kHz, pierna B a 60 Hz |
| Modulación (cargador) | **Unipolar:** las dos piernas a 24 kHz |
| Capacitores de bus | 4 × Rubycon **100ZLH820MEFC18X40** + 4 × 10 µF + 4 × 4.7 µF X7R + film 1 µF |
| Medición de corriente | Shunts Littelfuse SSA2512 de 1 mΩ en el retorno de la batería (1 a 3 piezas) |
| Cobre | **2 capas, 1 oz (35 µm)**: es la condición de diseño, porque es lo que se consigue en trinchera |
| Tamaño de la tarjeta | ~166 × 140 mm |

---

## 2. Archivos

| Archivo | Contenido |
| --- | --- |
| `PUENTE_H_INV48V_REV_C_2026-10_GATE_CENTRO.kicad_pro` | Proyecto de KiCad 9 |
| `PUENTE_H_INV48V_REV_C_2026-10_GATE_CENTRO.kicad_sch` | Esquemático principal (hoja raíz) |
| `CAPASITORES.kicad_sch` | Hoja jerárquica: banco de capacitores del bus |
| `untitled.kicad_sch` | Hoja jerárquica: disipadores |
| `PUENTE_H_INV48V_REV_C_2026-10_GATE_CENTRO.kicad_pcb` | Layout |
| `auditoria/` | Auditoría de corriente y temperatura del layout: herramientas, figuras y [README](auditoria/README.md) |

La huella del shunt (`SSA2512_Littelfuse_Kelvin.kicad_mod`) y su modelo 3D están en `hardware/sensores/sensor-dc-ina-comparador/`.

---

## 3. Cómo está organizada

```
 BATERÍA 48 V
   (+)──Faston J5, J6────────────── plano 48 V (F.Cu, mitad inferior) ──┬── Q1 (alto A) ── Q3 (alto B)
   (−)──Faston J1, J2── GND_IN ── shunts R16/R28/R17 ── plano GND ──────┴── Q2 (bajo A) ── Q4 (bajo B)
                                     │ Kelvin (NT5/NT6)
                                     └── J3 → módulo SENSOR_DC_INA_COMPARADOR

   Pierna A (24 kHz): Q1 + Q2 → VS1 → bornes J24 + J27 ─┐
                                                        ├── transformador (dos cables en paralelo por lado)
   Pierna B (60 Hz):  Q3 + Q4 → VS2 → bornes J23 + J22 ─┘

   Fila central: C1 C2 (pierna A) · J10 (gate driver A) · C3 C4 (pierna B) · J9 (gate driver B)
   J8: salida de 48 V protegida (R14) para el buck de 15 V de la tarjeta de control
```

| Zona | Qué hay |
| --- | --- |
| Izquierda | Entrada de batería (Faston), shunts, conector del sensor J3, films C9 y C10, salida J8 al buck |
| Centro izquierda | Pierna A: Q2 arriba (bajo), Q1 abajo (alto), con sus disipadores; C1 y C2 en medio |
| Centro | J10, el socket del gate driver de la pierna A |
| Centro derecha | Pierna B: Q4 arriba (bajo), Q3 abajo (alto); C3 y C4 en medio |
| Derecha | J9, el socket del gate driver de la pierna B |

---

## 4. Conectores

| Ref. | Tipo | Señales | Notas |
| --- | --- | --- | --- |
| J5, J6 | Faston 6.35 mm (TE 63823) | **48 V** de batería | Dos en paralelo, ~15 A cada uno a plena carga. Confirmar la corriente por terminal en la hoja de datos de TE. |
| J1, J2 | Faston 6.35 mm | **GND_IN** (negativo de batería) | Antes de los shunts |
| J24, J27 | Borne M3 con vías | **VS1** | Dos cables en paralelo a una terminal del transformador |
| J23, J22 | Borne M3 con vías | **VS2** | Dos cables en paralelo a la otra terminal |
| J10 | Socket 1 × 9, 2.54 mm | Gate driver de la pierna A | Pinout en la tabla siguiente |
| J9 | Socket 1 × 9, 2.54 mm | Gate driver de la pierna B | Mismo pinout |
| J7 | JST EH 2 pines | 15V_GA + GND | Alimentación del gate driver A (LED D6) |
| J4 | JST EH 2 pines | 15V + GND | Alimentación del gate driver B y del sensor (LED D5) |
| J3 | Header 1 × 6, 2.54 mm | 1 GND · 2 IN+ · 3 IN− · 4 GND · 5 **llave** · 6 15V | Módulo SENSOR_DC_INA_COMPARADOR (sensado Kelvin del shunt) |
| J8 | JST EH 2 pines | 48 V protegido + GND | Al buck de 15 V de la tarjeta de control, a través de R14 (sección 10) |

**Pinout de J10 y J9** (igual al J5 de todos los módulos gate driver; ver `docs/identificacion_dt_pin9.md`):

| Pin | Pierna A (J10) | Pierna B (J9) | Función |
| --- | --- | --- | --- |
| 1 | HO_A | HO_B | Gate del MOSFET alto |
| 2 | VS1_k | VS2_k | Retorno Kelvin del MOSFET alto |
| 3 | NC | NC | — |
| 4 | GND_KELVIN_A | GND_KELVIN_B | Retorno Kelvin del MOSFET bajo |
| 5 | LO_A | LO_B | Gate del MOSFET bajo |
| 6 | GND | GND | Tierra de potencia |
| 7 | 15V_GA | 15V | Alimentación del módulo |
| 8 | **Llave** (posición tapada) | **Llave** | Poka-yoke: el módulo solo entra en una posición |
| 9 | NC | NC | Reservado |

---

## 5. Las dos piernas y por qué no son iguales

| | Pierna A | Pierna B |
| --- | --- | --- |
| MOSFET | Q1 (alto), Q2 (bajo) | Q3 (alto), Q4 (bajo) |
| Nodo de salida | VS1 | VS2 |
| Conmutación en modo inversor | **24 kHz** | **60 Hz** |
| Conmutación en modo cargador | 24 kHz | 24 kHz |
| Pérdidas estimadas por MOSFET (inversor) | ~4.1 W | ~1.2 W |

**La pierna A es la de alta frecuencia y queda fija por hardware.** El firmware no debe intercambiar las piernas. La razón y la comparación con la modulación bipolar y la unipolar están en [`docs/modulacion_spwm.md`](../../docs/modulacion_spwm.md).

**Potencial de los disipadores.** La pestaña del TO-247 es el drenaje, y los disipadores se sueldan a la net de su MOSFET:

| Disipador | MOSFET | Potencial | Comentario |
| --- | --- | --- | --- |
| HS5 | Q1 (alto A) | 48 V | Fijo; sin problema de ruido |
| HS7 | Q3 (alto B) | 48 V | Fijo |
| HS6 | Q2 (bajo A) | **VS1** | Se mueve 48 V a 24 kHz: es la principal fuente de ruido por capacitancia al chasis |
| HS8 | Q4 (bajo B) | VS2 | En modo inversor solo se mueve a 60 Hz: casi silencioso |

**No toques los disipadores con el equipo encendido**: todos están a 48 V o a VS.

---

## 6. Red de compuerta y sensado Kelvin

Cada MOSFET tiene la misma red, junto a sus patas, en la cara de soldadura:

| MOSFET | Encendido | Apagado rápido | Gate–source | Kelvin |
| --- | --- | --- | --- | --- |
| Q1 (alto A) | R2 10 Ω | R3 2.2 Ω + D2 ES1D | R1 10 kΩ a VS1 | NT4: VS1_k ↔ VS1 |
| Q2 (bajo A) | R8 10 Ω | R7 2.2 Ω + D3 ES1D | R9 10 kΩ a GND | NT2: GND_KELVIN_A ↔ GND |
| Q3 (alto B) | R5 10 Ω | R4 2.2 Ω + D1 ES1D | R6 10 kΩ a VS2 | NT3: VS2_k ↔ VS2 |
| Q4 (bajo B) | R11 10 Ω | R12 2.2 Ω + D4 ES1D | R10 10 kΩ a GND | NT1: GND_KELVIN_B ↔ GND |

- **Encendido por 10 Ω y apagado por 2.2 Ω + diodo:** el MOSFET enciende suave (menos sobretiro y ruido) y apaga rápido (menos riesgo de conducción cruzada). El ánodo del ES1D va hacia el gate.
- **10 kΩ gate–source:** mantienen apagado el MOSFET si el módulo no está puesto o se desconecta.
- **Kelvin:** el retorno de cada gate driver se toma en el source de su MOSFET, a través de un *net tie* colocado **del lado contrario al flujo de potencia**, para que la caída de la corriente de carga no se sume al voltaje de compuerta.
- **Pares acoplados:** cada señal de gate viaja junto a su retorno Kelvin, con una separación mediana de 0.73 mm.

---

## 7. Banco de capacitores del bus

| Ref. | Valor | Dónde | Función |
| --- | --- | --- | --- |
| C1, C2 | 820 µF / 100 V, Rubycon ZLH 18 × 40 | Pierna A, X = 85.5 y 113 | Rizo de 24 kHz y energía del bus |
| C3, C4 | 820 µF / 100 V, Rubycon ZLH 18 × 40 | Pierna B, X = 161 y 184.75 | Ídem |
| C5–C8 | 10 µF X7R 1210, ≥ 100 V | **B.Cu, entre los pines de cada Rubycon** | Refuerzo de alta frecuencia, lazo mínimo |
| C11, C12 | 4.7 µF X7R 1210 | F.Cu, entre C1 y C2, sobre la frontera GND / 48 V | Flancos de conmutación (MHz) de la pierna A |
| C13, C14 | 4.7 µF X7R 1210 | F.Cu, entre C3 y C4 y a la izquierda de C3 | Flancos de la pierna B |
| C9 | 1 µF / 250 V MKP | Cerca de la entrada, 48 V–GND | Film del bus y de entrada |
| C10 | 1 µF / 250 V MKS | En la entrada, 48 V–GND_IN | Filtro de la batería, antes de los shunts |

**Cómo funciona el desacoplo:**
- **Plano partido bajo la fila de capacitores:** en F.Cu, GND ocupa la mitad superior (hacia los MOSFET bajos) y 48 V la inferior (hacia los altos). Los 4.7 µF cruzan esa frontera con un pad en cada plano, sin pistas.
- **Lazo de conmutación doblado:** VS1 y VS2 van en B.Cu, justo debajo de los planos de 48 V y GND de F.Cu. La corriente de ida y la de regreso quedan encimadas a 1.6 mm, y eso da **~4 nH** de lazo en la tarjeta aunque los MOSFET de una pierna estén a 74 mm entre sí.
- **Islas con vías:** cada pin de Rubycon tiene su isla de 48 V o de GND en las dos capas, con su anillo de vías.

**Rizo calculado** (detalles en [`auditoria/README.md`](auditoria/README.md), sección 8): de los 28.6 A RMS que el puente toma del bus, **13 A son de alta frecuencia y los entregan los Rubycon, ~3.25 A cada uno (95–97 % de su nominal a 105 °C)**. El bote queda en ~47 °C con 40 °C de ambiente. **No se debe quitar ningún Rubycon**: con 3, cada uno pasaría al ~127 % de su nominal.

**Sobre C9:** está a 27–37 mm de los MOSFET de la pierna A, así que su función es de **film del bus y de la entrada**. El trabajo de alta frecuencia de la pierna A lo hacen C11, C12, C5 y C6, que sí están sobre el lazo.

---

## 8. Medición de corriente (shunts)

| Ref. | Pieza | Función |
| --- | --- | --- |
| R16 | Littelfuse SSA2512LR001FWR, 1 mΩ, 6 W | **Posición Kelvin: siempre montada.** De sus pads salen IN+ (NT5) e IN− (NT6) hacia J3. |
| R28 | Ídem | Segunda pieza (operación) |
| R17 | Ídem | Tercera pieza (opcional) |

| Piezas montadas | Resistencia | Límite de medición | Uso |
| --- | --- | --- | --- |
| 1 (R16) | 1 mΩ | ±34 A | Pruebas |
| 2 (R16 + R28) | 0.5 mΩ | ±68 A | **Operación** |
| 3 | 0.33 mΩ | ±102 A | Ver el README del sensor |

Las fórmulas, la calibración y la constante del firmware (`R_SHUNT_OHM`) están en el [README del sensor](../sensores/sensor-dc-ina-comparador/README.md). Con 2 piezas, cada shunt disipa ~0.23 W a plena carga, de 6 W nominales.

---

## 9. Transformador compatible

El transformador no tiene que ser exacto. **Si el lado de baja de tu transformador está entre 24 y 28 V para 120 V, es compatible.**

| Lado bajo del transformador | 24 V | 26 V (punto óptimo) | 28 V |
| --- | --- | --- | --- |
| Relación | 1 : 5.0 | 1 : 4.6 | 1 : 4.29 |
| Primario para 127 V de salida | 25.4 V RMS / 35.9 V pico | 27.5 V RMS / 38.9 V pico | 29.6 V RMS / 41.9 V pico |
| Corriente del primario a 1000 W | ~39 A RMS | ~36 A RMS | ~34 A RMS |
| Margen con la batería baja (~44 V) | Amplio (m ≈ 0.82) | Bueno | Justo (m ≈ 0.95): la salida puede caer a ~115–120 V a plena carga |
| Cargador (pico de la red < voltaje de carga) | ✅ | ✅ | ✅ |

- **Con 24 V**, la salida se sostiene mejor cuando la batería está baja, pero circula ~15 % más corriente por el cobre y los MOSFET.
- **Con 28 V**, el equipo es más eficiente, pero se queda sin margen al final de la descarga.
- En la salida de 127 V va un **capacitor de polipropileno X2 (MKP) de 4.7 µF, 305 V AC**, fuera de esta tarjeta.

La auditoría del layout se hizo con 30.5 V de primario y 33 A RMS. Con un transformador de 24 V, las pérdidas en el cobre suben ~(39/33)² ≈ 1.4 veces.

---

## 10. Salida J8 para el buck de 15 V

La tarjeta de control lleva un buck **extraíble**, para aceptar distintos fabricantes y tecnologías, que convierte los 48 V a 15 V. J8 le entrega el voltaje a través de una protección:

| Elemento | Valor | Por qué |
| --- | --- | --- |
| **R14** | **2.2 Ω, ½ W, THT** (huella DIN0309, paso 12.7 mm) | Resistencia fusible: limita un corto a ~22 A y se abre en milisegundos. Con el consumo normal (~0.2 A) disipa 0.09 W y cae 0.44 V. |
| Fusible en línea | **T1A, 5 × 20, de preferencia de cuerpo cerámico**, en un portafusible a ≤ 10 cm de J8 | Cubre las sobrecargas pequeñas (0.5–2 A), donde la resistencia es lenta |
| Buck | **Entrada ≥ 80 V** (por ejemplo, módulos con XL7015; confirmar en la hoja de datos) | El bus llega a ~60 V en carga. **No usar módulos con LM2596** (máximo 40 V): revientan en este bus. |

- R14 se monta **elevada 3–5 mm** sobre la tarjeta, y su pad de 48 V tiene alivios térmicos para que el calor de una sobrecarga se quede en la resistencia.
- **Si no hay T1A**, se improvisa con una resistencia de 2.2 Ω ½ W o con un fusible de baja corriente de otro aparato. **Nunca se puentea.**

---

## 11. Layout

| Aspecto | Decisión |
| --- | --- |
| Capas | 2, **1 oz**, FR-4 de 1.6 mm |
| Planos | F.Cu: GND arriba, 48 V abajo, VS1/VS2 en la zona de los MOSFET bajos. B.Cu: VS1 y VS2 cubriendo cada pierna completa; GND alrededor. |
| Separaciones | 1 mm en la frontera GND / 48 V de F.Cu; 0.6 mm entre las islas y los planos VS (IPC-2221B: ≥ 0.6 mm para 31–100 V en capa externa) |
| Conexión de los Rubycon a los planos | Sólida, en las dos capas: se necesita un cautín de ≥ 60–80 W |
| Holgura de los módulos gate driver | Los TIP de los módulos de trinchera sobresalen 21.1 mm del socket. C3 se recorrió a X = 161 y C4 a X = 184.75 para dejar ~2 mm de aire **sin mover J10 ni J9**, cuyas pistas a los gates están calibradas. Los cerámicos (~2.5 mm de alto) pasan por debajo de los TIP. |
| Ventanas para alambre | Cuatro ventanas en B.Mask, de la pata del MOSFET a su borne VS, con la leyenda "REFORZAR CON ALAMBRE Cu 12 AWG SOLDADO" |
| Tiras sin máscara en POS y NEG | Opcionales con 1 oz; quedan para quien suba la potencia |

### 11.1 Auditoría de cobre y temperatura

La auditoría completa está en [`auditoria/README.md`](auditoria/README.md). Resumen, a 1000 W con 40 °C de ambiente y sin ventilador:

| Resultado | Cobre solo (1 oz) | Con alambre 12 AWG |
| --- | --- | --- |
| Calentamiento del cobre por su propia corriente | +12.9 °C | +8.7 °C |
| Caída en la salida VS (MOSFET alto → borne) | ~21 mV | ~4 mV |
| Unión de Q1 / Q2 | ~62 °C | ~61 °C |
| Rubycon | 45–47 °C | 45–46 °C |
| Pérdidas totales (MOSFET + cobre + Rubycon) | ~14 W (1.4 %) | ~13 W |

- **El refuerzo con alambre es opcional a 1000 W.** Se recomienda en gabinetes cerrados o en clima caliente, y es necesario si se sube la potencia.
- **El calor dominante viene de los MOSFET de la pierna A**, no del cobre.
- Margen de las estimaciones: ±10–15 % en el cobre; −33 % / +70 % en los aumentos de temperatura, por la incertidumbre de la conmutación. Aun en el caso pesimista, todo queda dentro de los límites.

---

## 12. Ensamble

**Orden recomendado:**
1. SMD de la cara de soldadura: redes de gate (R1–R12, D1–D4) y *net ties*.
2. SMD de la cara de componentes: C11–C14, R13, R15, D5, D6, shunts R16 y R28 (y R17 si aplica).
3. **Rubycon C1–C4.** Revisa la polaridad: el "+" va hacia el lado de 48 V (abajo). La conexión es sólida al plano, así que usa un cautín de ≥ 60–80 W.
4. **Cerámicos de 10 µF en B.Cu (C5–C8) después de los Rubycon**, con punta fina. Quedan a ~0.5 mm del pad del Rubycon (misma net); si se sueldan antes, el calor del pin grande puede fracturarlos.
5. Films C9 y C10, conectores, R14 elevada 3–5 mm.
6. MOSFET con sus disipadores (pasta térmica entre la pestaña y el disipador).
7. **Alambre 12 AWG en las ventanas** (opcional): alambre desnudo de cable de instalación, estañado primero, soldado desde la pata del MOSFET hasta el borne.
8. **Antes de energizar, mide continuidad.** No debe haber continuidad entre:
   - 48 V y GND;
   - VS1 o VS2 y 48 V;
   - VS1 o VS2 y GND;
   - cada gate y su VS o GND (debe marcar ~10 kΩ, no corto).
9. **Silicón RTV de curado neutro** (nunca acético), al final y ya probada la tarjeta: filete en la base de cada Rubycon y un cordón entre botes vecinos, **sin tapar la cruz de venteo** de la tapa.

---

## 13. Validación en banco

1. **Sin módulos:** revisión visual y las mediciones de continuidad de la sección 12.
2. **Bus con fuente limitada:** aplica 12–24 V con límite de ~0.5 A. Confirma que la corriente en reposo es casi cero y que no hay calentamiento.
3. **Con módulos gate driver y sin carga:** confirma que cada módulo declara bien su tiempo muerto por P9 y mira en el osciloscopio HO/LO de las dos piernas (pierna A a 24 kHz, pierna B a 60 Hz).
4. **Con transformador y carga resistiva pequeña,** a voltaje reducido: mide V_AB, el sobretiro en VS1 y VS2 y la corriente del primario.
5. **Protección:** con el sensor DC montado, provoca una sobrecorriente controlada a bajo voltaje y mide el tiempo de corte (objetivo < 2–3 µs).
6. **Plena carga, 30 minutos:** temperatura en los puntos de la [auditoría](auditoria/README.md) (sección 11): cobre entre Q1 y J27, entre Q3 y J22, entre Q4 y los shunts, bote de C1/C2 y aleta del disipador de la pierna A. Anótalas en la bitácora para recalibrar el modelo.

---

## 14. Lista de materiales

| Ref. | Cant. | Valor / parte | Huella | Notas |
| --- | --- | --- | --- | --- |
| Q1–Q4 | 4 | Infineon IRF100P219 | TO-247-3 vertical | — |
| HS5–HS8 | 4 | Fischer SK 129 50.8 STS | Heatsink_Fischer_SK129-STS_42x25mm | Soldado a la net de su MOSFET |
| C1–C4 | 4 | Rubycon 100ZLH820MEFC18X40 | CP_Radial_D18.0mm_P7.50mm | No reducir la cantidad |
| C5–C8 | 4 | 10 µF X7R, ≥ 100 V | 1210 | B.Cu, entre los pines del Rubycon |
| C11–C14 | 4 | 4.7 µF X7R, ≥ 100 V | 1210 | F.Cu, sobre la frontera de planos |
| C9 | 1 | 1 µF / 250 V MKP | C_Rect_L26.5mm_W7.0mm_P22.50mm | — |
| C10 | 1 | 1 µF / 250 V MKS | Ídem | — |
| R16, R28 (R17) | 2 (3) | Littelfuse SSA2512LR001FWR, 1 mΩ 6 W | SSA2512_Littelfuse_Kelvin | R16 siempre montada |
| R2, R5, R8, R11 | 4 | 10 Ω | 1206 | Encendido |
| R3, R4, R7, R12 | 4 | 2.2 Ω | 1206 | Apagado |
| R1, R6, R9, R10 | 4 | 10 kΩ | 1206 | Gate–source |
| D1–D4 | 4 | ES1D | SMA | Apagado rápido |
| R13, R15 | 2 | 10 kΩ | 1206 | LED de 15 V |
| D5, D6 | 2 | LED | 1206 | Indicadores de 15 V |
| R14 | 1 | 2.2 Ω ½ W, película metálica o fusible | R_Axial_DIN0309, P 12.7 mm | Montada elevada |
| NT1–NT6 | 6 | *Net tie* | NetTie-2_SMD_Pad0.5mm | — |
| J1, J2, J5, J6 | 4 | Faston TE 63823 | Faston 63823 | Batería |
| J22–J24, J27 | 4 | Borne M3 | MountingHole_3.5mm_Pad_Via | Ojillo M3 crimpado y soldado |
| J9, J10 | 2 | Socket 1 × 9, 2.54 mm, con la posición 8 tapada | PinHeader_1x09 | Gate drivers |
| J3 | 1 | Socket 1 × 6, 2.54 mm, con la posición 5 tapada | PinHeader_1x06 | Sensor DC |
| J4, J7, J8 | 3 | JST EH 2 pines | JST_EH_B2B-EH-A | 15 V y salida al buck |
| H1–H4 | 4 | Barreno M4 | MountingHole_4.5mm | Sin net |
| — | — | Fusible T1A 5 × 20 + portafusible en línea | — | Cable de J8 al buck |
| — | — | Alambre de cobre 12 AWG desnudo | — | Refuerzo opcional |
| — | — | Silicón RTV de curado neutro | — | Fijación de los Rubycon |

---

## 15. Pendientes para V0.2

| Pendiente | Prioridad |
| --- | --- |
| **Ventanas del MOSFET bajo:** quedan a 1.49–1.58 mm del gate de Q2 y Q4. Subir a ≥ 2 mm. | **Alta, antes de fabricar** |
| Serigrafía: queda un "H0_A_G" con cero; marca "RTV" entre los Rubycon; "60HZ" junto a J9 | Media |
| Fusible principal de batería (clase T o ANL) y precarga del bus: fuera de la tarjeta, documentar en el manual | Alta (instalación) |
| Divisor de voltaje del bus y NTC para la tarjeta de control | Media |
| Aislar el disipador de la pierna A (lámina térmica + disipador a GND) si el ruido lo exige | Según la medición |
| Snubber RC en VS2 para el modo cargador, si el sobretiro lo exige | Según la medición |
| Auditoría en modo cargador (unipolar) | Media |

---

## 16. Bitácora

| Fecha | Qué se decidió | Resultado |
| --- | --- | --- |
| 2026-11-04 | Holgura de los TIP de los módulos de trinchera sin mover J10 ni J9: C3 a X = 161, C4 a X = 184.75 | ~2 mm de aire; los cerámicos pasan por debajo de los TIP |
| 2026-11-04 | Banco cerámico: 10 µF en B.Cu entre los pines de cada Rubycon y 4.7 µF en F.Cu sobre la frontera de planos, simétrico en las dos piernas | Lazo de alta frecuencia mínimo |
| 2026-11-04 | C9 (film) se queda junto a la entrada; la pierna A queda fija como la de alta frecuencia | Asignación de piernas fija en firmware |
| 2026-11-05 | Interfaz lógica de los gate drivers a 5 V (ATmega328P) | Documentado en `docs/identificacion_dt_pin9.md` |
| 2026-11-05 | Transformador compatible en rango 24–28 V / 120 V | Serigrafía actualizada |
| 2026-11-05 | Salida J8 al buck protegida con R14 (2.2 Ω ½ W THT) + T1A en línea; buck ≥ 80 V | J8 y R14 vinculadas al esquemático |
| 2026-11-05 | **1 oz como condición de diseño** (lo que existe en trinchera); auditoría de cobre y temperatura; ventanas para alambre 12 AWG hasta la pata del MOSFET | Refuerzo opcional a 1000 W; el bus DC no lo necesita |
| 2026-11-05 | Auditoría con MOSFET y Rubycon; sensibilidad y validación del método | Todo dentro de sus límites, incluso en el caso pesimista |
| 2026-11-05 | Nets de compuerta unificadas: `HO_A` / `LO_A` (pierna A) y `HO_B` / `LO_B` (pierna B), de J10/J9 hasta las redes de gate | Ninguna pista queda con net distinta a la del pad que toca |

---

## 17. Referencias

- Infineon, hoja de datos IRF100P219. <https://www.farnell.com/datasheets/2849758.pdf>
- Rubycon, hoja de datos de la serie ZLH. <https://sy-dep-epc-lpc.web.cern.ch/components/datasheets/epc-lpc%20(converters)/ZLH-Aluminium-Capacitor_Rubycon.pdf>
- Fischer Elektronik SK 129 50.8 STS (ficha del distribuidor). <https://nz.element14.com/fischer-elektronik/sk-129-50-8-sts/heat-sink-extruded-pcb-mounting/dp/1319813>
- Littelfuse SSA2512, ver el README del sensor DC.
- IPC-2221B (separaciones y capacidad de corriente del cobre) e IPC-2152.
- Texas Instruments, *Grid Connected Inverter Reference Design* (TIDM-HV-1PH-DCAC), con modulación de una pierna a alta frecuencia y otra a frecuencia de red. <https://www.ti.com/lit/pdf/tidub21>
- Documentos del proyecto: [`docs/modulacion_spwm.md`](../../docs/modulacion_spwm.md), [`docs/identificacion_dt_pin9.md`](../../docs/identificacion_dt_pin9.md), [`auditoria/README.md`](auditoria/README.md).

---

*Hardware bajo licencia CERN-OHL-S v2; documentación CC BY-SA 4.0 (ver `LICENSE.md` en la raíz).*
