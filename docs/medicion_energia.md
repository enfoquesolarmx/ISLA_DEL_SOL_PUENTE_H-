# Medición de energía: qué ve el trinchero y por qué

**Proyecto:** Isla del Sol — inversor 48 V → 127 V / 60 Hz, 1000 W
**Módulo:** `energia` (auxiliar) · **Estado:** diseño acordado, **sin construir**
**Fecha:** 2026-11-17

> **En resumen: watts para lo que pasa ahora, kWh para lo que se acumuló, y la máxima solo cuenta si se sostuvo 2 segundos.**
>
> **Y todo se lleva por partida doble: lo que la casa consumió y lo que entró a la batería. El signo de la corriente es la única diferencia.**

---

## 1. Qué ve el usuario

Seis pantallas en el LCD de 16×2. Nada más.

```
 780 W      78%        CONSUMO DE HOY
MAX24h  950 W               4.8 kWh

AYER                   PROMEDIO kWh/DIA
     6.2 kWh           7d  6.2 30d  5.8

HISTORICO              DESPIERTO
     1247 kWh              45d 07h
```

**Tres de las cuatro llevan la etiqueta arriba y el número solo abajo.** No es capricho de diseño: es lo que hace que el número nunca se desborde del renglón (sección 2.1), y de paso deja el dato grande y despejado, que es como se lee de un vistazo.

| Número | Qué es |
| --- | --- |
| **780 W** | Lo que está jalando en este momento |
| **78 %** | Qué tanto de los 1000 W del inversor va ocupado |
| **MAX24h 950 W** | La potencia **sostenida** más alta de las últimas 24 horas |
| **CONSUMO DE HOY 4.8 kWh** | Energía consumida en las últimas 24 horas |
| **HISTORICO 1247 kWh** | Energía consumida desde que se instaló |
| **AYER 6.2 kWh** | El último día cerrado |
| **PROMEDIO 7d / 30d** | Consumo diario promedio de la semana y del mes |
| **DESPIERTO 45d 07h** | Cuánto lleva encendido sin reiniciarse |

---

## 2. Watts para la potencia, kWh para la energía

**Decisión: la potencia se muestra en watts, no en kilowatts.**

El inversor trabaja entre 0 y 1000 W. En kW eso sería de 0.00 a 1.00: puros ceros y decimales que hay que traducir mentalmente. En watts son los números que el trinchero ya conoce, porque así vienen marcados los aparatos — el foco dice 9 W, el tostador 800 W, la bomba 750 W.

**Y hay una segunda razón, más importante: separar potencia de energía.** Confundirlas es *el* error clásico de todo medidor. Si las unidades son distintas —W para una, kWh para la otra— nadie va a creer que son lo mismo medido diferente.

**De pilón, los watts aguantan bien los arranques.** Cuando arranca la bomba, la pantalla marca `1850 W 185%`, y ese número arriba de 100 % **es la alarma por sí solo**, sin agregar nada. En kW habría que leer "1.85" y pensarlo.

### 2.1 Nunca se cambia a MWh

**El total se queda en kWh toda la vida del equipo.** No se pasa a MWh al llegar a 100 000, ni nunca.

Tres razones:

1. **MWh no es unidad de trinchera.** El recibo de la luz viene en kWh, los aparatos vienen en watts. Nadie piensa en megawatt-hora en su casa.
2. **Cambiar de unidad parece que se reseteó el medidor.** El usuario vería pasar `99999 kWh` a `100.0 MWh` y pensaría que perdió su historial.
3. **No hace falta.** El problema no era la unidad, era el renglón.

**El problema real y su solución.** La etiqueta y el número no caben juntos en 16 caracteres — `HISTORICO ACUMULADO` por sí sola ya son 19, así que se quedó en **`HISTORICO`**, que dice lo mismo en 9. Y aun así, con el número pegado a la etiqueta el renglón se rompe:

