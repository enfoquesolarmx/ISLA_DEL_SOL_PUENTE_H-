# Protocolo de banco: tarjeta de control y módulo ADC AC

**Proyecto:** Isla del Sol — Puente H SPWM (inversor 48 V → 127 V / 60 Hz, 1000 W)
**Documento:** protocolo de pruebas en protoboard, previo a mandar imprimir el PCB THT
**Estado:** escrito, sin ejecutar
**Fecha:** 2026-11-16

> **En resumen: un viaje, todos los pendientes.**

---

## Para qué existe este documento

El viaje al banco cuesta gasolina y materiales. Lo que no se mida en esa corrida se queda pendiente otro mes, y lo que se olvide anotar hay que volver a medirlo.

Este documento junta **en un solo orden** todo lo que el proyecto tiene pendiente de medir, repartido hoy en cinco READMEs distintos, con el criterio de aceptación de cada punto. Se usa así:

- Cada corrida trae **objetivo**, **montaje**, **procedimiento**, **criterio de aceptación** y **qué desbloquea**.
- **Se anota el resultado aunque salga bien.** Un resultado sin anotar es un resultado perdido: el documento no sirve para presumir, sirve para que el siguiente que lo lea no repita la medición.
- Las corridas están en orden de dependencia. La **parte A** es la que desbloquea el PCB; la **parte B** son los pendientes acumulados que ya no bloquean nada pero que conviene cerrar en el mismo viaje.
- Cada corrida dice si el firmware **ya existe** o **falta escribirlo**. Las que faltan hay que escribirlas *antes* de salir, no en el banco.
- **Las tres corridas del ADC (A6, A7, A8) corren con un solo sketch**, [`ISLA_CRITICO_PRUEBA_ADC_AC`](../firmware/critico/ISLA_CRITICO_PRUEBA_ADC_AC/ISLA_CRITICO_PRUEBA_ADC_AC.ino), y su aritmética ya está validada contra una senoidal sintética en la PC.

---

## Lo que ya hay en el banco

| Equipo | Para qué |
| --- | --- |
| 2 × Arduino Nano (ATmega328P, clon) | CRÍTICO y AUX |
| Analizador lógico | Tiempos: tiempo muerto, periodo, disparo del ADC, duración de la ISR |
| Multímetro | Voltajes DC y AC de referencia; patrón para calibrar el ADC |
| Batería 16S LiFePO4 (56 V al 100 % de SOC) | Punto exacto para B2 |
| Fuente ajustable, **si la hay** | Recorrer los umbrales de batería en B2 |

---

## Lista de compras

Derivada de las corridas de abajo, no de un catálogo. Si algo ya está en el cajón, se tacha.

| Cant. | Pieza | Para qué | Nota |
| --- | --- | --- | --- |
| 1 | Transformador 120 → 12 VAC, ~500 mA, **con tap central (3 hilos)** | A2 a A8 | **La pieza que manda.** Sin él no hay parte A. Se usa el devanado completo; el tap queda como contingencia (ver A2) |
| 2 | Resistencia 100 kΩ ¼ W | Divisor AC, rama de arriba | El valor final lo decide **A3**; lleva también 2 × 120 kΩ y 2 × 150 kΩ por si sale alto |
| 2 | Resistencia 8.2 kΩ | Divisor AC, rama de abajo (parte fija) | |
| 2 | Trimmer 2 kΩ multivuelta (Bourns 3296W o 3362P) | Ajuste del divisor AC | Se conecta **como reóstato**: cursor unido a un extremo (DC-006). **No es artículo de Steren** — es de AG Electrónica, tiendas de electrónica o MercadoLibre |
| 1 | *Sustituto, si no hay multivuelta:* potenciómetro de una vuelta de 10 kΩ sin switch (Steren 101-10K, ~$9) | Herramienta de banco para A4, A5 y A6 | ⚠️ **No se suelda en la tarjeta**: en serie con los 8.2 kΩ se pasa del riel. Ver la nota al final de A4 |
| 6 | Resistencia 10 kΩ | Bias de 2.5 V (2 por canal) + P9 | |
| 2 | Capacitor electrolítico 10 µF | Bias de 2.5 V, valor de DC-006 | |
| 2 | **Capacitor electrolítico 100 µF** | Bias de 2.5 V, **la alternativa buena** | Ver "Alternativas del bias" abajo |
| 6 | Capacitor cerámico 100 nF | Bias y desacoplo | |
| 2 | Capacitor cerámico 10 nF | Filtro pegado al pin (A0 y A6) | |
| 2 | Resistencia 68 kΩ | Divisor de batería (DC-007) | |
| 1 | Resistencia 20 kΩ + 1 × 10 kΩ | P9, los dos casos de tiempo muerto | |
| 2 | Resistencia 1 kΩ | Serie al pin del ADC | |
| 1 | Capacitor 1 µF | Divisor de batería | |
| 2 | Protoboard | Una para la cadena AC, otra para el resto | |
| 1 | Juego de alambre puente | | |
| 1 | Módulo LCD 16×2 con PCF8574 | B4 (`LCD_TIPO 0`) | Si no está en el cajón |
| 4 | Botón push + resistencias de la escalera | B4, botonera física en A7 | Valores en `botonera.cpp` |

### Repuestos: los 5 USD que evitan los 50 USD de volver

El viaje cuesta más que los materiales. Todo lo de esta tabla se compra **estando ahí**, aunque probablemente no se use, porque la alternativa es otra vuelta completa.

| Cant. | Pieza | Qué falla previene | Prioridad |
| --- | --- | --- | --- |
| 1 | **Segundo transformador 120 → 12 VAC, mismo modelo y mismo lote** | **A5 no se puede contestar con uno solo.** La pregunta es si ±10 % de trimmer cubre la dispersión entre unidades, y eso exige dos unidades. **Verificar en la tienda que los dos traigan tap central:** uno con tap y otro sin tap obliga al devanado completo y cierra la contingencia de A2 | **Alta** |
| 1 | **Nano de repuesto** | Un Nano muerto termina la corrida en seco, con todo armado | **Alta** |
| 2 c/u | Resistencias 110 kΩ, 120 kΩ, 150 kΩ | A3 puede pedir cualquiera de ellas y no se sabe cuál hasta medir A2 | **Alta** |
| 2 | Electrolítico 220 µF | Si 100 µF no alcanza para el bias | Media |
| 2 | Trimmer 5 kΩ multivuelta | Si el rango de ±10 % del de 2 kΩ no cubre la dispersión (A5) | Media |
| 1 | LM358 (DIP-8) + socket | Último recurso del bias, si el capacitor no basta. **Un LM358 trae dos canales: uno por cada bias** | Media |
| 2 c/u | Cerámicos 1 nF, 4.7 nF, 22 nF | Si los 10 nF del pin dan un desfase distinto al esperado (A7) | Media |
| 1 | Surtido de resistencias ¼ W | Cubre A3, A7 y todo lo que no se previó | **Alta si no hay uno en el cajón** |

> **El criterio de compra en la tienda:** si la pieza cuesta menos que un litro de gasolina y puede detener la corrida, se compra. No hay nada en esta tabla que no cumpla eso.

---

## Alternativas del bias de 2.5 V

Se mide en **A1**, y hay que llegar con las tres opciones en la bolsa porque la decisión se toma en el banco.

**La impedancia del nodo, calculada:**

| Alternativa | 60 Hz | 1 kHz | 24 kHz (portadora) | Costo |
| --- | --- | --- | --- | --- |
| **Opción 1** — DC-006: 10k/10k + 10 µF + 100 nF | 265 Ω | 15.9 Ω | 0.66 Ω | lo que ya está |
| **Opción 2** — bajar el divisor a 2.2k/2.2k | 258 Ω | 15.9 Ω | 0.66 Ω | centavos |
| **Opción 3** — el mismo divisor con **100 µF** | **26.5 Ω** | **1.6 Ω** | **0.07 Ω** | centavos |
| **Opción 4** — seguidor LM358 + 10 Ω en serie | 10 Ω | 10 Ω | **10 Ω** | ~0.60 USD |

**Lo que dicen los números, y no es lo que uno esperaría:**

- **Bajar las resistencias del divisor no compra nada** (258 contra 265 Ω). A 60 Hz el que manda es el capacitor, no el divisor. La opción 2 se descarta.
- **El capacitor le gana al operacional en la frecuencia que importa.** El seguidor necesita los 10 Ω en serie para no oscilar con la carga capacitiva, y esos 10 Ω son 15 veces peor que lo que ya da un simple 10 µF a 24 kHz, que es donde está el ruido del puente. La opción 4 solo gana en DC y a baja frecuencia.
- **La opción 3 es la buena:** diez veces menos impedancia en *todas* las frecuencias, por el precio de un capacitor más grande.

**Lo que el bias tiene que aguantar de verdad.** La corriente de señal que pasa por el divisor AC es de **206 µA pico** (22.6 V de pico sobre 110 kΩ). Con la opción 1 eso mueve el bias **54.5 mV a 60 Hz**, un 2.6 % de la señal. **No es un error:** ese movimiento está en fase con la señal, así que solo cambia un poco la relación de atenuación, y el trimmer lo absorbe al calibrar.

Donde sí sería un error es **si los dos canales compartieran el bias**: entonces los 54.5 mV de un canal se le aparecen al otro como señal ajena. Por eso DC-006 pide **un bias por canal**, y por eso A1 incluye la prueba de acoplamiento. Esa prueba es más importante que la de impedancia.

**Orden de decisión en el banco:**
1. Armar la opción 1 (como está en DC-006) y correr A1.
2. Si el acoplamiento entre canales pasa y el rizado pasa → **quedarse con la opción 1**, y poner 100 µF de todos modos porque no cuesta nada.
3. Si el rizado no pasa → opción 3 (100 µF, luego 220 µF).
4. Si con 220 µF todavía no pasa → el problema no es impedancia, es acoplamiento por el montaje. Ahí sí el LM358, y además hay que revisar por dónde pasan los cables.

---

# Parte A — Módulo ADC AC

**Esta es la parte que desbloquea el PCB.** Todo lo de aquí vive en cobre; lo de la parte B vive en firmware y se puede arreglar después.

