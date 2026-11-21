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

**Lo medido, 25 reportes de un segundo:**

| | Valor | Nota |
| --- | --- | --- |
| Línea | **119.96 V** media | Fluke: 119.9 → **+0.05 %** |
| Secundario | 14.29 VAC | |
| Repetibilidad | **σ = 0.077 %** | 25 veces mejor que la exactitud del Fluke |
| Frecuencia | 60.04 Hz, σ = 0.034 Hz | portal del inversor: 59.95–60.00 |
| Factor de forma | 1.1140 | senoidal = 1.1107 |
| Factor de cresta | 1.429 | √2 = 1.4142, o sea +1.05 % |
| Disparos perdidos | **0** | con 4 periodos por disparo |
| Muestras entregadas | 5992 de 6006 (99.76 %) | |

**El patrón, y lo que no es.** El Fluke 324 **sí** es de valor eficaz verdadero, pero su exactitud en volts AC es **1.5 % ± 5 dígitos** en un rango único de 600.0 V, o sea **±2.30 V a 120 V (±1.92 %)**. Y «5 dígitos» son medio volt: **Fluke no garantiza el dígito de los décimos**. De ahí dos cosas: que no tiene sentido perseguir la cuarta cifra de las constantes, y que la **exactitud absoluta de esta cadena está limitada por el instrumento del usuario, no por el circuito**. La tarjeta es un instrumento más *preciso* que el Fluke (σ 0.077 % contra 1.92 %); nomás no más *exacto*, porque la exactitud necesita un patrón. **En producción, la calibración de voltaje tiene que ser una perilla del usuario.**

### Lo que esta corrida encontró de paso

**1. El 17.5 % de las conversiones se perdía, y era el presupuesto del ADC.** Con 3 periodos por disparo el presupuesto entre muestras son 124.875 µs y la conversión a 125 kHz se come 104: el ADC al **83 %**, y los 20.9 µs que sobran tienen que absorber tres ISR del Timer1 más la del ADC. No cabía — y eso **sin SPWM**, que en el firmware real vive dentro de esa misma ISR. Con **4 periodos** el ADC baja al 62 %, `perdidos` cae a cero y la tasa sube a 99.76 %. La ventana no se movió: 300 muestras a 6006/s son los mismos 49.95 ms que 400 a 8008/s.

**2. La frecuencia no se estabilizó por bajar la tasa, sino porque dejaron de perderse muestras.** Los cuatro reportes malos de antes (59.170, 59.173, 59.192, 59.226) son `60.0 × 72/73 = 59.178`: **un cruce perdido cada uno**. El detector necesita dos muestras consecutivas que abracen el cero; si la de junto al cruce se pierde, el cruce se pierde. La precisión del cruce **no depende de la tasa** — es una cuenta del ADC entre la pendiente, 6.85 µs a 166.5 µs y 6.86 µs a 124.875.

**3. El bamboleo de ±0.08 V no es ruido: es un batido.** La autocorrelación da periodo 8 reportes (79 % de la varianza), y la aritmética lo predice: la ventana de 300 muestras son **3.00616 ciclos**, no 3.00000, así que el pedacito sobrante rota de fase. Separado: **73 mV de batido determinístico y 37.6 mV de ruido real (0.031 %)**. Se quita con ventana delimitada por cruces por cero.

**4. El filtro antialias está flojo.** El nodo tiene 9.26 kΩ de Thévenin y C7 son 10 nF → **fc = 1719 Hz**, con Nyquist en 3003: apenas **−6.1 dB** en Nyquist. Probablemente explica por qué el factor de forma se corrió 0.397 % al cambiar sólo la tasa de muestreo (el aliasing depende de la tasa), y por qué la cresta brincó 2.6 % sin que el FF la siguiera. **Pendiente de decidir**, y hay tensión: subir C7 a 33 nF da −15 dB en Nyquist pero se come el 29 % del 13º armónico y mete 6.6° de fase a 60 Hz, que le pega al comparador de cruce por cero. Un polo solo no puede hacer las dos cosas.

