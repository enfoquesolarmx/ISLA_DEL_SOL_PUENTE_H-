# Visión: la Isla del Sol

> **Estado: borrador semilla.** Este documento guarda la idea que da sentido al proyecto, para desarrollarla más adelante. Los datos están citados; las ideas marcadas como *pendiente* son trabajo por hacer.

---

## 1. La idea en una frase

> **Energía del sol, conocimiento de la comunidad.**

Un inversor abierto que se construye, se entiende y se repara en la trinchera, porque la soberanía energética empieza ahí.

### Por qué hace falta una alternativa

Las cadenas de suministro de hidrocarburos son frágiles, y no solo en las islas:

- En 2022, la Agencia Internacional de Energía describió la situación como **"la primera crisis energética global"**, con precios del gas natural en el mercado de contado **nunca vistos**, después de los recortes de suministro de gas a Europa [7].
- Por el estrecho de Ormuz pasan **~20 millones de barriles de petróleo al día**, cerca de **una quinta parte** del consumo mundial de líquidos de petróleo, y también **una quinta parte** del comercio mundial de gas natural licuado (EIA, datos de 2024) [8]. Un solo punto de paso puede mover el precio del combustible en todo el planeta.
- Al final de esa cadena están las comunidades más alejadas. Ahí el combustible ya llega caro por la distancia, y un alza o una interrupción se siente primero (sección 2).

Esa fragilidad obliga a replantear y a esforzarse en construir una **alternativa para el mundo**: una fuente de energía cuya "materia prima" llega sola cada día y cuyo equipo se puede reparar donde se usa.

### Una plataforma abierta

La Isla del Sol no es un producto cerrado sino una **plataforma abierta**:
- **Hardware** con licencia CERN-OHL-S, **documentación** con CC BY-SA y **firmware** con MIT: cualquiera puede fabricarlo, estudiarlo, adaptarlo y mejorarlo, con la condición de compartir las mejoras.
- **Módulos intercambiables** con la misma interfaz (gate drivers, sensor de corriente, buck), para que cada región use las piezas que tiene.
- **Herramientas abiertas** (como la auditoría del layout) para que otros diseñadores comprueben sus propias tarjetas.

---

## 2. Dos cadenas para encender el mismo foco

Para encender un foco con un generador diésel hace falta que funcione, todos los días, una cadena enorme: exploración, extracción, refinación, transporte y distribución. Para encenderlo con una Isla del Sol hace falta que salga el sol.

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

### 4.1 Llevar la energía a donde está la gente, no la gente a donde está la energía

La gente tiende a concentrarse cerca de donde hay energía: la red eléctrica, el combustible y los servicios llegan primero a las ciudades y a sus periferias, y quien quiere trabajar con una herramienta, un refrigerador o una bomba termina mudándose hacia allá. Una plataforma abierta y reparable **invierte esa lógica**: lleva la energía al lugar donde vive la gente, para que pueda tener una **actividad productiva sin dejar su territorio** y sin terminar en la orilla de una ciudad.

**Lo que dice la evidencia (y sus matices):**

| Estudio | Hallazgo | Lo que nos enseña |
| --- | --- | --- |
| Dinkelman, *American Economic Review*, 2011: electrificación rural en Sudáfrica [9] | En cinco años, la electrificación **aumentó significativamente el empleo femenino** y las horas de trabajo. Al parecer liberó tiempo del trabajo doméstico y apoyó microempresas; también pudo influir en la migración. | La energía puede abrir trabajo productivo donde vive la gente |
| Lee, Miguel y Wolfram, *Journal of Political Economy*, 2020: experimento en Kenia rural [10] | Los hogares recién conectados consumieron **muy poca electricidad**, y no se encontraron efectos económicos importantes a mediano plazo | **Conectar no basta:** sin usos productivos, la energía casi no cambia la economía |
| ESMAP / Banco Mundial, *Accelerating the Productive Use of Electricity* [11] | El acceso impulsa el desarrollo rural **solo si se usa de forma productiva**: microempresas, pequeños productores, agricultura | Hay que diseñar para cargas productivas, no solo para focos |

**Consecuencia para el diseño:** la Isla del Sol tiene que servir para **trabajar**, no solo para alumbrar. Por eso apunta a **1000 W de inversor y 1 kVA de cargador**, para mover herramientas, bombas y refrigeración. Y por eso la documentación busca formar técnicos locales: el conocimiento para mantener el equipo también es una actividad productiva que se queda en la comunidad.

