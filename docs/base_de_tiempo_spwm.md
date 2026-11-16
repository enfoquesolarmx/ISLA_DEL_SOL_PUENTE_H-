# La base de tiempo del SPWM, línea por línea

**Proyecto:** Isla del Sol: inversor 48 V → 127 V / 60 Hz, 1000 W

**Archivos que explica:**
- [`firmware/critico/ISLA_CRITICO/config_portadora.h`](../firmware/critico/ISLA_CRITICO/config_portadora.h) (252 líneas)
- [`firmware/critico/ISLA_CRITICO/config_spwm.h`](../firmware/critico/ISLA_CRITICO/config_spwm.h) (82 líneas)

**Para quién:** el trinchero que va a armar, reparar o modificar el inversor y quiere saber **por qué** cada número vale lo que vale, no sólo cuánto vale.

**Estado de lo que aquí se afirma:**

| Marca | Significa |
| --- | --- |
| ✅ **Medido** | Se midió en un Arduino Nano real con analizador lógico (2026-11-12) |
| 🖥 **Calculado** | Sale de la fórmula y lo confirma la prueba en la PC, pero no se ha medido en hardware |
| ⏳ **Pendiente** | Falta validarlo con la etapa de potencia conectada |

Todo lo del **ATmega4809** (Nano Every) es 🖥 calculado: ese chip todavía no se ha probado.

---

## Antes de empezar

### Qué necesitas saber y qué puedes saltarte

No hace falta saber ensamblador. Según lo que quieras hacer, lees una parte u otra:

| Quieres… | Te basta con… | Necesitas saber |
| --- | --- | --- |
| **Armar y cargar** el firmware | La sección 14 ("Si quiero cambiar…") y la tabla resumen (sección 11) | Usar el Arduino IDE |
| **Entender** por qué funciona | La sección 0: la frase, el reparto de ticks y la receta | Aritmética y qué es un seno |
| **Modificar** el firmware o llevarlo a otro micro | Todo el documento | Timers, interrupciones y registros de un microcontrolador |

Si una palabra no te suena, está en el glosario.

### Glosario de trinchera

**El reloj y el contador**

| Palabra | Qué es, en palabras de taller |
| --- | --- |
| **Reloj (cristal)** | El metrónomo del chip. El Nano trae un cristal que marca 16 millones de golpes por segundo (16 MHz). Todo en el chip avanza a ese ritmo |
| **Tick (o ciclo de reloj)** | Un golpe de ese metrónomo. En el Nano dura 62.5 ns. Es la unidad de tiempo más pequeña que el chip puede manejar |
| **ns, µs, ms** | Nanosegundo, microsegundo y milisegundo: una milmillonésima, una millonésima y una milésima de segundo. 1 µs = 1000 ns |
| **Timer** | Un contador dentro del chip que avanza uno con cada tick, como un odómetro. Cuenta solo, sin que el programa lo empuje |
| **Cuenta** | El número que lleva el timer en un momento dado. Una cuenta de diferencia = un tick de tiempo |
| **TOP** | Hasta dónde cuenta el timer antes de regresar. Fija cuánto dura un periodo |
| **Doble pendiente** | El timer sube de 0 al TOP y luego **baja** de regreso a 0, como un columpio. Los pulsos quedan centrados |
| **Comparador** | Una marca en el recorrido del timer. Cuando la cuenta cruza la marca, la salida cambia sola, sin esperar al programa |
| **Registro** | Una casilla de memoria dentro del chip que controla una parte del hardware. Escribir un número ahí es como mover una perilla |
| **ppm** | Partes por millón. 1000 ppm = 0.1 %. Se usa para errores muy pequeños |

**La onda**

| Palabra | Qué es, en palabras de taller |
| --- | --- |
| **PWM** | Prender y apagar muy rápido. Cuanto más tiempo dura prendido cada pulso, más voltaje promedio sale. Como abrir y cerrar una llave de agua muy rápido para regular el chorro |
| **SPWM** | Un PWM donde el ancho de los pulsos va siguiendo la forma de un seno: angostos cerca del cero, anchos en el pico. Después del filtro queda una senoidal |
| **Portadora** | El ritmo fijo al que se prende y apaga: aquí, 24 000 veces por segundo |
| **Periodo** | Lo que dura un pulso completo de portadora: 41.6 µs |
| **Muestra** | El valor del seno que le toca a un pulso. Hay una muestra por cada periodo: 400 en un ciclo de 60 Hz |
| **Tabla de seno (LUT)** | Una lista de valores del seno ya calculados y guardados en el chip. El chip no calcula senos: los busca en la lista, que es mucho más rápido |
| **Cuarto de onda** | El tramo del seno de 0° a 90°. Con ese pedazo se arma la onda completa, leyéndolo al derecho y al revés |
| **Amplitud** | Qué tan ancho es el pulso en el pico de la onda. Es el "volumen" de la senoidal |
| **Índice de modulación (m)** | La amplitud como fracción del máximo posible. m = 0.94 significa que el pulso más ancho ocupa el 94 % del periodo |
| **Armónicas / subarmónicos** | Deformaciones de la onda: componentes a frecuencias más altas (armónicas) o más bajas (subarmónicos) que los 60 Hz. Calientan el transformador y hacen ruido |

**La potencia**

| Palabra | Qué es, en palabras de taller |
| --- | --- |
| **Puente H** | Los cuatro MOSFET que conectan la batería a la salida en un sentido o en el otro. Tiene dos piernas |
| **Pierna** | Dos MOSFET en serie entre +48 V y tierra. La pierna A conmuta a 24 kHz; la pierna B, sólo 120 veces por segundo, para cambiar la polaridad |
| **Lado alto / lado bajo** | El MOSFET de arriba (hacia +48 V) y el de abajo (hacia tierra) de una pierna. HA y LA son los de la pierna A; HB y LB, los de la pierna B |
| **Tiempo muerto** | La pausa con los dos MOSFET de una pierna apagados, entre apagar uno y prender el otro. Sin ella habría un corto directo a la batería |
| **Conducción cruzada** | Lo que el tiempo muerto evita: los dos MOSFET de una pierna prendidos a la vez |
| **Gate driver** | El circuito que recibe la señal débil del micro (5 V) y maneja con fuerza la compuerta del MOSFET |
| **Bootstrap** | El capacitor que alimenta la compuerta del MOSFET de arriba. Se recarga cada vez que conduce el de abajo; si el de abajo nunca conduce, se descarga |
| **P9** | El pin con el que cada módulo gate driver avisa qué tiempo muerto necesita: a tierra, 500 ns; abierto, 1 µs |

**El programa**

| Palabra | Qué es, en palabras de taller |
| --- | --- |
| **Interrupción (ISR)** | Un timbre. Cuando suena, el chip deja lo que estaba haciendo, atiende lo urgente y regresa. Aquí suena una vez por periodo para calcular el pulso siguiente |
| **Flash** | El cuaderno donde está escrito el programa. No se borra al apagar. El Nano tiene 32 KB |
| **RAM** | El pizarrón donde el chip hace sus cuentas. Se borra al apagar. El Nano tiene sólo 2 KB |
| **Constante (`constexpr`)** | Un número con nombre que la computadora calcula **al compilar**. En el chip ya llega resuelto: no gasta tiempo ni RAM |
| **Candado (`static_assert`, `#error`)** | Una revisión que hace la computadora antes de generar el programa. Si un número rompe una regla, no compila y dice por qué |
| **Compilar** | Traducir el programa escrito a las instrucciones que entiende el chip. Lo hace el Arduino IDE al dar "Verificar" o "Subir" |
| **Truncar / redondear hacia arriba** | Truncar es quitar los decimales (333.33 → 333). Redondear hacia arriba es subir al entero siguiente (8.2 → 9) |
| **Q16** | Una forma de guardar fracciones como enteros: 65 535 representa 1.0. Así el chip trabaja con enteros, que le cuestan mucho menos que los decimales |

---

## 0. Cómo leer este documento

### La idea en una frase

> **Estos dos archivos son el reglamento de cómo se reparte cada tick del reloj.**

No generan ninguna señal: dicen cuántos ticks le tocan a cada cosa. Cada periodo de portadora tiene 666 ticks (333 de subida y 333 de bajada del contador), y se reparten así:

| Ticks | Para qué |
| --- | --- |
| 8 o 16 en cada flanco | Tiempo muerto: nadie conduce |
| Al menos 8 | Pulso mínimo: si no alcanza, no se prende |
| Hasta 316 o 313 por lado | Pulso de potencia, según lo que pida el seno |
| El resto | Pulso del lado contrario, que recarga el bootstrap |

Hacia arriba, 400 periodos hacen un ciclo de 60 Hz (480 a 50 Hz), sin que sobre ni falte uno.

El reparto se cumple por dos razones: **lo ejecuta el hardware del timer** (por eso el tiempo muerto midió 1000 y 500 ns exactos) y **los candados del compilador** no dejan compilar un reparto que rompa las reglas. Dos cosas no son perfectas: el tick depende del cristal (el del Nano corre 0.03 % lento) y la pierna B la mueve el programa, no el timer (su hueco varía entre 40.4 y 42.2 µs). Las dos se explican en la sección 12.

### La idea en un párrafo

El inversor fabrica una senoidal de 60 Hz prendiendo y apagando los MOSFET 24 000 veces por segundo. Qué tan ancho es cada pulso decide cuánto voltaje sale en ese instante. Estos dos archivos **no prenden ni apagan nada**: sólo guardan los números con los que trabaja el resto del firmware (cada cuánto se conmuta, cuántos pulsos lleva un ciclo, cuánto se espera entre apagar un MOSFET y prender el otro, qué tan angosto y qué tan ancho puede ser un pulso). Son la "hoja de datos" del SPWM.

### Sólo cinco números se eligen a mano

