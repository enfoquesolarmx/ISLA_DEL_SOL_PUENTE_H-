# Cómo se trabaja en este repositorio

Esto no es un reglamento de estilo. Son las reglas que ya nos costaron algo, escritas para no volver a pagarlas. Cada una dice **qué falló** cuando no estaba.

---

## 1. Un submódulo se publica validado, y con su documentación en el mismo movimiento

El repositorio es la referencia con la que un trinchero arma. Así que lo que llega a `main` tiene que estar **validado en banco y explicado**, en el mismo empujón.

**Validado** quiere decir que hay una corrida con números y un criterio que se cumplió — no que compila, no que «debería funcionar». El protocolo de banco ([`docs/protocolo_banco_control.md`](docs/protocolo_banco_control.md)) es el formato: criterio, resultado, margen.

**Y con su documentación** no es cortesía: es seguridad.

> ⚠️ **Lo que pasó el 24 de noviembre de 2026.** Se subió el firmware de la etapa 4 —que usa **D6 como entrada del comparador** (`AIN0`)— quince minutos antes que la DC-023, que es la decisión que mudó `TN_CMD` de D6 a D13. En esos quince minutos `main` dijo dos cosas distintas del mismo pin: el firmware lo usaba de entrada analógica y el README publicado lo daba como la orden del relé K5. **Quien hubiera armado desde `main` en ese rato habría cableado la bobina de un relé al pin del comparador.**

El firmware solo, sin la decisión que lo explica, no es documentación incompleta: es una trampa para quien arma. **Van juntos o no van.**

### Y la tarjeta terminada va al final

Cada submódulo validado sube como referencia. **La tarjeta completa es la culminación de todos los submódulos validados**, y se publica cuando esté validada en físico — no antes, y no como el primer paso.

Con un matiz que vale la pena saber distinguir: **poner al día una tarjeta ya publicada no es publicarla.** Si una decisión validada cambia un pin, el esquemático tiene que seguirla en el mismo PR. Si no, se repite exactamente la falla de arriba.

---

## 2. Ningún cambio eléctrico sin autorización

Valores de componentes, asignación de pines, topología, umbrales de protección: **se proponen, no se cambian.** Con la razón, el número y la fuente, y se espera la respuesta.

Esto incluye los cambios que parecen obvios o gratuitos. Varias decisiones de este repositorio se ven arbitrarias y no lo son — están contra una medición que no está a la vista.

---

## 3. La frecuencia de línea se elige en un solo lugar

**50 o 60 Hz se selecciona únicamente en [`firmware/critico/ISLA_CRITICO/config_portadora.h`](firmware/critico/ISLA_CRITICO/config_portadora.h), en tiempo de compilación.** Nunca desde el menú de SETUP, nunca por I2C, nunca en caliente.

La razón es que de la frecuencia de línea cuelgan el TOP de la portadora, el largo de la tabla de seno, el reparto del ADC y el emparejamiento de las ventanas de RMS. Un menú que la cambiara tendría que recalcular todo eso con el puente conmutando.

---

## 4. Toda afirmación lleva fuente, y la fuente se verifica

Hojas de datos, normas y mediciones propias. **Una cuenta que no se puede rehacer no es un argumento.**

- Si el número sale de una medición, va con el instrumento y la corrida.
- Si sale de una hoja de datos, va con la referencia numerada en [`docs/decisiones_criticas.md`](docs/decisiones_criticas.md).
- Si las fuentes se contradicen, **se anota la contradicción** en vez de elegir una. Hay por lo menos dos casos así escritos, y los dos siguen abiertos a propósito.
- Si el número es una estimación, dice que lo es. El costo de la ISR de TWI lleva semanas diciendo «sin medir» y eso es correcto.

---

## 5. Un número que no se midió no se escribe como medido

El error más caro de este proyecto no fue un cálculo mal hecho: fue **una estimación que se usó tres días como si fuera una medición**. La ISR de TWI se estimó en ~112 ciclos leyendo un cajón del histograma, y la corrida de carga demostró que ese cajón no medía TWI — el lazo está ciego justo durante la transferencia.

De ahí salen tres hábitos:

- **Decir qué no puede medir el instrumento.** Si hay una ventana ciega, se cuantifica y se escribe.
- **Un criterio puede estar mal, no sólo el resultado.** `rechazados = 0` era imposible de cumplir por física del comparador, y el error estaba en el criterio.
- **La medición puede perturbar lo que mide.** La estadística del jitter dentro de una ISR costó 117 ciclos y tiró 14 pasos de onda. Se mide desde el lazo cuando se puede.

---

## 6. Una decisión, una rama, una entrada

| | |
| --- | --- |
| Decisiones | `docs/dc-0NN-tema-corto` |
| Firmware | `firmware/tema-corto` |
| Hardware | `hardware/tarjeta-tema` |

Cada decisión de diseño se escribe en [`docs/decisiones_criticas.md`](docs/decisiones_criticas.md) con número, estado, fecha y referencias, y se agrega al índice del encabezado. Una decisión que reemplaza a otra **no la borra**: la marca como reemplazada y dice por qué.

### La trampa de GitHub con los PR encadenados

Las entradas se escriben una sobre otra en el mismo archivo, así que sus PR quedan encadenados: cada uno con base en el anterior.

> ⚠️ **GitHub sólo reapunta la base de un PR cuando la rama de abajo se *borra*, no cuando se *fusiona*.** Si se fusiona la cadena de abajo hacia arriba sin esperar ese borrado, cada PR se fusiona **dentro de su rama base** en vez de en `main`, y `main` no recibe nada. No se pierde trabajo —queda todo en la rama más baja— pero hay que recogerlo con un PR de cierre.

Las dos formas correctas: **cambiar la base a `main` a mano** antes de fusionar cada uno, o fusionar sólo la rama más profunda, que ya contiene a todas las demás.

---

## 7. El orden de conexión en el banco no es negociable

Está en el protocolo de banco y se repite aquí porque es el que se salta solo:

1. Armar el divisor completo **con el pin del micro al aire**.
2. Medir el pico con el multímetro, con el ajuste en los dos extremos.
3. **Sólo con el pico confirmado abajo del riel**, conectar el micro.

Armar al revés —el micro puesto y después girar el trimmer— es la forma fácil de pasarse del riel sin enterarse.

---

*Documentación bajo licencia CC BY-SA 4.0 (ver [`LICENSE.md`](LICENSE.md)).*
