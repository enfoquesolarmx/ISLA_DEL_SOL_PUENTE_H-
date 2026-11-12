# Módulo gate driver de medio puente H con UCC21520DW

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Módulo:** gate driver de **medio puente H** (una rama: MOSFET alto + MOSFET bajo) con el driver aislado de dos canales **TI UCC21520DW** (UVLO de 8 V) y bootstrap para el lado alto
**Intercambiable con:** IRS2186, TLP250 e IR2110 de trinchera (misma interfaz J4/J2, mismo firmware)
**Tiempo muerto declarado por P9:** **500 ns** (P9 a GND)
**Estado:** esquemático (por bloques A–G) y PCB subidos y verificados contra este README: conectividad completa, esquemático = PCB. Pendiente: DRC en KiCad, fabricación y validación en banco
**Fecha:** 2026-11-05 · **Revisión:** 2026-11-12 (R3 = 49.9 kΩ, UCC21520DW, esquemático por bloques)
**Documentos relacionados:** [`docs/identificacion_dt_pin9.md`](../../../docs/identificacion_dt_pin9.md), [módulo IRS2186](../irs2186/README.md)

| Cara de componentes | Cara de soldadura |
| --- | --- |
| ![Módulo UCC21520, vista 3D superior](img/ucc21520_3d_superior.png) | ![Módulo UCC21520, vista 3D inferior](img/ucc21520_3d_inferior.png) |

*Izquierda: arriba, la bornera J4 de entrada (DT · G · L · G · H) con las resistencias y los filtros de entrada; en medio, el **UCC21520DW** (SOIC-16 ancho, aislado); a la izquierda, el bootstrap (2.2 Ω + ES1J) con sus 2 × 10 µF; abajo, el header J2 hacia el puente H. Derecha: la leyenda **"DT-500NS"**, el zener de 18 V del bootstrap y la llave de la posición 8 de J2 (sin pad).*

---

## 1. Por qué este módulo

El UCC21520 es un driver de dos canales **con aislamiento galvánico** entre la entrada y cada salida. Cada canal tiene su propio UVLO y su propia tierra. Comparado con el IRS2186:
- Corriente de pico más alta (4 A de fuente / 6 A de sumidero), útil para muchos MOSFET en paralelo.
- **Tiempo muerto programable por hardware** con una resistencia (sección 4): si el firmware se equivoca, el driver no deja que los dos MOSFET se enciendan juntos.
- Si las dos entradas llegan en alto al mismo tiempo, **apaga las dos salidas** de inmediato.

No es una pieza de trinchera: se consigue con distribuidor. Es la opción "de producción" de mayor margen.

## 2. Interfaz

### 2.1 J4: entrada, hacia la tarjeta de control (bornera de 5 terminales)

| Pin | Señal | Conexión |
| --- | --- | --- |
| 1 | GND | Tierra |
| 2 | HA_IN (H) | INA, a través de R1 10 Ω; C2 100 pF y R5 10 kΩ a GND |
| 3 | GND | Tierra |
| 4 | LA_IN (L) | INB, a través de R2 10 Ω; C1 100 pF y R6 10 kΩ a GND |
| 5 | P9 ("DT") | **A GND**: declara 500 ns a la tarjeta de control |

Los umbrales de entrada del UCC21520 son de 1.6–2.0 V para el "1" y de 0.8–1.2 V para el "0", así que la lógica de 5 V del proyecto entra con margen amplio.

### 2.2 J2: salida, hacia la tarjeta del puente H (header de 9 posiciones)

| Pin | Señal |
| --- | --- |
| 1 | HA_OUT (gate del MOSFET alto) |
| 2 | VS1 |
| 3 | NC |
| 4 | GND_K |
| 5 | LA_OUT (gate del MOSFET bajo) |
| 6 | GND |
| 7 | 15 V |
| 8 | **Llave** (sin pad) |
| 9 | GND (reservado, sin función; inofensivo) |

## 3. Circuito

![Esquemático del módulo UCC21520 por bloques](img/ucc21520_esquematico.png)

*Esquemático por bloques (KiCad 9, hoja A3): **A** entrada J4 con filtro y pull-down · **B** lado de entrada (VCCI, DIS y tiempo muerto) · **C** UCC21520DW · **E** canal B, lado bajo · **F** bootstrap del canal A · **G** salida J2. Cada bloque lleva su nota de diseño.*