Todo lo demás lo calcula el compilador a partir de estos cinco:

| # | Constante | Valor | Dónde |
| --- | --- | --- | --- |
| 1 | `FRECUENCIA_LINEA_HZ` | 60 (o 50) | `config_portadora.h`, línea 85 |
| 2 | `FSW_OBJETIVO_HZ` | 24 000 Hz | línea 104 |
| 3 | `DT_MODERNO_NS` / `DT_LEGACY_NS` | 500 ns / 1000 ns | líneas 108–109 |
| 4 | `PULSO_MIN_NS` | 500 ns | `config_spwm.h`, línea 38 |
| 5 | `M_TECHO_PORCIENTO` | 95 % | línea 65 |

El reloj del chip (16 MHz en el Nano) no se elige: lo pone la placa.

### De los cinco números a todo lo demás (ATmega328P a 16 MHz)

```
Reloj 16 MHz ──┬── TOP = 16 000 000 / (2 × 24 000) = 333 cuentas
24 000 Hz ─────┘     │
                     ├── portadora real = 16 000 000 / (2 × 333) = 24 024.02 Hz
                     ├── una cuenta = 62.5 ns
                     └── 666 ciclos de CPU por periodo (presupuesto de la interrupción)

24 000 / 60 = 400 muestras por ciclo ── 200 por semiciclo ── 100 en la tabla (cuarto de onda)
24 000 / 50 = 480 muestras por ciclo ── 240 por semiciclo ── 120 en la tabla

salida = 24 024.02 / 400 = 60.060 Hz      (a 50 Hz: 24 024.02 / 480 = 50.050 Hz)

500 ns  / 62.5 ns =  8 cuentas  (tiempo muerto con P9 a GND)
1000 ns / 62.5 ns = 16 cuentas  (tiempo muerto con P9 abierto)
500 ns  / 62.5 ns =  8 cuentas  (pulso mínimo)

amplitud máxima = la menor de:  95 % × 333 = 316      (techo)
                                333 − DT − 4          (límite físico: 321 con 8, 313 con 16)
                → 316 con tiempo muerto de 500 ns, 313 con 1 µs
```

### La receta: las fórmulas, paso a paso

Como en una receta: primero los ingredientes, luego los pasos en orden. Cada paso usa lo que salió en los anteriores. Los números son los del ATmega328P a 16 MHz y 60 Hz.

**Símbolos:** `⌊x⌋` = quitar los decimales (truncar); `⌈x⌉` = subir al entero siguiente; `mín(a, b)` = el menor de los dos.

**Ingredientes** (lo que se elige o lo que pone la placa):

| Símbolo | Qué es | Valor |
| --- | --- | --- |
| `f_reloj` | Reloj del chip | 16 000 000 Hz |
| `f_línea` | Frecuencia de la salida | 60 Hz |
| `f_port` | Portadora deseada | 24 000 Hz |
| `t_DT` | Tiempo muerto que pide P9 | 500 ns o 1000 ns |
| `t_mín` | Pulso mínimo | 500 ns |
| `techo` | Modulación máxima permitida | 95 % |

**Paso 1. El tick y el TOP**

```
t_cuenta = 1 / f_reloj                    = 1 / 16 000 000         = 62.5 ns
TOP      = ⌊ f_reloj / (2 × f_port) ⌋     = ⌊ 16 000 000 / 48 000 ⌋ = ⌊333.33⌋ = 333
```

**Paso 2. La portadora real y su periodo**

```
f_port_real = f_reloj / (2 × TOP)         = 16 000 000 / 666       = 24 024.02 Hz
T_periodo   = 2 × TOP × t_cuenta          = 666 × 62.5 ns          = 41.625 µs
ticks por periodo = 2 × TOP               = 666
```

**Paso 3. Las muestras**

```
N (por ciclo)      = f_port / f_línea     = 24 000 / 60            = 400
S (por semiciclo)  = N / 2                                         = 200
Q (cuarto, tabla)  = N / 4                                         = 100
```

**Paso 4. La frecuencia de salida y su error**

```
f_salida  = f_port_real / N               = 24 024.02 / 400        = 60.060 Hz
error_ppm = (f_salida − f_línea) / f_línea × 1 000 000
          = (60.060 − 60) / 60 × 1 000 000                         = 1000 ppm (+0.1 %)
```

**Paso 5. La tabla de seno** (la calcula `genera_tabla_seno.py`, no el chip)

```
tabla[k] = redondear( 65 535 × sen( (k + 0.5) × 180° / S ) )       para k = 0 … Q − 1

ejemplo, k = 10:   65 535 × sen(10.5 × 0.9°) = 65 535 × sen(9.45°) = 10 760
```

El 65 535 es la escala: 65 535 representa 1.0 (formato Q16), para guardar fracciones como enteros de 2 bytes.

**Paso 6. Leer la tabla con el espejo** (k = número de muestra dentro del semiciclo, 0 … S − 1)

```
si k < S/2:   valor = tabla[k]              (subida, 0° a 90°)
si k ≥ S/2:   valor = tabla[S − 1 − k]      (bajada, 90° a 180°: la misma tabla al revés)
```

**Paso 7. Los tiempos en ticks** (siempre hacia arriba)

```
DT_cuentas   = ⌈ t_DT × f_reloj ⌉         = ⌈ 500 ns × 16 MHz ⌉    = 8      (1000 ns → 16)
mín_cuentas  = ⌈ t_mín × f_reloj ⌉        = ⌈ 500 ns × 16 MHz ⌉    = 8
fracción del periodo que se lleva el tiempo muerto
             = DT_cuentas / (2 × TOP)     = 8 / 666                = 1.2 %  (16 → 2.4 %)
```

**Paso 8. La amplitud máxima**

```
A_techo  = ⌊ TOP × techo / 100 ⌋          = ⌊ 333 × 95 / 100 ⌋     = 316
A_física = TOP − DT_cuentas − mín_cuentas / 2
         = 333 −  8 − 4                                            = 321   (con 500 ns)
         = 333 − 16 − 4                                            = 313   (con 1 µs)
A_máx    = mín( A_techo, A_física )       = mín(316, 321)          = 316   (con 500 ns)
                                          = mín(316, 313)          = 313   (con 1 µs)
m        = A / TOP                        = 313 / 333              = 0.940
```

**Paso 9. El ancho de cada pulso** (esto lo hace la interrupción, 24 024 veces por segundo, en `spwm_328p.cpp`)

```
d = ( valor × A ) / 65 536                 A = amplitud actual (0 … A_máx)
si d < mín_cuentas:  d = 0                 pulso demasiado angosto: no se genera

ancho del pulso de potencia = 2 × d × t_cuenta
voltaje promedio de ese periodo = ± V_batería × d / TOP
```

**Paso 10. Los dos comparadores del timer** (aquí aparece el tiempo muerto)

```
semiciclo positivo:   lado alto  = d                  lado bajo = d + DT_cuentas
semiciclo negativo:   lado bajo  = TOP − d            lado alto = TOP − d − DT_cuentas
tope del comparador del lado alto = TOP − DT_cuentas (peor caso) = 333 − 16 = 317
```

**Un ejemplo completo: la muestra 10, con amplitud máxima y tiempo muerto de 1 µs**

```
ángulo         = (10 + 0.5) × 0.9°                    = 9.45°
valor          = tabla[10]                            = 10 760
d              = 10 760 × 313 / 65 536                = 51 cuentas     (51 ≥ 8: sí se genera)
lado alto      = 51            lado bajo = 51 + 16    = 67
pulso          = 2 × 51 × 62.5 ns                     = 6.375 µs de los 41.625 µs del periodo
voltaje        = 48 V × 51 / 333                      = 7.4 V en promedio en ese periodo
comprobación   = 48 V × 0.940 × sen(9.45°)            = 7.4 V   ✓
```

Y la misma cuenta en otros puntos del semiciclo:

| Muestra k | Ángulo | `tabla` | d (cuentas) | Pulso | Comentario |
| --- | --- | --- | --- | --- | --- |
| 0 | 0.45° | 515 | 2 → **0** | no se genera | menor que el pulso mínimo |
| 1 | 1.35° | 1 544 | 7 → **0** | no se genera | menor que el pulso mínimo |
| 2 | 2.25° | 2 573 | 12 | 1.5 µs | primer pulso del semiciclo |
| 10 | 9.45° | 10 760 | 51 | 6.4 µs | el del ejemplo |
| 50 | 45.45° | 46 703 | 223 | 27.9 µs | a media subida |
| 99 | 89.55° | 65 533 | 312 | 39.0 µs | el pico: quedan 2.6 µs para tiempo muerto y bootstrap |

**Paso 11. Lo que se puede medir para comprobar la receta**

```
periodo de la salida       = N × T_periodo        = 400 × 41.625 µs  = 16.650 ms  (medido: 16.656 ms)
hueco de la pierna B       = T_periodo                               = 41.625 µs  (medido: 40.4–42.2 µs)
lado bajo de la pierna B en alto = (S − 1) / N    = 199 / 400        = 49.75 %    (medido: 49.7476 %)
tiempo muerto de la pierna A = DT_cuentas × t_cuenta = 16 × 62.5 ns  = 1000 ns    (medido: 1000 ns)
voltaje de pico ideal      = m × V_batería        = 0.940 × 48 V     = 45 V       (sin medir: falta potencia)
```

Para 50 Hz sólo cambian los pasos 3 a 6: N = 480, S = 240, Q = 120 y f_salida = 50.050 Hz. Para el ATmega4809 cambia el paso 1: t_cuenta = 50 ns y TOP = 416.

### La ficha de cada constante

Cada constante lleva la misma ficha de cinco renglones:

