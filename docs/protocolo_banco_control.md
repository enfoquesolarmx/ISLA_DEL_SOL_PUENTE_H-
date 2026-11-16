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
| 1 | Transformador 120 → 12 VAC, ~500 mA | A2 a A8 | **La pieza que manda.** Sin él no hay parte A |
| 2 | Resistencia 100 kΩ ¼ W | Divisor AC, rama de arriba | El valor final lo decide **A3**; lleva también 2 × 120 kΩ y 2 × 150 kΩ por si sale alto |
| 2 | Resistencia 8.2 kΩ | Divisor AC, rama de abajo (parte fija) | |
| 2 | Trimmer 2 kΩ multivuelta (Bourns 3296W o equivalente) | Ajuste del divisor AC | Se conecta **como reóstato**: cursor unido a un extremo (DC-006) |
| 6 | Resistencia 10 kΩ | Bias de 2.5 V (2 por canal) + P9 | |
| 2 | Capacitor electrolítico 10 µF | Bias de 2.5 V | |
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

**Procedimiento.**
1. Conectar el primario a la red, el secundario **al aire** (sin divisor, sin nada).
2. Medir el secundario con el multímetro en AC. Anotar: **Vsec**.
3. **En el mismo instante**, medir el voltaje de la red en el mismo contacto. Anotar: **Vlínea**.
4. Escalar al peor caso: `Vsec_max = Vsec × (Vlínea_max / Vlínea)`.

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
| 17.0 VAC | 106.6 kΩ | **110 kΩ** | 4.540 V |
| 18.0 VAC | 113.4 kΩ | **120 kΩ** | 4.494 V |
| 19.0 VAC | 120.3 kΩ | **130 kΩ** | 4.455 V |
| 20.0 VAC | 127.2 kΩ | **130 kΩ** | 4.558 V |
| 22.0 VAC | 140.9 kΩ | **150 kΩ** | 4.481 V |

**Lo que hay que entender de esta tabla.** Los 100 kΩ de DC-006 solo alcanzan si `Vsec_max` **no pasa de 16 VAC**, y `Vsec_max` ya incluye la red alta. Eso significa que a 127 V de línea el transformador no debe dar más de **~14.5 VAC en vacío**. Un transformador de "12 VAC" barato sube entre 15 y 25 % sin carga, así que puede caer justo arriba. **Es la razón por la que esta medición va antes del PCB.**

**Por qué igual no es catastrófico si sale alto.** En THT una resistencia se cambia desoldando. Lo que sí sería caro es descubrirlo con la tarjeta ya poblada y el firmware persiguiendo un RMS equivocado.

---

## A4. Atenuador completo y prueba de recorte

**Firmware:** no hace falta para la medición con multímetro.

**Objetivo.** Armar la cadena real y confirmar que no recorta en ningún extremo del trimmer.

**Montaje.** Transformador → `Rt` (la elegida en A3) → nodo → (8.2 kΩ + trimmer 2 kΩ) → bias de 2.5 V. Del nodo, 1 kΩ al pin del ADC, y **10 nF pegado al pin**.

**Procedimiento.**
1. Trimmer al tope. Medir el pico en el pin. Si hay osciloscopio, directo; si solo hay multímetro, medir en AC y multiplicar por √2 (la red es senoidal, el error es pequeño).
2. Trimmer al mínimo. Medir otra vez.
3. Repetir en el segundo canal.

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

**Criterio de aceptación.**
- El recorrido cubre al menos **±8 %** alrededor del centro.
- **Monótono, sin saltos.** Un salto es cursor sucio o mal contacto: ese trimmer se tira.
- Conectado como reóstato, al levantar el cursor con la pinza la resistencia debe irse a **2 kΩ**, no al aire (es la protección que pidió DC-006).

---

## A6. ADC contra multímetro — calibración

**Firmware:** ⚠️ **falta escribirlo** (`ISLA_CRITICO_PRUEBA_ADC_AC`: lee A0 y A6, calcula RMS, lo saca por serie).

**Objetivo.** Que el número que reporta el ATmega coincida con el multímetro.

**Procedimiento.**
1. Medir el secundario con el multímetro en AC.
2. Leer lo que reporta el firmware.
3. Girar el trimmer hasta que coincidan. Anotar la posición.
4. Lo que quede de error va por software, a la EEPROM (DC-006).