| Histórico | `HISTORICO 1234567 kWh` | Número en su propio renglón |
| --- | --- | --- |
| 1 247 kWh | 18 car. ❌ | ✅ |
| 99 999 kWh | 19 car. ❌ | ✅ |
| 100 000 kWh | 20 car. ❌ | ✅ |
| 1 234 567 kWh | **21 car. ❌** | ✅ |
| 9 999 999 kWh | **21 car. ❌** | ✅ |

Poniendo el número solo en el segundo renglón caben **7 dígitos** sin apretar. Y eso es suficiente para siempre:

| Meta | A 1000 W continuos (el tope del inversor) | A 300 W promedio (uso real de casa) |
| --- | --- | --- |
| 100 000 kWh | 11 años | 38 años |
| 1 000 000 kWh | 114 años | 381 años |
| **9 999 999 kWh** | **1 142 años** | **3 805 años** |

El inversor entrega 1000 W como máximo, o sea 8 760 kWh al año si trabajara a tope las 8 760 horas. **El renglón de 7 dígitos le sobrevive al equipo, al instalador y a los nietos.**

### 2.2 Las etiquetas van sin acento

El juego de caracteres estándar del HD44780 —el de la mayoría de los módulos 16×2— **no trae vocales acentuadas**. A menos que el módulo traiga la ROM europea, en pantalla va `HISTORICO`, sin acento. En esta documentación sí lleva el suyo.

### 2.3 El acumulador no se puede guardar en Wh enteros

Esta es la trampa que de verdad importa, y es silenciosa.

Con la foto llegando cada 100 ms, **a 780 W cada muestra aporta 0.0217 Wh**. Si el acumulado se guarda como entero en Wh y se le suma eso, **cada suma redondea a cero y el medidor marca cero para siempre**. No falla, no avisa: simplemente nunca sube.

Y la salida obvia tampoco sirve: acumular en **watt-segundo** con 32 bits topa en 4 294 967 W·s = **1 193 kWh**, que a 300 W promedio **se desborda en cinco meses y medio**.

**La forma correcta, y es barata:**

```c
uint32_t wh_total;     // energía en Wh enteros
uint16_t resto_ws;     // lo que falta para el siguiente Wh, en W·s

// en cada foto fresca:
resto_ws += (uint16_t)(watts * 0.1);     // W·s de esta muestra
while (resto_ws >= 3600) {               // 3600 W·s = 1 Wh
    resto_ws -= 3600;
    wh_total++;
}
```

**Exacto, sin pérdida, y tope de 4 294 967 kWh.** El resto nunca pasa de 3600 más lo que aporte una muestra (185 W·s en el peor pico), así que 16 bits le quedan grandes.

Esto **no es un detalle de implementación**: es la diferencia entre un medidor que funciona y uno que marca cero y nadie sabe por qué.

---

## 3. DESPIERTO: cuánto lleva encendido

**Para qué sirve.** Es la evidencia de que el equipo trabaja. Un número grande dice, sin palabras, que el aparato lleva meses sin tropezarse.

**Y se diagnostica solo.** Si el usuario ve `2h` cuando esperaba `45d` y él no tocó nada, **eso ya le dijo que algo se reinició**. No hace falta agregar ninguna alarma: el número ausente es la alarma.

### La trampa de `millis()`

**`millis()` se desborda a los 49.7 días** (2³² milisegundos). Calculando el tiempo directo de ahí, el contador **se regresa a cero justo cuando empieza a ser presumible**: quien llevaba 49 días sin un reinicio vería el número caerse solo y creería que su equipo se reinició cuando no pasó nada. La medalla al revés.

El arreglo cuesta 8 bytes y una resta bien hecha:

```c
static uint32_t segundos = 0;
static uint32_t ultimo = 0;
uint32_t ahora = millis();
if (ahora - ultimo >= 1000) {              // esta resta es correcta aun
    segundos += (ahora - ultimo) / 1000;   // cuando millis() se desborda
    ultimo   += ((ahora - ultimo) / 1000) * 1000;
}
```

Con `uint32_t` de segundos el tope son **136 años**.

### Mes y año, definidos