**5. Tres defectos del instrumento, encontrados y corregidos.** (a) El voltaje se convertía en tres truncadas encadenadas y `cv = mv × 1.08` se saltaba un valor cada doce pasos: con `RELACION` 8379 el cv 1430 era inalcanzable y **119.8 V no se podía desplegar** — la lectura brincaba 119.7 ↔ 119.9 con la línea quieta. Se tiraban 7× de resolución. Ahora es una sola expresión y el paso es de 30 mV. (b) `imprimeDec` supone escala potencia de diez; con escala 16 imprimía el resto módulo diez, así que «270.5 cuentas» quería decir 270.31. (c) La frecuencia suponía el intervalo nominal entre muestras y con la pérdida real salía inflada a 72.77 Hz.

### Lo que queda abierto de A6

- **El 0.24 % de muestras que no cuadra** (5992 de 6006). Puede ser unas pocas ISR que llegan tarde, o el mecanismo de `hayVentana` —que es una bandera, no una cola— soltando una ventana durante el `Serial.print`. Es 74 veces más chico que lo que traíamos; se mide en B5.
- **La frecuencia absoluta.** El sketch da 60.041 Hz y el portal del inversor 59.95–60.00: **+0.11 %**. No se puede decir quién tiene razón — el sketch mide con el cristal del Nano y el inversor *genera* con el suyo. Dos relojes sin juez. El jitter que queda (σ 0.034 Hz) es **la resolución de un milisegundo de `millis()`**, no la línea: la correlación entre la frecuencia reportada y la tasa de muestreo es **+0.996**. Se arregla usando el intervalo nominal, pero **no antes** de entender el 0.24 %, porque entonces ese 0.24 % se vuelve un sesgo de +0.14 Hz.
- **La resistencia de 1 kΩ al pin** está en el montaje de A4 y no en el esquemático. Eléctricamente casi da igual (el Thévenin del divisor domina: la inyección en sobretensión pasa de 30 a 33 µA), pero el documento y la tarjeta tienen que decir lo mismo.

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

**Firmware:** ⚠️ falta una línea (levantar D7 al entrar a la ISR, bajarlo al salir). D7 está libre y reservado para esto por DC-017.

**Criterio.** El ancho del pulso cabe en el periodo de portadora de 41.6 µs **con espacio de sobra para el ADC y para el Timer0 de `millis()`**. Si no cabe, hay que optimizar la multiplicación de 32 bits antes de meterle el ADC.

Esta es la medición más barata del viaje: un pin, una línea de firmware y el analizador que ya está conectado.

## B6. El primer ciclo de amplitud 0 y el pulso solitario

**Firmware:** ✅ ya existe.

Confirmar con el analizador lo que GTKWave ya mostró: el primer ciclo arranca sin complementario (amplitud 0), y el pulso solitario de la pierna A aparece donde el modelo lo predice.

**Criterio.** Coincide con la traza de `firmware/critico/img/primer_ciclo_amplitud_cero.png`. Si coincide, el modelo de registros de la PC queda validado contra fierro por segunda vez.

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
| A3 resistencia elegida | 2026-11-21 | `Rt` = **110 kΩ** (mínima calculada 108.8 kΩ con la cresta medida) | ✅ | **decidido con números medidos, no estimados.** Con 100 kΩ el pin se pasa de 4.60 V ya a 130 V de línea. Lo construido en banco trae 100 kΩ |
| A4 atenuador y recorte | | tope ___ V / mínimo ___ V | | **no corrida**: faltó barrer el trimmer de tope a tope |
| A5 rango del trimmer | | ___ % | | **no corrida**: necesita el trimmer multivuelta y un segundo transformador |
| A6 ADC vs multímetro | 2026-11-20/21 | error **+0.42 %** sin calibrar (criterio 1 %) | ✅ | línea 119.96 V vs Fluke 119.9 · σ = 0.077 % · `perdidos` 0 · `DIVISOR_X100` 1077, `RELACION_X1000` 8399, las dos medidas y **separadas** · ver «Resultado de banco» en A6 |
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
