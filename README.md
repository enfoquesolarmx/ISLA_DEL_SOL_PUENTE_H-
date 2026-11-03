**# Isla del Sol — Puente H SPWM 48 V → 127 V / 60 Hz

Inversor / cargador de **puente H con modulación SPWM** para sistemas solares aislados: **48 V de batería → 127 V / 60 Hz, 1000 W**, con modo cargador de **1 kVA**.

El proyecto se publica como **referencia de estudio**: cada decisión de diseño está justificada y documentada (cálculos, hojas de datos, errores encontrados y cómo se corrigieron).

> ⚠️ **Advertencia de seguridad:** este equipo maneja corrientes altas en el lado de 48 V y **127 V AC en la salida, que pueden ser mortales**. Es un prototipo en desarrollo, no un producto certificado. Úsalo bajo tu propio riesgo y solo si tienes experiencia con electrónica de potencia.

---

## Características principales

| Parámetro | Valor |
| --- | --- |
| Entrada | Batería de 48 V |
| Salida | 127 V / 60 Hz, 1000 W (transformador 1 : 4.16) |
| Modo inversor | Modulación híbrida: rama A a 24 kHz, rama B a 60 Hz |
| Modo cargador | Unipolar, ambas ramas a 24 kHz, 1 kVA |
| MOSFET | Infineon IRF100P219 (100 V), disipador Fischer SK 129 |
| Capacitores de bus | 4 × Rubycon 100ZLH820MEFC18X40 |
| Filtro de salida | ~2 mH (dispersión + inductor de ferrita E65 N87) + 4.7 µF MKP X2 |

---

## Gate drivers universales

Los módulos de gate driver son **intercambiables**: usan los mismos conectores (J1 hacia la tarjeta de control, J5 hacia el puente H) y **el mismo firmware**.

| Módulo | Tecnología | Tiempo muerto | Para qué | Estado |
| --- | --- | --- | --- | --- |
| IRS2186 | Driver de bootstrap | 500 ns | Producción, bajo costo | **Diseñado** (P9 en J1) |
| UCC21520ADWR | Driver aislado | 500 ns | Mejor desempeño | Diseñado |
| **TLP250 + tótem TIP41C/TIP42C** | Optoacoplador DIP + transistores TO-220 | **1 µs** | **Modo trinchera:** zonas sin acceso a componentes modernos; reparable con partes de tienda local | **V.0.1 diseñado** |
| **IR2110 + tótem TIP41C/TIP42C** | Driver de bootstrap legacy (DIP-14) + transistores TO-220 | **1 µs** | **Modo trinchera:** el driver de medio puente más fácil de conseguir o rescatar; reparable con partes de tienda local; de 1 a 5 MOSFET por posición | **Próximamente** |

### Identificación automática del tiempo muerto (P9)

Cada módulo **declara** qué tiempo muerto necesita por la terminal **P9** de J1:

| P9 | Tiempo muerto |
| --- | --- |
| A GND | 500 ns (IRS2186, UCC21520) |
| Abierto o en falla | **1 µs** (TLP250, o cualquier duda) |

El modo seguro es el valor por defecto: un módulo legacy, un cable suelto o una soldadura fría nunca llevan a un tiempo muerto insuficiente.

### Diseño poka-yoke

El sistema está pensado para que **el error humano no pueda causar el daño**:

- Llave mecánica en la posición 8 de J5: el módulo solo entra en una posición.
- P9 abierto = 1 µs: falla segura.
- Serigrafía que dice qué hace cada módulo ("OPERACION DT TLP250=1US").
- Un solo firmware para los tres módulos: no hay versión equivocada que cargar.

---

## Contenido del repositorio

```
ISLA_DEL_SOL_PUENTE_H-/
├── README.md                                   ← este archivo
├── LICENSE.md                                  ← licencias (hardware, documentación, firmware)
├── .gitignore
├── docs/
│   ├── identificacion_dt_pin9.md
│   └── compensacion_adaptativa_tiempo_muerto.md
└── hardware/
    └── gate-drivers/
        ├── irs2186/
        │   ├── README.md
        │   ├── GATE_DRIVER_IRS2186.kicad_pro
        │   ├── GATE_DRIVER_IRS2186.kicad_sch
        │   └── GATE_DRIVER_IRS2186.kicad_pcb
        └── tlp250-trinchera/
            ├── README.md
            ├── GATE_TLP_250_TOTEM_TIP.kicad_pro
            ├── GATE_TLP_250_TOTEM_TIP.kicad_sch
            └── GATE_TLP_250_TOTEM_TIP.kicad_pcb
```