*Que la energía abierta ayude a que la gente no tenga que concentrarse en las periferias es una **hipótesis del proyecto**. La evidencia indica que puede ocurrir cuando hay usos productivos, pero hay que **medirla en el campo** (sección 7).*

---

## 5. La soberanía se construye en las trincheras

Un equipo que solo se puede reparar con piezas de un fabricante, con un software cerrado o con un técnico que viene de lejos **no le pertenece del todo a quien lo usa**. La Isla del Sol busca lo contrario: que la comunidad pueda **entender, construir, reparar y mejorar** su propia fuente de energía con lo que tiene a la mano.

La literatura llama a esto **soberanía energética**. Timmermann y Noboa (2022) la describen como un concepto basado en valores, inspirado en el movimiento de soberanía alimentaria, e identifican ocho: accesibilidad, empoderamiento y reconocimiento, cuidado y sostenibilidad, autosuficiencia, resiliencia, paz, transparencia y autodeterminación, y justicia de género [6]. Así se reflejan en el diseño:

| Valor | Cómo se construye en este proyecto |
| --- | --- |
| **Accesibilidad** | Hardware abierto (CERN-OHL-S) y piezas de bajo costo; la documentación está en español |
| **Empoderamiento** | Cada decisión está explicada y la documentación funciona como curso de electrónica de potencia; las herramientas de auditoría permiten que cualquiera revise su propia tarjeta |
| **Sostenibilidad** | Generación solar descentralizada, sin combustible que transportar cada día |
| **Autosuficiencia** | **Modo trinchera:** módulos con TLP250, IR2110 y TIP41C/TIP42C, partes de tienda local o rescatadas, cobre de 1 oz, ATmega328P a 5 V |
| **Resiliencia** | El sistema sigue funcionando si se rompe la cadena de suministro: se repara con lo que hay; los fusibles se improvisan, pero nunca se puentean |
| **Transparencia** | Código, esquemáticos, cálculos y errores corregidos están publicados; la auditoría muestra sus supuestos y su margen de error |

### 5.1 Soberanía no es cero dependencia, es poder sustituir

Los paneles, las baterías y los MOSFET todavía se fabrican lejos. La soberanía no consiste en fingir lo contrario, sino en **diseñar para que ninguna pieza sea irremplazable**:

| Dónde | Qué se puede sustituir |
| --- | --- |
| Gate driver | IRS2186, UCC21520, TLP250 o IR2110: **misma interfaz y mismo firmware**; P9 elige el tiempo muerto correcto sola |
| Transformador | Cualquiera entre **24 y 28 V / 120 V** |
| Buck de 15 V | "XL7015 **o equiparable**", con entrada ≥ 80 V |
| Microcontrolador | ATmega328P (ecosistema Arduino), con soporte para una década |
| Compuerta de corte | 74HC08 o 74HCT08, la que se consiga |
| Fusible | T1A, o una resistencia de 2.2 Ω ½ W como improvisación de trinchera |
| Refuerzo de cobre | Alambre de cable de instalación eléctrica, el que hay en cualquier casa |

**La regla de diseño:** si una pieza falla en una isla o en una comunidad remota, **tiene que existir al menos un sustituto que se consiga ahí mismo**, y la documentación tiene que decir cuál es.

#### Segunda vida para los recursos y contra la obsolescencia programada

Poder sustituir también significa tener **alternativas para darle una segunda vida** a lo que ya existe en la región, en lugar de tirarlo:

| Recurso que suele terminar en la basura | Segunda vida en una Isla del Sol |
| --- | --- |
| Transformadores de UPS e inversores viejos | Si su lado de baja está entre 24 y 28 V para 120 V, son compatibles (ver el README del puente H) |
| Circuitos integrados y transistores de equipos dañados | IR2110, TLP250, TIP41C/TIP42C rescatados alimentan los módulos de trinchera |
| Disipadores, borneras, cable de instalación | Disipadores para los MOSFET, alambre de cobre para reforzar el layout |
| Un módulo que falla | Se cambia **solo ese módulo**, no el inversor completo |
| Baterías de segunda vida | Bancos de 48 V con su protección (BMS), en una etapa posterior |