- **Qué es:** en palabras de taller.
- **De dónde sale:** la fórmula con la cuenta hecha.
- **Si lo subes** y **si lo bajas:** qué le pasa al inversor.
- **Candado:** qué revisión del compilador (`static_assert` o `#error`) impide dejarlo mal. Si un candado se dispara, **el firmware no compila** y el mensaje dice por qué. Es a propósito: es preferible un error en la computadora que un MOSFET quemado en el banco.

### Por qué todo va en enteros

El ATmega no tiene unidad de punto flotante: cada operación con `float` cuesta cientos de ciclos y kilobytes de flash. Por eso las frecuencias se guardan en **centésimas o milésimas de Hz**, los tiempos en **décimas de nanosegundo** y los errores en **partes por millón**. Y como todo es `constexpr`, las cuentas las hace la computadora al compilar: en el chip no ocupan RAM ni tiempo.

---

# Parte A: `config_portadora.h`

## 1. Chip y reloj (líneas 36–54)

```c
#if defined(__AVR_ATmega328P__)
  #define ISLA_CHIP_328P 1
  constexpr uint32_t RELOJ_ESPERADO_HZ = 16000000UL;
#elif defined(__AVR_ATmega4809__)
  #define ISLA_CHIP_4809 1
  constexpr uint32_t RELOJ_ESPERADO_HZ = 20000000UL;   // MegaCoreX a 20 MHz
#else
  #error "Chip no soportado..."
#endif
```

### `ISLA_CHIP_328P` / `ISLA_CHIP_4809`

- **Qué es:** una bandera que dice qué micro se está compilando. `__AVR_ATmega328P__` la define sola el compilador según la placa escogida en el Arduino IDE.
- **Para qué sirve:** el resto del firmware la usa para escoger el archivo de registros correcto (`spwm_328p.cpp` o `spwm_4809.cpp`). Los dos chips tienen timers distintos; la lógica es la misma (decisión DC-018, zócalo multimodelo).
- **Candado:** con cualquier otro chip (un Mega, un ESP32, un Uno con otro micro), `#error` detiene la compilación.

### `RELOJ_ESPERADO_HZ` (16 000 000 o 20 000 000)

- **Qué es:** el reloj que **debe** tener cada chip.
- **De dónde sale:** el Nano trae un cristal de 16 MHz. El Nano Every puede correr a 16 o a 20 MHz; aquí se exige 20 MHz (core MegaCoreX), porque da más resolución: 50 ns por cuenta contra 62.5 ns.
- **Por qué importa tanto:** todos los tiempos se cuentan en ciclos de reloj. Si el chip corriera a 8 MHz con este firmware, la portadora saldría a 12 kHz, la salida a 30 Hz y el tiempo muerto al doble.

### `RELOJ_HZ = F_CPU` y sus dos candados (líneas 47–54)

- **Qué es:** el reloj **real** con que el IDE está compilando (`F_CPU` lo pone el IDE según la placa).
- **Candado 1** (`#ifndef F_CPU` → `#error`): si no hay placa seleccionada, no compila.
- **Candado 2** (`static_assert(RELOJ_HZ == RELOJ_ESPERADO_HZ)`): si el reloj de la placa no es el previsto, no compila. Caso real: el core oficial del Nano Every corre a 16 MHz; con él **este firmware no compila**, y el mensaje pide MegaCoreX a 20 MHz.

> **Regla de trinchera:** si el firmware no compila al cambiar de placa, no es una falla. Es el candado avisando que con ese reloj la onda saldría mal.

---

## 2. Frecuencia de línea (líneas 59–88)

```c
constexpr uint8_t LINEA_60_HZ = 60;
constexpr uint8_t LINEA_50_HZ = 50;
constexpr uint8_t FRECUENCIA_LINEA_HZ = LINEA_60_HZ;   // <-- 60 o 50
```

### `LINEA_60_HZ` y `LINEA_50_HZ`

- **Qué son:** los dos únicos valores permitidos, con nombre, para no escribir un 60 o un 50 suelto en el código.
- **Por qué sólo dos:** son las dos frecuencias de red que existen en el mundo. 60 Hz en México, casi toda América y parte de Asia; 50 Hz en Europa, África y el Cono Sur.

### `FRECUENCIA_LINEA_HZ` (60)

- **Qué es:** la frecuencia que va a entregar **este** equipo. Es la línea que se cambia para hacer un inversor de 50 Hz.
- **Qué cambia en el firmware con 50:** el número de muestras por ciclo (480 en lugar de 400) y la tabla de seno (120 valores en lugar de 100). **No cambian** la portadora, el TOP, el tiempo muerto, el pulso mínimo ni la amplitud máxima.
- **Si pones 50 con un transformador de 60 Hz:** el flujo en el núcleo es proporcional a V / f. A 50 Hz con el mismo voltaje, el núcleo trabaja con 60 / 50 = **20 % más flujo**. Un transformador calculado justo para 60 Hz se satura: zumba, se calienta y jala corriente de más.
- **Además:** casi todas las redes de 50 Hz son de 220–240 V. El transformador de 48 V → 127 V no sirve; hacen falta otra relación de vueltas y relés, capacitores y fusibles de salida para 250 V.
- **Por qué se elige aquí y no en el menú SETUP:** quien pasa de 60 a 50 Hz **tiene que cambiar el transformador**. Esa persona ya tiene el equipo abierto y una computadora a la mano: cambia esta línea y vuelve a cargar el firmware. Si estuviera en el menú, alguien podría cambiarla con los botones por accidente y saturar el transformador. (La tecnología de batería sí se escoge en el SETUP, pero con candados que revisa el CRÍTICO; ver la sección 7.4 del README del firmware.)
- **Candado** (línea 87): `static_assert` sólo acepta 60 o 50. Un 55 o un 400 no compilan.
- **Estado:** ✅ las dos frecuencias se midieron en el Nano (sección 12).

---

## 3. La portadora: 24 kHz (líneas 90–104)

```c
constexpr uint32_t FSW_OBJETIVO_HZ = 24000UL;
```

### `FSW_OBJETIVO_HZ` (24 000)

- **Qué es:** cuántas veces por segundo conmuta la pierna A del puente H. "Objetivo" porque la frecuencia real queda en 24 024 Hz (sección 6 explica por qué).

- **De dónde sale el 24 000.** Tiene que cumplir cuatro condiciones a la vez:

  1. **Arriba de lo audible.** El oído llega a unos 20 kHz. Debajo de eso, el transformador y el inductor de salida zumban con un pitido constante.
  2. **Múltiplo exacto de 60 y de 50 Hz.** Así cada ciclo de la salida tiene un número **entero** de pulsos (400 o 480) y el cruce por cero cae siempre en el mismo punto. Si no fuera múltiplo, el patrón de pulsos se iría recorriendo de un ciclo a otro y aparecerían subarmónicos: componentes debajo de 60 Hz que hacen vibrar al transformador y pueden meterle corriente directa.
  3. **Las muestras por ciclo deben ser múltiplo de 4,** para poder guardar sólo un cuarto de onda (sección 5).
  4. **Las muestras por semiciclo deben caber en 8 bits** (máximo 255), porque el contador de muestras de la interrupción es de un byte: más rápido de manejar en un micro de 8 bits.

  Juntando 2 y 3, la portadora tiene que ser múltiplo de 1200 Hz (el mínimo común de 240 y 200). Entre 20 y 25 kHz sólo hay cuatro candidatas:

  | Portadora | Muestras a 60 Hz | Muestras a 50 Hz | Observación |
  | --- | --- | --- | --- |
  | 20 400 Hz | 340 | 408 | En el borde de lo audible |
  | 21 600 Hz | 360 | 432 | Válida |
  | 22 800 Hz | 380 | 456 | Válida |
  | **24 000 Hz** | **400** | **480** | **La elegida:** la más alejada de lo audible y con números redondos |

  *(Esta tabla es un análisis de este documento a partir de los candados del archivo; el archivo sólo dice que debe ser múltiplo de 60 y de 50.)*

- **Si la subes** (por ejemplo a 48 kHz): el filtro de salida trabaja más fácil y el rizo baja, pero:
  - las pérdidas de conmutación de los MOSFET suben al doble (cada conmutación cuesta energía);
  - el tiempo muerto se come el doble del periodo (de 2.4 % a 4.8 % con 1 µs), lo que deforma la onda;
  - la interrupción sólo tendría 333 ciclos por periodo, y hoy usa unos 170–180: quedaría a más de la mitad de la carga;
  - las muestras por semiciclo (400 y 480) ya no caben en 8 bits.

- **Si la bajas** (por ejemplo a 12 kHz): menos pérdidas de conmutación, pero el equipo **pita** y el rizo de corriente en el filtro sube al doble.

- **Por qué se queda en 24 kHz también a 50 Hz** (líneas 97–103). La alternativa era bajar a 20 kHz en 50 Hz para tener 400 muestras y 50.000 Hz exactos. Se descartó: con la portadora fija **no cambia nada del hardware ni de los límites** (TOP, tiempo muerto, pulso mínimo, amplitud, rizo del filtro, pérdidas, ruido). El precio es que la salida queda en 50.05 Hz en lugar de 50.00 Hz, el mismo +0.1 % que ya tiene la de 60 Hz.

- **Candados:**
  - línea 163: debe ser múltiplo entero de 60 y de 50;
  - línea 165: las muestras por ciclo deben ser múltiplo de 4;
  - línea 167: las muestras por semiciclo deben ser ≤ 255;
  - línea 212: la portadora **real** debe quedar entre 20 y 25 kHz.

- **Estado:** ✅ medida indirectamente: la salida da 60.04 Hz con 400 pulsos por ciclo, o sea 24 016 Hz de portadora (el cristal del Nano corre 0.03 % lento).

---

## 4. Los tiempos muertos que pide P9 (líneas 106–109)