Sin reloj de calendario no hay de otra: **un mes son 30 días y un año son 365**. Queda escrito para que nadie saque cuentas contra el calendario y crea que el medidor está mal.

### Se muestran las dos unidades más grandes que apliquen

| Tiempo despierto | Se ve |
| --- | --- |
| Menos de 2 días | `7h 23m` |
| Menos de 3 meses | `45d 07h` |
| Menos de 1 año | `5m 12d` |
| Más | `1a 2m 15d` |

Mostrar `0a 00m 00d 07h` sería llenar el renglón de ceros para esconder el único número que importa.

---

## 3.5 Los dos modos: inversor y cargador

**Un solo sensor da los dos.** El INA240 mide la corriente de batería **en los dos sentidos**, así que no hacen falta dos sensores ni dos protocolos: basta el signo.

```
watts > 0  ->  sale de la batería  ->  CONSUMO (modo inversor)
watts < 0  ->  entra a la batería  ->  CARGA   (modo cargador)
```

De ahí en adelante **la aritmética es idéntica**: cada sentido lleva su integración, su ventana de 24 h, su día en curso, su historial y sus promedios.

### Dos cosas que NO se duplican, a propósito

| | Por qué |
| --- | --- |
| **La máxima sostenida** | Sólo del consumo. La máxima del cargador la fija el cargador, no la casa, así que no dice nada de cómo se usa la instalación |
| **El tiempo despierto** | Es del equipo, no de un modo |

### El historial baja de 365 a 180 días por sentido

Dos anillos de un año en Wh completo son **1460 B** y la EEPROM tiene **1024**. Hubo que elegir:

| Opción | EEPROM | Problema |
| --- | --- | --- |
| 2 × 365 días × 2 B | 1494 B | **No cabe** |
| 2 × 365 días × 1 B (pasos de 100 Wh) | 764 B | Tope de **25.5 kWh/día** y el equipo puede entregar 24: **6 % de margen** antes de que el número se achate |
| **2 × 180 días × 2 B** ✅ | **754 B** | Medio año menos de historial |

Se eligió **180 días de cada sentido con resolución completa en Wh**. Perder medio año de historial se paga solo con no tener que preocuparse por un tope que está a un 6 % de la realidad.

### Un número que sale gratis: el rendimiento

Con los dos históricos, la división dice **de la energía que entró a la batería, cuánta volvió a salir**:

```
RENDIMIENTO
      80 %
```

Es el rendimiento de ida y vuelta, y **un número que baja mes con mes es una batería envejeciendo**. No cuesta nada: una división de dos valores que ya existen.

---

## 4. Por qué se mide en DC y no en AC

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

## 5. La potencia máxima: la regla de los 2 segundos

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

## 6. La ventana de 24 horas

Hay dos maneras, y se eligió la cara porque cabe:

| | Cómo funciona | RAM | Problema |
| --- | --- | --- | --- |
| Simple | un contador que se borra cada 24 h | ~8 B | Justo después del borrado dice "máxima: 300 W" aunque ayer hayan sido 1200. Es un odómetro de viaje |
| **Ventana real** ✅ | 24 cubetas de una hora; al entrar una sale la más vieja | 96 B | Ninguno |

Con la ventana real **los dos números dicen la verdad siempre**, sin el hueco de "se me acaba de borrar".

**La base de tiempo son los ciclos de línea que cuenta el CRÍTICO**, no `millis()`. Vienen del cristal y ya viajan en la foto: 216 000 ciclos son una hora a 60 Hz. No hay reloj de tiempo real en el proyecto, así que son **24 horas de operación**: si se apaga el equipo, el reloj se detiene con él. Eso se dice tal cual, no se disimula.

---

## 7. La EEPROM: 365 escrituras al año

**El acumulado se guarda una vez cada 24 horas.** Son 365 escrituras al año, y con los 100 000 ciclos que aguanta la celda eso da **274 años**. El desgaste deja de ser un tema, resuelto por escribir menos.