**Criterio de aceptación.** Coinciden dentro de **1 %** después de ajustar.

> ⚠️ **Un multímetro de valor medio** (no *true RMS*) está calibrado para senoidal. Para la **red** vale. Para la **salida del inversor** no: ahí la onda es SPWM filtrada y un multímetro de valor medio miente. Esa calibración se hace aparte, cuando haya potencia.

---

## A7. Simetría entre canales — la prueba estrella

**Firmware:** ⚠️ **falta escribirlo** (el mismo sketch de A6, reportando el instante del cruce por cero de cada canal).

**Objetivo.** Validar de un golpe **dos** cosas: que el sesgo de DC-020 es el calculado, y que los filtros de los dos canales son simétricos como supone DC-006.

**La idea.** Alimentar **A0 y A6 del mismo secundario**. Si los dos ven exactamente la misma señal, la única diferencia entre sus cruces por cero tiene que ser el sesgo del multiplexado, que DC-020 predice en **124.875 µs = 2.697° a 60 Hz**. Cualquier diferencia extra es asimetría de los filtros.

**Esto no necesita osciloscopio.** El patrón es la propia aritmética del firmware: el sketch reporta los dos cruces y se compara el número con la predicción.

**Procedimiento.**
1. Puentear A0 y A6 a la misma salida del divisor (un solo canal alimentando los dos pines, cada uno con su 1 kΩ y su 10 nF).
2. Correr el sketch 10 s y promediar.
3. Anotar la diferencia medida entre los dos cruces.
4. **Invertir el orden de lectura** (A6 primero) y repetir.

**Criterio de aceptación.**
- Diferencia medida: **124.9 µs ± 23 µs** (23 µs = 0.5° a 60 Hz, la décima parte de la ventana de sincronía de 5°).
- Al invertir el orden, la diferencia debe **cambiar de signo y conservar la magnitud**.

**El diagnóstico, si falla.** Los dos pasos de arriba separan las dos causas posibles:
- Cambia de signo y conserva magnitud, pero la magnitud no es 124.9 µs → **el modelo del sesgo está mal** (revisar DC-020).
- No cambia de signo, o cambia con otra magnitud → **los filtros no son simétricos** (hay un offset fijo que no viene del multiplexado). Eso sí toca el cobre.

**Qué desbloquea.** La cancelación de los ~2° del filtro que DC-006 da por hecha, y el constante que el PLL va a restar.

---

## A8. DC-020: frecuencia de muestreo e instante del disparo

**Firmware:** ⚠️ **falta escribirlo** (el mismo sketch, levantando un pin libre al disparar la conversión y bajándolo en la ISR de fin de conversión).

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
2. **La decisión de la protección de A7 al aire** (B1), que puede agregar una resistencia al cobre.
3. **Confirmación de que el bias por canal se aguanta solo** (A1), o la necesidad de separarlo.
4. **Recomendación que no depende de ninguna medición:** dejar en el PCB el **footprint del seguidor de bias, puenteado con un jumper**. Es la única prueba de esta lista que no se puede hacer antes de imprimir (el ruido del puente), y así no obliga a una reimpresión: si el 10 µF aguanta, el jumper se queda y el footprint queda vacío; si no aguanta, se quita el jumper y se puebla. Cuesta dos almohadillas.

---

## Hoja de registro

Se llena en el banco, con lápiz, y después se pasa aquí.

| Corrida | Fecha | Valor medido | ¿Pasa? | Notas |
| --- | --- | --- | --- | --- |
| A1 bias 2.5 V | | | | |
| A2 secundario en vacío | | `Vsec` = ___ a `Vlínea` = ___ → `Vsec_max` = ___ | — | marca del transformador: |
| A3 resistencia elegida | | `Rt` = ___ | — | |
| A4 atenuador y recorte | | tope ___ V / mínimo ___ V | | |
| A5 rango del trimmer | | ___ % | | |
| A6 ADC vs multímetro | | error ___ % | | |
| A7 simetría entre canales | | ___ µs (esperado 124.9) | | invertido: ___ µs |
| A8 fs e instante | | ___ µs / ___ µs | | disparos perdidos: |
| B1 P9 con R reales | | | | protección de A7 al aire: |
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