Próximamente: `hardware/gate-drivers/ucc21520/`, `hardware/gate-drivers/ir2110-trinchera/`, `hardware/puente-h/` (tarjeta de potencia) y `firmware/`.

### Documentos de sistema (`docs/`)

| Documento | Qué explica | Útil para |
| --- | --- | --- |
| [Identificación del tiempo muerto por P9](docs/identificacion_dt_pin9.md) | Cómo cada módulo declara su tiempo muerto (P9 a GND = 500 ns, abierto = 1 µs); soporte legacy para trincheras; costo contra beneficio; filosofía poka-yoke; llave mecánica en la posición 8; análisis de fallas; código de referencia para ATmega328 y ESP32; validación en banco | Entender la interfaz universal y escribir el firmware |
| [Compensación adaptativa de tiempo muerto](docs/compensacion_adaptativa_tiempo_muerto.md) | Por qué el tiempo muerto genera armónicos impares; compensación con el signo de la corriente; medición del 3er armónico con Goertzel; ajuste automático; hoja de ruta en 5 etapas | Etapa posterior: mejorar la calidad de la onda |

### Módulos gate driver (`hardware/gate-drivers/`)

Todos son **gate drivers de medio puente H** (una rama: MOSFET alto + MOSFET bajo). El puente completo usa dos módulos iguales, uno por rama.

| Módulo | Documento | Archivos KiCad | Qué incluye |
| --- | --- | --- | --- |
| **IRS2186** | [README](hardware/gate-drivers/irs2186/README.md) | ✓ Esquemático, PCB y proyecto | Interfaz con P9 a GND; tierras compartidas (lógica = potencia), unión única GND_K–GND con *net tie* y cómo controlar el lazo de tierra; bootstrap con R antes del diodo y referencias; cálculo de los 500 ns; **alcance por cantidad de MOSFET** (1 IRF100P219 o hasta 2 IRF540N); opciones con tótem; lista de materiales; validación |
| **TLP250 + tótem TIP41C/TIP42C** (trinchera) | [README](hardware/gate-drivers/tlp250-trinchera/README.md) | ✓ Esquemático, PCB y proyecto | Propósito educativo y de supervivencia; entrada aislada; por qué 1 µs; tótem y regla de 250 nC; **evaluación de 9 transistores** (por qué TIP41/42 y no BD, Darlington, IRF540 o BC); layout; BOM; ensamble con RTV neutro; validación en 10 pasos; **registro de errores corregidos durante el diseño** |
| UCC21520ADWR | Próximamente | Próximamente | — |
| **IR2110 + tótem TIP41C/TIP42C** (trinchera) | Próximamente | Próximamente | Ver adelanto abajo |

### Próximo módulo de trinchera: IR2110 + tótem TIP41C/TIP42C (adelanto de diseño)

Igual que el módulo TLP250, este módulo existe para **cuando los drivers modernos no llegan**. El IR2110 es un driver de medio puente **legacy** (International Rectifier, hoy Infineon) que se encuentra en casi cualquier tienda de electrónica y se rescata de variadores, soldadoras inverter y UPS viejos. Viene en **DIP-14**: se monta en zócalo y se cambia sin cautín. Todo el módulo se puede armar con **partes de tienda local**: IR2110, TIP41C/TIP42C, 78L05, diodos, resistencias y capacitores comunes.

| | TLP250 + tótem (trinchera) | IR2110 + tótem (trinchera) |
| --- | --- | --- |
| Driver | Optoacoplador TLP250H (DIP-8), uno por canal | IR2110 (DIP-14), un chip para los dos canales |
| Aislamiento de la entrada | **Sí** (óptico) | Parcial: VSS y COM separados hasta ±5 V |
| Diferencia de retardo entre canales | Hasta **400 ns** (peor caso) | **10–20 ns** |
| Margen dentro de 1 µs | ~150–250 ns con 2 IRF100P219 | **Mucho mayor:** casi toda la ventana queda para el apagado |
| Tótem | TIP41C/TIP42C (D44H11/D45H11 de mejora) | TIP41C/TIP42C (D44H11/D45H11 de mejora), mismo footprint TO-220 B-C-E |

