# Medición de energía: qué ve el trinchero y por qué

**Proyecto:** Isla del Sol — inversor 48 V → 127 V / 60 Hz, 1000 W
**Módulo:** `energia` (auxiliar) · **Estado:** diseño acordado, **sin construir**
**Fecha:** 2026-11-17

> **En resumen: watts para lo que pasa ahora, kWh para lo que se acumuló, y la máxima solo cuenta si se sostuvo 2 segundos.**

---

## 1. Qué ve el usuario

Dos pantallas en el LCD de 16×2. Nada más.

```
 780 W      78%
MAX24h  950 W
```

```
HOY    4.8 kWh
TOTAL 1247 kWh
```

| Número | Qué es |
| --- | --- |
| **780 W** | Lo que está jalando en este momento |
| **78 %** | Qué tanto de los 1000 W del inversor va ocupado |
| **MAX24h 950 W** | La potencia **sostenida** más alta de las últimas 24 horas |
| **HOY 4.8 kWh** | Energía consumida en las últimas 24 horas |
| **TOTAL 1247 kWh** | Energía consumida desde que se instaló |

---

## 2. Watts para la potencia, kWh para la energía

**Decisión: la potencia se muestra en watts, no en kilowatts.**

El inversor trabaja entre 0 y 1000 W. En kW eso sería de 0.00 a 1.00: puros ceros y decimales que hay que traducir mentalmente. En watts son los números que el trinchero ya conoce, porque así vienen marcados los aparatos — el foco dice 9 W, el tostador 800 W, la bomba 750 W.

**Y hay una segunda razón, más importante: separar potencia de energía.** Confundirlas es *el* error clásico de todo medidor. Si las unidades son distintas —W para una, kWh para la otra— nadie va a creer que son lo mismo medido diferente.

**De pilón, los watts aguantan bien los arranques.** Cuando arranca la bomba, la pantalla marca `1850 W 185%`, y ese número arriba de 100 % **es la alarma por sí solo**, sin agregar nada. En kW habría que leer "1.85" y pensarlo.

---

## 3. Por qué se mide en DC y no en AC

Esta es la decisión que define todo lo demás.

**En AC no basta con multiplicar voltaje por corriente.** V eficaz × I eficaz da **VA, no watts**. Con un foco incandescente da igual, pero con un refrigerador, una bomba o cualquier fuente conmutada la corriente va desfasada o deformada, y el número sale inflado. Para sacar watts de verdad hay que multiplicar **v(t) × i(t)** muestra por muestra y promediar — y eso exige un sensor de corriente **en AC** que el proyecto no tiene.

**En DC el problema no existe:** P = V × I, sin fase, sin factor de potencia. Y los dos sensores ya están en la tarjeta:

| Qué | Dónde | Estado |
| --- | --- | --- |
| Voltaje de batería | A3 del CRÍTICO, divisor de [DC-007](decisiones_criticas.md) | ✅ funcionando, ya viaja en la foto |
| Corriente de batería, en los dos sentidos | `I_OUT` del INA240 (0–3.3 V, centro 1.67 V) | ⚠️ el hardware existe, **el firmware no la lee** |

El README del sensor ya decía para qué servía `I_OUT`: *"ADC: potencia, eficiencia, control del cargador, pantalla"*. El terreno estaba puesto desde antes.

**Y midiendo en DC se mide lo que de verdad importa: lo que se le está yendo de la batería.** Los watts que entrega el inversor son esos menos la eficiencia. Si alguna vez se muestran, **se dicen como estimado**.

---

## 4. La potencia máxima: la regla de los 2 segundos

**Para registrarse, la potencia debe sostenerse 2 segundos.**

La razón no es técnica, es de utilidad: un refrigerador puede arrancar en 1800 W y trabajar en 150. Si el arranque cuenta como "máxima", el número no dice nada de cómo se consume. Lo que el usuario quiere saber es **cuánto trabajo sostenido aguanta su instalación** — el tostador de 800 W, la bomba bombeando, no el chispazo del arranque.