Referencia de diseño: [DC-006](decisiones_criticas.md#dc-006-sensado-ac-con-dos-transformadores-de-12-v-directos-al-adc) (el cobre) y [DC-020](decisiones_criticas.md#dc-020-reloj-del-adc-a-128-y-muestreo-amarrado-a-la-portadora-precisión-antes-que-velocidad) (el firmware).

---

## A1. Bias de 2.5 V, sin transformador

**Firmware:** no hace falta. Solo multímetro.

**Objetivo.** Validar que el bias de cada canal se para solo y que un canal no se cuela en el otro.

**Montaje.** Dos redes independientes: 10 kΩ + 10 kΩ de 5 V a tierra, con 10 µF y 100 nF en el punto medio. Una por canal, como pide DC-006.

**Procedimiento.**
1. Medir el punto medio de cada canal con el multímetro en DC.
2. Medir el rizado con el multímetro en AC (si tiene escala de mV).
3. Cortocircuitar momentáneamente el bias de un canal a tierra con una resistencia de 1 kΩ y ver cuánto se mueve el otro.

**Criterio de aceptación.**
- Cada bias: **2.50 V ± 0.05 V.**
- Rizado: **< 5 mV AC.**
- Acoplamiento entre canales: el otro canal se mueve **< 10 mV.** Si se mueve más, las dos redes no están tan separadas como cree el esquemático.

**Qué desbloquea.** Confirma la decisión de "un bias por canal" de DC-006. Si falla el acoplamiento, el PCB necesita separar los rieles o poner un seguidor.

---

## A2. El secundario en vacío — el número que manda

**Firmware:** no hace falta.

**Objetivo.** Medir cuánto da de verdad el secundario del transformador **sin carga**. Todo el atenuador cuelga de este número, y hoy nadie lo ha medido: DC-006 supuso 16 VAC.

> ⚠️ **Precaución.** El primario va a 120 V de la red. El secundario está aislado y es inofensivo, el primario no. Conectar con la clavija afuera y las puntas del multímetro ya puestas.

> ⚠️ **Estos transformadores traen tres hilos, no dos.** Validado en tienda el 18 de noviembre de 2026: el de 120 → 12 VAC tiene **tap central**, y entre un extremo y el central da ~6 V. Con tres hilos hay **tres pares que medir**, y medir el par equivocado mete un error de **2×** en todo el atenuador. Este proyecto usa el **devanado completo** (los dos extremos); el por qué está abajo.

**Procedimiento.**
1. **Identificar los tres hilos antes de nada.** Medir los tres pares con el primario conectado: el par que da **~12 V son los dos extremos**, y el central es el hilo que da ~6 V contra cualquiera de los otros dos. Etiquetarlos.
2. Conectar el primario a la red, el secundario **al aire** (sin divisor, sin nada).
3. Medir **de extremo a extremo** con el multímetro en AC. Anotar: **Vsec**. *No* medir contra el tap central.
4. **En el mismo instante**, medir el voltaje de la red en el mismo contacto. Anotar: **Vlínea**.
5. Escalar al peor caso: `Vsec_max = Vsec × (Vlínea_max / Vlínea)`.
6. **El tap central no se conecta a nada.** Se aísla con cinta y no se deja colgando cerca de la tarjeta de control: es un conductor flotante con 6 V AC encima, o sea una antena.

**Por qué el devanado completo y no el tap.** El medio devanado hereda **dos** tolerancias en vez de una — la relación de vueltas del transformador *y* la posición del tap, que ningún fabricante barato especifica. Dos unidades cuyas relaciones de vueltas coincidan dentro del 3 % pueden tener los taps corridos 4 % en sentidos opuestos, y entonces el devanado completo dispersa 2.9 % mientras el medio dispersa 10.4 %: **el completo lo cubre el trimmer de 2 kΩ y el medio no.** Eso le pegaría directo a A5, que es la corrida que decide si el trimmer sube a 5 kΩ — y eso toca el esquemático antes de imprimir.

Todo lo demás sale **idéntico** entre las dos opciones, porque el divisor se dimensiona para poner el mismo voltaje en el nodo: las mismas cuentas de ADC (778 contra 750 a línea nominal), la misma corriente por el divisor (205 contra 198 µA), **el mismo corrimiento del bias de ~54 mV** y la misma relación señal/ruido. El tap **no** arregla lo que prueba A1.

**El tap se queda como salida de emergencia.** Si `Vsec_max` sale arriba de ~24 VAC punta a punta, el devanado completo pediría más de 150 kΩ, la impedancia del nodo sube y los valores estándar se acaban. Ahí el tap parte el problema a la mitad. Es una contingencia, no un plan.

Para `Vlínea_max` se usa **140 V** (127 V + 10 %) como margen de diseño de este proyecto. Si conoces el rango real de tu red, usa el tuyo y anótalo.

**Criterio de aceptación.** Ninguno: es una medición, no una prueba. Alimenta a A3.

**Qué anotar.** `Vsec`, `Vlínea`, `Vsec_max` y la marca del transformador. Un transformador de otra marca da otro número.

---

## A3. La resistencia de arriba sale de A2

**Firmware:** no hace falta. Es aritmética en el banco.

**Objetivo.** Elegir la rama de arriba del divisor **con el número medido**, no con el supuesto.

**El criterio.** Con el trimmer en su tope (rama de abajo = 10.2 kΩ), el pin del ADC no debe pasar de **4.60 V** absolutos, lo que deja 400 mV de reserva antes de los 5 V.

**La fórmula.**

```
Rt ≥ Rb × ( Vsec_max × √2 / 2.10 − 1 )        con Rb = 10.2 kΩ
```

**La tabla, ya resuelta:**

| `Vsec_max` medido | `Rt` mínima | R estándar a usar | Pin absoluto resultante |
| --- | --- | --- | --- |
| 14.0 VAC | 86.0 kΩ | **100 kΩ** | 4.333 V |
| 15.0 VAC | 92.8 kΩ | **100 kΩ** | 4.463 V |
| 16.0 VAC | 99.7 kΩ | **100 kΩ** | 4.594 V ← el supuesto de DC-006, al límite |
| | | *o 110 kΩ* | *4.420 V — ver la nota de abajo* |
| 17.0 VAC | 106.6 kΩ | **110 kΩ** | 4.540 V |
| 18.0 VAC | 113.4 kΩ | **120 kΩ** | 4.494 V |
| 19.0 VAC | 120.3 kΩ | **130 kΩ** | 4.455 V |
| 20.0 VAC | 127.2 kΩ | **130 kΩ** | 4.558 V |
| 22.0 VAC | 140.9 kΩ | **150 kΩ** | 4.481 V |

**Lo que hay que entender de esta tabla.** Los 100 kΩ de DC-006 solo alcanzan si `Vsec_max` **no pasa de 16 VAC**, y `Vsec_max` ya incluye la red alta. Eso significa que a 127 V de línea el transformador no debe dar más de **~14.5 VAC en vacío**. Un transformador de "12 VAC" barato sube entre 15 y 25 % sin carga, así que puede caer justo arriba. **Es la razón por la que esta medición va antes del PCB.**

**El techo no es un acantilado: es una perilla.** Esta tabla escoge siempre **la resistencia estándar más chica que pasa**, que es la que aprovecha más el ADC y la que deja menos reserva. Subir al siguiente valor compra margen pagando rango:

| `Vsec_max` | R mínima | Pin | Cuentas de ADC | | R siguiente | Pin | Cuentas de ADC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 16.0 VAC | 100 kΩ | 4.594 V | 778 | | **110 kΩ** | **4.420 V** | 713 |
| 18.0 VAC | 120 kΩ | 4.494 V | 741 | | 130 kΩ | 4.352 V | 688 |
| 20.0 VAC | 130 kΩ | 4.558 V | 765 | | 150 kΩ | 4.301 V | 669 |

Cuentas de pico a pico a **línea nominal** (127 V), de las 1024 del ADC. El renglón de 16 VAC que la tabla de arriba marca "al límite" cuesta **65 cuentas, un 6 % del rango**, y a cambio el pin baja de 4.594 a 4.420 V. **Decídalo en el banco con el número medido**, no aquí: si `Vsec` sale cómodo, use la mínima y quédese con el rango; si sale justo contra los 16 VAC, suba al siguiente valor y pague el 6 %.

**Por qué igual no es catastrófico si sale alto.** En THT una resistencia se cambia desoldando. Lo que sí sería caro es descubrirlo con la tarjeta ya poblada y el firmware persiguiendo un RMS equivocado.

### ✅ Decidido en banco — 21 de noviembre de 2026: **110 kΩ**

Con los números **medidos** en A6 (razón del transformador 8.3986, divisor 10.774, bias 2.50 V) y el factor de cresta medido de 1.466:

| Línea | Secundario | Pico en el pin | Contra el techo de 4.60 V |
| --- | --- | --- | --- |
| 120 V (lo de hoy) | 14.29 V | 4.444 V | pasa |
| 127 V (nominal) | 15.12 V | 4.558 V | pasa, apretado |
| 130 V | 15.48 V | **4.606 V** | **se pasa** (con cresta senoidal: 4.532 V, pasa) |
| 140 V (red alta) | 16.67 V | **4.768 V** | **se pasa** (con cresta senoidal: 4.688 V, **también se pasa**) |

> **Cuál renglón es robusto y cuál no.** El de 140 V se pasa **con cualquiera de las dos crestas**, así que la conclusión no depende del supuesto. El de 130 V sólo se pasa con la cresta medida de 1.466, y esa cresta está inflada: `pico` guarda un *extremo* de 6000 muestras y el filtro antialias flojo deja entrar contenido impulsivo (ver el hallazgo 4 de A6). **La decisión se toma por el renglón de 140 V**, que es además el criterio que A3 ya tenía escrito.

**Con 100 kΩ la cadena recorta en red alta.** La resistencia mínima para quedar dentro a 140 V sale de

```
  Rt ≥ Rb × ( Vsec_max × cresta / 2.10 − 1 )     con Rb = 10 231 Ω
```

y da **104.6 kΩ** con cresta senoidal y **108.8 kΩ** con la cresta medida. **Las dos apuntan al mismo valor comercial: 110 kΩ.** El resultado es robusto a la duda sobre la cresta, que es lo que lo hace decidible.

Esto es exactamente lo que el viaje al banco existía para contestar, y se contestó **antes de imprimir el PCB**. Costo: 65 cuentas de ADC, un 6 % del rango.

**Montado y verificado el mismo día.** No había una resistencia de 110 kΩ en el banco, así que se puso **100 kΩ + 10 kΩ en serie** — eléctricamente idéntico: mismo ruido térmico que una sola del valor total, 1.9 mW de disipación y décimas de picofaradio de parásita contra los 10 nF que ya tiene el nodo.

| | Predicho | **Medido** (18 reportes) |
| --- | --- | --- |
| Pin del ADC | 1215 mV | **1220.7 mV** |
| Cuentas RMS | 249 | **249.75** |
| `DIVISOR_X100` | 1175 | **1171** |
| `Rt` efectiva | 110.0 kΩ | **109.6 kΩ** (el 10 kΩ resultó 3.8 % bajo, dentro de ±5 %) |

Verificado por los dos lados, y los dos coinciden: con 1171 el sketch reporta **14.294 V** de secundario (Fluke 14.3) y **120.06 V** de red (Fluke 120.1).

Margen a 140 V con la `Rt` real: **+88 mV** con cresta senoidal, **+67 mV** con la cresta medida y **+14 mV** con la peor cresta observada en cuatro corridas. Pasa con las tres, aun con la resistencia 3.8 % baja.

> **Y `RELACION_X1000` no se tocó**, porque el transformador es el mismo. Esa es la ganancia concreta de haber separado las dos constantes: con la constante revuelta habría que recalibrar ambas contra el Fluke cada vez que se cambia una resistencia del divisor.

> **Lo que sigue abierto es el transitorio del propio inversor**, que es harina de otro costal: a 156 V el pin iría a 4.94 V aun con 110 kΩ. Eso depende del techo de amplitud por flujo de DC-021 — **ese techo se decide primero**, porque si se implementa, la salida máxima posible baja de 188 a ~138 V y el problema desaparece solo. El canal de la red no necesita ese margen, pero DC-006 exige canales idénticos.

---

## A4. Atenuador completo y prueba de recorte

**Firmware:** no hace falta para la medición con multímetro.

**Objetivo.** Armar la cadena real y confirmar que no recorta en ningún extremo del trimmer.

**Montaje.** Transformador → `Rt` (la elegida en A3) → nodo → (8.2 kΩ + trimmer 2 kΩ) → bias de 2.5 V. Del nodo, 1 kΩ al pin del ADC, y **10 nF pegado al pin**.

> ⚠️ **Orden de conexión. El Nano se conecta al último.** Primero se arma el divisor y se mide el pico **con el multímetro**, con el pin del ADC **al aire**. Sólo cuando el pico confirmado esté abajo de 4.60 V se conecta el Nano. Armar al revés —Nano puesto y después girar el ajuste— es la forma fácil de pasarse del riel sin enterarse.

**Procedimiento.**
1. Trimmer al tope, **pin del ADC al aire**. Medir el pico. Si hay osciloscopio, directo; si solo hay multímetro, medir en AC y multiplicar por √2 (la red es senoidal, el error es pequeño).
2. Trimmer al mínimo. Medir otra vez.
3. Repetir en el segundo canal.
4. Con los dos canales confirmados abajo de 4.60 V, conectar el Nano.

### Si no se consigue el trimmer multivuelta

En noviembre de 2026, Steren —que es donde muchos van a comprar— **no tiene trimmer multivuelta de 2 kΩ**. Lo que tiene son **potenciómetros de una vuelta sin switch** de 5 kΩ y de 10 kΩ (el de 10 kΩ es el SKU 101-10K, ~$9 MXN). Sirven, pero **como herramienta de banco, no como pieza de la tarjeta** — y entre los dos, **el de 5 kΩ es el bueno.**

**Por qué no va en la tarjeta.** En serie con los 8.2 kΩ fijos, ese pot da un `Rb` de 8.2 a 18.2 kΩ, y el extremo alto revienta el riel:

| `Rb` | Pin absoluto | Corriente inyectada |
| --- | --- | --- |
| 10.2 kΩ — trimmer de 2 kΩ al tope, lo que pide DC-006 | 4.594 V | ninguna |
| 13.2 kΩ — **pot de 5 kΩ al tope** | 5.139 V | **ninguna** |
| 18.2 kΩ — pot de 10 kΩ al tope | 5.984 V | 30 µA |

> **Corregido el 20 de noviembre de 2026.** Esta tabla decía que el pot de 10 kΩ inyecta **0.98 mA**, al borde del máximo absoluto de ±1.0 mA del ATmega328P, y de ahí concluía que «sobrevive, pero eso no es margen, es suerte». **El número estaba 33 veces alto.** El error: se dividió la sobretensión sólo entre la resistencia de 1 kΩ en serie, ignorando los **15.4 kΩ de Thévenin del propio divisor**, que están en el mismo camino. La impedancia real vista desde el pin son 16.4 kΩ, así que la inyección son **30 µA**. Eléctricamente los tres casos son seguros; el diodo de protección ni siquiera conduce en los dos primeros, porque su umbral son ~5.5 V.

**Entonces, ¿por qué preferir el de 5 kΩ?** No por seguridad: **por recorrido útil.** El ajuste sirve mientras `Rb` va de 8.2 kΩ a 10.23 kΩ, que es donde el pin toca el techo de 4.60 V. Eso son 2.03 kΩ de pista:

| | Giro útil, de los 270° |
| --- | --- |
| **Pot de 5 kΩ** | **110°** |
| Pot de 10 kΩ | 55° |

Con el de 10 kΩ, todo el ajuste cabe en un sexto de vuelta y el resto de la pista no sirve. Con el de 5 kΩ hay el doble de recorrido para el mismo ajuste. **Si hay que escoger uno para el banco, es el de 5 kΩ.** *(Comprimirlo con 2.2 kΩ en paralelo sí arregla el rango —`Rb` de 8.20 a 10.00 kΩ— pero a media vuelta ya recorrió el 85 % del recorrido, así que todo el ajuste útil se aprieta en el primer cuarto de giro. Peor para calibrar, no mejor.)*

**Por qué sí sirve de herramienta.** La resolución alcanza de sobra:

| | Ω por grado | Giro para ajustar 1 % |
| --- | --- | --- |
| Pot de 10 kΩ, una vuelta (270°) | 37.0 | **2.5°** |
| Trimmer de 2 kΩ, 25 vueltas | 0.22 | 414° |

Lo que compra el multivuelta **no es precisión: es que no se mueva solo.** Para dejarlo en una tarjeta que va a vibrar, eso importa. Para encontrar un número en la mesa, no.

**El método.** Girar el pot en A4, A5 y A6 hasta que el ADC cuadre con el multímetro → **desconectarlo y medirlo con el óhmetro** → soldar una resistencia fija de ese valor. **El pot nunca llega al PCB.** Es el método clásico y esquiva los tres problemas de arriba.

**Y puede que el trimmer sobre.** A6 dice que el error residual se corrige por software, a la EEPROM. Entonces el trimmer no está para calibrar: está para que la señal **quepa en la ventana del ADC** a pesar de la variación entre unidades, porque el software corrige ganancia pero no recupera una señal recortada. **Eso es justo lo que mide A5.** Si la dispersión sale chica, una `Rb` fija más la calibración por software alcanza y el trimmer **desaparece de la lista de materiales**: nada que ajustar, nada que se mueva, nada que conseguir. Para la trinchera eso vale más que el trimmer — una pieza que no se consigue es un problema, y ninguna pieza es mejor que una pieza difícil.

**Criterio de aceptación.**
- Trimmer al tope: pin absoluto **≤ 4.60 V**.
- Trimmer al mínimo: pico **≥ 1.5 V** sobre el bias. Más abajo se está tirando resolución del ADC a la basura.
- Los dos canales deben coincidir dentro de **5 %** entre sí con el trimmer en la misma posición. Si no coinciden, es tolerancia de resistencias, y eso es justo lo que el trimmer existe para corregir.

**Qué desbloquea.** El valor definitivo del divisor en el esquemático y en la lista de materiales del PCB.

---

## A5. Rango del trimmer

**Firmware:** no hace falta.

**Objetivo.** Confirmar que ±10 % de ajuste alcanza para la dispersión real del transformador, y que el cursor no salta.

**Procedimiento.** Girar el trimmer de tope a tope **despacio**, midiendo cada media vuelta. Anotar la lectura mínima, la máxima y si hubo algún salto.

> **Esta parte necesita el trimmer de verdad.** Con el potenciómetro de una vuelta sustituto (ver A4) se puede hacer todo lo demás del protocolo, pero **no esto**: el ±8 % de recorrido es una propiedad del trimmer que va a quedar en la tarjeta, y un pot de 10 kΩ de una vuelta no responde esa pregunta. Si no consiguió el multivuelta, **salte al apartado de los dos transformadores** —que sí se puede hacer— y deje este criterio pendiente. Puede que A5 lo vuelva innecesario: ver el final de A4.

**Criterio de aceptación.**
- El recorrido cubre al menos **±8 %** alrededor del centro.
- **Monótono, sin saltos.** Un salto es cursor sucio o mal contacto: ese trimmer se tira.
- Conectado como reóstato, al levantar el cursor con la pinza la resistencia debe irse a **2 kΩ**, no al aire (es la protección que pidió DC-006).

**La parte que necesita dos transformadores.** El ±10 % no existe para calibrar un transformador: existe para **absorber la diferencia entre unidades**, que es lo que va a pasar cuando alguien más arme esta tarjeta con el transformador de su tienda. Con una sola unidad esa pregunta no se puede contestar.

Por eso el segundo transformador del mismo modelo está en la lista de repuestos:

1. Medir el secundario en vacío de los dos, en el mismo contacto, con pocos minutos de diferencia.
2. Anotar la diferencia en porcentaje.

**Criterio.** Si la diferencia entre dos unidades del mismo modelo es **menor al 8 %**, el trimmer de 2 kΩ alcanza y DC-006 queda confirmado. Si es mayor, hay que subir a 5 kΩ (el que viene en los repuestos) y **eso cambia el reparto fijo/ajustable del divisor**, o sea que toca el esquemático antes de imprimir.

Dos unidades no son una muestra estadística, pero una diferencia grande entre dos ya basta para descartar el ±10 %. Es lo más que se puede saber por el precio de un transformador.

**Y de paso, dos mediciones gratis con el tap central.** Ya que los dos transformadores están en la mesa y el multímetro en la mano, medir también **medio devanado** de cada unidad (extremo a tap central) y anotar **las dos dispersiones por separado**:

| | Unidad 1 | Unidad 2 | Dispersión |
| --- | --- | --- | --- |
| Devanado completo (extremo a extremo) | | | % |
| Medio devanado (extremo a tap) | | | % |

Son cuatro lecturas y cierran una pregunta que si no se mide aquí nadie va a volver a medir. La predicción de A2 es que **la del medio devanado sale peor**, porque hereda la tolerancia de la posición del tap además de la de la relación de vueltas. Si sale peor, queda confirmado por qué este proyecto usa el devanado completo. **Si sale igual o mejor**, el argumento de A2 estaba equivocado y hay que revisarlo — y entonces el tap vuelve a la mesa como opción de diseño, no solo como salida de emergencia.

---

## A6. ADC contra multímetro — calibración

**Firmware:** ✅ ya existe — [`ISLA_CRITICO_PRUEBA_AC_1CANAL`](../firmware/critico/ISLA_CRITICO_PRUEBA_AC_1CANAL/ISLA_CRITICO_PRUEBA_AC_1CANAL.ino), **un solo canal**. Antes de cargar, llenar las perillas `VCC_MV` (el riel de 5 V medido), `DIVISOR_X100` (la relación del divisor) y `RELACION_X1000` (la razón del transformador).

> **No usar el sketch de dos canales para A6.** Un pin de ADC al aire **no lee cero: lee el canal anterior**, porque el capacitor de muestreo y retención guarda la carga de la conversión pasada. En banco el canal conectado daba 14.26 VAC y el canal al aire daba 14.12 — el 99 %, un número perfectamente creíble que no mide nada. Con un solo canal esa trampa no existe por construcción.

**Objetivo.** Que el número que reporta el ATmega coincida con el multímetro.

**Procedimiento.**
1. Medir el secundario con el multímetro en AC.
2. Leer lo que reporta el firmware.
3. Girar el trimmer hasta que coincidan. Anotar la posición.
4. Lo que quede de error va por software, a la EEPROM (DC-006).

**Criterio de aceptación.** Coinciden dentro de **1 %** después de ajustar.

> ⚠️ **Un multímetro de valor medio** (no *true RMS*) está calibrado para senoidal. Para la **red** vale. Para la **salida del inversor** no: ahí la onda es SPWM filtrada y un multímetro de valor medio miente. Esa calibración se hace aparte, cuando haya potencia.

### ✅ Resultado de banco — 20 y 21 de noviembre de 2026

**PASA.** Error sin calibrar **+0.42 %** contra el criterio de 1 %.

**Montaje tal como se armó** (el del esquemático, con una sustitución):

| | Diseño | En el banco |
| --- | --- | --- |
| `R27` (rama de arriba) | 100 kΩ | 100 kΩ |
| `R9` (rama de abajo, fija) | 8.2 kΩ | 8.2 kΩ |
| `RV2` (ajuste) | trimmer 2 kΩ multivuelta | **potenciómetro de una vuelta de 5 kΩ** (Steren), medido en **2.03 kΩ** |
| `C7` | 10 nF al nodo | 10 nF al nodo |
| Bias `R29`/`R30` | 10 k / 10 k | 10 k / 10 k |
| `C2` / `C3` | 10 µF / 100 nF | 10 µF / 100 nF |
| Resistencia de 1 kΩ al pin | **la pide el montaje de A4, no está en el esquemático** | no se puso |
| Fuente | — | red de casa, por un inversor offgrid de onda pura |
| Canal | — | **A6**, un solo canal |

**Constantes, y cómo salieron.** Las dos se midieron con el Fluke 324 en **los dos lados** del transformador, lo que cancela su error de ganancia porque es un cociente del mismo instrumento:

```
  razón del transformador = 120.1 V / 14.3 V       = 8.3986   -> RELACION_X1000 8399
  secundario con la línea en 119.9                 = 14.276 V
  divisor = 14 276 mV / 1325 mV (pin del ADC)      = 10.774   -> DIVISOR_X100    1077
  Rb = 100k / (10.774 - 1)                         = 10 231 Ω = 8.2k + 2.03k
```

**Separarlas importa.** Antes `RELACION_X1000` estaba calibrada de punta a punta y absorbía el error del divisor, el del riel de 5 V y la razón del transformador, todo revuelto: la línea salía bien pero ninguna constante decía lo que su nombre promete. Con eso no se podía calcular el margen del pin contra el riel (A3), que necesita el divisor solo, y cambiar de transformador obligaba a recalibrar las dos. El efecto en el voltaje de la línea al separarlas fue de **−0.04 %**: no se trataba de exactitud, se trataba de que los números signifiquen algo.

**Lo medido.** La corrida que cierra A6 es la de **31 reportes con `Rt` = 110 kΩ ya calibrada**:

| | Valor | Nota |
| --- | --- | --- |
| Línea | **120.005 V** media | Fluke: 120.1 → **−0.079 %** |
| Secundario | **14.288 VAC** | Fluke: 14.3 → **−0.087 %** |
| Repetibilidad | **σ = 0.069 %** (82.8 mV) | 28 veces mejor que la exactitud del Fluke |
| Frecuencia | 60.038 Hz, σ = 0.032 Hz | portal del inversor: 59.95–60.00 |
| Factor de forma | 1.1139, **σ = 0.0003** | senoidal = 1.1107 |
| Factor de cresta | 1.4294, σ = 0.0049 | √2 = 1.4142, o sea +1.05 % |
| Disparos perdidos | **0 en los 31** | el `1` del primer reporte es el arranque |
| Cruces por reporte | **60** (61 en cuatro) | ni uno perdido |
| Muestras entregadas | 5991 de 6006 (99.76 %) | |

> **Pagar 8 % de señal no costó repetibilidad.** Al subir `Rt` de 100 a 110 kΩ las cuentas bajaron de 271 a 249.7, un 8 % menos de rango de ADC — y la σ relativa se quedó **idéntica, en 0.069 %**. Era la predicción: la dispersión no la limita la cuantización (que sola daría 8.7 mV contra los 82.8 observados), sino el batido del hallazgo 3. Se pagó margen contra el riel y no se perdió nada a cambio.

**El patrón, y lo que no es.** El Fluke 324 **sí** es de valor eficaz verdadero, pero su exactitud en volts AC es **1.5 % ± 5 dígitos** en un rango único de 600.0 V, o sea **±2.30 V a 120 V (±1.92 %)**. Y «5 dígitos» son medio volt: **Fluke no garantiza el dígito de los décimos**. De ahí dos cosas: que no tiene sentido perseguir la cuarta cifra de las constantes, y que la **exactitud absoluta de esta cadena está limitada por el instrumento del usuario, no por el circuito**. La tarjeta es un instrumento más *preciso* que el Fluke (σ 0.077 % contra 1.92 %); nomás no más *exacto*, porque la exactitud necesita un patrón. **En producción, la calibración de voltaje tiene que ser una perilla del usuario.**

### Lo que esta corrida encontró de paso

**1. El 17.5 % de las conversiones se perdía, y era el presupuesto del ADC.** Con 3 periodos por disparo el presupuesto entre muestras son 124.875 µs y la conversión a 125 kHz se come 104: el ADC al **83 %**, y los 20.9 µs que sobran tienen que absorber tres ISR del Timer1 más la del ADC. No cabía — y eso **sin SPWM**, que en el firmware real vive dentro de esa misma ISR. Con **4 periodos** el ADC baja al 62 %, `perdidos` cae a cero y la tasa sube a 99.76 %. La ventana no se movió: 300 muestras a 6006/s son los mismos 49.95 ms que 400 a 8008/s.

**2. La frecuencia no se estabilizó por bajar la tasa, sino porque dejaron de perderse muestras.** Los cuatro reportes malos de antes (59.170, 59.173, 59.192, 59.226) son `60.0 × 72/73 = 59.178`: **un cruce perdido cada uno**. El detector necesita dos muestras consecutivas que abracen el cero; si la de junto al cruce se pierde, el cruce se pierde. La precisión del cruce **no depende de la tasa** — es una cuenta del ADC entre la pendiente, 6.85 µs a 166.5 µs y 6.86 µs a 124.875.

**3. El bamboleo de ±0.08 V no es ruido: es un batido, y su periodo son 4 reportes.** La ventana de 300 muestras son **3.00616 ciclos**, no 3.00000, así que el pedacito sobrante rota de fase. La aritmética:

```
  corrimiento por ventana          +0.00616 ciclo
  por reporte (20 ventanas)        +0.1232  ciclo
  la FASE se repite cada            8.12 reportes
  pero el error del RMS va como cos(2 x fase), asi que se repite cada
                                    4.06 reportes   <-- esto es lo observable
```

La autocorrelación de los 31 reportes da exactamente eso: `r(2) = −0.81`, `r(4) = +0.81`, `r(6) = −0.72`, `r(8) = +0.66` — un periodo 4 de libro, que explica el **86.4 %** de la varianza.

> **Corregido.** La primera lectura de este hallazgo (20 de noviembre, con 25 reportes) dijo «periodo 8». El 8 también puntuaba alto porque es múltiplo de 4, pero el periodo verdadero es 4: **la fase de la ventana se repite cada 8.12 reportes, y el error del RMS va con el coseno del doble de esa fase.** La corrida de 31 reportes con 110 kΩ lo separó sin ambigüedad.

Separado: **77 mV de batido determinístico y 30.6 mV de ruido real (0.025 % de la línea)**. Se quita con ventana delimitada por cruces por cero.

**4. El filtro antialias está flojo.** El nodo tiene 9.26 kΩ de Thévenin y C7 son 10 nF → **fc = 1719 Hz**, con Nyquist en 3003: apenas **−6.1 dB** en Nyquist. Probablemente explica por qué el factor de forma se corrió 0.397 % al cambiar sólo la tasa de muestreo (el aliasing depende de la tasa), y por qué la cresta brincó 2.6 % sin que el FF la siguiera. **Pendiente de decidir**, y hay tensión: subir C7 a 33 nF da −15 dB en Nyquist pero se come el 29 % del 13º armónico y mete 6.6° de fase a 60 Hz, que le pega al comparador de cruce por cero. Un polo solo no puede hacer las dos cosas.

**5. Tres defectos del instrumento, encontrados y corregidos.** (a) El voltaje se convertía en tres truncadas encadenadas y `cv = mv × 1.08` se saltaba un valor cada doce pasos: con `RELACION` 8379 el cv 1430 era inalcanzable y **119.8 V no se podía desplegar** — la lectura brincaba 119.7 ↔ 119.9 con la línea quieta. Se tiraban 7× de resolución. Ahora es una sola expresión y el paso es de 30 mV. (b) `imprimeDec` supone escala potencia de diez; con escala 16 imprimía el resto módulo diez, así que «270.5 cuentas» quería decir 270.31. (c) La frecuencia suponía el intervalo nominal entre muestras y con la pérdida real salía inflada a 72.77 Hz.

### Lo que queda abierto de A6

- **El 0.24 % de muestras que no cuadra** (5992 de 6006). Puede ser unas pocas ISR que llegan tarde, o el mecanismo de `hayVentana` —que es una bandera, no una cola— soltando una ventana durante el `Serial.print`. Es 74 veces más chico que lo que traíamos; se mide en B5.
- **La frecuencia absoluta.** El sketch da 60.041 Hz y el portal del inversor 59.95–60.00: **+0.11 %**. No se puede decir quién tiene razón — el sketch mide con el cristal del Nano y el inversor *genera* con el suyo. Dos relojes sin juez. El jitter que queda (σ 0.034 Hz) es **la resolución de un milisegundo de `millis()`**, no la línea: la correlación entre la frecuencia reportada y la tasa de muestreo es **+0.996**. Se arregla usando el intervalo nominal, pero **no antes** de entender el 0.24 %, porque entonces ese 0.24 % se vuelve un sesgo de +0.14 Hz.
- **La resistencia de 1 kΩ al pin** está en el montaje de A4 y no en el esquemático. Eléctricamente casi da igual (el Thévenin del divisor domina: la inyección en sobretensión pasa de 30 a 33 µA), pero el documento y la tarjeta tienen que decir lo mismo.

### 📄 Caracterización contra cargas reales

El 22 de noviembre la cadena se puso a prueba contra tres eventos de carga de la casa —resistiva de 1000 W, motor universal de 700 W en régimen, y el **arranque** de ese motor— usando la ventana por comparador. **Los cinco detectores hicieron su trabajo**, y uno de ellos sólo sirvió cuando se le dio la resolución temporal correcta.

El análisis completo está en **[`caso_estudio_sensado_ac.md`](caso_estudio_sensado_ac.md)**. Los tres resultados que más pesan:

- **El factor de forma distingue el tipo de carga por el signo**: +0.083 % con la resistiva, **−0.533 % con el motor** (46 σ), cruzando por debajo del valor de una senoidal. Los dos reversibles.
- **El canal de CD atrapó algo por primera vez**: +2.40 V transitorios en el arranque, y **sólo visibles con ventana de 50 ms** — con un segundo se promedian a cero.
- **La ventana decide lo que se puede ver.** El mismo arranque midió **−3.51 %** con ventana de 1 s y **−12.57 %** con ventana de 50 ms. Un promedio no miente, pero contesta otra pregunta.

---

## A7. Simetría entre canales — la prueba estrella

**Firmware:** ✅ ya existe — el mismo sketch de A6. La perilla `ORDEN_INVERTIDO` cambia el orden de lectura para el diagnóstico.

**Objetivo.** Validar de un golpe **dos** cosas: que el sesgo de DC-020 es el calculado, y que los filtros de los dos canales son simétricos como supone DC-006.

**La idea.** Alimentar **A0 y A6 del mismo secundario**. Si los dos ven exactamente la misma señal, la única diferencia entre sus cruces por cero tiene que ser el sesgo del multiplexado, que DC-020 predice en **124.875 µs = 2.697° a 60 Hz**. Cualquier diferencia extra es asimetría de los filtros.

**Esto no necesita osciloscopio.** El patrón es la propia aritmética del firmware: el sketch reporta los dos cruces y se compara el número con la predicción.

**Procedimiento.**
1. Puentear A0 y A6 a la misma salida del divisor (un solo canal alimentando los dos pines, cada uno con su 1 kΩ y su 10 nF).
2. Correr el sketch 10 s y promediar.
3. Anotar la diferencia medida entre los dos cruces.
4. **Invertir el orden de lectura** (A6 primero) y repetir.

**El sketch ya descuenta el sesgo**, porque ubica cada cruce con la cuenta de portadora de su propio disparo. Así que lo que reporta **no son 124.9 µs: es lo que sobra después de descontarlo, y debe salir cero.** Los 124.875 µs son lo que saldría si el firmware tratara las dos muestras como simultáneas — justo el error que DC-020 manda corregir.

**Criterio de aceptación.**
- Sesgo reportado con 1 s de promedio: **|sesgo| < 2 µs** (0.043° a 60 Hz). Validado en la PC con el mismo código entero: la aritmética sola da **0.39 µs** en 1 s y menos de 10 ns pasados los 5 s, así que todo lo que pase de 2 µs **es hardware, no es la cuenta**.
- Contador `desc` (pares de cruces descartados): unos pocos por segundo es normal. Si son la mayoría, la señal está muy sucia o muy chica.
- Al invertir el orden de lectura (`ORDEN_INVERTIDO 1`), el sesgo debe **quedar igual, con el mismo signo y la misma magnitud**.

**El diagnóstico, si falla.** Invertir el orden separa las dos causas posibles:
- El sesgo **conserva signo y magnitud** al invertir el orden → es una propiedad de los canales, no del muestreo: **los filtros no son simétricos.** Eso toca el cobre.
- El sesgo **cambia de signo** al invertir el orden → el error está en la contabilidad del tiempo, no en el hardware. Avisar, porque sería un error del sketch.

> **Lo que la validación en la PC ya le ahorró al banco.** Al correr la aritmética del sketch contra una senoidal sintética apareció un error que habría arruinado la medición: **los dos canales no siempre agarran el mismo cruce.** Si la ventana empieza encima de un cruce, un canal lo atrapa y el otro agarra el del ciclo siguiente, y la diferencia salta un periodo entero de línea. El promedio de 1 s salía con **1.7 ms** de error en lugar de 0.39 µs — y en el banco se habría visto como "los filtros están asimétricos", mandando a cambiar cobre que estaba bien.
>
> Hay un candado exacto, y ya está en el sketch: como las dos rejillas de muestreo están corridas 3 periodos de portadora, para un mismo cruce la diferencia de cuentas **sólo puede ser +3 o −3**. Cualquier otro valor es un par mal emparejado y se descarta: eso es el contador `desc`.

**Qué desbloquea.** La cancelación de los ~2° del filtro que DC-006 da por hecha, y el constante que el PLL va a restar.

---

## A8. DC-020: frecuencia de muestreo e instante del disparo

**Firmware:** ✅ ya existe — el mismo sketch. La perilla `PIN_TRAZA_ADC` (D8 por omisión) sube al disparar la conversión y baja al terminarla. La perilla `CARGA_FALSA_CICLOS` quema en la ISR los mismos ciclos que gasta la del SPWM, para ver si le deja espacio al ADC.

**Objetivo.** Confirmar con el analizador que el ADC corre como dice DC-020.

**Procedimiento.** Analizador en el pin de traza y en una salida del puente (para ver la portadora). Capturar 10 s.

**Criterio de aceptación.**
- Intervalo entre disparos: **124.9 µs ± 2 µs** (3 periodos de portadora).
- Ancho del pulso (duración de la conversión): **104 µs ± 2 µs**.
- **Cero disparos perdidos** en 10 s. Un disparo perdido significa que la ISR se retrasó, y eso mete jitter en la fase.
- El disparo cae siempre en el mismo punto del periodo de portadora (BOTTOM).

**Qué desbloquea.** DC-020 completo, que hoy es aritmética sobre la hoja de datos y nada más.

---

# Parte B — Pendientes acumulados

Ya no bloquean el PCB, pero están abiertos desde hace semanas y el banco ya está armado. Cerrarlos en el mismo viaje es gratis.

## B1. P9 en A7 con resistencias reales

**Firmware:** ✅ ya existe (`dt_p9.h/.cpp`).

Los cuatro casos de la sección 7.2 de [`identificacion_dt_pin9.md`](identificacion_dt_pin9.md), ahora con resistencias de 10 kΩ y 20 kΩ físicas en lugar de los túneles de SimulIDE. Incluye el caso de **A7 al aire**, que sigue sin decidirse entre la opción A (detector en firmware) y la opción C (doble R12 en el cobre).

**Criterio.** Los cuatro casos identifican el tiempo muerto correcto en las 16 lecturas de arranque, y la vigilancia de cada 50 ms no da falsos. **Decidir la protección de A7 al aire y anotarla**, porque la opción C sí toca el PCB.

## B2. Batería en A3

**Firmware:** ✅ ya existe (`bateria.h/.cpp`, validado en SimulIDE).

**Procedimiento.** Divisor 68 k + 68 k + 10 k de DC-007 sobre la batería 16S real (56 V). Comparar con el multímetro. Si hay fuente ajustable, recorrer los umbrales de las tres tecnologías.

**Criterio.** La lectura coincide con el multímetro dentro de **0.3 V**. Con fuente ajustable: los seis umbrales de cada tecnología disparan con sus retardos.

**Pendiente extra.** Anotar a qué voltaje corta el BMS de la 16S, para confirmar que el margen de 0.1 V/celda del perfil LFP_16S está del lado correcto.

## B3. Protocolo v2 por I2C entre los dos Nano

**Firmware:** ✅ ya existe (validado en simavr: 83/83 tramas, 0 errores de CRC).

**Criterio.** **0 errores de CRC en 1000 tramas.** El voltaje de batería llega en décimas de V. El comando de tecnología se aplica, se guarda en EEPROM y **sobrevive un ciclo de apagado**.

## B4. LCD PCF8574 y botonera física

**Firmware:** ✅ ya existe (`LCD_TIPO 0`).

**Criterio.** La pantalla arranca **sin** las resistencias de 10 kΩ que SimulIDE exigía en SDA/SCL (el módulo de tienda ya las trae). Los cinco umbrales de la escalera leen limpio con el antirrebote de 3 lecturas × 10 ms, y **ningún falso en 1 minuto** sin tocar nada.

## B5. Duración de la ISR del SPWM — sale gratis

**Firmware:** ⚠️ falta una línea (levantar un pin al entrar a la ISR, bajarlo al salir).

> ⚠️ **Corregido por DC-023.** Esta prueba decía originalmente «D7, que está libre y reservado para esto por DC-017». **Ya no lo está:** D7 es ahora `AIN1`, la entrada negativa del comparador, y D6 es `AIN0`. El pin de traza sale de otro lado — ver [Parte C](#parte-c--todo-corriendo-al-mismo-tiempo-cabe-en-tiempo-y-está-equilibrado), que absorbe esta prueba como su etapa 1.

**Criterio.** El ancho del pulso cabe en el periodo de portadora de 41.6 µs **con espacio de sobra para el ADC y para el Timer0 de `millis()`**. Si no cabe, hay que optimizar la multiplicación de 32 bits antes de meterle el ADC.

Esta es la medición más barata del viaje: un pin, una línea de firmware y el analizador que ya está conectado.

## B6. El primer ciclo de amplitud 0 y el pulso solitario

**Firmware:** ✅ ya existe.

Confirmar con el analizador lo que GTKWave ya mostró: el primer ciclo arranca sin complementario (amplitud 0), y el pulso solitario de la pierna A aparece donde el modelo lo predice.

**Criterio.** Coincide con la traza de `firmware/critico/img/primer_ciclo_amplitud_cero.png`. Si coincide, el modelo de registros de la PC queda validado contra fierro por segunda vez.

---

# Parte C — Todo corriendo al mismo tiempo: ¿cabe en tiempo y está equilibrado?

Las partes A y B miden cada cosa por separado. La Parte C hace la única pregunta que no se puede contestar por partes:

> **Con el SPWM generando, el ADC muestreando tres canales, el I2C hablando con el AUX y el comparador marcando los cruces de la red — ¿alcanza el tiempo, y le toca a cada quien lo que le toca?**

Todo lo que hemos escrito del presupuesto del ADC descansa en **un número que seguimos estimando y nunca medimos**: cuánto dura de verdad la ISR del SPWM. De ese número dependen el margen de [DC-024](decisiones_criticas.md#dc-024-análisis-armónico-selectivo-goertzel-fuera-de-la-v1-y-qué-haría-falta-para-la-v2), el reparto de [DC-025](decisiones_criticas.md#dc-025-reparto-del-adc-entre-los-canales-tres-prioridades-y-por-qué-caben) y el presupuesto de [DC-017](decisiones_criticas.md#dc-017-presupuesto-de-cpu-y-memoria-de-los-atmega328p-y-alternativa-en-el-mismo-socket-nano-every-atmega4809).

Esta parte absorbe la prueba **B5**.

## C0. La condición de seguridad: J15 desconectado

En el CRÍTICO **no sobra un solo pin**: los veinte están asignados, D0–D13 y A0–A7. El pin de traza tiene que salir de algún lado, y sale de las tres órdenes a la tarjeta de transferencia:

| Pin | Puerto | Red | Qué manda normalmente |
| --- | --- | --- | --- |
| **D4** | PD4 | `INV_D4` | INV_CMD — la casa pasa al inversor (K4 + K2) |
| **D5** | PD5 | `CARG_CMD` | CARG_CMD — el cargador (K1 + K3) |
| **D13** | PB5 | `TN_CMD` | TN_CMD — la unión N–PE (K5) |

Y hay un cuarto, que **sólo sirve en las etapas 1 a 4**:

| Pin | Puerto | Red | Qué manda normalmente |
| --- | --- | --- | --- |
| **D12** | PB4 | `D12_ON_OF` | EN_HW — el permiso del puente, vía R25 y el interruptor J4 |

**Por qué se puede.** U4 es un **74HC08** y cada salida es `PWM AND EN_HW` (sección 12 del README de la tarjeta). El 74HC08 está **aguas abajo** del microcontrolador, y este protocolo mide **en el pin del micro**. Alternar D12 recorta lo que sale del 74HC08, pero **no toca lo que medimos**.

**Por qué no en la etapa 5.** Ahí el puente tiene que conmutar de verdad, así que EN_HW debe quedarse **alto y quieto**. En esa etapa D12 regresa a su trabajo.

> ⚠️ **D12 como traza sólo con la etapa de potencia desconectada.** Con los gate drivers puestos y el bus energizado, alternar EN_HW a 24 kHz picaría las compuertas. Es la misma condición que ya tienen las etapas 1–4, pero conviene decirla aparte porque este pin **sí** llega al puente.
>
> Efecto lateral: **D8 (`EN_SENSE`) va a leer basura** mientras D12 sea traza, porque R38 lo trae de EN_HW. Si el firmware vigila que EN_SENSE concuerde con D12, hay que desactivar esa vigilancia en las etapas 1–4 — o va a disparar sola.

Desconectando **J15**, los tres quedan libres y no mandan nada a ningún relé. Son puertos directos, así que `PORTD |= (1<<PD4)` son **2 ciclos, 125 ns**: la sonda no distorsiona lo que mide.

> ⚠️ **J15 desconectado durante toda la Parte C.** Con la tarjeta de transferencia puesta, alternar `CARG_CMD` a 24 kHz sería castañetear K1 y K3 cuarenta y ocho mil veces por segundo. No es un detalle de procedimiento: es **la** condición de la prueba. Antes de energizar, verificar a ojo que J15 está fuera.

Consecuencia que hay que tener presente: **con J15 fuera no hay transferencia**, así que la salida del inversor no llega a la casa en ninguna etapa de la Parte C. Es lo correcto para banco.

## C1. El presupuesto que hay que probar

A 16 MHz, con el Timer1 en modo 8 y TOP = ICR1 = 333:

```
periodo de portadora   41.625 µs   =  666 ciclos
conversión del ADC    104 µs       (reloj /128 = 125 kHz, 13 ciclos de ADC)
disparo del ADC       cada 4 periodos = 166.5 µs  → 62.4 % ocupado, 62.5 µs libres
ISR del comparador    cada 8.33 ms  = cada 200 periodos
```

**El AVR no anida interrupciones.** La que está corriendo bloquea a las demás. Entonces el caso peor no es el promedio de ocupación — es la **colisión**: que en un mismo periodo de portadora caigan encima la ISR de Timer1, la del ADC, la de TWI y la del Timer0 de `millis()`.

El criterio duro sale de ahí:

```
T(Timer1) + T(ADC) + T(TWI) + T(Timer0)  <  41.625 µs
```

Si la suma se pasa, **se pierde una actualización de OCR** y el SPWM salta un paso de la tabla. El criterio de diseño va más apretado, para dejar margen a lo que todavía no existe:

| | Umbral | Por qué |
| --- | --- | --- |
| **Pase** | suma ≤ **29 µs** (70 % del periodo) | queda 30 % para crecer |
| **Aviso** | 29–37 µs (70–90 %) | cabe hoy, no cabe mañana: congelar funciones nuevas |
| **Falla** | > 37 µs | optimizar antes de seguir |

## C2. Las dos piernas fallan distinto — y por eso se miden distinto

Esto es lo que vuelve útil a la Parte C, y es fácil pasarlo por alto:

**Pierna A (HA/LA, D9/D10) — la genera el hardware.** `OCR1A` y `OCR1B` están de doble búfer y se actualizan en BOTTOM. Una ISR que entra **tarde pero termina** antes del siguiente BOTTOM produce pulsos **perfectos**: el jitter de entrada no se ve en el pin. Lo que sí se ve es la ISR que **no termina**: ahí se pierde la actualización y el ancho del periodo anterior **se repite**.

> **Firma de falla de la pierna A: no es un pulso deforme, es un paso de la onda que no se actualizó.** Si se busca «pulsos raros» en el analizador no se encuentra nada y se da por buena una prueba que falló.
>
> ⚠️ **Corregido el 22-nov-2026.** Aquí decía que se detectaba buscando «anchos repetidos» en el analizador. **Eso no funciona:** cerca del pico de la onda el paso de la tabla tiende a cero, así que los anchos **se repiten solos, sin que haya falla**. Buscarlos ahí da falsos positivos garantizados, y cerca del cruce por cero el paso son 307 ns, que un analizador barato ni resuelve.
>
> **El detector bueno va en software y es exacto.** El micro limpia su bandera de desborde al entrar a la rutina; si al salir está puesta otra vez, es que hubo un segundo desborde mientras seguíamos adentro — o sea, no terminamos a tiempo. Una línea:
>
> ```c
> if (TIFR1 & _BV(TOV1)) sobrepasos++;
> ```
>
> Sin umbral, sin falsos positivos y sin analizador. Implementado en `ISLA_CRITICO_PRUEBA_ISR_SPWM`; en la etapa 1 dio **cero** en las cinco corridas.

**Pierna B (HB/LB, D11/D3) — la escribe el software dentro de la ISR.** Esos pines cargan el jitter de entrada **uno a uno**. Y el error cae justo en el cruce por cero, que es donde nace la asimetría entre semiciclos.

Eso no es cosmético. En el estudio de A6 quedó medido que el RMS de medio ciclo es exacto a **0.0000 %** para cualquier onda con sólo armónicos impares — y que se rompe con armónicos pares (−1.65 %) y con CD (+6.15 %). **La asimetría de la pierna B es exactamente lo que produce armónicos pares y CD.** O sea: el jitter de esta pierna degrada la onda *y además* envenena el instrumento que la mide.

| Pierna | Qué la daña | Qué se mide | Criterio |
| --- | --- | --- | --- |
| **A** (24 kHz) | ISR que no termina | contador `sobrepasos`, en software | **cero** en 60 s |
| **B** (60 Hz) | jitter de entrada a la ISR | duración de semiciclos consecutivos | diferencia ≤ **20 µs** (0.24 %) |

El criterio de la pierna B tiene comprobación cruzada gratis: el propio firmware puede comparar el RMS de semiciclos consecutivos. Si el cobre dice 20 µs y el RMS dice asimetría, coinciden dos medidas independientes.

## C3. Las ocho puntas

Con D12 sumado, **las cuatro ISR se ven al mismo tiempo** y no hay que reasignar nada entre etapas:

```
                    ETAPAS 1–4                        ETAPA 5
CH1   D9    HA      pierna A, lado alto 24 kHz        igual
CH2   D10   LA      pierna A, lado bajo 24 kHz        igual   ← nunca traslapados
CH3   D11   HB      pierna B, lado alto 60 Hz         igual
CH4   D3    LB      pierna B, lado bajo 60 Hz         igual
CH5   D4    ISR de Timer1                             igual
CH6   D5    ISR del ADC                               igual
CH7   D12   ISR de TWI                                A5 (SCL) — D12 vuelve a EN_HW
CH8   D13   ISR del comparador                        igual
```

**El SCL deja de hacer falta en las etapas 1–4.** Sólo decía *cuándo* ocurrió el I2C; la traza de TWI dice **cuándo y cuánto costó**, que es lo que buscamos. En la etapa 5, cuando D12 regresa a ser EN_HW, el SCL vuelve al CH7: ahí ya conocemos el costo de TWI de la etapa 3 y sólo necesitamos saber si había tráfico encima de una anomalía.

Poder ver las cuatro ISR juntas es lo que permite cachar la colisión de tres —Timer1 + ADC + TWI en el mismo periodo— que es el caso peor del presupuesto de C1. Reasignando un canal, ese evento no se veía completo.

Probar en el **pin del microcontrolador**, no después de R39/R25/R38: el resistor y el LED del Nano redondean los flancos sin agregar información.

## C4. Los tres parches de firmware

**1. Las trazas.** Macros, no funciones, para que no quede una llamada:

```c
#define TRZ_T1_ALTO    (PORTD |=  (1 << PD4))
#define TRZ_T1_BAJO    (PORTD &= ~(1 << PD4))
#define TRZ_ADC_ALTO   (PORTD |=  (1 << PD5))
#define TRZ_ADC_BAJO   (PORTD &= ~(1 << PD5))
#define TRZ_TWI_ALTO   (PORTB |=  (1 << PB4))   // D12, solo etapas 1-4
#define TRZ_TWI_BAJO   (PORTB &= ~(1 << PB4))
#define TRZ_CMP_ALTO   (PORTB |=  (1 << PB5))
#define TRZ_CMP_BAJO   (PORTB &= ~(1 << PB5))
```

Primera línea y última línea de cada ISR. **Los 125 ns de la propia traza se restan al final**; se miden una vez con una ISR vacía.

**2. El contador de cruces rechazados — sin esto la etapa 5 no prueba nada.** El sketch del comparador trae el candado:

```c
if (t - ultimoCruce < CANDADO_DESB) return;      // 280 desbordes = 11.65 ms
```

Ese candado **se traga los cruces espurios en silencio**. Si la conmutación del puente se acopla a la pista de 44 mm que corre junto a `AC_FB_GRID_A6`, el comparador dispara de más, el candado lo tapa, y el reporte sale limpio — con nosotros creyendo que no hay acoplamiento. El parche es una palabra:

```c
if (t - ultimoCruce < CANDADO_DESB) { rechazados++; return; }
```

y `rechazados` al reporte. **En etapas 1–4 debe ser 0. Lo que aparezca en la etapa 5 es acoplamiento, medido.**

**3. Contadores por canal, para «equilibrado».** [DC-025](decisiones_criticas.md#dc-025-reparto-del-adc-entre-los-canales-tres-prioridades-y-por-qué-caben) reparte `A0, A6, A0, A3` → 50.1 / 25.0 / 25.0 muestras por ciclo. Un contador por canal en el reporte convierte eso de intención en medición.

## C5. Las cinco etapas

Prenderlo todo de golpe y ver jitter no sirve: no se sabe quién lo causó. **Cada etapa suma un contribuyente, y la resta entre dos etapas es el costo de ese contribuyente.**

| Etapa | Qué corre | Qué contesta | D12 |
| --- | --- | --- | --- |
| **C1** | SPWM solo | **B5**: la ISR limpia, sin nadie estorbando | traza |
| **C2** | + ADC (3 canales) | cuánto cuesta el ADC, y si el reparto se cumple | traza |
| **C3** | + I2C con el AUX | cuánto cuesta TWI — **la incógnita grande**, y quedó sin medir | traza |
| **C4** | + red y comparador | si la medición coincide con el Fluke | traza |
| **C5** | + etapa de potencia conmutando | acoplamiento al canal de red | **EN_HW**, alto y quieto |

El cambio de la etapa 5 no es un detalle de instrumentación: **es la frontera entre "el puente no puede encender" y "el puente está conmutando".** Mientras D12 sea traza, EN_HW anda picado y el 74HC08 no deja pasar nada completo — que es justamente lo que vuelve seguras a las etapas 1–4.

### C5.1 Etapa 1 — el SPWM solo

Sin ADC, sin I2C, sin comparador. Es **B5** en estado puro.

**Criterio.** El ancho de CH5 es el valor de referencia de todo lo demás. Anotarlo con tres cifras. Si ya aquí pasa de 29 µs, no tiene caso seguir: hay que optimizar primero la multiplicación de 32 bits.

### C5.2 Etapa 2 — entra el ADC

**Criterio.** (a) Ancho de CH6 ≤ **15 µs**. (b) Suma CH5 + CH6 ≤ 29 µs cuando se traslapan. (c) `sobrepasos` = **0** durante 60 s. (d) Los contadores por canal dan 50/25/25 ± 1 % — eso es «equilibrado».

### C5.3 Etapa 3 — entra el I2C

⚠️ **Corregido por la corrida del 23 de noviembre.** Esta etapa se diseñó con el CRÍTICO de **esclavo**, como lo había dejado DC-010 («el CRÍTICO nunca espera al bus»), y **falló**: perdió pasos de la onda. Ahora el CRÍTICO es **maestro del bus** y el AUX esclavo en 0x21 — pero **maestro del bus no es quien manda**: el que manda sigue siendo el AUX, y el CRÍTICO nada más reporta su estado y recoge órdenes ([DC-027](decisiones_criticas.md#dc-027-el-enlace-i2c-al-revés-el-crítico-es-maestro-del-bus-y-maestro-del-bus-no-es-quien-manda)). Los resultados están en la sección de las etapas 2 y 3, más abajo.

En cualquiera de los dos papeles, **la ISR de TWI corre en el CRÍTICO**, una vez por byte, y cae donde caiga.

A 100 kHz un byte son 90 µs de bus — más que un periodo de portadora. Durante una trama hay una ISR de TWI cada ~2.2 periodos. **El riesgo no es la ocupación promedio, que es mínima: es la colisión.**

**Criterio.** (a) Ancho de CH7 (TWI) anotado. (b) La suma de las tres en el peor traslape observado ≤ 29 µs. (c) `sobrepasos` = **0** en 60 s — un minuto, no diez segundos, porque la colisión es rara y hay que darle oportunidad de ocurrir. (d) 0 errores de CRC, que enlaza con **B3**.

### C5.4 Etapa 4 — entra la red y el comparador

**Criterio.** (a) `cruces` = **120/s exactos** (60 Hz × 2). (b) `rechazados` = **0**. (c) Frecuencia dentro de **0.01 Hz** del reporte de la Parte A. (d) RMS dentro de **±0.3 V** del Fluke, recordando que el Fluke sólo garantiza ±2.30 V a 120 V y no avala el dígito de las décimas. (e) σ del RMS en el orden de los **12.5 mV** que dio el comparador en A6; si se disparó, algo la está estorbando.

### C5.5 Etapa 5 — conmutando de verdad

La etapa de potencia trabajando, con J15 todavía fuera.

**Criterio.** (a) `rechazados` **sigue en 0** — cualquier cosa distinta es acoplamiento, y ya sabemos dónde buscarlo: los 44 mm de pista paralela. (b) Las dos piernas conservan su tiempo muerto; **HA y LA nunca altos a la vez**, ni un solo flanco. (c) Diferencia entre semiciclos consecutivos ≤ 20 µs. (d) El RMS de la red no se mueve cuando el puente arranca: si se mueve, el canal de red está viendo al inversor y no a la red.

## C6. Si no cabe: el orden de los remedios

Del más barato al más caro. **No saltarse pasos**: cada uno se mide antes de pasar al siguiente.

| # | Remedio | Costo |
| --- | --- | --- |
| 1 | Sacar de la ISR de Timer1 todo lo que no sea tiempo-crítico | horas |
| 2 | Quitar la multiplicación de 32 bits de la ruta caliente (tabla pre-escalada) | un día |
| 3 | Subir el disparo del ADC de 4 a 5 o 6 periodos | pierde muestras por ciclo; choca con DC-025 |
| 4 | Bajar el tráfico I2C: menos tramas por segundo, o tramas más cortas | toca el protocolo v2 (**B3**) |
| 5 | Bajar la portadora de 24 kHz | toca [DC-018](decisiones_criticas.md#dc-018-modulación-híbrida-a-24-khz-también-en-el-cargador-y-socket-multimodelo-atmega328p--atmega4809) y el ruido audible |
| 6 | Nano Every (ATmega4809), mismo socket | la alternativa que [DC-017](decisiones_criticas.md#dc-017-presupuesto-de-cpu-y-memoria-de-los-atmega328p-y-alternativa-en-el-mismo-socket-nano-every-atmega4809) dejó abierta |

Que el remedio 6 exista y esté en el mismo socket es la razón de que esta prueba no dé miedo: si no cabe, **ya hay plan**.

## C7. Hoja de registro de la Parte C

| | Etapa 1 | Etapa 2 | Etapa 3 | Etapa 4 | Etapa 5 |
| --- | --- | --- | --- | --- | --- |
| ISR Timer1 (µs) | | | | | |
| ISR ADC (µs) | — | | | | |
| ISR TWI (µs) | — | — | | | — |
| ISR comparador (µs) | — | — | — | | |
| **Peor suma observada (µs)** | | | | | |
| `sobrepasos` (pasos de onda perdidos) | | | | | |
| Dif. entre semiciclos (µs) | | | | | |
| Muestras A0 / A6 / A3 por ciclo | — | | | | |
| `cruces` por segundo | — | — | — | | |
| **`rechazados`** | — | — | — | | |
| RMS red vs Fluke (V) | — | — | — | | |
| Errores de CRC | — | — | | | |

**Antes de empezar (etapas 1–4):** J15 fuera ☐ · etapa de potencia desconectada ☐ · trazas compiladas ☐ · vigilancia de `EN_SENSE` desactivada ☐ · `rechazados` en el reporte ☐ · contadores por canal en el reporte ☐ · ISR vacía medida para restar los 125 ns ☐

**Antes de la etapa 5:** D12 devuelto a **EN_HW** ☐ · vigilancia de `EN_SENSE` reactivada ☐ · CH7 movido a SCL ☐ · J15 **sigue** fuera ☐

---

# C8. Resultado de la etapa 1 — 22 de noviembre de 2026

## La pregunta, en una línea

El micro tiene que dibujar la onda **24 024 veces por segundo**. Cada vez dispone de **41.6 µs**. La pregunta de B5 era: *¿cuánto de esos 41.6 µs se gasta de verdad en dibujar?*

Nunca lo habíamos medido. Lo estimábamos.

## La respuesta

```
            ocupado 12.75 µs                      libre 28.9 µs
    |###############################|.................................................|
    0                              30.6 %                                         41.6 µs
```

**Se gasta 12.75 µs — el 30.6 %.** Quedan dos terceras partes libres para el ADC, el I2C y lo que venga.

Tres cosas le dan peso al número:

- **Dos Nano físicos distintos dieron exactamente lo mismo**, hasta el último ciclo. No fue suerte de una tarjeta.
- **Antes de ir al banco lo simulamos** compilando el código real y corriéndolo en un simulador de AVR. La simulación dijo 12.07 µs y el banco dijo 12.75. **Difieren 1.6 %.** Dos caminos independientes llegando al mismo lugar.
- **Nunca se perdió un paso de la onda.** El contador de fallas marcó **cero** en las cinco corridas, incluso en la que saboteamos a propósito.

> **Por qué dos tarjetas dan lo mismo hasta el último ciclo.** No es sospechoso: en esta prueba no hay nada que llegue de afuera. Los dos temporizadores y el puerto serie cuelgan del mismo reloj y arrancan a la misma instrucción después del *reset*. El sistema es repetible, y de paso eso confirmó que la segunda tarjeta trae un ATmega328P auténtico: un clon LGT8F328P habría dado otros tiempos.

## Lo que encontramos sin buscarlo

Al medir, salió que **la rutina de la onda a veces arranca tarde**. No porque tarde más, sino porque alguien más tiene agarrado al micro y no la deja entrar.

Para entender por qué, hace falta una idea sola:

> Cuando un programa necesita leer una variable que la rutina de la onda también toca, **cierra la puerta** un momento para que nadie se la cambie a media lectura. Mientras la puerta está cerrada, **la rutina de la onda espera afuera**, aunque ya le tocara entrar.

Cerrar la puerta es correcto y hace falta. El problema es **cuánto tiempo la dejas cerrada y cuántas veces por segundo la cierras**.

### El experimento

Dejamos el mismo programa y **sólo cambiamos cuántos datos se leen con la puerta cerrada**:

| Qué se leía con la puerta cerrada | Puerta cerrada | ISR que llegaron tarde |
| --- | --- | --- |
| 11 bytes (dos contadores grandes de más) | ~40 ciclos | **8.9 %** |
| **5 bytes, lo mínimo** | **~20 ciclos** | **0.54 %** |
| 5 bytes + 4 lecturas de adorno, a propósito | ~66 ciclos | **29.3 %** |

**De 0.54 % a 29.3 % es un factor de 54, y la única diferencia son cuatro lecturas.**

Y se ve clarito en el reparto de las esperas. Cada columna es una corrida; cada renglón, cuánto tuvo que esperar la rutina:

```
   esperó     puerta ~40c   puerta ~20c   puerta ~66c
   nada          41.1 %        62.2 %        42.3 %
   1-10 c        40.4 %        32.5 %        26.4 %
   11-20 c       11.8 %         4.8 %         2.3 %
   21-40 c        6.3 %         0.2 %        10.4 %   ← se vacía y se vuelve a llenar
   41-70 c        0.2 %         0.2 %        17.5 %   ← sólo aparece con la puerta ancha
   71-110 c       0.2 %         0.2 %         1.1 %
   más de 110     0.0 %         0.0 %         0.0 %   ← nunca, en ninguna
```

**Las esperas largas aparecen y desaparecen junto con el ancho de la puerta.** Eso es lo que prueba de dónde vienen: no hay que creerlo, se ve.

### Dos cosas que no son culpables

- **El reloj del sistema** (`millis()` de Arduino) también cierra la puerta, pero sólo 976 veces por segundo. Sale en el renglón de 71-110 y **se queda en 0.2 % en todas las corridas**. Es el retraso más largo, pero es tan raro que no estorba.
- **La espera de 1 a 10 ciclos es inevitable y no importa.** El micro no puede interrumpir a media instrucción: si justo estaba ejecutando una, la termina. Son uno o dos ciclos y no hay nada que hacer, ni falta.

## La regla que sale de aquí, y es para el firmware de verdad

> **El daño no se mide en tiempo total, se mide así:**
>
> **cuánto dura cada cierre × cuántas veces por segundo cierras**

Lo contraintuitivo, y lo comprobamos sin querer: **hacer el lazo más lento mejoró las cosas**. Cuando le agregamos trabajo al programa, dio menos vueltas por segundo, cerró menos veces la puerta, y el retraso **bajó a la mitad**. Un lazo rapidísimo que anda preguntando todo el tiempo es lo peor que se puede hacer.

Cuatro reglas prácticas para el firmware de control:

1. **Con la puerta cerrada, lee lo mínimo.** Nada más lo que cambia en cada interrupción.
2. **Los contadores grandes, fuera de la ruta rápida.** Léelos de vez en cuando, no en cada vuelta. En esta prueba pasaron de consultarse 55 000 veces por segundo a 46, y eso solo bajó el retraso 16 veces.
3. **Una bandera de un byte no necesita cerrar la puerta.** El micro la lee de un golpe y no se puede partir a la mitad.
4. **Cuidado con los lazos que encuestan.** Entre más vueltas dé, más veces cierra.

Esto pesa más que cualquier cosa que hayamos discutido de `millis()`, y es exactamente lo que el firmware de control va a hacer si no lo cuidamos: un lazo rápido leyendo variables que comparte con el ADC y el comparador.

## Lo que vio el analizador lógico

Medido con PulseView y un analizador FX2 de 8 canales a **16 MS/s**. Salió una coincidencia que ayuda mucho: el analizador y el micro corren a la misma frecuencia, así que **una muestra del analizador = una cuenta del temporizador**. Las lecturas no son aproximadas, son contadas.

### Tiempo muerto de la pierna A: 1.000 µs

```
LA baja   →  4 997 500 ns
              ← 1000 ns de hueco →
HA sube   →  4 998 500 ns
```

Lo esperado eran **16 cuentas × 62.5 ns = 1.000 µs**, y eso dio, exacto. **HA y LA nunca están altos al mismo tiempo**: LA baja antes de que HA suba, con el hueco completo en medio.

### Tiempo muerto de la pierna B: un periodo de portadora, y es a propósito

La pierna B no tiene 1 µs, tiene **42**. No es una falla: está en el código.

```c
if (muestra == ultimaMuestra) {   // última muestra del semiciclo
    ... apaga el lado que venía conduciendo
} else if (muestra == 0) {        // primera muestra del siguiente
    ... enciende el otro lado
}
```

Apaga en la última muestra y enciende en la primera del semiciclo siguiente: **un periodo de portadora completo en medio**, por diseño.

| | µs | sobra sobre el periodo |
| --- | --- | --- |
| Periodo de portadora (teórico) | 41.625 | — |
| Cruce 1 | **42.187** | +562 ns = **9 ciclos exactos** |
| Cruce 2 | **41.813** | +188 ns = **3 ciclos exactos** |

### Los ciclos que sobran son el jitter, confirmado por segunda vía

Las dos escrituras están a tres instrucciones una de otra dentro de la rutina. Si las dos entradas fueran igual de puntuales, el hueco sería **exactamente** un periodo. Lo que sobra es **la rutina que entró tarde**.

Y cae justo donde el histograma dijo que debía caer: ahí el 52 % de las entradas se retrasan entre 1 y 20 ciclos, y estas dos dieron **9 y 3**.

> **Es la misma medición por un camino completamente distinto.** El histograma la sacó contando con el temporizador desde adentro del micro; esto la sacó midiendo cobre con un instrumento externo. Coinciden. El jitter de entrada no es una teoría nuestra: se ve en los pines.

Y no son 8.7 ni 3.4 ciclos — son **9 y 3 clavados**, porque la cuantización es de un ciclo.

### Simetría de la pierna B: pasa con 53 veces de margen

```
criterio    ≤ 20 µs
medido       0.374 µs   (6 ciclos exactos)
```

Hay una razón de fondo para que salga tan bien: **la duración del semiciclo no se mide, se cuenta.** El firmware lleva 200 muestras por semiciclo y cambia de signo al llegar a 200 — siempre 200, nunca 199 ni 201. Los dos semiciclos duran lo mismo **por construcción**, no por ajuste. Lo único que puede variar es dónde cae el flanco dentro de la rutina, y eso son los 374 ns.

Encima, esos 374 ns ocurren **en el cruce por cero**, donde la tensión instantánea es casi nula y el puente está apagado de todos modos. La componente de CD que inyectan es todavía menor de lo que sugiere la proporción de tiempo. Y la CD importa: es lo único que el cálculo de RMS por medio ciclo **no** puede absorber (+6.15 % de error).

### El reloj, por fin medido contra una referencia de afuera

Hasta aquí todo se había medido con el reloj del micro midiéndose a sí mismo, que no prueba nada. El analizador tiene **su propio cristal**, así que por primera vez hay con qué comparar.

Con el decodificador **Timing** sobre la pierna B, en **dos canales distintos** de la misma captura:

```
Timing: Averages    16.655 ms  (60.043 Hz)     ← idéntico en los dos canales
```

**La salida del inversor es de 60.043 Hz.** Y eso se descompone en dos cosas que conviene no confundir:

| | | |
| --- | --- | --- |
| Lo que el firmware genera por diseño | **60.060 Hz** | cuantización, no error |
| Lo que midió el analizador | **60.043 Hz** | |
| Diferencia = error de reloj | **0.030 %** | el cristal anda en ~15.9952 MHz |

**Los 60.060 Hz no son una falla, son aritmética.** Con TOP = 333 y 400 muestras por ciclo:

```
16 000 000 / (2 × 333 × 400) = 60.0601 Hz
```

No hay nada que ajustar ahí: el número cae donde cae.

**Y el cristal resultó bueno.** DC-017 anota que el Nano usa resonador, y un resonador anda en ±0.5 %. Esto está **dieciséis veces mejor**. Parte de esos 300 ppm pueden ser del cristal del propio analizador —sin un tercer reloj no se separan— pero el total de los dos juntos es 0.030 %, así que ninguno puede andar peor que eso.

### El jitter rebota, no se acumula — y para el PLL eso lo es todo

Los ciclos individuales no son iguales:

```
16.650  16.651  16.653  16.655  16.655  16.655  16.656  16.658  16.659 ms
  ↑                                                               ↑
60.059 Hz                                                    60.027 Hz
```

Son **9 µs de reparto, o sea 144 ciclos de reloj** — justo el tamaño del jitter de entrada que vimos en el histograma, donde los retrasos raros por `millis()` llegaban a 128-170 ciclos. Si un ciclo tiene su flanco golpeado y el siguiente no, la diferencia sale de ese tamaño. **Es la cuarta vez que el mismo fenómeno aparece**, ahora en la frecuencia de salida.

Pero el promedio de ocho ciclos **no se mueve ni un dígito**. Eso quiere decir que el error es de colocación de flanco, no de conteo: el firmware nunca pierde ni gana una muestra, cuenta 400 y son 400.

Para el PLL la distinción lo decide todo:

- **si fuera deriva**, habría que perseguirla eternamente
- **siendo jitter acotado**, cualquier promediado lo borra y el lazo engancha firme

En la onda esos ±4.5 µs son **±0.097° de fase**. Para distorsión armónica, nada.

### Lo único que un trinchero sí va a notar: los relojes de los aparatos

El microondas, la estufa, el horno — los que cuentan el tiempo contando ciclos de la red.

```
a 60.043 Hz  →  se adelantan ~62 segundos al día
```

Un minuto diario, media hora al mes. **No es una falla**, es física: si la frecuencia va 0.07 % arriba, los relojes van 0.07 % arriba.

**Se puede mejorar, y queda anotado para la V2.** Buscando combinaciones con número par de muestras por ciclo:

| TOP | muestras | frecuencia | error | reloj de pared |
| --- | --- | --- | --- | --- |
| **333** | **400** | 60.060 Hz | +0.100 % | +86 s/día |
| 330 | 404 | **60.006 Hz** | +0.010 % | +8.6 s/día |

Diez veces mejor por diseño. No es gratis: mueve la portadora a 24.24 kHz, obliga a **regenerar la tabla senoidal** con 101 entradas por cuarto en vez de 100, y toca [DC-018](decisiones_criticas.md#dc-018-modulación-híbrida-a-24-khz-también-en-el-cargador-y-socket-multimodelo-atmega328p--atmega4809) y [DC-020](decisiones_criticas.md#dc-020-frecuencia-de-muestreo-del-adc-e-instante-del-disparo). O sea, revalidar lo que acabamos de cerrar.

Y aun arreglándolo, **el cristal sigue poniendo sus 0.030 %**, que son 26 s/día. El salto real sería de 62 a ~26 segundos diarios, no a cero.

### Dos cosas del diseño que hay que tener presentes

**1. En el cruce por cero el puente queda abierto ~83 µs, no 42.** El código también fuerza apagada la pierna A durante esas dos muestras:

```c
if (sig == 0 || sig == ultimaMuestra) { OCR1A = 0; OCR1B = TOP_CUENTAS; }
```

Con carga resistiva da igual. **Con carga inductiva la corriente tiene que irse por los diodos de cuerpo durante esos 83 µs.** Hay que vigilarlo en la **etapa 5**, cuando el puente conmute de verdad: es donde aparecerían los picos si algo está mal dimensionado.

**2. Van a faltar pulsos cerca del cruce, y está bien.** El código los quita a propósito:

```c
if (d < PULSO_MIN_CUENTAS) d = 0;   // pulso demasiado angosto: no se genera
```

Si en el analizador aparecen periodos sin pulso cerca del cruce por cero, **no es una falla** — es el firmware evitando pulsos tan angostos que el driver no alcanzaría a formar. Queda anotado para que nadie lo reporte como problema.

## Lo que queda abierto de la etapa 1

**Una cuenta que no cierra.** El ancho de la puerta medido por dónde se cortan las esperas dice ~70 ciclos, pero la *cantidad* de retrasos dice como el doble. Las dos no pueden ser ciertas. Lo más probable es que el compilador del IDE genere ese pedazo distinto de como lo contamos aquí. Se cierra pidiéndole al IDE el binario compilado y mirándolo. **No cambia ninguna conclusión** — el mecanismo quedó probado en tres puntos — pero queda anotado.

**~~Los dos renglones del analizador.~~** **Hechos el 22 de noviembre** — ver la sección de arriba. Ya no queda nada pendiente de la etapa 1.

## Tabla para la hoja de registro

| | Resultado | Criterio | Margen |
| --- | --- | --- | --- |
| Duración de la rutina de la onda | **204 ciclos = 12.75 µs = 30.6 %** | ≤ 70 % | 2.3× |
| Peor ocupación observada | 354 ciclos = 22.13 µs = 53.2 % | ≤ 70 % | 1.3× |
| Pasos de onda perdidos | **0** | 0 | — |
| ISR que llegaron tarde (código limpio) | 0.54 % | — | — |
| Tiempo muerto pierna A | **1.000 µs**, sin traslape | ~1 µs | exacto |
| Tiempo muerto pierna B | 42.187 / 41.813 µs | un periodo, por diseño | +9 / +3 ciclos |
| **Asimetría entre semiciclos** | **0.374 µs** | ≤ 20 µs | **53×** |
| Frecuencia de salida (promedio) | **60.043 Hz** | — | 60.060 por diseño |
| Error del cristal | **0.030 %** | — | resonador sería ±0.5 % |
| Jitter de ciclo a ciclo | ±4.5 µs = ±0.097° | — | no se acumula |

**Veredicto: PASA en todo.** B5 cerrado con dos métodos independientes de acuerdo al 1.6 %, y el jitter de entrada confirmado por dos caminos distintos. Queda margen para abrir la etapa 2.

---

## Etapas 2 y 3: entran el ADC y el I2C

Corridas del 22 y 23 de noviembre de 2026, con el mismo instrumento de la etapa 1 y los mismos dos Nano.

### El ADC entra y no se nota

| | Etapa 1 (SPWM solo) | Etapa 2 (+ ADC) |
| --- | --- | --- |
| Costo propio | 204 c = 30.6 % | **218 c = 13.63 µs = 32.7 %** |
| Cuerpo más corto | 49 c | **57 c** (predicho: 57) |
| Pasos perdidos | 0 | **0** |
| Disparos de ADC | — | **6 003/s**, reparto 2:1:1 exacto |
| Disparos perdidos | — | **0** |

El cuerpo mínimo cayó donde la cuenta decía que iba a caer, al ciclo. Y el disparo del ADC quedó en 6 006 Hz nominales por construcción: un disparo cada cuatro desbordes de la portadora ([DC-020](decisiones_criticas.md#dc-020-reloj-del-adc-a-128-y-muestreo-amarrado-a-la-portadora-precisión-antes-que-velocidad)).

**Lo que nadie diseñó y conviene saber.** Una conversión son 104 µs y el periodo de portadora 41.625 µs: 104 ÷ 41.625 = **2.4985 periodos**. O sea que la ISR del ADC termina siempre a medio periodo, unos 20.9 µs después del desborde y 20.9 µs antes del siguiente. **La colisión con la ISR del SPWM se evita por construcción, y fue por casualidad.** En los 24 reportes de la etapa 2 el cajón de 161–220 ciclos quedó vacío, que es exactamente lo que predice ese 2.4985. Queda anotado porque **es frágil**: cambiar el TOP de la portadora o el preescalador del ADC lo rompe sin avisar.

### La etapa 3 falló — el primer FALLA de toda la Parte C

Con el CRÍTICO de **esclavo** del bus, como lo había dejado DC-010:

```
SOBREPASOS = 2, 4 y 6   en tres corridas    ≈ 0.2 – 0.35 pasos perdidos por segundo
```

No era falsa alarma, y la cuenta cierra sola:

```
entrada 491  +  cuerpo 140  +  epílogo 35  =  666 ciclos  =  el periodo exacto
```

Para llegar a una entrada de 491 ciclos hacía falta apilar dos ladrones, y había los dos:

| | ancho | de dónde salió |
| --- | --- | --- |
| copia de la foto de estado bajo `cli()` | **212 c** | contado en el binario del módulo del enlace |
| ISR de TWI del núcleo de Arduino | ~150 c | estimada, nunca medida (ver más abajo) |

Es la «colisión de tres» que advirtió C1, ocurriendo por primera vez. Ninguno de los dos solo alcanza; juntos sí. Y el aviso temprano funcionó como estaba diseñado: `dobladas` —las lecturas del cronómetro que pasaron el TOP— acompañó al fallo en proporción ~6:1 y separó limpio por ocupación (423 ciclos cuando había dobladas, 360 cuando no).

### Lo que se hizo: voltear los papeles del bus

El CRÍTICO pasó a ser **maestro del bus**, el AUX a esclavo en 0x21. El por qué completo está en [DC-027](decisiones_criticas.md#dc-027-el-enlace-i2c-al-revés-el-crítico-es-maestro-del-bus-y-maestro-del-bus-no-es-quien-manda); aquí lo que importa para el banco:

> ⚠️ **Maestro del bus no es quien manda.** El que manda sigue siendo el AUX: es el panel, pide los perfiles, trae los permisos del BMS y tiene el AUX_KILL por hardware. El CRÍTICO lleva el reloj del bus y en cada transferencia nada más **reporta su estado y recoge órdenes**. Es maestro por una razón mecánica: el que abre la transferencia es el único que puede cerrarla y el único que tiene el reloj para destrabar el bus.

Y de paso **desaparece el `ATOMIC_BLOCK` de 212 ciclos**: de maestro la foto se arma y se manda en la misma llamada, nadie más la lee, y no hay carrera que proteger. Un cambio, dos problemas.

### Las tres corridas de carga

Subiendo el tráfico a propósito con `ENLACE_MS_ENTRE_ENVIOS`:

| transferencias/s | bus ocupado | `vueltas/s` predicho | `vueltas/s` medido | pasos perdidos |
| --- | --- | --- | --- | --- |
| 10 (producción) | 2.6 % | — (referencia) | 22 016 | **0** en 45 reportes |
| 48.4 | 12.8 % | 19 712 | 19 700 | **0** en 19 reportes |
| 192 | 49.8 % | 11 348 | 10 752 | **0** en 22 reportes |

A 192 transferencias/s —**20× el tráfico de producción, con la mitad del lazo bloqueado**— son ~528 000 periodos de portadora sin perder uno. En las tres: `PERDIDAS = 0` en el ADC, `CRC malo = 0` en los dos micros, `PROPIO = 218 c` y `OCUPA` peor ~51 %.

La diferencia de 5 % en el tercer punto tiene nombre: armar la foto y calcular el CRC-8 de 15 bytes **no bloquea, pero corre 19 veces más seguido** — unos 2 200 ciclos ≈ 137 µs por transferencia.

### ⚠️ Lo que este instrumento NO puede medir: la ISR de TWI

C5.3 decía que la ISR de TWI se iba a medir «por su efecto», con la técnica del corte del histograma que quedó calibrada en la etapa 1. **No funcionó, y hay que decir por qué.**

Primero pareció que sí: con el enlace corriendo, el cajón de 111–160 ciclos pasó de ~2 cuentas (etapa 2) a ~36 por segundo. De ahí salió una estimación de ~112 ciclos por ISR de TWI. **La corrida de 48.4/s la tumbó:** con cinco veces el tráfico el cajón no creció — se quedó plano (~30), y a 192/s bajó a ~19. Normalizado por muestra recogida: 1.68, 1.59 y 1.8 por mil. **Plano. Ese cajón no mide TWI.**

**Por qué el instrumento es ciego justo ahí.** El lazo sólo recoge una muestra cuando está corriendo. Durante los 2.63 ms que dura una transferencia el lazo está detenido dentro de `endTransmission()`, así que **las ~63 ISR que corren en esa ventana no se recogen: se sobreescriben.** Las muestras interesantes son precisamente las que se pierden.

La cuenta cuadra en tres puntos, y eso vuelve la ceguera una limitación **medida** y no una sospecha:

| transferencias/s | muestras no recogidas, predicho | medido |
| --- | --- | --- |
| 10 | 10.9 % (8.3 % de base + 2.6 % de ventana ciega) | 10.9 % |
| 48.4 | 21.2 % (8.3 % + 12.9 %) | 20.5 % |
| 192 | 57.9 % (8.3 % + 49.6 %) | 56.5 % |

**La diferencia con la etapa 1 es de fondo.** Ahí el ladrón era un `cli()` que **retrasaba** la ISR, y el retraso quedaba en el histograma. Un bloqueo por *llamada bloqueante* no retrasa la ISR: **esconde la muestra.** La técnica del corte no aplica a este caso.

**Queda sin medir, y se deja escrito así** en vez de dejar un número equivocado en la documentación.

### El límite duro que sí sirve

`SOBREPASOS` se cuenta con `TIFR1` **adentro de la ISR, en todas**, recogidas o no: no tiene ventana ciega. Cero sobrepasos significa que en **cada una** de esas 528 000 ISR se cumplió

```
entrada + cuerpo (140) + epílogo (35) < 666    →    entrada < 491 ciclos, siempre
```

Así que sin conocer el máximo exacto, sabemos que **nunca pasó de 491** — ni en las muestras que el lazo no alcanzó a ver. Y 491 es justo la cifra donde la etapa 3 fallaba. Lo que se quitó fue el **apilamiento**: con el `cli()` de 212 ciclos fuera, el TWI solo ya no llega.

### Tabla para la hoja de registro — etapas 2 y 3

| | Resultado | Criterio | Margen |
| --- | --- | --- | --- |
| Costo propio de la ISR del SPWM (con ADC e I2C) | **218 c = 13.63 µs = 32.7 %** | ≤ 70 % | 2.1× |
| Peor ocupación observada | 340 c = 21.25 µs = 51.1 % | ≤ 70 % | 1.4× |
| Pasos de onda perdidos, a 10/s | **0** en 45 s | 0 en 60 s | — |
| Pasos de onda perdidos, a 192/s | **0** en 22 s (~528 000 ISR) | — | 20× el tráfico |
| Entrada máxima, acotada por `TIFR1` | **< 491 c siempre** | < 491 c | límite exacto |
| Disparos de ADC perdidos | **0** en las tres corridas | 0 | — |
| Reparto del ADC | **2:1:1 exacto**, 6 003 disparos/s | 50/25/25 ± 1 % | exacto |
| Errores de CRC en el enlace (**B3**) | **0** en los dos micros | 0 | — |
| Transferencia completa | 2.63 ms | — | 2.6 % del lazo a 10/s |
| Duración de la ISR de TWI | **sin medir** | — | ver arriba |

**Veredicto: PASA.** Las etapas 2 y 3 quedan cerradas. La etapa 3 falló primero de esclavo, se corrigió con [DC-027](decisiones_criticas.md#dc-027-el-enlace-i2c-al-revés-el-crítico-es-maestro-del-bus-y-maestro-del-bus-no-es-quien-manda) y pasó con 20× de margen de tráfico.

### Lo que queda abierto de la etapa 3

- **El máximo real de la entrada no se conoce**, sólo su límite de 491 ciclos. Se cierra guardando el máximo **dentro del enganche** (`if (isr_t0 > isr_t0_max) isr_t0_max = isr_t0;`, unos 8 ciclos) en vez de muestrearlo desde el lazo. No cambia ninguna decisión de diseño: se anota para la próxima vez que se toque el instrumento.
- **La ISR de TWI sigue sin medir.** Si algún día hace falta el número, el camino es el analizador lógico mirando D4 contra SCL: la punta no es ciega durante la transferencia, el muestreo sí.
- **El doblez del cronómetro no está resuelto, sólo no se activó.** `dobladas = 0` en las tres corridas porque las entradas se quedaron lejos del TOP. El arreglo de raíz sigue siendo mover la medición del cuerpo al Timer2 libre con preescalador /1 (16 µs de alcance contra 8.8 µs de cuerpo máximo), porque entre t0 ≈ 207 y 245 **ningún umbral distingue** una lectura doblada de una buena.
- **El AUX necesita *watchdog*.** Como esclavo del bus sí puede trabarse — es el precio del cambio, y es el correcto porque no tiene trabajo de tiempo real. Pero hay que reiniciarlo solo.

---

## Lo que esta corrida NO puede validar

Se dice aquí para que nadie crea que el módulo quedó cerrado:

| Pendiente | Por qué no se puede aquí | Cuándo |
| --- | --- | --- |
| **Estabilidad del bias con el puente conmutando** | El ruido lo mete el puente con 48 V y potencia | Con el puente armado |
| Pulso mínimo con potencia real | Necesita carga | Con transformador de salida |
| Bootstrap de la pierna A | Necesita los drivers y 48 V | Con el puente armado |
| Los 81 µs del cruce por cero con corriente atrasada | Necesita carga inductiva | Con carga real |
| THD real y saturación del transformador | Necesita potencia | Con el puente armado |
| Desfase propio del transformador de 12 VAC | Necesita un patrón de fase | V3, inyección a red |

---

## Lo que esta corrida le entrega al PCB

1. **El valor definitivo del divisor AC** (A2 + A3).
2. **El reparto fijo/ajustable del divisor** (A5, con los dos transformadores): si la dispersión pasa del 8 %, el trimmer sube a 5 kΩ y cambia la resistencia fija que lo acompaña.
3. **La decisión de la protección de A7 al aire** (B1), que puede agregar una resistencia al cobre.
4. **Confirmación de que el bias por canal se aguanta solo** (A1), o la necesidad de separarlo.
5. **Tres recomendaciones que no dependen de ninguna medición**, porque cuestan casi nada y cubren la única prueba que no se puede hacer antes de imprimir (el ruido del puente conmutando):
   - **Footprint del electrolítico del bias holgado, para 220 µF.** Es la alternativa que la aritmética señaló como la buena, y poblarlo con 100 µF o con 220 µF después no cuesta nada si el espacio ya está. Un footprint para 10 µF cierra esa puerta.
   - **Footprint del seguidor de bias (LM358, dos canales) presente y puenteado con un jumper.** Es el último recurso, no el primero — el capacitor le gana a 24 kHz — pero son dos almohadillas y un jumper, y evita una reimpresión en el caso de que el problema resulte ser acoplamiento y no impedancia.
   - **Footprint de un capacitor X2 (paso 15 mm) entre L y N del primario de cada transformador, sin poblar.** Si A1 o A7 muestran que el ruido de la red está llegando al ADC, ahí se puebla. Tres advertencias que van en la serigrafía o en la nota del esquemático, porque esta posición se presta a sustituciones peligrosas:
     1. **Sólo capacitor de seguridad clase X2 certificado, 275 VAC** (IEC 60384-14). Un poliéster genérico de 250 V no sirve aunque el número alcance: no tiene modo de falla validado ni ensayo de impulso, y en posición X una falla en corto deja un arco sostenido entre L y N. El riesgo es incendio, no choque.
     2. **0.1 µF, no 1 µF.** Un microfaradio a 127 V son 47.9 mA de corriente reactiva permanente; 0.1 µF son 4.8 mA.
     3. **El mismo valor en los dos transformadores.** Poblar uno y el otro no los desfasa entre sí, y A7 lo marcaría como asimetría de filtros — mandando a revisar cobre que está bien.

   Nada de esto aplica a la placa de transferencia, que conmuta corriente de carga de verdad y necesita su propio amortiguamiento. Ahí un capacitor solo **no** es snubber: a través de un contacto que cierra en el pico de la onda, 1 µF da entre 99 y 396 A de pico según la resistencia del cableado, y eso castiga el contacto en vez de protegerlo. Un snubber lleva **R en serie** (lo típico, 100 Ω + 0.1 µF). Es otra decisión de diseño y va por su cuenta.

---

## Hoja de registro

Se llena en el banco, con lápiz, y después se pasa aquí.

| Corrida | Fecha | Valor medido | ¿Pasa? | Notas |
| --- | --- | --- | --- | --- |
| A1 bias 2.5 V | | | | |
| A2 secundario en vacío | 2026-11-20 | `Vsec` (extremo a extremo) = **14.3 V** a `Vlínea` = **120.1 V** → razón **8.3986**, `Vsec_max` (a 140 V) = **16.67 V** | ✅ | 3 hilos con tap central, identificados en tienda · medio devanado ≈ 6 V, **no se usa** (hereda dos tolerancias) · se usó el devanado completo |
| A3 resistencia elegida | 2026-11-21 | `Rt` = **110 kΩ**, montada y verificada (`DIVISOR_X100` = 1171) | ✅ | **decidido con números medidos, no estimados.** Con 100 kΩ el pin se pasa de 4.60 V a 140 V de red, con cresta senoidal. Montada como **100 k + 10 k en serie**; la `Rt` efectiva es 109.6 kΩ y el margen a 140 V queda en +67 mV |
| A4 atenuador y recorte | | tope ___ V / mínimo ___ V | | **no corrida**: faltó barrer el trimmer de tope a tope |
| A5 rango del trimmer | | ___ % | | **no corrida**: necesita el trimmer multivuelta y un segundo transformador |
| A6 ADC vs multímetro | 2026-11-21 | **−0.08 %** calibrada con `Rt` = 110 kΩ (criterio 1 %) · +0.42 % sin calibrar |  ✅ | línea 120.005 V vs Fluke 120.1 · σ = 0.069 % · `perdidos` 0 y **ni un cruce perdido** en 31 reportes · `DIVISOR_X100` 1171, `RELACION_X1000` 8399, las dos medidas y **separadas** · ver «Resultado de banco» en A6 |
| A7 simetría entre canales | | ___ µs (esperado 124.9) | | **no corrida**: necesita los DOS canales conectados. Un pin al aire lee el canal anterior, no cero |
| A8 fs e instante | 2026-11-20 | **166.5 µs** nominal / **166.8 µs** medido | ⚠️ | con 3 periodos se perdía el **17.5 %**; con 4 periodos `perdidos` = 0 y la tasa es 99.76 %. Falta explicar el **0.24 %** restante → B5 |
| B1 P9 con R reales | 2026-11-20 | 1023 / 705 / 511 / 417 cuentas (esperado 1023 / 704 / 512 / 417 con `R14` = 22 kΩ) | ✅ | los 4 estados clasifican correcto con la sustitución de 22 kΩ · **faltan los estados 5 y 6** (pin A7 al aire y a GND): el 5 decide la pregunta abierta de DC-011 |
| B2 batería en A3 | | error ___ V | | corte del BMS: ___ V |
| B3 protocolo v2 | | ___ errores / 1000 tramas | | |
| B4 LCD y botonera | | | | |
| B5 duración de la ISR | | ___ µs de 41.6 | | |
| B6 primer ciclo y pulso | | | | |

---

## Referencias

- [Decisiones críticas](decisiones_criticas.md): DC-006 (cobre del sensado AC), DC-007 (batería), DC-011 (P9), DC-014 (corte por hardware), DC-017 (presupuesto de CPU), DC-020 (reloj del ADC).
- [Base de tiempo del SPWM](base_de_tiempo_spwm.md): de dónde salen los 41.6 µs de la portadora.
- [Identificación del tiempo muerto por P9](identificacion_dt_pin9.md): sección 7.2.
- [Corte por hardware](corte_por_hardware.md): por qué el ADC no necesita ser rápido.
- `firmware/critico/README.md` y `firmware/auxiliar/README.md`: estado de cada módulo.

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz).*