```c
constexpr uint16_t DT_MODERNO_NS = 500;    // P9 = GND
constexpr uint16_t DT_LEGACY_NS  = 1000;   // P9 abierto o falla (valor seguro)
```

**Qué es el tiempo muerto.** En una pierna del puente hay dos MOSFET en serie entre +48 V y tierra. Nunca deben conducir los dos a la vez: sería un corto directo a la batería. Pero un MOSFET no se apaga al instante. Por eso, al cambiar, primero se apaga uno, **se espera un momento con los dos apagados** y luego se prende el otro. Esa espera es el tiempo muerto.

### `DT_MODERNO_NS` (500 ns)

- **Qué es:** el tiempo muerto para los gate drivers rápidos: IRS2186 y UCC21520DW.
- **De dónde sale:** de las hojas de datos de esos drivers y de los MOSFET del puente; está acordado en `docs/identificacion_dt_pin9.md`. El módulo lo "pide" poniendo su pin P9 a tierra.
- **Si lo subes:** más seguro contra el corto, pero la onda se deforma más cerca del cruce por cero y se pierde un poco de voltaje de salida.
- **Si lo bajas:** riesgo de conducción cruzada (los dos MOSFET prendidos un instante): calentamiento y, en el peor caso, MOSFET destruidos.

### `DT_LEGACY_NS` (1000 ns)

- **Qué es:** el tiempo muerto para los drivers lentos: TLP250H (optoacoplador) e IR2110 de trinchera. **También es el valor seguro:** se usa si P9 está abierto, si el cable se suelta o si la lectura es dudosa.
- **Por qué el doble:** esos drivers tardan más en apagar. Con 500 ns podrían traslapar.
- **Regla del firmware:** 500 ns **sólo** si los dos módulos lo piden; cualquier otra cosa, 1 µs.

- **Candados** (líneas 245–250):
  - el legacy debe ser **mayor** que el moderno (si alguien los invierte, no compila);
  - convertidos a cuentas, ninguno puede quedar **por debajo** de lo pedido (sección 8).
- **Estado:** ✅ los dos se midieron exactos en el Nano: 1000 ns y 500 ns.

---

## 5. Las muestras de la tabla seno (líneas 111–168)

```c
constexpr uint16_t MUESTRAS_CICLO_60     = FSW_OBJETIVO_HZ / LINEA_60_HZ;   // 400
constexpr uint16_t MUESTRAS_SEMICICLO_60 = MUESTRAS_CICLO_60 / 2;           // 200
constexpr uint16_t MUESTRAS_CUARTO_60    = MUESTRAS_CICLO_60 / 4;           // 100
constexpr uint16_t MUESTRAS_CICLO_50     = FSW_OBJETIVO_HZ / LINEA_50_HZ;   // 480
constexpr uint16_t MUESTRAS_SEMICICLO_50 = MUESTRAS_CICLO_50 / 2;           // 240
constexpr uint16_t MUESTRAS_CUARTO_50    = MUESTRAS_CICLO_50 / 4;           // 120
```

### `MUESTRAS_CICLO_60` (400) y `MUESTRAS_CICLO_50` (480)

- **Qué es:** cuántos pulsos de portadora caben en un ciclo de la salida. En cada pulso el firmware lee un valor de la tabla de seno: una muestra por pulso.
- **De dónde sale:** 24 000 / 60 = **400**; 24 000 / 50 = **480**.
- **No se eligen:** salen de la portadora y la frecuencia de línea. Para cambiarlos hay que cambiar la portadora.

### `MUESTRAS_SEMICICLO_*` (200 y 240)

- **Qué es:** las muestras de medio ciclo. Es lo que cuenta la interrupción: de 0 a 199 (o a 239), y vuelve a empezar cambiando la polaridad.
- **Candado** (línea 167): deben ser ≤ 255, porque ese contador es de 8 bits.

### `MUESTRAS_CUARTO_*` (100 y 120): por qué sólo se guarda un cuarto de onda

- **Qué es:** el tamaño de la tabla que de verdad se graba en la memoria del chip.
- **La idea** es una propiedad de la senoidal que se enseña en bachillerato: **el tramo de 0° a 90° contiene toda la información**. De 90° a 180° la onda baja por el mismo camino por el que subió (espejo), y de 180° a 360° se repite con el signo cambiado.

  | Tramo | Cómo se obtiene |
  | --- | --- |
  | 0° a 90° (subida) | `tabla[k]`, tal cual |
  | 90° a 180° (bajada) | `tabla[S − 1 − k]`, la misma tabla leída al revés |
  | 180° a 360° | Los mismos valores; **el signo lo pone la pierna B**, no la tabla |

  En la modulación híbrida la pierna A sólo necesita la **magnitud** |sen θ|. La polaridad la da la pierna B al cambiar de lado en el cruce por cero. Por eso la tabla nunca guarda negativos.

- **Lo que se ahorra** (cada muestra ocupa 2 bytes):

  | Opción a 60 Hz | Muestras | Flash |
  | --- | --- | --- |
  | Ciclo completo | 400 | 800 B |
  | Semiciclo | 200 | 400 B |
  | **Cuarto de onda** | **100** | **200 B** |

  Un ahorro del **75 %**. Las dos tablas juntas (60 y 50 Hz) ocupan 440 B.

- **Lo que cuesta:** una comparación y una resta dentro de la interrupción, unos 3–4 ciclos de los 666 que hay por periodo (~0.5 %). Flash casi gratis.

- **Por qué en flash y no en RAM:** el ATmega328P tiene sólo 2 KB de RAM. Una tabla completa calculada en RAM al arrancar (800 B) se comería el 40 %.

- **Por qué los fabricantes grandes suelen usar la tabla completa:** en un DSP o un ARM sobran flash y RAM, y leer directo es más simple. Aquí se trata de **sacarle el máximo a un micro de 8 bits sin coprocesador matemático ni hardware dedicado**: cuando la memoria es limitada, las matemáticas básicas maximizan los recursos.

### El detalle que no se debe cambiar: muestrear en k + 0.5 (líneas 144–155)

Cada valor de la tabla se calcula **a la mitad** de su periodo de portadora, no en el borde:

```
tabla[k] = sen( (k + 0.5) × 180° / S )      ← correcto (S = 200 a 60 Hz)
tabla[k] = sen(  k        × 180° / S )      ← mal
```

- **Por qué.** Cada pulso dura un periodo completo (41.6 µs) y representa a la senoidal durante todo ese rato. El mejor valor que lo representa es el del **centro** del periodo, no el del principio.

- **El espejo sale perfecto.** A 60 Hz cada muestra avanza 180° / 200 = 0.9°:

  | Muestra | Ángulo con k + 0.5 | Valor | Su pareja en el espejo | Ángulo | Valor |
  | --- | --- | --- | --- | --- | --- |
  | 0 | 0.45° | 0.00785 | 199 | 179.55° | 0.00785 |
  | 99 | 89.55° | 0.99997 | 100 | 90.45° | 0.99997 |

  La muestra k y la muestra S − 1 − k valen **exactamente lo mismo**. Las dos mitades del semiciclo son idénticas.

- **Qué pasa muestreando en el borde.** La muestra 0 caería en 0° (vale 0) y su pareja, la 199, en 179.1° (vale 0.0157): **no coinciden**. El pico de 90° caería en una sola muestra, la 100, sin pareja. Al leer la tabla en espejo, la bajada sale corrida una muestra respecto de la subida: la onda queda un poco asimétrica y aparece distorsión de armónicas pares, que se evita sin costo.

- **Consecuencia:** las muestras por ciclo tienen que ser **múltiplo de 4**, para que el ciclo se parta en cuatro cuartos exactos. 400 / 4 = 100 y 480 / 4 = 120.

- **Otra consecuencia útil:** la primera muestra vale 0.785 % del pico (0.65 % a 50 Hz). Aun con la amplitud al máximo, eso es un pulso de unos 300 ns (41.6 µs × 0.00785 × 0.94), más angosto que el pulso mínimo: no se genera. Por eso el firmware puede apagar la pierna A justo en el cruce por cero sin perder onda.

- **La tabla no se escribe a mano:** la genera `herramientas/genera_tabla_seno.py`. Si se cambia el número de muestras, se vuelve a generar.

- **Candados** (líneas 163–168): múltiplo de 60 y 50, múltiplo de 4 y semiciclo ≤ 255.
- **Estado:** ✅ el número de muestras se confirmó en el Nano: el lado bajo de la pierna B pasa en alto 49.7476 % del ciclo, contra 199 / 400 = 49.75 % calculado. 🖥 La forma senoidal pulso por pulso sólo está verificada en la prueba de la PC.

---

## 6. El TOP del timer y el modo 8 (líneas 170–184)

```c
constexpr uint16_t TOP_CUENTAS = (uint16_t)(RELOJ_HZ / (2UL * FSW_OBJETIVO_HZ));
constexpr uint16_t CICLOS_CPU_PERIODO = (uint16_t)(2UL * TOP_CUENTAS);
```

### Cómo trabaja el timer: doble pendiente

El Timer1 del ATmega328P es un contador. En el **modo 8** (*Phase and Frequency Correct PWM*, con el tope en el registro ICR1) cuenta de 0 hasta el TOP y luego **regresa** de TOP a 0, una cuenta por ciclo de reloj. Dibuja un triángulo:

```
TOP ...........    /\          /\
                  /  \        /  \
comparador ----- /----\------/----\-----
                /      \    /      \
0 ............ /        \  /        \  /
                         \/          \/
              |<-- un periodo, 41.6 µs -->|
```

La salida está en alto mientras el contador va por debajo del comparador. Como el triángulo es simétrico, **el pulso queda centrado** en el periodo: si el comparador sube, el pulso crece igual hacia los dos lados.

**Por qué este modo y no el PWM rápido (diente de sierra):**