Y el diseño evita, a propósito, lo que vuelve obsoleto a un equipo antes de tiempo:
- **Firmware abierto**, sin bloqueos ni "apagado a distancia": el equipo no depende de un servidor ni de una actualización del fabricante.
- **Interfaces documentadas** (J1/J5 con P9, el sensor y el buck), para que una pieza nueva sirva en una tarjeta vieja y al revés.
- **Componentes estándar** en encapsulados que se sueldan a mano (THT, 1206, 1210).
- **Explicación de cada valor**, para que un técnico pueda sustituir una pieza descontinuada por otra equivalente.

**La escala del problema:** en 2022 el mundo generó un récord de **62 millones de toneladas de residuos electrónicos**, y solo el **22.3 %** se recolectó y recicló de forma documentada. Para 2030 se esperan ~82 millones de toneladas [12]. Por su parte, la Unión Europea aprobó en 2024 una directiva que promueve la reparación de bienes, conocida como "derecho a reparar" [13]. La Isla del Sol va en la misma dirección desde el diseño: **lo que se puede reparar no se tira.**

### 5.2 Formar técnicos es parte del hardware

Un equipo soberano necesita personas que lo entiendan. Por eso el repositorio no solo publica archivos: explica **por qué** cada valor, documenta los errores que se cometieron y cómo se corrigieron, y deja herramientas para comprobar el diseño antes de fabricar. Cada técnico que aprende con estos documentos es una trinchera más.

### 5.3 Donde parte todo

> *No vale lo mismo un inversor que llega de lejos sin refacciones ni información para repararlo, que un inversor que nace en el sitio, en la trinchera, donde los técnicos tienen acceso a la información para reparar cada módulo. Creo que ahí parte todo.*
> — Idea fundadora del proyecto

Cuando un inversor cerrado falla, la comunidad **vuelve a quedar a oscuras** y depende de que alguien de afuera lo reemplace. No es un problema de dónde se fabricó, sino de que **no se puede entender ni reparar**. Un inversor que nace en el sitio, con su documentación abierta, piezas que se consiguen ahí y técnicos que saben por qué cada valor es el que es, **se repara y sigue funcionando**.

Por eso este proyecto entiende la soberanía energética como algo que se construye con **cariño y transferencia de conocimiento**: cada documento explicado, cada error compartido y cada técnico formado dejan en la región una capacidad que ya no depende de nadie más. Esto coincide con el valor de **empoderamiento y reconocimiento** que describen Timmermann y Noboa, que incluye desarrollar la capacidad de las personas para participar en las decisiones sobre su propia energía [6]; la idea del cariño como motor es del proyecto.

---

## 6. Honestidad: lo que la Isla del Sol también cuesta

Una visión que se quiere desplegar en el mundo real tiene que ser honesta:

| Punto | Realidad |
| --- | --- |
| Los paneles también tienen cadena | Se fabrican con minería, refinación de silicio y energía. Pero esa cadena se paga **una vez**; el diésel la paga cada día. |
| Emisiones de ciclo de vida | Según el IPCC (AR5), la mediana para la fotovoltaica es de **41–48 gCO₂eq/kWh**, contra **490** del gas en ciclo combinado y **820** del carbón [4]. UNECE reporta valores parecidos o menores para la fotovoltaica moderna [4]. |
| La batería es el punto débil | Tiene vida limitada, costo alto y requiere reciclaje. El diseño del inversor la cuida: bajo consumo en vacío, protección por hardware, cargador CC/CV. |
| Días nublados | Hay que dimensionar panel y batería para la autonomía que necesite el lugar. Un generador diésel puede seguir existiendo como **respaldo**, ya no como fuente principal. |

---

## 7. Ideas para trabajar más adelante *(pendiente)*

1. **Calculadora de costo diésel contra solar por kWh** para que cada comunidad use sus propios números:
   - Diésel: costo por kWh ≈ precio del litro puesto en sitio ÷ kWh por litro. Un generador pequeño da del orden de **~3 kWh por litro** *(estimación a confirmar midiendo un generador real)*.
   - Solar: costo del sistema ÷ energía entregada en su vida útil.
