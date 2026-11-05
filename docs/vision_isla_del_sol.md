# Visión: la Isla del Sol

> **Estado: borrador semilla.** Este documento guarda la idea que da sentido al proyecto, para desarrollarla más adelante. Los datos están citados; las ideas marcadas como *pendiente* son trabajo por hacer.

---

## 1. La idea en una frase

**La Isla del Sol convierte radiación electromagnética (la luz del sol) en energía útil y la conmuta 24 000 veces por segundo para entregar 127 V / 60 Hz donde no llega la red. Es eficiencia al servicio del bien común.**

Para encender un foco con un generador diésel hace falta que funcione, todos los días, una cadena enorme: exploración, extracción, refinación, transporte y distribución. Para encenderlo con una Isla del Sol hace falta que salga el sol.

---

## 2. Dos cadenas para encender el mismo foco

| Paso | Generador diésel | Isla del Sol |
| --- | --- | --- |
| Origen de la energía | Petróleo formado hace millones de años | Luz del sol, que llega cada día |
| Para obtenerla | Exploración, perforación y extracción | Un panel orientado al sol |
| Para hacerla usable | Refinación | Efecto fotovoltaico, en el panel |
| Para llevarla al lugar | Ductos, barcos, camiones y la "última milla" hasta la isla o la comunidad | Nada: llega sola |
| Para guardarla | Tanque de combustible | Batería |
| Para convertirla en electricidad | Motor de combustión y alternador | Puente H SPWM a 24 kHz y transformador de 60 Hz |
| Lo que se repite cada día | **Comprar y transportar combustible** | **Nada** (solo mantenimiento) |
| Si la cadena se rompe | Huracán, bloqueo, alza de precio o camino cerrado: **se apaga** | El sol sale igual |

La diferencia de fondo es **qué se repite**. El diésel depende de una cadena de suministro que tiene que funcionar todos los días. La Isla del Sol depende de una cadena de fabricación que se paga **una vez**, al instalar.

IRENA describe el costo de generar electricidad en zonas remotas, donde domina el diésel, como "extremadamente alto", y concluye que renovables con almacenamiento pueden bajar el costo y mejorar el acceso y la independencia energética de las islas [1].

---

## 3. De la luz al enchufe

| Etapa | Qué pasa | Dónde está en el proyecto |
| --- | --- | --- |
| 1. Radiación | Llegan ~1361 W/m² fuera de la atmósfera (constante solar) [2]; los paneles se especifican a **1000 W/m²** en superficie (condición estándar de prueba) | — |
| 2. Panel | El efecto fotovoltaico convierte la luz en corriente continua | Externo |
| 3. Batería | Guarda la energía para la noche: banco de 48 V | Externo |
| 4. Puente H | Conmuta a 24 kHz con modulación híbrida para formar una senoidal de 60 Hz | `hardware/puente-h/` |
| 5. Gate drivers | Encienden y apagan los MOSFET con el tiempo muerto correcto | `hardware/gate-drivers/` |
| 6. Protección | Mide la corriente del bus y corta por hardware antes que el fusible | `hardware/sensores/` |
| 7. Transformador | Eleva a 127 V y aísla la salida de la batería | Externo (rango 24–28 V / 120 V) |

**Por qué importa la eficiencia:** en una isla solar, cada watt que se pierde en el inversor es un watt que hubo que captar con panel y guardar en batería. Por eso el proyecto eligió la modulación híbrida (la mitad de pérdidas de conmutación y bajo consumo en vacío) y cuida el consumo de noche. Ver [Modulación SPWM](modulacion_spwm.md).

---

## 4. Bien común

- **El problema es grande:** según *Tracking SDG7 2026* (IEA, IRENA, ONU, Banco Mundial y OMS), **655 millones de personas no tenían electricidad en 2024**, y **el 86 %** de ellas vive en África subsahariana [3].
- **Hardware abierto:** los diseños se publican con licencia CERN-OHL-S, para que cualquiera los pueda fabricar, estudiar y mejorar.
- **Modo trinchera:** los módulos se pueden armar y reparar con partes de tienda local o rescatadas (TLP250, IR2110, TIP41C/TIP42C, ATmega328P a 5 V). Si una pieza falla en una isla, se repara ahí mismo.
- **Poka-yoke:** el diseño evita que un error humano cause daño (llave mecánica, P9 con falla segura, fusible que no se puentea), porque en una comunidad remota no hay técnico de guardia.
- **Documentación para estudiar:** cada decisión está justificada, para que el proyecto también forme técnicos.