1. **Pulsos centrados:** menos contenido armónico que con pulsos alineados a un borde.
2. **Tiempo muerto gratis y exacto:** con dos comparadores separados unas cuentas, el hueco aparece en **los dos flancos** de cada pulso y lo hace el hardware del timer, no el programa (sección 8). No depende de que la interrupción llegue a tiempo.
3. **Actualización limpia:** en el modo 8 los comparadores nuevos se aplican sólo cuando el contador pasa por 0. Nunca cambia un pulso a la mitad.

### `TOP_CUENTAS` (333 en el 328P; 416 en el 4809)

- **Qué es:** hasta dónde cuenta el timer. Fija la frecuencia de la portadora.
- **De dónde sale.** Un periodo son 2 × TOP cuentas (subida y bajada), así que:

  ```
  portadora = reloj / (2 × TOP)      →      TOP = reloj / (2 × portadora)

  328P:  16 000 000 / 48 000 = 333.33  →  333      (portadora real 24 024.02 Hz)
  4809:  20 000 000 / 48 000 = 416.67  →  416      (portadora real 24 038.46 Hz)
  ```

- **Por qué se trunca y no se redondea:** el TOP tiene que ser entero. Al truncar (333 en lugar de 333.33), el periodo queda un poco más corto y la portadora un poco **arriba** de 24 000. En este caso redondear daría lo mismo (333), pero truncar garantiza que la portadora nunca quede por debajo del objetivo.
- **El TOP también es la resolución del pulso:** el ancho del pulso se puede ajustar en 333 pasos. Es la "regla" con la que se mide todo lo demás: el tiempo muerto, el pulso mínimo y la amplitud se expresan en cuentas de este mismo contador.
- **Si lo subes** (portadora más baja): más resolución, pero la portadora baja hacia lo audible. Con TOP = 400 serían 20 kHz.
- **Si lo bajas** (portadora más alta): menos resolución y el tiempo muerto pesa más. Con TOP = 166 serían 48 kHz, y el tiempo muerto de 16 cuentas ocuparía casi el 10 % del semiperiodo.
- **No se cambia a mano:** se deriva. Para moverlo se cambia `FSW_OBJETIVO_HZ`.
- **A futuro:** para sincronizarse con la red, el PLL moverá el TOP unas cuentas arriba o abajo para acelerar o frenar la onda (módulo `ac_fb_grid`, pendiente).
- **Candados:** línea 212 (portadora real entre 20 y 25 kHz) y línea 251 (el tiempo muerto no puede pasar del 10 % del semiperiodo).
- **Estado:** ✅ el reporte de arranque del Nano muestra `TOP (cuentas): 333`.

### `CICLOS_CPU_PERIODO` (666 en el 328P; 832 en el 4809)

- **Qué es:** cuántos ciclos de reloj tiene el CPU entre un pulso y el siguiente. Es **el presupuesto de la interrupción**: en ese tiempo tiene que leer la tabla, calcular el ancho del siguiente pulso y escribir los comparadores.
- **De dónde sale:** 2 × 333 = **666**. En tiempo: 666 × 62.5 ns = 41.6 µs.
- **Cuánto se usa:** la interrupción del SPWM tarda **169–180 ciclos** (medido en el simulador del chip, 🖥), alrededor del 27 % del presupuesto. El resto queda para `loop()`: leer sensores, atender el enlace I2C, vigilar la batería.

  > La estimación inicial (DC-017) era de 80–120 ciclos; el valor medido en simulación es 169–180. **Falta medirlo en el chip real** con un pin de prueba (D7 está reservado para eso).

- **Por qué importa:** si la interrupción tardara más que el periodo, se perdería un pulso y la onda se deformaría. Es la razón por la que el inversor usa **dos** microcontroladores: el CRÍTICO sólo genera la onda y protege; la pantalla, el CAN y los mensajes van en el AUX.

---

## 7. Las frecuencias reales (líneas 186–216)

```c
constexpr uint32_t FSW_REAL_CHZ    = (RELOJ_HZ * 100ULL) / (2UL * TOP_CUENTAS);
constexpr uint32_t F_SALIDA_60_MHZ = (RELOJ_HZ * 1000ULL) / (2ULL * TOP_CUENTAS * MUESTRAS_CICLO_60);
constexpr uint32_t F_SALIDA_50_MHZ = (RELOJ_HZ * 1000ULL) / (2ULL * TOP_CUENTAS * MUESTRAS_CICLO_50);
```

Como el TOP se truncó, las frecuencias reales no son exactamente las de diseño. Estas constantes las calculan para **reportarlas con honestidad** en el arranque y para que un candado las vigile.

### `FSW_REAL_CHZ` (2 402 402 = 24 024.02 Hz)

- **Qué es:** la portadora real, en **centésimas** de Hz.
- **De dónde sale:** 16 000 000 × 100 / (2 × 333) = 2 402 402. Se multiplica por 100 antes de dividir para conservar dos decimales sin usar `float`.
- **En el 4809:** 2 403 846 (24 038.46 Hz) 🖥.

### `F_SALIDA_60_MHZ` (60 060) y `F_SALIDA_50_MHZ` (50 050)

- **Qué es:** la frecuencia real de la salida, en **milésimas** de Hz.
- **De dónde sale:** portadora real entre muestras por ciclo:

  ```
  60 Hz:  24 024.02 / 400 = 60.060 Hz
  50 Hz:  24 024.02 / 480 = 50.050 Hz
  ```

- **¿Importa ese 0.06 Hz de más?** En modo isla, no. Las cargas, los motores y las fuentes toleran mucho más que eso; la propia red eléctrica varía más a lo largo del día. Un reloj de pared que cuente los ciclos de la línea adelantaría 86 segundos por día. En modo red, el PLL ajustará el TOP para seguir a la red.

### `ERROR_FRECUENCIA_PPM` y `ERROR_FRECUENCIA_50_PPM` (1000 ppm)

- **Qué es:** cuánto se aleja la salida de la nominal, en partes por millón. Positivo = va más rápida.
- **De dónde sale:**

  ```
  (60 060 − 60 000) × 1 000 000 / 60 000 = 1000 ppm = 0.10 %
  ```

  Es el mismo en 60 y 50 Hz, porque sólo depende de la portadora real.
- **En el Monitor Serie** aparece como `Error de frecuencia ppm: 1000`. **No es una falla:** es el nombre del renglón; el valor dice que la salida va 0.1 % arriba de la nominal, por diseño.
- **En el 4809:** +0.16 % (60.096 Hz) 🖥.

### Candados (líneas 212–216)

- La portadora real debe quedar entre 20 y 25 kHz. Atrapa un reloj equivocado o una portadora mal escrita.
- La salida no puede alejarse más de **0.5 %** (5000 ppm) de la nominal. Atrapa combinaciones de reloj y portadora donde el truncado del TOP pesa demasiado.

- **Estado:** ✅ medido 60.040 Hz y 50.033 Hz. La diferencia con lo calculado se explica en la sección 12.

---

## 8. Resolución y tiempo muerto en cuentas (líneas 218–252)

### `NS_X10_POR_CUENTA` (625 = 62.5 ns)

```c
constexpr uint16_t NS_X10_POR_CUENTA = (uint16_t)(10000000000ULL / RELOJ_HZ);
```

- **Qué es:** cuánto dura una cuenta del timer, en **décimas** de nanosegundo. Es el paso más fino con que se puede mover un flanco.
- **De dónde sale:** un ciclo de reloj = 1 / 16 000 000 s = 62.5 ns. En décimas: 625. (En el 4809: 50 ns.)
- **Por qué en décimas:** 62.5 no es entero; 625 sí.
- **Qué significa en la práctica:** todo tiempo de este firmware es múltiplo de 62.5 ns. No existe un tiempo muerto de 550 ns: sería de 562.5 (9 cuentas).

### `ISLA_NS_A_CUENTAS(ns)`: convertir redondeando hacia arriba

```c
#define ISLA_NS_A_CUENTAS(ns) \
  ((uint8_t)((((uint64_t)(ns)) * RELOJ_HZ + 999999999ULL) / 1000000000ULL))
```

- **Qué hace:** convierte nanosegundos a cuentas del timer.
- **La cuenta:** cuentas = ns × reloj / 1 000 000 000. El `+ 999 999 999` antes de dividir hace que cualquier fracción **suba** al entero siguiente.
- **Por qué hacia arriba:** en tiempo muerto, quedarse corto es peligroso (conducción cruzada) y pasarse un poco es inofensivo. Si algún día se pide 550 ns, saldrán 9 cuentas (562.5 ns), nunca 8 (500 ns).

### `DT_MODERNO_CUENTAS` (8) y `DT_LEGACY_CUENTAS` (16)

```c
constexpr uint8_t DT_MODERNO_CUENTAS = ISLA_NS_A_CUENTAS(DT_MODERNO_NS);
constexpr uint8_t DT_LEGACY_CUENTAS  = ISLA_NS_A_CUENTAS(DT_LEGACY_NS);
```

- **Qué es:** el tiempo muerto en la unidad que entiende el timer.
- **De dónde sale:**

  | Tiempo muerto | 328P (62.5 ns/cuenta) | 4809 (50 ns/cuenta) |
  | --- | --- | --- |
  | 500 ns | 500 / 62.5 = **8** cuentas | 500 / 50 = 10 cuentas |
  | 1000 ns | 1000 / 62.5 = **16** cuentas | 1000 / 50 = 20 cuentas |

  En los dos chips la división sale exacta, así que el tiempo muerto real es justo el pedido.