| Punto | Decisión preliminar | Por qué |
| --- | --- | --- |
| Corriente de salida | 2 A / 2 A → **tótem por canal: TIP41C/TIP42C** de trinchera; **D44H11/D45H11** como mejora en el mismo footprint | Con 2 A, un solo IRF100P219 ya no apaga en 500 ns (~590 ns). El TIP es lento (f_T 3 MHz), pero con la poca diferencia entre canales del IR2110 sobra margen en 1 µs |
| Tiempo muerto | **P9 abierto → 1 µs** | Módulo de trinchera: el tiempo muerto no debe depender de qué transistor se monte |
| Alimentación lógica | **VDD = 5 V con un 78L05** en el módulo, desde los 15 V | J1 no trae VDD, y los umbrales de entrada del IR2110 suben con VDD (a VDD = 15 V el "1" pide ≥ 9.5 V). Con MCU de 3.3 V, verificar la gráfica de umbrales |
| Tierras | **VSS → GND de J1**, **COM → GND_K** | El IR2110 tolera ±5 V entre VSS y COM: aleja el rebote de tierra de las entradas (mejor que el IRS2186, aunque no es aislamiento completo como el TLP250) |
| Pin SD | **A VSS** | Si queda flotando o con ruido, apaga las salidas. A futuro: apagado de emergencia por sobrecorriente |
| Alcance | **1 a 5 MOSFET por posición** (por ejemplo 5 × IRF540N rescatados) | Picos de ~9–12 A en el tótem (TIP41C: 10 A pico; usar 6.8 Ω por MOSFET para quedar en ~9 A); apagado de ~350–450 ns con TIP, ~150–300 ns con D44H11 |
| Bootstrap con 3 o más MOSFET | **4 × 10 µF** y resistencias de compuerta a fuente de **22 kΩ** en la tarjeta del puente H | Con 5 × 10 kΩ en paralelo, la rama de 60 Hz caería bajo el UVLO del IR2110 en los 8.3 ms |
| Ensamble | TIP acostados, aletas separadas, **silicón RTV neutro** al final | Mismas reglas que el módulo TLP250 |
| Serigrafía | "OPERACION DT IR2110=1US", "TRENZAR H Y L CON GND", "B C E", "X" en J5-8 | Poka-yoke visible, igual que los otros módulos |

Datos de la hoja de datos (Infineon IR2110E4): retardo 120/94 ns típ. (150/125 ns máx.), diferencia entre canales 10–20 ns, VCC 10–20 V, UVLO ~8.2–8.7 V típ., consumo del lado alto ≤ 230 µA.

### Por dónde empezar a estudiar

1. **Este README:** visión general del inversor y de la interfaz universal.
2. **[Identificación por P9](docs/identificacion_dt_pin9.md):** el "contrato" que cumplen todos los módulos.
3. **[Módulo IRS2186](hardware/gate-drivers/irs2186/README.md):** el driver de bootstrap clásico, la base para entender los demás.
4. **[Módulo TLP250](hardware/gate-drivers/tlp250-trinchera/README.md):** cómo resolver lo mismo con partes de tienda local, y los errores típicos al diseñar un tótem.
5. **[Compensación de tiempo muerto](docs/compensacion_adaptativa_tiempo_muerto.md):** el siguiente paso, cuando el hardware ya esté validado.

## Herramientas

- **KiCad** (esquemáticos y PCB).
- Firmware: ATmega328 para el prototipo del inversor; STM32 o ESP32 (MCPWM) en etapas posteriores.

## Estado

| Etapa | Estado |
| --- | --- |
| Diseño de la tarjeta del puente H | En curso |
| Módulo UCC21520 | Diseñado; pendiente llevar P9 a J1 |
| Módulo TLP250 de trinchera | V.0.1 diseñado; pendiente fabricar y validar |
| Módulo IRS2186 | Diseñado con P9 en J1; pendiente fabricar y validar |
| Módulo IR2110 + tótem TIP41C/TIP42C (trinchera) | Próximamente (definición preliminar lista) |
| Firmware | Pendiente |
| Validación en banco | Pendiente |

## Licencia

- **Hardware** (archivos de KiCad, Gerbers): CERN-OHL-S v2
- **Documentación**: CC BY-SA 4.0
- **Firmware**: MIT

Ver [`LICENSE.md`](LICENSE.md).

Las hojas de datos de los fabricantes no se incluyen en el repositorio por derechos de autor; los documentos citan la fuente de cada dato.
**
