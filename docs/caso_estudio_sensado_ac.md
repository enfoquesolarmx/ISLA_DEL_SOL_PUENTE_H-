# Caso de estudio: la cadena de sensado AC contra tres eventos de carga reales

**22 de noviembre de 2026 · banco, canal A6 · [`ISLA_CRITICO_PRUEBA_AC_COMPARADOR`](../firmware/critico/ISLA_CRITICO_PRUEBA_AC_COMPARADOR/ISLA_CRITICO_PRUEBA_AC_COMPARADOR.ino)**

> **El sujeto de este documento es la cadena de sensado, no el inversor que se midió.** La pregunta no es «¿qué hace ese aparato?» sino **«¿qué alcanzó a detectar nuestro circuito, y detectó lo que debía?»**

La respuesta corta: **sí**, y uno de los detectores sólo sirvió cuando se le dio la resolución temporal correcta — lo cual es, en sí mismo, el hallazgo más útil del ejercicio.

---

## 1. Qué se puso a prueba

La cadena de [DC-006](decisiones_criticas.md#dc-006-sensado-ac-con-dos-transformadores-de-12-v-directos-al-adc) con la ventana por comparador de [DC-023](decisiones_criticas.md#dc-023-comparador-analógico-interno-para-el-cruce-por-cero-y-tn_cmd-se-muda-a-d13):

| | |
| --- | --- |
| Divisor | `Rt` = 110 kΩ · `Rb` = 10.23 kΩ · `DIVISOR_X100` = **1171**, medido |
| Bias | 10 k / 10 k con 10 µF y 100 nF, centrado en ~511 cuentas |
| Transformador | razón **8.3986**, medida con el Fluke en los dos lados |
| Ventana | delimitada por cruces del comparador: **ciclos enteros por construcción** |
| Repetibilidad | σ del RMS = **12.5 mV** · σ de la frecuencia = **0.0015 Hz** |

**Fuente medida:** la red de la casa, que viene de un inversor offgrid comercial de onda pura, clase ~1 kW. **No es nuestro puente H**, que nunca se ha energizado. Todo lo que sigue describe a ese aparato; lo que valida es nuestro instrumento.

---

## 2. Los tres eventos

| | Carga | Ventana | Qué se buscaba |
| --- | --- | --- | --- |
| **A** | Resistiva 1000 W, escalón de encendido y apagado | 60 ciclos (1 s) | la respuesta del lazo del inversor |
| **B** | Motor universal 700 W (aspiradora), régimen | 60 ciclos (1 s) | el par con A: ¿el tipo de carga cambia la forma? |
| **C** | El mismo motor, **el arranque** | **3 ciclos (50 ms)** | la profundidad real del transitorio |

---

## 3. Qué detectó cada canal

| Detector | Evento A (resistiva) | Evento B (motor, régimen) | Evento C (arranque) |
| --- | --- | --- | --- |
| **RMS** | −1.54 % con 1000 W | −1.47 % con **700 W** | **−12.57 %** |
| **Factor de forma** | **+0.083 %** (7.5 σ) | **−0.533 %** (46 σ) | mínimo 1.1000, **100 ms después** del fondo de voltaje |
| **Factor de cresta** | +0.27 % (4.3 σ) | −0.504 % (4.5 σ) | **1.622** contra 1.418 basal |
| **Bias (CD)** | sin cambio | sin cambio | **+2.40 V, luego −0.96 V** |
| **Frecuencia** | sin cambio | sin cambio | excursión de ±2–3 pasos, **no concluyente** |

**Los cinco detectores hicieron su trabajo**, incluyendo los dos que reportaron «nada»: que el bias no se moviera en A y B es un resultado, no un hueco.

---

## 4. Los cuatro hallazgos sobre el instrumento

### 4.1 El factor de forma distingue el tipo de carga **por el signo**

```
  resistiva  1000 W   ->  FF  +0.083 %     la onda se vuelve mas PICUDA
  motor       700 W   ->  FF  -0.533 %     la onda se vuelve mas PLANA
```

Y en el caso del motor **cruza la línea**:

```
  senoidal pura       1.11072
  en vacio            1.11550   (+0.430 %)   mas picuda que una senoidal
  con el motor        1.10955   (-0.105 %)   mas PLANA que una senoidal
```

No es que se deforme más: **cambia de lado**. Los dos efectos son reversibles — al quitar la carga el FF vuelve a su basal dentro del ruido, que es lo que separa física de deriva.

**Mecanismo, para el caso del motor:** una aspiradora lleva motor universal con control por triac, o sea **corriente recortada por fase**. El triac dispara a mitad de semiciclo y la corriente arranca de golpe; contra la impedancia de salida del inversor eso produce **muescas de voltaje**. Una muesca quita más del RMS (que pesa al cuadrado) que del promedio de |v|, así que **el FF baja**. Cuadra, y la cresta bajando al mismo tiempo lo confirma.

**Del caso resistivo no hay mecanismo defendible.** Una carga lineal no debería cambiar la forma. El efecto es real (7.5 σ) pero es 6.4 veces menor, y lo más probable es que sea el propio lazo del inversor respondiendo. Queda como pregunta abierta.

### 4.2 La cresta sola no sirve; con el FF, sí

En corridas anteriores la cresta se movió **sin** que el FF la siguiera (1.429 → 1.466 con correlación de apenas +0.14). Eso era **ruido impulsivo** entrando por el filtro antialias flojo, no cambio de forma: la cresta es un *extremo* de miles de muestras y un solo pico la mueve.

Aquí, en los eventos A y B, **los dos se movieron juntos**. Esa es la discriminación:

> **Cresta sola → ruido impulsivo. Cresta y FF juntos → cambio de forma de onda.**

### 4.3 El canal de CD atrapó algo por primera vez, y sólo en modo captura

```
  #209  bias 511     0.00 V de CD en la linea
  #210  bias 516   +2.40 V            <- el arranque
  #211  bias 509   -0.96 V
  #212  bias 510   -0.48 V
  #213  bias 511     0.00 V
```

Un motor universal arrancando jala **corriente asimétrica** los primeros ciclos, y eso mete una componente de corriente directa transitoria de unos **2.4 V sobre la línea**, que se acaba en ~150 ms.

**Con ventanas de un segundo eso es invisible**: se promedia a cero. Sólo apareció con ventanas de 50 ms. Es exactamente el caso para el que el bias existe —detectar componente de CD, que satura transformadores y calienta motores— y con regulación por pico sería **estructuralmente invisible**, porque un rectificador no distingue asimetría.

### 4.4 La lección metodológica: **la ventana decide lo que se puede ver**

El mismo evento, medido de dos maneras:

```
  ventana de 60 ciclos (1 s)   ->  hundimiento aparente   -3.51 %
  ventana de  3 ciclos (50 ms) ->  hundimiento real      -12.57 %
                                                          ────────
  la ventana larga escondia                               10.85 V
```

Antes de medirlo se estimó que si el transitorio ocupaba ~20 % de la ventana de un segundo, el fondo real andaría en **~105 V**. Medido: **104.88 V**.

> **Un promedio no miente, pero contesta otra pregunta.** La ventana de un segundo responde «¿cuánto bajó el voltaje eficaz durante ese segundo?» y la respuesta es correcta. La pregunta que el diseño necesitaba era otra: «¿qué tan hondo llegó?». **Un instrumento con la ventana mal elegida da un número correcto a la pregunta equivocada**, y eso es más peligroso que un número ruidoso, porque no se ve.

---

## 5. Lo que sí es del aparato medido

Separado del instrumento, para que no se confunda:

**Dos constantes de tiempo superpuestas.** El ajuste exponencial del arranque da **τ ≈ 200 ms**, que es **el motor acelerando**, no el lazo. El lazo del inversor es el de **3–4 segundos con 1 % de sobrepaso** que se vio en el escalón resistivo. Sólo se separan porque hubo resolución de 50 ms.

**La caída sigue la potencia aparente, no la activa:**

```
  resistiva   1000 W  ->  1.848 V  =  1.85 mV por watt
  motor        700 W  ->  1.762 V  =  2.52 mV por watt   (+36 %)
```

El motor carga el inversor un 36 % más **por cada watt que entrega**. Esa es la diferencia entre VA y W apareciendo directamente en el voltaje, y es el argumento cuantitativo para el canal de corriente: sin medirla, esa diferencia es invisible.

**La regulación por pico se equivocaría 0.50 %.** Un detector de pico lee `V_pico/√2` y lo llama RMS:

```
  en vacio         cresta 1.4243  ->  el estimado lee  +0.71 %  sobre el RMS real
  con el motor     cresta 1.4171  ->                   +0.21 %
                                                       ────────
  el estimado cae                                      -0.50 %
```

El lazo subiría la salida medio punto por encima del RMS verdadero mientras dure la carga.

> **Corrección.** Al argumentar antes contra la regulación por pico se usó un ejemplo hipotético con cresta 1.30 que daba **8 %**. Lo medido da **0.50 %**: dieciséis veces menos. El argumento sigue en pie —medio punto permanente es real— pero **la magnitud que se pintó era muy exagerada**, y queda corregida aquí.

---

## 6. Lo que NO se puede concluir

- **No es nuestro inversor.** Los 12.57 % de hundimiento mezclan tres cosas que desde afuera no se separan: el bus cayendo, la impedancia de salida del aparato y la lentitud de su lazo. De hecho una cuenta rápida sugiere que **la mayor parte no es el bus**: un arranque de 700 W a 4× de corriente sobre una batería de 48 V con 20 mΩ totales daría ~2.4 % de caída, no 12.6 %.
- **La excursión de frecuencia no es dato.** En modo captura la resolución es 0.050 Hz, y el basal ya alterna un paso. La excursión del arranque son 2–3 pasos **y coincide con la peor distorsión**, así que lo más probable es que sea el cruce desplazado por la deformación, no el inversor cambiando de frecuencia. Un generador SPWM saca la frecuencia de su cristal.
- **Del caso resistivo no hay mecanismo.** Ver 4.1.

---

## 7. Lo que esto le deja al diseño

| | |
| --- | --- |
| **Un banco de referencia** | Un aparato comercial de esta clase se hunde **12.57 %** en un arranque de motor y tarda más de 200 ms. El nuestro debería hacerlo mejor, y ahora hay contra qué compararse |
| **Una pregunta para [DC-021](decisiones_criticas.md#dc-021-transformador-fuera-de-rango-qué-pasa-qué-se-detecta-solo-y-qué-no)** | Los 40 V del piso de batería, ¿son el mínimo **en reposo** o el mínimo **hundido**? Si es lo primero, al presupuesto de amplitud le falta la partida de arranque de motor |
| **El argumento de RMS contra pico** | Ya no es teoría: **0.50 %**, medido, con una carga doméstica común |
| **El canal de corriente** | La caída sigue los VA, no los W. Sin CT, el 36 % de diferencia por watt es invisible |
| **La detección de CD** | Funciona, y sólo sirve con ventana corta. Para protección hay que decidir con qué resolución temporal corre |

---

**Referencias:** el registro de banco de A6 en [`protocolo_banco_control.md`](protocolo_banco_control.md), y las entradas [DC-006](decisiones_criticas.md#dc-006-sensado-ac-con-dos-transformadores-de-12-v-directos-al-adc), [DC-020](decisiones_criticas.md#dc-020-reloj-del-adc-a-128-y-muestreo-amarrado-a-la-portadora-precisión-antes-que-velocidad), [DC-021](decisiones_criticas.md#dc-021-transformador-fuera-de-rango-qué-pasa-qué-se-detecta-solo-y-qué-no), [DC-023](decisiones_criticas.md#dc-023-comparador-analógico-interno-para-el-cruce-por-cero-y-tn_cmd-se-muda-a-d13) y [DC-025](decisiones_criticas.md#dc-025-reparto-del-adc-entre-los-canales-tres-prioridades-y-por-qué-caben).