- **Cómo se convierte en tiempo muerto.** El lado alto (HA) y el lado bajo (LA) de la pierna A usan dos comparadores del mismo timer, separados por esas cuentas:

  ```
  comparador del lado bajo = comparador del lado alto + DT
  ```

  HA está en alto mientras el contador va **debajo** de su comparador; LA, mientras va **arriba** del suyo. En la franja de DT cuentas entre los dos comparadores, **ninguno conduce**. Como el contador pasa por esa franja al subir y al bajar, el hueco aparece en los **dos flancos** de cada pulso. Lo hace el hardware: aunque el programa se atrase, el tiempo muerto no cambia.

- **Estado:** ✅ medido en el Nano con cursores: **1000 ns** (16 cuentas) y **500 ns** (8 cuentas), en los dos flancos.

### `COMPARADOR_ALTO_MAX` (317 en el 328P; 396 en el 4809)

```c
constexpr uint16_t COMPARADOR_ALTO_MAX = TOP_CUENTAS - DT_LEGACY_CUENTAS;
```

- **Qué es:** el valor más alto que puede tomar el comparador del lado alto.
- **De dónde sale:** 333 − 16 = **317**. El comparador del lado bajo va DT cuentas arriba; si el del lado alto pasara de 317, el del lado bajo pasaría de 333 (el TOP), y un comparador arriba del TOP nunca coincide con el contador: ese lado se quedaría fijo.
- **Por qué con el DT legacy:** es el peor caso (el más grande). Si cabe con 16, cabe con 8.

### `DT_MODERNO_MILESIMAS` (12) y `DT_LEGACY_MILESIMAS` (24)

```c
constexpr uint16_t DT_MODERNO_MILESIMAS = (uint32_t)DT_MODERNO_CUENTAS * 1000UL / CICLOS_CPU_PERIODO;
```

- **Qué es:** qué fracción del periodo se lleva el tiempo muerto, en milésimas.
- **De dónde sale:** 8 × 1000 / 666 = **12** (1.2 %); 16 × 1000 / 666 = **24** (2.4 %).
- **Para qué sirve:** es lo que "cuesta" el tiempo muerto en voltaje de salida y en distorsión. Coincide con la tabla de `docs/modulacion_spwm.md`. Con drivers rápidos se pierde la mitad que con los lentos: por eso vale la pena leer P9.

### Los cuatro candados (líneas 245–252)

| Candado | Qué impide |
| --- | --- |
| `DT_LEGACY_CUENTAS > DT_MODERNO_CUENTAS` | Que alguien invierta los valores y el "seguro" resulte el más corto |
| `DT_MODERNO_CUENTAS × 62.5 ns ≥ 500 ns` | Que la conversión deje menos tiempo muerto del pedido |
| `DT_LEGACY_CUENTAS × 62.5 ns ≥ 1000 ns` | Lo mismo para el legacy |
| `DT_LEGACY_CUENTAS × 10 < TOP` | Que el tiempo muerto ocupe más del 10 % del semiperiodo. Con 16 cuentas se dispararía si el TOP bajara de 161, o sea con una portadora de 50 kHz o más |

---

# Parte B: `config_spwm.h`

Este archivo usa los números de la portadora para fijar **los dos límites del ancho del pulso**: el más angosto y el más ancho que se permiten.

## 9. El pulso mínimo (líneas 19–39)

```c
constexpr uint16_t PULSO_MIN_NS      = 500;
constexpr uint8_t  PULSO_MIN_CUENTAS = ISLA_NS_A_CUENTAS(PULSO_MIN_NS);   // 8 / 10
```

### `PULSO_MIN_NS` (500 ns)

- **Qué es:** el pulso de compuerta más angosto que el firmware genera. Si la senoidal pide uno más angosto, **no se genera ninguno**.
- **Por qué existe.** Hay dos problemas, uno en cada extremo de la onda:

  1. **Cerca del cruce por cero** la senoidal pide pulsos de unas cuantas cuentas (la primera muestra pide 0.8 % del periodo: unos 300 ns con la amplitud al máximo). El gate driver y el MOSFET no alcanzan a encender por completo en ese tiempo: el MOSFET se queda a medias, en su zona lineal, donde disipa calor sin entregar energía útil. Es mejor no prenderlo.
  2. **Cerca del pico** el que se hace angosto es el pulso del **otro** lado de la pierna. Ese pulso es el que recarga el capacitor de *bootstrap*, que alimenta la compuerta del lado alto. Si desaparece, el lado alto se queda sin voltaje de compuerta y deja de conducir bien. Por eso se limita la amplitud máxima (sección 10).

- **De dónde sale el 500:** es un valor **inicial**, igual al tiempo muerto moderno. La lógica: si el driver necesita 500 ns para conmutar con seguridad, un pulso más corto que eso no alcanza a hacer nada útil.
- **Si lo subes:** se pierde un poco más de onda cerca del cruce por cero (más distorsión) y baja la amplitud máxima.
- **Si lo bajas:** pulsos que calientan sin entregar energía, y menos margen para el bootstrap.
- **Estado:** 🖥 la prueba en la PC confirma que nunca sale un pulso de menos de 500 ns. ⏳ **El valor en sí falta validarlo con osciloscopio y los gate drivers reales** (¿enciende completo el MOSFET en 500 ns?, ¿se recarga el bootstrap?). El encabezado del archivo lo dice: el pulso mínimo es un valor inicial, por validar con potencia.

### `PULSO_MIN_CUENTAS` (8 en el 328P; 10 en el 4809)

- **De dónde sale:** 500 / 62.5 = **8** cuentas, con la misma conversión hacia arriba.

- **El detalle fino: el umbral es el ancho completo, no la mitad** (líneas 31–37). En doble pendiente, un comparador de valor *c* produce normalmente un pulso de **2 × c** cuentas (c en la bajada del triángulo y c en la subida). Parecería entonces que para un pulso de 8 cuentas basta exigir c ≥ 4. **No es así.** El pulso centrado en el fondo del triángulo se arma con la bajada de **un** periodo y la subida **del siguiente**:

  ```
  pulso = c1 (bajada del periodo actual) + c2 (subida del periodo siguiente)
  ```

  Si el periodo vecino no lleva pulso (c2 = 0, por ejemplo junto al cruce por cero), el pulso se queda sólo con su mitad: **c1 cuentas**. Con el umbral en 4, salían pulsos de 4 cuentas = **250 ns**, la mitad de lo permitido.

  Esto lo encontró la prueba en la PC (`pruebas/prueba_spwm.cpp`) antes de llegar al banco. La corrección: exigir c ≥ **8**, el ancho completo. Así, aun en el peor caso, el pulso mide 500 ns.

- **Candado** (línea 77): al menos 1 cuenta.

---

## 10. El techo del 95 % y la amplitud máxima (líneas 41–82)

**Qué es la amplitud.** Es el ancho del pulso de potencia en el **pico** de la senoidal, en cuentas por lado del triángulo. El firmware multiplica cada valor de la tabla por la amplitud: con amplitud 0 no sale nada; con amplitud máxima sale todo el voltaje. El lazo de voltaje (pendiente) la subirá o la bajará para mantener 127 V con cualquier carga y cualquier batería.

**Índice de modulación:** m = amplitud / TOP. Con m = 1 el pulso del pico ocuparía todo el periodo.

La amplitud máxima es **la menor de dos límites**.

### Límite 1: el techo absoluto

```c
constexpr uint8_t  M_TECHO_PORCIENTO = 95;
constexpr uint16_t AMPLITUD_TECHO_CUENTAS = (uint32_t)TOP_CUENTAS * M_TECHO_PORCIENTO / 100;
```

#### `M_TECHO_PORCIENTO` (95)

- **Qué es:** el índice de modulación máximo permitido con cualquier módulo. Decisión del proyecto: la modulación nunca pasa de 0.95.
- **Por qué no 100 %:**
  1. **Margen para el lazo de voltaje.** Un lazo que trabaja pegado al tope ya no puede corregir hacia arriba: si la batería baja, el voltaje de salida cae y el lazo no tiene de dónde sacar más.
  2. **Margen para el PLL.** Al sincronizar con la red, el PLL podrá **bajar** el TOP unas cuentas. Una amplitud calculada con TOP = 333 no debe pasarse si el TOP baja a 330.
  3. **Siempre queda pulso para el bootstrap.**
- **Si lo bajas** (por ejemplo a 85): menos voltaje de salida disponible; con batería baja el inversor no alcanza los 127 V.
- **Candado** (línea 78): `M_TECHO_PORCIENTO <= 95`. Se puede bajar; **subirlo no compila**.

#### `AMPLITUD_TECHO_CUENTAS` (316 en el 328P; 395 en el 4809)

- **De dónde sale:** 333 × 95 / 100 = 316.35 → **316** (división entera: se trunca, nunca se pasa del 95 %).

### Límite 2: el físico, según el tiempo muerto en uso

```c
constexpr uint16_t AMPLITUD_FISICA_MODERNO = TOP_CUENTAS - DT_MODERNO_CUENTAS - PULSO_MIN_CUENTAS / 2;
constexpr uint16_t AMPLITUD_FISICA_LEGACY  = TOP_CUENTAS - DT_LEGACY_CUENTAS  - PULSO_MIN_CUENTAS / 2;
```

#### `AMPLITUD_FISICA_MODERNO` (321) y `AMPLITUD_FISICA_LEGACY` (313)

- **Qué es:** hasta dónde puede crecer el pulso de potencia dejando sitio, en el mismo periodo, para **el tiempo muerto** y para **un pulso mínimo del lado contrario** (el que recarga el bootstrap).
- **De dónde sale.** En cada lado del triángulo hay TOP cuentas. De ahí se restan:

  ```
  amplitud física = TOP − tiempo muerto − mitad del pulso mínimo

  DT 500 ns:   333 −  8 − 4 = 321
  DT 1 µs:     333 − 16 − 4 = 313
  ```