| Bloque | Componentes | Notas |
| --- | --- | --- |
| A · Entrada | J4; R1/R2 10 Ω + C2/C1 100 pF C0G (τ ≈ 1 ns); R5/R6 10 kΩ a GND | Redes filtradas INA_F / INB_F. Las entradas quedan en bajo si no hay control conectado |
| B · Lado de entrada | VCCI (pines 3 y 8) a **15 V**, C3 1 µF + C7 100 nF; DIS (5) a GND; **R3 49.9 kΩ 1 % + C4 1 nF C0G** del pin DT a GND | VCCI de 3 a 18 V. Umbrales de entrada fijos (1.6–2.0 V / 0.8–1.2 V), no dependen de VCCI: la lógica de 5 V entra con margen |
| C · Driver | **U1 UCC21520DW** | NC (7, 12, 13) sin conectar. Sin cobre bajo el centro del chip: es la barrera de aislamiento (ranura en el PCB) |
| E · Canal B (lado bajo) | VDDB (11) a 15 V, VSSB (9) a GND_K, C8 10 µF; OUTB → LA_OUT | — |
| F · Canal A (lado alto) | Bootstrap: 15 V → R4 2.2 Ω 1 W → D1 **ES1J** → **VDA** (VDDA, 16); C5 + C6 2 × 10 µF de VDA a VS1; Z1 18 V (cátodo a VDA); VSSA (14) a VS1; OUTA → HA_OUT | El mismo bootstrap que los otros módulos, con la R antes del diodo |
| G · Salida | J2 | Pinout de la sección 2.2 |

## 4. Tiempo muerto: R3 = 49.9 kΩ (500 ns en hardware)

Según la hoja de datos del UCC21520: **t_DT (ns) = 10 × R_DT (kΩ)**, con R_DT de 2 a 500 kΩ. La tabla de tiempos da **400–600 ns** con 50 kΩ.

| R3 | Tiempo muerto del hardware |
| --- | --- |
| 5 kΩ (versión anterior) | 50 ns (40–60) |
| **49.9 kΩ 1 % (actual)** | **500 ns (400–600)** |

**El tiempo muerto del firmware y el del driver no se suman.** Los dos relojes arrancan con el mismo evento (la bajada de la otra entrada) y corren en paralelo. El driver aplica **el mayor** de los dos: *"the longer of either the driver's programmed dead time or the input signal's own dead time"*.

```
t = 0       el AVR baja INB  → el UCC apaga OUTB y arranca su reloj de 500 ns
t = 500 ns  el AVR sube INA  → el reloj del UCC acaba en ese mismo momento → OUTA sube
            Tiempo muerto real: 500 ns (no 1000)
```

| Caso | AVR (P9 = GND) | R3 | Tiempo muerto real |
| --- | --- | --- | --- |
| Normal | 500 ns | 400–600 ns | **500–600 ns** |
| Error del firmware (DT = 0) | 0 ns | 400–600 ns | **400–600 ns**: el hardware protege |
| Con R3 = 5 kΩ | 500 ns | 50 ns | 500 ns, pero con el firmware fallando solo **50 ns** |

- **Por qué 49.9 kΩ y no menos:** en el peor caso da 400 ns, arriba de los ~390 ns que pide el IRF100P219. Con 39 kΩ el peor caso bajaría a ~312 ns.
- **Por qué no más:** arriba de ~50 kΩ el hardware le ganaría siempre al firmware y ya no se podría ajustar el tiempo muerto desde el AVR.
- **C4 de 1 nF C0G:** la hoja de datos pide **1 nF o menos** en paralelo con R_DT, pegado al pin 6. (Una revisión vieja de la familia pedía 2.2 nF o más; vale la actual.)
- **Nunca dejar DT al aire** (TI no lo recomienda) **ni unirlo a VCCI** (quita el tiempo muerto y permite que las salidas se encimen).
- **Para la compensación de tiempo muerto del firmware:** con este módulo el tiempo muerto efectivo queda entre 500 y 600 ns según el chip. Medirlo una vez con el osciloscopio (HA_OUT contra LA_OUT) y usar ese valor en `DT_S`.

El esquema del proyecto se mantiene: **última generación** (IRS2186, UCC21520) con P9 a GND = **500 ns**; **legacy de trinchera** (IR2110 + tótem, TLP250H + tótem) con P9 abierto = **1 µs**. En el UCC21520, R3 solo es el respaldo por hardware de esos 500 ns.

## 5. Versión del chip: UCC21520DW, no UCC21520ADW

La única diferencia entre las dos versiones es el UVLO de las salidas (VDDA / VDDB). Mismo encapsulado y mismos pines.

| Versión | Enciende en | Se apaga en | VDD mínimo de operación |
| --- | --- | --- | --- |
| UCC21520ADW (5 V UVLO) | 5.7–6.3 V | 5.4–6.0 V | 6.5 V |
| **UCC21520DW (8 V UVLO)** | **7.7–8.9 V** | **7.2–8.4 V** | **9.2 V** |

Con la versión A, si el bootstrap se cae, el MOSFET alto seguiría encendido con la compuerta a ~5.5 V: a medio encender y calentándose. Con la DW se apaga antes, igual que los otros módulos. El bootstrap da ~11.8 V al final de los 8.3 ms de la rama de 60 Hz, así que queda con margen sobre los 9.2 V. El símbolo de KiCad sigue siendo `UCC21520ADW` (los pines son idénticos); lo que manda es el **valor** `UCC21520DW (UVLO 8 V)`.