**Más un guardado extra que sale gratis.** Escribir una vez al día significa que un corte puede borrar hasta 24 horas del acumulado. No hace falta aguantárselo: la foto ya trae `estadoBateria`, y cuando pasa a **aviso** o **corte bajo**, el auxiliar sabe que esto se va a apagar. Entonces guarda también ahí. Esos eventos son raros —unos pocos al mes—, así que el conteo apenas se mueve, y el único escenario de pérdida grande queda tapado.

Lo que no cubre: un corte brusco sin aviso (alguien desconecta la batería). Ahí se pierde hasta un día. **Se acepta y se documenta**; taparlo pediría un capacitor y un detector de caída, y no vale la pena para un medidor de trinchera.

---

## 8. Lo que hay que construir antes

En orden. Los dos primeros son el trabajo de verdad; los dos últimos son lo que se ve.

| # | Qué | Por qué |
| --- | --- | --- |
| **1** | **`corriente.cpp` en el CRÍTICO** | Leer `I_OUT` del INA240. **Sin esto no hay nada**: el hardware existe pero el firmware nunca lo ha leído |
| **2** | **Protocolo v3** | La foto está llena: los 16 bytes del v2 están todos ocupados. Hay que crecerla y revalidar el enlace en los dos micros |
| **3** | `energia.cpp` en el auxiliar | Integrar, la ventana de 24 h, la EEPROM |
| **4** | Las pantallas del LCD | Las dos de la sección 1 |

---

## 9. Presupuesto de recursos, medido

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
| Acumulado total en Wh (32 bits) + resto en W·s (16 bits) | 6 |
| Acumulado del día en Wh (32 bits) | 4 |
| Potencia actual y auxiliares | 20 |
| DESPIERTO: segundos + marca de `millis()` | 8 |
| **Total calculado** | **138** |
| **Medido al compilar `energia.cpp` para AVR** | **136** |

**Medido, no estimado.** Compilando `energia.cpp` para el ATmega328P:

```
codigo (flash)  2449 B
RAM estatica     136 B
```

Con el módulo adentro, el auxiliar queda en **17 009 B de flash (55 %)** y **1205 B de RAM (59 %)**. Descontando la pila quedan **393 B utilizables de margen**. Cabe con holgura.

---

## 10. Lo que este módulo NO hace, y por qué

| No hace | Por qué |
| --- | --- |
| **Watts verdaderos en AC** | Haría falta un sensor de corriente en AC que la tarjeta no tiene. Ver sección 4 |
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

## 11. Pendientes

- [ ] Confirmar en qué pin del CRÍTICO entra `I_OUT` y si queda libre
- [ ] Definir el formato de la foto del protocolo v3
- [ ] Calibrar la corriente contra un patrón (pinza o fuente conocida)
- [ ] Decidir si se muestran watts de AC estimados o todo se habla en términos de batería
- [ ] Validación en banco: el acumulado contra un medidor de referencia durante 24 h
- [ ] Prueba del DESPIERTO al pasar los 49.7 días: comprobar que no se regresa a cero cuando `millis()` se desborda
- [ ] Prueba del acumulador: correrlo con potencia constante conocida y comprobar que los Wh suben al ritmo esperado. Es la prueba que cacha el error de redondeo de la sección 2.3, que de otro modo no avisa

---

## 12. Referencias

- [`decisiones_criticas.md`](decisiones_criticas.md): DC-007 (divisor de batería), DC-014 (corte por hardware fail-safe), DC-016 (niveles de protección), DC-020 (reloj del ADC)
- [README del sensor DC](../hardware/sensores/sensor-dc-ina-comparador/README.md): qué entrega `I_OUT` y con qué escala
- `firmware/critico/ISLA_CRITICO/enlace_protocolo.h`: el protocolo v2 que hay que crecer
- `firmware/auxiliar/ISLA_AUX_PANEL/enlace_aux.h`: la foto como la recibe el auxiliar

---

*Documentación bajo licencia CC BY-SA 4.0 (ver `LICENSE.md` en la raíz).*