- **Por qué aquí sí es la mitad del pulso mínimo** (y en la sección 9 era el ancho completo). El pulso del lado contrario está centrado en la **punta** del triángulo, dentro de un mismo periodo: siempre tiene sus dos mitades iguales. Reservando 4 cuentas por lado, el pulso mide 8 cuentas = 500 ns. El caso de la sección 9 era distinto: ahí el pulso se armaba con mitades de **dos periodos distintos**, y una podía faltar.

### El resultado: la menor de las dos

```c
constexpr uint16_t AMPLITUD_MAX_MODERNO =
    AMPLITUD_TECHO_CUENTAS < AMPLITUD_FISICA_MODERNO ? AMPLITUD_TECHO_CUENTAS : AMPLITUD_FISICA_MODERNO;
constexpr uint16_t AMPLITUD_MAX_LEGACY =
    AMPLITUD_TECHO_CUENTAS < AMPLITUD_FISICA_LEGACY ? AMPLITUD_TECHO_CUENTAS : AMPLITUD_FISICA_LEGACY;
```

#### `AMPLITUD_MAX_MODERNO` (316) y `AMPLITUD_MAX_LEGACY` (313)

| Tiempo muerto | Techo (95 %) | Físico | **Máxima** | Quién manda | m |
| --- | --- | --- | --- | --- | --- |
| 500 ns (8 cuentas) | 316 | 321 | **316** | El techo | 0.949 |
| 1 µs (16 cuentas) | 316 | 313 | **313** | El límite físico | 0.940 |

- **Lectura de la tabla:** con drivers rápidos sobra sitio físico y manda la decisión del 95 %. Con drivers lentos, el tiempo muerto de 16 cuentas se come el margen y manda la física: quedan 3 cuentas menos (1 % menos de voltaje máximo).
- **Quién escoge cuál:** `spwm_preparar()` recibe el tiempo muerto que se leyó en P9 y fija la amplitud máxima que le corresponde. `spwm_fijar_amplitud()` recorta cualquier pedido que se pase.
- **En el 4809:** 395 (techo) y 391 (físico) 🖥.

- **Qué significa en volts.** Con m = 0.94 el pico de la salida del puente es 0.94 × Vbat. Con la batería en 48 V son 45 V de pico, unos 32 V eficaces antes del transformador. Es el dato del que parte la relación de vueltas del transformador, y la razón por la que con batería muy baja el inversor ya no puede sostener los 127 V. *(Cálculo ideal, sin caídas en los MOSFET ni en el filtro.)*

### Los cuatro candados (líneas 77–82)

| Candado | Qué impide |
| --- | --- |
| `PULSO_MIN_CUENTAS >= 1` | Un pulso mínimo de cero cuentas |
| `M_TECHO_PORCIENTO <= 95` | Subir el techo arriba del 95 % |
| `AMPLITUD_MAX_LEGACY <= COMPARADOR_ALTO_MAX` y `AMPLITUD_MAX_MODERNO <= TOP − DT moderno` | Que la amplitud empuje un comparador más allá del TOP |
| `AMPLITUD_MAX_LEGACY × 1000 / TOP >= 900` | Que la combinación de pulso mínimo y tiempo muerto deje el índice máximo abajo de 0.90. Avisa que el inversor ya no daría el voltaje de diseño |

- **Estado:** ✅ el reporte de arranque del Nano muestra amplitud 313 con m = 0.939 (1 µs) y 316 con m = 0.948 (500 ns); el reporte corta el tercer decimal en lugar de redondearlo. 🖥 Que el pico real mida eso con carga falta verificarlo.

---

# Parte C: cierre

## 11. Tabla resumen

| Constante | 328P a 16 MHz | 4809 a 20 MHz 🖥 | Unidad |
| --- | --- | --- | --- |
| `RELOJ_HZ` | 16 000 000 | 20 000 000 | Hz |
| `FRECUENCIA_LINEA_HZ` | 60 (o 50) | 60 (o 50) | Hz |
| `FSW_OBJETIVO_HZ` | 24 000 | 24 000 | Hz |
| `DT_MODERNO_NS` / `DT_LEGACY_NS` | 500 / 1000 | 500 / 1000 | ns |
| `MUESTRAS_CICLO_60` / `_50` | 400 / 480 | 400 / 480 | muestras |
| `MUESTRAS_SEMICICLO_60` / `_50` | 200 / 240 | 200 / 240 | muestras |
| `MUESTRAS_CUARTO_60` / `_50` | 100 / 120 | 100 / 120 | muestras |
| `TOP_CUENTAS` | **333** | 416 | cuentas |
| `CICLOS_CPU_PERIODO` | 666 | 832 | ciclos |
| `FSW_REAL_CHZ` | 2 402 402 (24 024.02 Hz) | 2 403 846 (24 038.46 Hz) | centésimas de Hz |
| `F_SALIDA_60_MHZ` | 60 060 (60.060 Hz) | 60 096 (60.096 Hz) | milésimas de Hz |
| `F_SALIDA_50_MHZ` | 50 050 (50.050 Hz) | 50 080 (50.080 Hz) | milésimas de Hz |
| `ERROR_FRECUENCIA_PPM` | 1000 (+0.10 %) | 1600 (+0.16 %) | ppm |
| `NS_X10_POR_CUENTA` | 625 (62.5 ns) | 500 (50 ns) | décimas de ns |
| `DT_MODERNO_CUENTAS` | **8** | 10 | cuentas |
| `DT_LEGACY_CUENTAS` | **16** | 20 | cuentas |
| `COMPARADOR_ALTO_MAX` | 317 | 396 | cuentas |
| `DT_MODERNO_MILESIMAS` / `_LEGACY_` | 12 / 24 | 12 / 24 | milésimas del periodo |
| `PULSO_MIN_NS` | 500 | 500 | ns |
| `PULSO_MIN_CUENTAS` | **8** | 10 | cuentas |
| `M_TECHO_PORCIENTO` | 95 | 95 | % |
| `AMPLITUD_TECHO_CUENTAS` | 316 | 395 | cuentas |
| `AMPLITUD_FISICA_MODERNO` / `_LEGACY` | 321 / 313 | 401 / 391 | cuentas |
| `AMPLITUD_MAX_MODERNO` | **316** (m 0.949) | 395 (m 0.950) | cuentas |
| `AMPLITUD_MAX_LEGACY` | **313** (m 0.940) | 391 (m 0.940) | cuentas |

---

## 12. Lo que se midió en banco

**Montaje:** Arduino Nano (ATmega328P) por USB, **sin etapa de potencia** (D12 en 0), analizador lógico genérico de 24 MHz muestreando a 16 MHz, PulseView. Medición manual con los decodificadores PWM y Timing y con cursores, más el Monitor Serie. Fecha: 2026-11-12. Detalle y capturas: [README del firmware del CRÍTICO, secciones 7.0, 7.0b y 7.0c](../firmware/critico/README.md).

| Qué | Constantes que comprueba | Calculado | Medido | |
| --- | --- | --- | --- | --- |
| Frecuencia de salida a 60 Hz | TOP, muestras, portadora | 60.060 Hz (16.650 ms) | **60.040 Hz** (16.656 ms) | ✅ |
| Frecuencia de salida a 50 Hz | Lo mismo con 480 muestras | 50.050 Hz | **50.033 Hz** (19.987 ms) | ✅ |
| Ciclos en 10 s a 60 Hz | Frecuencia de salida | 600–601 | 601, 601, 600 | ✅ |
| Ciclos en 10 s a 50 Hz | Frecuencia de salida | 500–501 | 501 | ✅ |
| Lado bajo de la pierna B en alto, 60 Hz | 200 muestras por semiciclo | 199 / 400 = 49.75 % | **49.7476 %** | ✅ |
| Lado bajo de la pierna B en alto, 50 Hz | 240 muestras por semiciclo | 239 / 480 = 49.79 % | **49.79–49.80 %** | ✅ |
| Tiempo muerto de la pierna A, P9 abierto | `DT_LEGACY_CUENTAS` = 16 | 1000 ns | **1000 ns** (a 60 y 50 Hz) | ✅ |
| Tiempo muerto de la pierna A, P9 a GND | `DT_MODERNO_CUENTAS` = 8 | 500 ns | **500 ns**, en los dos flancos | ✅ |
| Hueco de la pierna B en el cruce por cero | Un periodo de portadora | 41.625 µs | **40.4–42.2 µs** | ✅ |
| Amplitud máxima con 1 µs | `AMPLITUD_MAX_LEGACY` | 313, m 0.939 | 313, m 0.939 (reporte) | ✅ |
| Amplitud máxima con 500 ns | `AMPLITUD_MAX_MODERNO` | 316, m 0.948 | 316, m 0.948 (reporte) | ✅ |

### Por qué 60.040 y no 60.060 Hz

La diferencia es de **−333 ppm** (0.033 %). No viene del firmware: viene del **cristal del Nano** y del reloj del analizador. Un cristal de 16 MHz de clon tiene tolerancias de ese orden.

La prueba: a 50 Hz la desviación es **la misma** (50.033 contra 50.050 Hz = −340 ppm). Si el error fuera del cálculo, cambiaría con el número de muestras; como es igual en las dos frecuencias, es del reloj. En modo red, el PLL lo corrige.

### Por qué la pierna B da 40.4–42.2 µs y no un valor fijo

La pierna A y la pierna B hacen su tiempo muerto de dos formas distintas:

- **Pierna A (24 kHz): la hace el hardware del timer.** Por eso mide 1000 ns o 500 ns **exactos**, siempre.
- **Pierna B (60 Hz): la mueve el programa** dentro de la interrupción: apaga un lado en una interrupción y prende el otro en la siguiente, un periodo después (41.625 µs). Pero la interrupción no siempre entra en el mismo instante: el CPU termina primero la instrucción en curso, o espera a que acabe otra interrupción (el reloj de `millis()`, el puerto serie). Por eso el hueco varía un par de microsegundos.