2. **Emisiones del diésel por kWh** con el factor de la EPA (10.21 kg de CO₂ por galón, ≈ 2.7 kg por litro) [5]. Con ~3 kWh por litro (y 3.785 L por galón) da **del orden de 0.9 kg CO₂/kWh**, a confirmar con mediciones de campo.
3. **Kit de despliegue:** lista de lo mínimo para levantar una Isla del Sol (panel, batería, inversor, cableado, protecciones) con versiones "trinchera" y "distribuidor".
4. **Casos de campo:** documentar instalaciones reales, con consumo, autonomía, fallas y reparaciones.
5. **Manual para el usuario de trinchera:** "si tu transformador está entre 24 y 28 V / 120 V, es compatible"; "nunca puentees el fusible"; "el buck debe ser de 80 V o más".
6. **Formación:** usar la documentación del repositorio como curso de electrónica de potencia aplicada.
7. **Uso productivo:** en cada instalación, registrar qué actividades productivas habilita (herramientas, bombeo, refrigeración, talleres) y si cambia la decisión de las familias de mudarse, para poner a prueba la hipótesis de la sección 4.1.

---

## 8. Referencias

1. IRENA, citado en *Energy-Storage.news*, "IRENA: Clear 'socio-economic benefits' to using renewables-plus-storage for islands". <https://energy-storage.news/irena-clear-socio-economic-benefits-to-using-renewables-plus-storage-for-islands-and>
2. G. Kopp, J. L. Lean, "A new, lower value of total solar irradiance: Evidence and climate significance", *Geophysical Research Letters*, 38, L01706, 2011. DOI: [10.1029/2010GL045777](https://doi.org/10.1029/2010GL045777)
3. IEA, IRENA, UNSD, Banco Mundial, OMS, *Tracking SDG7: The Energy Progress Report 2026*. Resumen en AllAfrica: <https://allafrica.com/stories/202606290024.html> · Informe anterior: IEA, "Electricity access continues to improve in 2024": <https://www.iea.org/commentaries/electricity-access-continues-to-improve-in-2024-after-first-global-setback-in-decades>
4. IPCC, *Climate Change 2014: Mitigation of Climate Change*, AR5 WGIII, Anexo III (medianas de emisiones de ciclo de vida). UNECE, *Life Cycle Assessment of Electricity Generation Options*, 2021. Compilación: <https://en.wikipedia.org/wiki/Life-cycle_greenhouse_gas_emissions_of_energy_sources>
5. U.S. EPA, *GHG Emission Factors Hub* (edición 2025): diésel (*distillate fuel oil No. 2*), 10.21 kg de CO₂ por galón. <https://www.epa.gov/climateleadership/ghg-emission-factors-hub> · Resumen de la tabla: <https://greencalculus.com/data/epa-ghg-emission-factors-hub/>
6. C. Timmermann, E. Noboa, "Energy Sovereignty: A Values-Based Conceptual Analysis", *Science and Engineering Ethics*, vol. 28, art. 54, 2022. <https://pmc.ncbi.nlm.nih.gov/articles/PMC9633473>
7. IEA, *World Energy Outlook 2022*, resumen ejecutivo: "The world is in the midst of its first global energy crisis". <https://iea.blob.core.windows.net/assets/fe7c251b-8651-4d3a-8362-0ffe3e50d37b/Executivesummary_WorldEnergyOutlook2022.pdf>
8. U.S. EIA, "Amid regional conflict, the Strait of Hormuz remains critical oil chokepoint", *Today in Energy*, 16 de junio de 2025. <https://www.eia.gov/todayinenergy/detail.php?id=65504>
9. T. Dinkelman, "The Effects of Rural Electrification on Employment: New Evidence from South Africa", *American Economic Review*, 101(7): 3078–3108, 2011. <https://econpapers.repec.org/RePEc:aea:aecrev:v:101:y:2011:i:7:p:3078-3108>
10. K. Lee, E. Miguel, C. Wolfram, "Experimental Evidence on the Economics of Rural Electrification", *Journal of Political Economy*, 128(4), 2020. <https://epic.uchicago.edu/research/experimental-evidence-on-the-economics-of-rural-electrification/>
11. ESMAP / Banco Mundial, *Accelerating the Productive Use of Electricity: Enabling Energy Access to Power Rural Economic Growth*. <https://www.esmap.org/publications/accelerating-productive-use-electricity-enabling-energy-access-power-rural-economic>
12. UNITAR / UIT, *Global E-waste Monitor 2024*. <https://www.unitar.org/about/news-stories/press/global-e-waste-monitor-2024-electronic-waste-rising-five-times-faster-documented-e-waste-recycling>
13. Directiva (UE) 2024/1799 del Parlamento Europeo y del Consejo, de 13 de junio de 2024, sobre normas comunes para promover la reparación de bienes. <https://eur-lex.europa.eu/eli/dir/2024/1799/2026-07-31/eng>

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz del repositorio).*