**Y los picos ya los vigila otro.** El comparador del módulo de corriente limita en ±75 A y corta en ±105 A ([DC-016](decisiones_criticas.md)). La protección de sobrecarga está resuelta en hardware, así que la pantalla no tiene que hacer de detector de picos: ese trabajo ya está tomado.

### Cómo se cuenta

Con la foto del CRÍTICO llegando cada 100 ms, **2 segundos son 20 lecturas seguidas**.

Se toma la **mínima** de cada bloque de 20, no el promedio. Si la mínima de esos 2 segundos fue 800 W, entonces estuvo arriba de 800 W **todo** el tiempo: eso es exactamente "se sostuvo". Al cerrar el bloque, esa mínima se compara contra el récord.

**Ventana por bloques y no deslizante**, por austeridad: 4 bytes en lugar de 40. La de bloques podría perderse una meseta que dure *exactamente* 2 segundos y caiga partida entre dos bloques, pero un tostador trabajando, una bomba bombeando o un refri enfriando duran minutos, no dos segundos justos.

### La trampa del enlace

**El contador solo avanza con fotos frescas.** El enlace entrega foto válida únicamente si tiene menos de 300 ms. Si el I2C se cae un rato y vuelve, no se vale juntar lecturas de antes y después del hueco y llamarlas "sostenidas": al perderse el enlace, **el bloque se descarta y vuelve a empezar**.

### La etiqueta importa

Si el LCD dijera nada más `MAX 847W`, quien sabe que su bomba jala mucho más al arrancar pensaría que el medidor está descompuesto. Por eso dice **`MAX24h`**, y aquí queda escrito que **los arranques no cuentan a propósito**.

---

## 5. La ventana de 24 horas

Hay dos maneras, y se eligió la cara porque cabe:

| | Cómo funciona | RAM | Problema |
| --- | --- | --- | --- |
| Simple | un contador que se borra cada 24 h | ~8 B | Justo después del borrado dice "máxima: 300 W" aunque ayer hayan sido 1200. Es un odómetro de viaje |
| **Ventana real** ✅ | 24 cubetas de una hora; al entrar una sale la más vieja | 96 B | Ninguno |

Con la ventana real **los dos números dicen la verdad siempre**, sin el hueco de "se me acaba de borrar".

**La base de tiempo son los ciclos de línea que cuenta el CRÍTICO**, no `millis()`. Vienen del cristal y ya viajan en la foto: 216 000 ciclos son una hora a 60 Hz. No hay reloj de tiempo real en el proyecto, así que son **24 horas de operación**: si se apaga el equipo, el reloj se detiene con él. Eso se dice tal cual, no se disimula.

---

## 6. La EEPROM: 365 escrituras al año

**El acumulado se guarda una vez cada 24 horas.** Son 365 escrituras al año, y con los 100 000 ciclos que aguanta la celda eso da **274 años**. El desgaste deja de ser un tema, resuelto por escribir menos.

**Más un guardado extra que sale gratis.** Escribir una vez al día significa que un corte puede borrar hasta 24 horas del acumulado. No hace falta aguantárselo: la foto ya trae `estadoBateria`, y cuando pasa a **aviso** o **corte bajo**, el auxiliar sabe que esto se va a apagar. Entonces guarda también ahí. Esos eventos son raros —unos pocos al mes—, así que el conteo apenas se mueve, y el único escenario de pérdida grande queda tapado.

Lo que no cubre: un corte brusco sin aviso (alguien desconecta la batería). Ahí se pierde hasta un día. **Se acepta y se documenta**; taparlo pediría un capacitor y un detector de caída, y no vale la pena para un medidor de trinchera.

---

## 7. Lo que hay que construir antes

En orden. Los dos primeros son el trabajo de verdad; los dos últimos son lo que se ve.

| # | Qué | Por qué |
| --- | --- | --- |
| **1** | **`corriente.cpp` en el CRÍTICO** | Leer `I_OUT` del INA240. **Sin esto no hay nada**: el hardware existe pero el firmware nunca lo ha leído |
| **2** | **Protocolo v3** | La foto está llena: los 16 bytes del v2 están todos ocupados. Hay que crecerla y revalidar el enlace en los dos micros |
| **3** | `energia.cpp` en el auxiliar | Integrar, la ventana de 24 h, la EEPROM |
| **4** | Las pantallas del LCD | Las dos de la sección 1 |