En cinco cruces medidos fue de **40.437 a 42.188 µs**. El valor más bajo sigue siendo unas 40 veces lo que necesita un gate driver (1 µs). En la pierna B el hueco puede ser largo porque sólo conmuta 120 veces por segundo: lo que se pierde de onda es despreciable.

### Lo que el banco enseñó y la simulación no tenía

En la primera muestra del semiciclo negativo aparece un **pulso solitario de HA** de unos 20 µs. El chip, al pasar un comparador de 0 a otro valor, pone la salida en el nivel que le toca aunque no haya coincidencia. Es seguro (el lado bajo está apagado y la salida queda en 0 V, que es lo que pide el seno en ese punto), y el modelo de la PC se corrigió para reproducirlo. **Lección:** cada paso se valida en hardware real.

---

## 13. Lo que NO está validado todavía

| Qué | Estado | Qué hace falta |
| --- | --- | --- |
| Que el MOSFET encienda completo con un pulso de 500 ns (`PULSO_MIN_NS`) | ⏳ | Osciloscopio en la compuerta, con gate driver y MOSFET reales |
| Que el bootstrap se recargue con el pulso mínimo en el pico (`AMPLITUD_FISICA_*`) | ⏳ | Osciloscopio en el capacitor de bootstrap, a amplitud máxima |
| Que 500 ns y 1 µs basten para **estos** MOSFET con carga (`DT_*_NS`) | ⏳ | Medir la corriente de la pierna buscando picos de conducción cruzada |
| El pulso más angosto real y la forma senoidal pulso por pulso | 🖥 | `pruebas/analiza_captura.py` sobre una captura completa, o cursores |
| Duración real de la interrupción (169–180 ciclos en simulación) | 🖥 | Pin de prueba D7 y analizador |
| Todo el ATmega4809 | 🖥 | Escribir `spwm_4809.cpp` y repetir el banco en un Nano Every |
| Voltaje de salida real con m = 0.94 | ⏳ | Etapa de potencia, transformador y carga |

Lo validado hasta hoy es **la temporización de las señales de compuerta**, sin potencia. Los números de este documento que dependen del comportamiento de los MOSFET y los drivers (pulso mínimo, tiempos muertos como "suficientes") son valores de diseño por confirmar.

---

## 14. "Si quiero cambiar…"

| Quiero… | Qué toco | Qué más debo revisar |
| --- | --- | --- |
| Un inversor de **50 Hz** | `FRECUENCIA_LINEA_HZ = LINEA_50_HZ` (línea 85) | **Transformador para 50 Hz** y, casi siempre, para 220–240 V; relés, capacitores y fusibles de salida |
| Usar un **Nano Every** | Nada en estos archivos: escoger la placa con MegaCoreX a 20 MHz | `spwm_4809.cpp` está pendiente: hoy no compila, a propósito |
| Otro **tiempo muerto** para un driver nuevo | `DT_MODERNO_NS` o `DT_LEGACY_NS` (líneas 108–109) | Actualizar `docs/identificacion_dt_pin9.md`; volver a medir con analizador; la amplitud máxima cambia sola |
| Un **pulso mínimo** distinto | `PULSO_MIN_NS` (`config_spwm.h`, línea 38) | Correr `pruebas/prueba_spwm.cpp`; validar con osciloscopio |
| **Bajar** el techo de modulación | `M_TECHO_PORCIENTO` (línea 65) | Que el voltaje de salida siga alcanzando con batería baja |
| **Subir** el techo arriba de 95 % | No se puede: el candado no deja compilar | Es una decisión del proyecto, no un descuido |
| Otra **portadora** | `FSW_OBJETIVO_HZ` (línea 104) | Debe ser múltiplo de 1200 Hz entre 20 y 25 kHz; **regenerar la tabla** con `herramientas/genera_tabla_seno.py`; revisar el filtro de salida y las pérdidas; repetir todo el banco |
| El **TOP**, las **muestras**, las **cuentas** o las **amplitudes** | **Nada a mano:** se derivan | Cambiar los cinco números de diseño; lo demás se recalcula |

**Regla general:** después de cualquier cambio, (1) que compile sin que salte ningún candado, (2) correr la prueba en la PC, (3) comparar el reporte de arranque del Monitor Serie con la tabla de la sección 11 y (4) medir con analizador lógico **antes** de conectar la etapa de potencia.

---

## 15. La misma receta en otro micro

> **Estas fórmulas son la base de tiempo de cualquier PWM.** Antes de escribir una sola línea de registros, con cualquier microcontrolador, hay que hacer estas cuentas: cuánto dura un tick, cuántos ticks tiene un periodo y cuántos le tocan al tiempo muerto, al pulso mínimo y al pulso de potencia. Lo que cambia de un micro a otro es el nombre de los registros y cuánto trabajo hace el hardware; la metodología es la misma.

### Los pasos que no cambian

1. Reloj del timer → cuánto dura un tick.
2. TOP = reloj / (2 × portadora), en doble pendiente (pulsos centrados).
3. Muestras = portadora / frecuencia de línea, múltiplo de 4.
4. Tabla de un cuarto de onda, muestreada en k + 0.5.
5. Tiempo muerto y pulso mínimo en ticks, redondeando hacia arriba.
6. Amplitud máxima = la menor entre el techo y el límite físico.
7. Candados al compilar que vigilen cada resultado.
8. Banco con analizador lógico **antes** de conectar la potencia.

### La receta aplicada a otros micros

**Son ejemplos de cálculo, sin validar:** el reloj del timer depende de cómo se configure cada chip y debe confirmarse con su hoja de datos. De estos, sólo el ATmega328P se ha probado en este proyecto.

| | ATmega328P (Nano) ✅ | ATmega4809 (Nano Every) 🖥 | STM32F103 (Bluepill) 🖥 | ESP32 🖥 |
| --- | --- | --- | --- | --- |
| Reloj del timer | 16 MHz | 20 MHz | 72 MHz | 160 MHz (módulo MCPWM) |
| Tick = 1 / reloj | 62.5 ns | 50 ns | 13.9 ns | 6.25 ns |
| TOP = ⌊reloj / 48 000⌋ | 333 | 416 | 1500 | 3333 |
| Portadora real | 24 024.02 Hz | 24 038.46 Hz | 24 000.00 Hz | 24 002.40 Hz |
| Salida a 60 Hz (400 muestras) | 60.060 Hz | 60.096 Hz | 60.000 Hz | 60.006 Hz |
| Tiempo muerto de 500 ns | 8 ticks | 10 ticks | 36 ticks | 80 ticks |
| Tiempo muerto de 1 µs | 16 ticks | 20 ticks | 72 ticks | 160 ticks |
| Pasos para ajustar el pulso | 333 | 416 | 1500 | 3333 |
| Modo de doble pendiente | Modo 8 del Timer1 | *Dual-slope* del TCA0 | *Center-aligned* del TIM1 | *Up-down* del MCPWM |
| Cómo se hace el tiempo muerto | Dos comparadores separados | Dos comparadores separados | Generador de tiempo muerto del propio timer | Módulo de tiempo muerto del MCPWM |

### Qué se gana con más MHz y más memoria

- **Tick más fino:** el ancho del pulso se ajusta en más pasos (1500 o 3333 contra 333). La senoidal sale con menos escalones y el lazo de voltaje puede corregir más fino.
- **Menos error de frecuencia:** con un TOP más grande, truncarlo pesa menos. Con 72 MHz la división sale exacta: 24 000.00 Hz y 60.000 Hz.
- **Más presupuesto para la interrupción:** más ciclos de CPU en los mismos 41.6 µs.
- **Más memoria:** caben tablas más finas, o la tabla completa si se prefiere la lectura directa. El cuarto de onda sigue siendo válido; simplemente deja de ser obligatorio.
- **Hardware dedicado:** los timers del STM32 y del ESP32 ya traen salidas complementarias con tiempo muerto. En el AVR se logra con el truco de los dos comparadores.

### Qué hay que cuidar al cambiar de familia

- **Lógica de 3.3 V:** las señales hacia los gate drivers y las lecturas del ADC cambian de nivel. La tarjeta de control actual está hecha para un Nano de 5 V.
- **El tiempo muerto siempre por hardware.** Con más razón en un ESP32, donde el WiFi y el sistema operativo pueden retrasar al programa.
- **La capa de registros se escribe de nuevo** para cada chip, como `spwm_328p.cpp`. Los archivos de configuración y la lógica son comunes: hay que agregar el chip en "Chip y reloj" (sección 1) con su reloj esperado.
- **Los candados se conservan.** Son los que avisan si con el reloj nuevo la portadora o la frecuencia se salen de rango.
- **El banco se repite completo.** El pulso solitario de HA (sección 12) enseñó que cada chip tiene comportamientos que la simulación no trae.

---

## Referencias dentro del repositorio

- [`docs/modulacion_spwm.md`](modulacion_spwm.md): por qué modulación híbrida y por qué 24 kHz sincronizados con la línea (sección 4.3).
- [`docs/identificacion_dt_pin9.md`](identificacion_dt_pin9.md): el contrato de P9 y los tiempos muertos por módulo.
- [`docs/decisiones_criticas.md`](decisiones_criticas.md): DC-017 (dos microcontroladores) y DC-018 (zócalo multimodelo).
- [`firmware/critico/README.md`](../firmware/critico/README.md): pruebas de banco con capturas (secciones 7.0, 7.0b y 7.0c).
- `firmware/critico/ISLA_CRITICO/spwm_328p.cpp`: cómo se usan estos números en el Timer1.
- `firmware/critico/pruebas/prueba_spwm.cpp`: la prueba en la PC que verifica tiempo muerto, pulso mínimo y traslapes cuenta por cuenta.