## 6. Layout

![Módulo UCC21520 en el editor de PCB](img/ucc21520_editor_pcb.png)

*Vista del editor con las dos capas. Arriba, la bornera J4 con las resistencias de entrada y la serigrafía "DT-500NS"; en el centro, el UCC21520 con su barrera de aislamiento; a la izquierda, el bootstrap (ES1J, 2.2 Ω, 2 × 10 µF); abajo, el header J2.*

- Placa de **28.2 × 46.0 mm**, 2 capas, con una **ranura de 1.0 × 11.4 mm** bajo el chip que separa el lado de entrada del de salida.
- Verificación (2026-11-12): conectividad completa, separaciones de cobre con más de 1 mm sobre el mínimo, los 19 componentes del PCB enlazados con el esquemático.

## 7. Prueba rápida en banco (sin MOSFET)

15 V en J2-7; J2-4 y J2-6 a la tierra de la fuente; **J2-2 (VS1) unido a J2-4** para que cargue el bootstrap; J4 a la tarjeta de control o a 5 V.

| Medir | Entre | Debe dar |
| --- | --- | --- |
| VCCI | Pin 3 a pin 4 | 15 V |
| R3 (sin alimentar) | Pin 6 a pin 4 | 49.9 kΩ (en paralelo con C4) |
| Salidas con J4 suelto | HA_OUT (J2-1) y LA_OUT (J2-5) | 0 V |
| L = 5 V | LA_OUT a GND_K | ≈ 15 V |
| H = 5 V (después de un pulso de L) | HA_OUT a VS1 | ≈ 14 V |
| H y L en 5 V a la vez | Las dos salidas | **0 V** (protección contra entradas encimadas) |
| Tiempo muerto | Osciloscopio, HA_OUT contra LA_OUT | 500–600 ns |

## 8. Pendientes

| Pendiente | Prioridad |
| --- | --- |
| DRC en KiCad, fabricar y validar en banco (sección 7) | Alta |
| Medir el tiempo muerto real y fijarlo en `DT_S` del firmware | Media |
| Lista de materiales y guía de ensamble, como en los otros módulos | Media |

## 9. Archivos

| Archivo | Contenido |
| --- | --- |
| `GATE_DRIVER_UCC21520.kicad_pro` / `.kicad_sch` / `.kicad_pcb` | Proyecto KiCad 9 |
| `img/` | Esquemático, vistas 3D y editor de PCB |

## 10. Bitácora

| Fecha | Cambio | Resultado |
| --- | --- | --- |
| 2026-11-05 | Esquemático y layout dibujados; borrador del README | R3 = 5 kΩ marcado para corregir |
| 2026-11-12 | **R3 = 49.9 kΩ 1 %** (500 ns en hardware, respaldo del firmware); **U1 = UCC21520DW** (UVLO 8 V); valores con dieléctrico y voltaje (C4 1 nF C0G); esquemático reordenado por bloques; zener → **Z1**; D1 con símbolo de diodo rápido (no Schottky); red `15V/15V-P11` → **15V**; entradas filtradas **INA_F / INB_F**; J2-3 y J2-8 sin conexión; PWR_FLAG para el ERC. Archivos de KiCad subidos | Esquemático = PCB (19/19), conectividad completa |

## 11. Referencias

- Texas Instruments, *UCC21520, UCC21520A 4-A, 6-A, 5.7-kVRMS Isolated Dual-Channel Gate Driver*, hoja de datos SLUSCJ9: ecuación del tiempo muerto, capacitor del pin DT, protección contra entradas encimadas, rango de VCCI y umbrales de entrada. <https://www.ti.com/document-viewer/lit/html/SLUSCJ9F/GUID-7AE416D5-4533-4C64-BDAA-2976FB545BFA>
- Texas Instruments, hoja de datos SLUSCJ9E: pin DT (R_DT de 2 a 500 kΩ, capacitor ≤ 1 nF, "the longer of" los dos tiempos muertos) <https://ti.com/document-viewer/lit/html/SLUSCJ9E/GUID-00E78A4B-9A68-40CF-A0B7-4C881377BD9F>; umbrales de UVLO de las versiones A y no A <https://ti.com/document-viewer/lit/html/SLUSCJ9E/GUID-0791B975-0AE7-4FCB-B881-F189646CE567>.
- TI E2E, *UCC21520 vs UCC21520A*: la única diferencia es el UVLO del lado secundario. <https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/1073301/ucc21520-vs-ucc21520a>

---

*Hardware bajo licencia CERN-OHL-S v2; documentación CC BY-SA 4.0 (ver `LICENSE.md` en la raíz).*