---

## 5. Honestidad: lo que la Isla del Sol también cuesta

Una visión que se quiere desplegar en el mundo real tiene que ser honesta:

| Punto | Realidad |
| --- | --- |
| Los paneles también tienen cadena | Se fabrican con minería, refinación de silicio y energía. Pero esa cadena se paga **una vez**; el diésel la paga cada día. |
| Emisiones de ciclo de vida | Según el IPCC (AR5), la mediana para la fotovoltaica es de **41–48 gCO₂eq/kWh**, contra **490** del gas en ciclo combinado y **820** del carbón [4]. UNECE reporta valores parecidos o menores para la fotovoltaica moderna [4]. |
| La batería es el punto débil | Tiene vida limitada, costo alto y requiere reciclaje. El diseño del inversor la cuida: bajo consumo en vacío, protección por hardware, cargador CC/CV. |
| Días nublados | Hay que dimensionar panel y batería para la autonomía que necesite el lugar. Un generador diésel puede seguir existiendo como **respaldo**, ya no como fuente principal. |

---

## 6. Ideas para trabajar más adelante *(pendiente)*

1. **Calculadora de costo diésel contra solar por kWh** para que cada comunidad use sus propios números:
   - Diésel: costo por kWh ≈ precio del litro puesto en sitio ÷ kWh por litro. Un generador pequeño da del orden de **~3 kWh por litro** *(estimación a confirmar midiendo un generador real)*.
   - Solar: costo del sistema ÷ energía entregada en su vida útil.
2. **Emisiones del diésel por kWh** con el factor de la EPA (10.21 kg de CO₂ por galón, ≈ 2.7 kg por litro) [5]. Con ~3 kWh por litro (y 3.785 L por galón) da **del orden de 0.9 kg CO₂/kWh**, a confirmar con mediciones de campo.
3. **Kit de despliegue:** lista de lo mínimo para levantar una Isla del Sol (panel, batería, inversor, cableado, protecciones) con versiones "trinchera" y "distribuidor".
4. **Casos de campo:** documentar instalaciones reales, con consumo, autonomía, fallas y reparaciones.
5. **Manual para el usuario de trinchera:** "si tu transformador está entre 24 y 28 V / 120 V, es compatible"; "nunca puentees el fusible"; "el buck debe ser de 80 V o más".
6. **Formación:** usar la documentación del repositorio como curso de electrónica de potencia aplicada.

---

## 7. Referencias

1. IRENA, citado en *Energy-Storage.news*, "IRENA: Clear 'socio-economic benefits' to using renewables-plus-storage for islands". <https://energy-storage.news/irena-clear-socio-economic-benefits-to-using-renewables-plus-storage-for-islands-and>
2. G. Kopp, J. L. Lean, "A new, lower value of total solar irradiance: Evidence and climate significance", *Geophysical Research Letters*, 38, L01706, 2011. DOI: [10.1029/2010GL045777](https://doi.org/10.1029/2010GL045777)
3. IEA, IRENA, UNSD, Banco Mundial, OMS, *Tracking SDG7: The Energy Progress Report 2026*. Resumen en AllAfrica: <https://allafrica.com/stories/202606290024.html> · Informe anterior: IEA, "Electricity access continues to improve in 2024": <https://www.iea.org/commentaries/electricity-access-continues-to-improve-in-2024-after-first-global-setback-in-decades>
4. IPCC, *Climate Change 2014: Mitigation of Climate Change*, AR5 WGIII, Anexo III (medianas de emisiones de ciclo de vida). UNECE, *Life Cycle Assessment of Electricity Generation Options*, 2021. Compilación: <https://en.wikipedia.org/wiki/Life-cycle_greenhouse_gas_emissions_of_energy_sources>
5. U.S. EPA, *GHG Emission Factors Hub* (edición 2025): diésel (*distillate fuel oil No. 2*), 10.21 kg de CO₂ por galón. <https://www.epa.gov/climateleadership/ghg-emission-factors-hub> · Resumen de la tabla: <https://greencalculus.com/data/epa-ghg-emission-factors-hub/>

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz del repositorio).*