---

## 8. Presupuesto de recursos, medido

Compilado el auxiliar tal como está hoy (Nano: 30 720 B de flash, 2048 B de RAM):

```
Flash: 14560 B de 30720 (47 %)  ->  LIBRES 16160 B
RAM  :  1069 B de  2048 (52 %)  ->  LIBRES   979 B
```

Reservando ~450 B de pila quedan **529 B utilizables**. El módulo pide:

| Partida | Bytes |
| --- | --- |
| Ventana de 24 h: 24 cubetas × 2 B de Wh | 48 |
| Ventana de 24 h: 24 máximas × 2 B | 48 |
| Máxima sostenida por bloques (mínima + contador) | 4 |
| Acumulado total en Wh (32 bits) | 4 |
| Acumulado del día en Wh (32 bits) | 4 |
| Potencia actual y auxiliares | 20 |
| **Total** | **128** |

**Quedan 401 B de margen, el 76 % de lo utilizable.** Cabe con holgura.

---

## 9. Lo que este módulo NO hace, y por qué

| No hace | Por qué |
| --- | --- |
| **Watts verdaderos en AC** | Haría falta un sensor de corriente en AC que la tarjeta no tiene. Ver sección 3 |
| **Registrar los picos de arranque** | A propósito: eso lo vigila el hardware (DC-016), y ensuciaría el número útil |
| **Guardar en microSD** | La librería de SD necesita un buffer de sector de 512 B y en el auxiliar solo hay 529 B utilizables. **No cabe**, y no es cuestión de optimizar: el sector de 512 B es un piso duro de FAT |
| **Fecha y hora reales** | No hay reloj de tiempo real. Las 24 horas son de operación, no de calendario |
| **Mandar datos a la red** | Fuera de alcance de esta versión. Ver abajo |

### Lo que se discutió y se dejó fuera, para que quede el rastro

Se evaluó agregar **un ESP32 con WiFi y microSD** como registrador. Resuelve tres cosas de golpe (la SD cabe de sobra, NTP da fecha y hora reales, y se ve desde el teléfono) y sale barato. **Se descartó por austeridad**, para quedarse con los recursos del 328.

Si algún día se retoma, estas dos quedaron acordadas y conviene no perderlas:

- **El cable va en un solo sentido.** TX del auxiliar a RX del ESP32, y **el TX del ESP32 no se conecta a nada**. No como promesa del firmware, sino como cable: por más que alguien entre al ESP32 por la red, no puede mandarle nada al inversor porque no hay alambre por donde. Misma filosofía que el `SENSOR_OK` de DC-014.
- **El acumulado seguiría viviendo en la EEPROM del auxiliar.** El medidor tiene que funcionar sin tarjeta, sin WiFi y sin ESP32 conectado.

Y queda dicho que **mandar órdenes al inversor desde la red es otra discusión completa** —autenticación, qué pasa si se cae el enlace a media orden, quién puede apagar un inversor que está alimentando algo— y no se cruza de contrabando con un medidor de energía.

---

## 10. Pendientes

- [ ] Confirmar en qué pin del CRÍTICO entra `I_OUT` y si queda libre
- [ ] Definir el formato de la foto del protocolo v3
- [ ] Calibrar la corriente contra un patrón (pinza o fuente conocida)
- [ ] Decidir si se muestran watts de AC estimados o todo se habla en términos de batería
- [ ] Validación en banco: el acumulado contra un medidor de referencia durante 24 h

---

## 11. Referencias

- [`decisiones_criticas.md`](decisiones_criticas.md): DC-007 (divisor de batería), DC-014 (corte por hardware fail-safe), DC-016 (niveles de protección), DC-020 (reloj del ADC)
- [README del sensor DC](../hardware/sensores/sensor-dc-ina-comparador/README.md): qué entrega `I_OUT` y con qué escala
- `firmware/critico/ISLA_CRITICO/enlace_protocolo.h`: el protocolo v2 que hay que crecer
- `firmware/auxiliar/ISLA_AUX_PANEL/enlace_aux.h`: la foto como la recibe el auxiliar

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz).*
