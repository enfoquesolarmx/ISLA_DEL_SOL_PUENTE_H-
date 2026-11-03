# Isla del Sol — Puente H SPWM 48 V → 127 V / 60 Hz

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
| IRS2186 | Driver de bootstrap | 500 ns | Producción, bajo costo | En diseño |
| UCC21520ADWR | Driver aislado | 500 ns | Mejor desempeño | Diseñado |
| **TLP250 + tótem TIP41C/TIP42C** | Optoacoplador DIP + transistores TO-220 | **1 µs** | **Modo trinchera:** zonas sin acceso a componentes modernos; reparable con partes de tienda local | **V.0.1 diseñado** |

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

## Estructura del repositorio

```
isla_del_sol_puente_h/
├── README.md
├── LICENSE.md
├── docs/                      ← documentos de diseño y justificación
├── hardware/
│   ├── puente-h/              ← tarjeta de potencia (KiCad)
│   └── gate-drivers/
│       ├── irs2186/
│       ├── ucc21520/
│       └── tlp250-trinchera/  ← módulo TLP250 + tótem TIP41C/TIP42C
└── firmware/
```

## Documentación

| Documento | Contenido |
| --- | --- |
| [`docs/identificacion_dt_pin9.md`](docs/identificacion_dt_pin9.md) | Identificación del tiempo muerto por P9, soporte legacy, poka-yoke, análisis de fallas y código de referencia |
| [`docs/compensacion_adaptativa_tiempo_muerto.md`](docs/compensacion_adaptativa_tiempo_muerto.md) | Compensación de tiempo muerto (etapa posterior): signo de corriente, Goertzel del 3er armónico, ajuste adaptativo |
| [`hardware/gate-drivers/tlp250-trinchera/README.md`](hardware/gate-drivers/tlp250-trinchera/README.md) | Módulo TLP250 de trinchera: cálculos, evaluación de transistores, layout, BOM, ensamble, validación y registro de errores |

## Herramientas

- **KiCad** (esquemáticos y PCB).
- Firmware: ATmega328 para el prototipo del inversor; STM32 o ESP32 (MCPWM) en etapas posteriores.

## Estado

| Etapa | Estado |
| --- | --- |
| Diseño de la tarjeta del puente H | En curso |
| Módulo UCC21520 | Diseñado |
| Módulo TLP250 de trinchera | V.0.1 diseñado; pendiente fabricar y validar |
| Módulo IRS2186 | Pendiente: llevar P9 a J1 |
| Firmware | Pendiente |
| Validación en banco | Pendiente |

## Licencia

- **Hardware** (archivos de KiCad, Gerbers): CERN-OHL-S v2
- **Documentación**: CC BY-SA 4.0
- **Firmware**: MIT

Ver [`LICENSE.md`](LICENSE.md).

Las hojas de datos de los fabricantes no se incluyen en el repositorio por derechos de autor; los documentos citan la fuente de cada dato.
