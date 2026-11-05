# Identificación del tiempo muerto por el pin 9 (P9) del módulo gate driver

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Tema:** selección automática del tiempo muerto (DT) según la tecnología del módulo gate driver instalado
**Estado:** implementado en el módulo TLP250 V.0.1.NOV.26.J.G (P9 en J1); pendiente unificar módulos IRS2186 y UCC21520 y firmware
**Fecha:** 2026-11-02 · **Revisión:** 2026-11-03 (P9 se ubica en J1, conector hacia la tarjeta de control)

---

## 1. Resumen

Cada módulo gate driver indica por **la terminal P9 de su conector de entrada J1** (el que va a la tarjeta de control) qué tiempo muerto necesita. El microcontrolador lo lee **al encender, antes de habilitar el PWM**, y configura el tiempo muerto y la compensación con ese dato.

| P9 del módulo (J1) | Lectura del MCU | Tiempo muerto | Módulos |
| --- | --- | --- | --- |
| Conectado a GND | 0 | **500 ns** | IRS2186, UCC21520ADWR |
| Abierto (sin conectar) | 1 | **1 µs** | TLP250H |
| Falla (sin módulo, cable suelto, pista rota, soldadura mala) | 1 | **1 µs** | — (valor seguro) |

Con esto hay **un solo firmware** para los tres módulos, y el técnico de campo **solo cambia el módulo**, sin reprogramar ni mover jumpers.

---

## 2. Por qué existe esta modalidad

### 2.1 Propósito: soporte legacy para trincheras

**El pin 9 se creó para dar soporte a gate drivers antiguos (legacy) en modo trinchera.** Un solo pin permite que el mismo equipo, con el mismo firmware y la misma tarjeta de potencia, funcione con drivers modernos o con tecnología vieja y fácil de conseguir, sin rediseñar nada.

- **Qué es "trinchera":** zonas o situaciones donde no llegan componentes modernos (África, Medio Oriente, partes de Sudamérica, comunidades aisladas, reparación de emergencia). Ahí lo que hay en el cajón del técnico son optoacopladores DIP como el TLP250, soldables a mano y rescatables de equipos viejos (variadores, UPS, soldadoras inverter).
- **El problema:** los drivers legacy son más lentos y menos precisos (retardo de 100–500 ns, ±150 ns entre piezas). Con el tiempo muerto de los drivers modernos (500 ns) causarían conducción cruzada.
- **La solución:** el módulo legacy deja el pin 9 abierto y el firmware sube el tiempo muerto a 1 µs por sí solo. El equipo sigue entregando la potencia completa con un poco más de armónicos (THD ~1.6 % en vez de ~0.8 %, aún por debajo del 5 % de IEEE 519 / IEC 61000-3-2 como referencia).
- **Principio de diseño:** **el modo moderno es la excepción declarada; el modo legacy es el valor por defecto.** Un módulo debe conectar el pin 9 a GND para pedir 500 ns. Cualquier módulo que no lo haga —un TLP250, un driver antiguo futuro, un módulo improvisado o uno mal soldado— cae automáticamente en 1 µs, el modo más tolerante.

| Generación | Ejemplos | Pin 9 | DT | Uso |
| --- | --- | --- | --- | --- |
| Moderna | IRS2186, UCC21520ADWR | GND | 500 ns | Producción, mejor calidad de onda |
| Legacy / trinchera | TLP250H; a futuro otros optoacopladores DIP (TLP350, HCPL-3120) | Abierto | 1 µs | Servicio y reparación en campo con lo disponible |

> **Regla para quien modifique el código fuente:** nunca invertir la lógica (pin abierto = 500 ns). Se rompería el soporte legacy: cualquier módulo de trinchera, o cualquier falla en el pin, llevaría a conducción cruzada. Si en el futuro se agrega otro driver legacy con retardos peores que el TLP250 (> 790 ns en el peor caso), se debe ampliar la identificación (ver sección 7) en lugar de reducir el margen de 1 µs.

### 2.2 Tres módulos con la misma interfaz

El proyecto tiene tres módulos de gate driver intercambiables (mismo conector de entrada J1 y de salida J2/J5):

| Módulo | Tecnología | Por qué existe | DT mínimo seguro |
| --- | --- | --- | --- |
| IRS2186 | Driver de bootstrap, SOIC-8 | Opción principal, bajo costo | ~390 ns → se usa **500 ns** |
| UCC21520ADWR | Driver aislado, SOIC-16 ancho | Mejor desempeño; interbloqueo y DT de respaldo en hardware | ~390 ns → se usa **500 ns** |
| TLP250H | Optoacoplador DIP-8, THT en zócalo | **Módulo de servicio** para zonas donde no se consiguen componentes SMD; reparable a mano | ~790 ns → se usa **1 µs** |

### 2.3 Por qué el TLP250 necesita 1 µs

El retardo de propagación del TLP250H va de **100 ns a 500 ns** y la diferencia entre piezas es de hasta **±150 ns** (hoja de datos de Toshiba). En el peor caso:

- El canal que se apaga es lento (500 ns) y el que enciende es rápido (100 ns): **~400 ns** de diferencia.
- Más el apagado del MOSFET IRF100P219 en el peor caso: **~390 ns**.
- Total: **~790 ns** → con margen, **1 µs**.

Con 500 ns, un TLP250 puede provocar **conducción cruzada** (los dos MOSFET de una rama encendidos a la vez) y destruir los MOSFET.

### 2.4 Por qué no usar 1 µs siempre

Se podría, pero empeora la salida en los módulos que no lo necesitan:

| Tiempo muerto (híbrida, 24 kHz, 1000 W, sin compensación) | Error del puente | 3er armónico | THD salida | Pérdida extra por MOSFET de 24 kHz |
| --- | --- | --- | --- | --- |
| 500 ns | 0.58 V | 0.58 % | 0.81 % | — |
| 1 µs | 1.15 V | 1.17 % | 1.62 % | ~+0.3 W |

Fórmula: **ΔV = k · VBUS · DT · f_sw** (k = 1 en híbrida, k = 2 en unipolar). Duplicar el tiempo muerto duplica todos los armónicos.

### 2.5 Por qué identificar en el módulo y no con un jumper en la tarjeta principal

| Opción | Riesgo |
| --- | --- |
| Jumper o selector en la tarjeta de potencia | Un técnico instala un TLP250 y olvida mover el jumper → 500 ns con TLP250 → **conducción cruzada** |
| **El módulo fija su propio pin 9** | El módulo "declara" lo que necesita; no hay combinación incorrecta posible ✓ |

### 2.6 Costo de la modalidad y argumento económico

Para llevar la señal P9 hasta el microcontrolador, **J1** (conector del módulo hacia la tarjeta de control) pasa de **4 a 5 terminales**: H, GND, L, GND, **P9**. El costo extra en componentes es de **~1 USD por equipo** (terminal adicional, pull-up de 10 kΩ y resistencia en serie en la tarjeta de control).

| Concepto | Costo aproximado | Comentario |
| --- | --- | --- |
| **Pin 9 (5ª terminal + resistencias)** | **~1 USD por equipo** | Se paga una sola vez, en fabricación |
| Falla por conducción cruzada (TLP250 con 500 ns) | Dos MOSFET IRF100P219 + driver u optoacoplador + fusible, posibles pistas dañadas | Varias veces el costo del pin 9 solo en partes |
| **Ida de reparación en campo (trinchera)** | **Cientos de USD** | Traslado (gasolina, lancha, terreno difícil), días de trabajo del técnico, viáticos |
| Tiempo sin energía | No se mide en dinero | Familia, clínica o escuela sin servicio mientras llega la reparación |

**Conclusión:** si el pin 9 evita **una sola** ida de reparación en toda la vida del equipo, ya se pagó cientos de veces. En equipos para zonas aisladas, el costo dominante no es el componente sino **el viaje**; por eso se diseña para que el técnico no pueda equivocarse.

> **Regla para revisiones de costo futuras:** no eliminar la 5ª terminal ni las resistencias del pin 9 para ahorrar. Si se busca bajar costo, cambiar el **tipo** de terminal (por ejemplo, pin de header de 2.54 mm en lugar de bornera de tornillo para esta señal lógica), no quitar la función.

### 2.7 Filosofía de diseño: poka-yoke

El pin 9 y la llave de la posición 8 son **poka-yoke** (a prueba de errores, concepto de Shigeo Shingo en el sistema de producción Toyota): el diseño hace que el error humano **no pueda causar el daño**, en lugar de depender de que el técnico recuerde algo.

- **Método de contacto:** el módulo declara su tipo por una característica física (pin 9 a GND o abierto; falta del pin 8).
- **Función de control, no de advertencia:** no se avisa con un LED que algo está mal; directamente se impide operar en el modo peligroso. Es la categoría más fuerte de poka-yoke.

| Poka-yoke del sistema | Tipo | Error que evita |
| --- | --- | --- |
| Llave mecánica en la posición 8 (sección 3.5) | Forma | Módulo insertado al revés o corrido → 15 V en señales de compuerta o MCU |
| Pin 9 de identificación | Contacto / control | TLP250 trabajando con 500 ns → conducción cruzada |
| Pin abierto = 1 µs (falla segura) | Valor por defecto seguro | Soldadura fría, pista rota o módulo ausente causando daño |
| Vigilancia del pin 9 en operación | Control | Módulo que se afloja con el equipo encendido |
| Posición 8 sin pad entre 15 V y J5-9 | Forma | Puente de soldadura que pone en corto el 15 V |
| Serigrafía "OPERACION DT …" en cada módulo | Información visible | Técnico que no sabe qué modo debe reportar el equipo |
| Par trenzado H/GND y L/GND indicado en la placa | Información visible | Ruido en las entradas por cableado suelto |
| Misma interfaz J1/J2 y mismo firmware | Estandarización | Versión de firmware equivocada para el módulo |
| Ciclo de trabajo máximo 95 % en firmware | Límite | Bootstrap descargado por ciclo de trabajo demasiado largo |

> **Regla para cambios futuros:** cualquier modificación de hardware o firmware debe conservar estos poka-yoke. Si un cambio obliga al técnico a "acordarse" de algo (un jumper, una configuración, una versión de firmware), está retrocediendo.

---

## 3. Hardware

### 3.1 Conectores del módulo

**Dónde va P9 y por qué (decisión del 2026-11-03):** la identificación va en **J1**, el conector de entrada que llega directo a la tarjeta de control.

- El MCU y su pull-up están en la tarjeta de control: P9 queda referido **al mismo GND que lee el MCU**, sin pasar por la tarjeta de potencia ni por el plano de GND donde circula la corriente de conmutación.
- En el módulo TLP250 el lado de entrada está **aislado** de la potencia (J1 GND ≠ GND_K). Si la identificación fuera por J5, quedaría referida a la tierra de potencia y no a la del MCU.
- J5 (salida al puente H) queda solo con señales de compuerta y alimentación, más la llave mecánica.

**J1 — entrada, hacia la tarjeta de control** (bornera de 5 terminales; serigrafía "TRENZAR H Y L CON GND"):

| Terminal | Señal | Nota |
| --- | --- | --- |
| 1 | H | Entrada lado alto (al LED del TLP / HIN / INA) |
| 2 | GND | Retorno de H (trenzar con H) |
| 3 | L | Entrada lado bajo |
| 4 | GND | Retorno de L (trenzar con L) |
| **5** | **P9 — ID DT** | **GND = 500 ns · abierto = 1 µs** |

- H y L viajan en **par trenzado con su GND** para reducir el ruido acoplado de la conmutación.
- **P9 tiene como vecino un GND (terminal 4).** En los módulos de 500 ns no importa (P9 va a GND). En el módulo TLP250 (P9 abierto) un hilo suelto o un puente de soldadura entre 4 y 5 lo pasaría a 500 ns: ver la falla correspondiente en la sección 4.

**J2 / J5 — salida, hacia la tarjeta del puente H** (header de 9 posiciones, 2.54 mm):

| Pin | Señal | Nota |
| --- | --- | --- |
| 1 | HO | Compuerta lado alto |
| 2 | VS1 | Fuente lado alto (nodo de conmutación) |
| 3 | — | Libre: separación del nodo conmutado |
| 4 | GND_K | Retorno Kelvin del lado bajo |
| 5 | LO | Compuerta lado bajo |
| 6 | GND / GND_PUENTE_H | Tierra de potencia. En el módulo TLP250 queda **sin uso** (la entrada está aislada) |
| 7 | 15 V | Alimentación del driver |
| 8 | — | **Llave mecánica:** sin pin ni pad en el módulo; posición tapada en la tarjeta del puente H (ver 3.5) |
| 9 | — | **Reservado, sin función.** En el módulo UCC21520 quedó conectado a GND (inofensivo); en el TLP250, abierto. La tarjeta del puente H no lo lee |

> **Historial:** la primera versión de este documento ubicaba la identificación en J2/J5-9, con pull-up en la tarjeta de potencia. Se movió a J1-P9 al diseñar el módulo TLP250 V.0.1. **Pendiente:** llevar P9 a J1 también en los módulos IRS2186 y UCC21520 (P9 a GND de entrada) para que los tres usen la misma asignación. La duda sobre INA_OUT en J5-9 del IRS2186 deja de afectar a la identificación.

#### Nivel lógico de la interfaz: 5 V

**Decisión (2026-11-05):** las señales H y L de J1 son **lógica de 5 V**, generadas por un **ATmega328P** (ecosistema Arduino) a través de la compuerta **74HC08** de corte por FAULT en la tarjeta de control.

| Por qué 5 V | Detalle |
| --- | --- |
| Soporte a largo plazo | El ATmega328P y las placas Arduino de 5 V se consiguen en todo el mundo y tienen soporte asegurado por muchos años |
| Trinchera | El 74HC08 es la variante más común de la familia; con entradas de 5 V no hace falta el 74HCT08 |
| Un solo valor por módulo | TLP250 con **330 Ω fijos** (~10 mA en el LED); IR2110 con margen completo en sus entradas a VDD = 5 V; IRS2186 con margen amplio sobre su V_IH de 2.5 V |
| Poka-yoke | No existen versiones del módulo "para 3.3 V" que se puedan confundir |

**Fuera del alcance actual:** MCU de 3.3 V (STM32, ESP32). Si en una etapa posterior se usan, la compuerta de corte se cambia por **74HCT08** (mismo pinout DIP-14, V_IH = 2.0 V, salida de 5 V) y los módulos no se tocan. **No usar 74LS08 ni CD4081:** su salida en alto es débil y no enciende bien el LED del TLP250 ni las entradas del IR2110.

### 3.2 Tarjeta de control (lado del MCU)

```
5 V (VCC del ATmega328P)
  │
 [10 kΩ]        ← pull-up externa (además de la interna del MCU)
  │
  ├──[1–4.7 kΩ]──► entrada digital del MCU   (resistencia serie de protección)
  │
 J1-P9 del módulo (cable desde la bornera)
  │
  ├─ IRS2186 / UCC21520: P9 unido a GND de J1 en el módulo
  └─ TLP250: P9 sin conectar
```

| Componente | Valor | Función |
| --- | --- | --- |
| Pull-up | 10 kΩ a 5 V (VCC del MCU) | Convierte "abierto" en un 1 definido; más inmune al ruido de 24 kHz que la pull-up interna |
| Resistencia serie | 1–4.7 kΩ | Limita corriente hacia el pin del MCU si llega un voltaje indebido |
| Capacitor (opcional) | 10–100 nF a GND junto al MCU | Filtra ruido acoplado de la conmutación |

**Cómo "sabe" el MCU que el pin está abierto:** no lo detecta directamente. La pull-up sube la línea a 5 V cuando nada la jala a tierra. Si el módulo la conecta a GND, circulan 5 V / 10 kΩ = **0.5 mA** y la línea queda en 0 V. Sin pull-up, una entrada flotante leería valores al azar.

### 3.3 Módulos

| Módulo | Cambio en layout | Serigrafía sugerida |
| --- | --- | --- |
| IRS2186 | J1-P9 → GND ✓ (2026-11-03). En este módulo GND de J1 = COM = tierra de potencia (no aislado) | "OPERACION DT IRS2186=500NS" (pendiente) |
| UCC21520ADWR | J1-P9 → GND de entrada (pendiente; hoy el ID está en J2-9) | "OPERACION DT UCC21520=500NS" |
| TLP250H | **J1-P9 sin pista** y sin relleno de GND alrededor ✓ (V.0.1) | **"OPERACION DT TLP250=1US"** ✓ |

La serigrafía dice **qué hace** el módulo, no solo cómo se llama: si en banco el equipo reporta un modo distinto al impreso, el técnico sabe que hay un problema en P9.

### 3.4 Nota para ESP32 (etapa posterior)

Los GPIO 34 a 39 del ESP32 son solo de entrada y **no tienen pull-up interna**. Si se usa uno de ellos, la pull-up externa es obligatoria.

### 3.5 Llave mecánica en la posición 8 (poka-yoke de forma)

El conector es un header de 9 posiciones a 2.54 mm. La posición 8 no lleva señal, así que se usa como **llave de polarización**: el módulo solo entra en una posición.

| Lado | Qué se hace |
| --- | --- |
| **Módulo gate driver (macho)** | Se quita el pin 8 del header (pinzas o corte al ras). En el footprint del PCB **se elimina el pad 8** (sin cobre y sin agujero) |
| **Tarjeta del puente H (hembra)** | Se **tapa la posición 8**: tapón de polarización, pin cortado metido en el hueco, gota de epóxico, o header hembra con la posición cegada de fábrica |

**Por qué funciona:** la posición 8 no está al centro del conector (el centro es la 5), así que la llave es asimétrica.

| Intento de inserción | Resultado |
| --- | --- |
| Correcta | Entra ✓ |
| Al revés (girado 180°) | El pin que cae en la posición 8 choca con el tapón ✗ |
| Corrida una posición | Un pin choca con el tapón ✗ |

**Por qué eliminar también el pad 8 del footprint del módulo:**
- Un header de 9 pines estándar no entra en el PCB del módulo si no se le quitó el pin 8: **el propio PCB obliga a quien ensambla a hacer la llave**.
- Entre J2-7 (15 V) y J2-9 queda un hueco de 5.08 mm entre centros **sin cobre**: no hay pad donde se forme un puente de soldadura y mejora el aislamiento.

**Implementación en KiCad:**

1. **Footprint:** copiar `PinHeader_1x09_P2.54mm_Vertical` a la librería del proyecto y borrar el pad 8.
2. **Símbolo:** conector propio con pines 1–7 y 9 (sin pin 8). Si el símbolo conserva el pin 8 y el footprint no tiene pad, *Update PCB* avisa de un pad faltante.
3. **Área sin cobre:** *rule area / keepout* de ~2 × 2.5 mm centrada en la posición 8, en **F.Cu y B.Cu**, con *No copper fill* y *No tracks*. Sin ella, el relleno de GND entra al hueco y se acerca a J2-7 (15 V) con el clearance normal, y se pierde la separación.
4. **Serigrafía:** una "X" o un triángulo en la posición 8, en el módulo y en la tarjeta del puente H junto al hueco tapado, para que el técnico vea la llave y no fuerce el conector.
5. **Rellenar zonas (B) y correr el DRC:** confirmar que J2-6 sigue unido a su plano (excepto en el TLP250, donde queda sin uso) y que J2-7 conserva su clearance.
6. **Recomendado para soldadura a mano:** *Pad connections → Thermal reliefs* en los pads unidos a planos, para evitar soldaduras frías. En los módulos de 500 ns, una soldadura fría en J1-P9 manda el módulo a 1 µs (falla segura).

> **Obligatorio:** aplicar la misma llave en **los tres módulos** (IRS2186, UCC21520ADWR, TLP250H) y en **los dos conectores** de la tarjeta del puente H (rama A y rama B), para que sigan siendo intercambiables.

**Estado:** UCC21520ADWR con pad 8 eliminado (2026-11-02). **TLP250 V.0.1: sin pad 8 y con "X" en la serigrafía ✓ (2026-11-03).** Pendientes: módulo IRS2186 y tapón en la tarjeta del puente H.

---

## 4. Análisis de fallas

El diseño está hecho para que la falla posible sea la inofensiva.

| Falla | Lo que lee el MCU | Resultado | Gravedad |
| --- | --- | --- | --- |
| UCC o IRS con P9 mal soldado o cable P9 suelto | 1 → 1 µs | Funciona con más tiempo muerto: THD ~1.6 % en lugar de ~0.8 %, ~0.3 W más por MOSFET | **Leve**: seguro |
| Conector flojo, pista rota o módulo ausente | 1 → 1 µs | Igual que arriba | **Leve**: seguro |
| Puente de soldadura en la posición 8 | Imposible: no hay pad 8 | Ninguno | Ninguna |
| Módulo insertado al revés o corrido una posición | — | No entra: la llave choca con el tapón de la posición 8 | Ninguna (evitada) |
| **TLP250 con P9 tocando GND** (defecto de fabricación) | 0 → 500 ns | Tiempo muerto insuficiente → posible conducción cruzada | **Grave**, muy poco probable |
| **TLP250: hilo suelto o puente entre J1-4 (GND) y J1-5 (P9)** | 0 → 500 ns | Igual que arriba | **Grave**, poco probable (bornera de 5 mm) |
| Pin 9 cambia con el equipo en operación (módulo flojo) | Cambio detectado | El firmware apaga el PWM y marca falla | Controlada |

**Mitigación de la falla grave:**
1. P9 sin pista y sin relleno de GND alrededor en el módulo TLP250.
2. Cable a la bornera con **puntas estañadas o con terminal**, sin hilos sueltos. Opcional en el TLP250: no instalar el cable P9 (la pull-up lee 1 → 1 µs).
3. Serigrafía explícita en el módulo: "OPERACION DT TLP250=1US".
4. El firmware muestra el modo detectado: si el módulo dice "1US" y el equipo indica 500 ns, el técnico detiene la prueba.
5. Respaldo: el sensado de corriente del bus (shunt + INA) puede detectar los picos de conducción cruzada y apagar el PWM.

---

## 5. Firmware

### 5.1 Secuencia de arranque

```
1. Se energiza el equipo
2. El MCU arranca con el PWM DESHABILITADO (compuertas en bajo)
3. Lee P9 (J1) de cada módulo (rama A y rama B), varias veces
4. Elige el tiempo muerto: 500 ns solo si TODAS las lecturas de AMBOS módulos son 0; si no, 1 µs
5. Configura el tiempo muerto del timer y el D_COMP de la compensación con el mismo dato
6. Indica el modo detectado (LED o puerto serie)
7. Precarga los bootstrap (lados bajos encendidos 0.5–2 ms)
8. Arranca la PWM
9. En operación: vigila P9; si cambia, apaga el PWM y marca falla
```

### 5.2 Código de referencia (estilo Arduino; válido para ATmega328 y ESP32)

```c
#define PIN_DT_ID_A   7      // J1-P9 del módulo de la rama A
#define PIN_DT_ID_B   8      // J1-P9 del módulo de la rama B
#define DT_NS_RAPIDO  500    // IRS2186 / UCC21520
#define DT_NS_SEGURO  1000   // TLP250 o duda
#define FSW_HZ        24000UL

static uint16_t dt_ns;       // tiempo muerto elegido
static uint8_t  id_A, id_B;  // lecturas guardadas para vigilar cambios

// Devuelve 0 solo si las 16 lecturas fueron bajas; cualquier lectura alta devuelve 1
static uint8_t leer_id_estable(uint8_t pin){
    for (uint8_t i = 0; i < 16; i++){
        if (digitalRead(pin) == HIGH) return 1;
        delayMicroseconds(100);
    }
    return 0;
}

void detectar_modo_dt(void){
    pinMode(PIN_DT_ID_A, INPUT_PULLUP);
    pinMode(PIN_DT_ID_B, INPUT_PULLUP);
    delay(5);                                   // estabilizar la línea
    id_A = leer_id_estable(PIN_DT_ID_A);
    id_B = leer_id_estable(PIN_DT_ID_B);
    dt_ns = (id_A == 0 && id_B == 0) ? DT_NS_RAPIDO : DT_NS_SEGURO;
}

// En el lazo principal o en una tarea periódica (no en la ISR de 24 kHz)
void vigilar_modo_dt(void){
    if (digitalRead(PIN_DT_ID_A) != id_A || digitalRead(PIN_DT_ID_B) != id_B){
        pwm_deshabilitar();
        marcar_falla(FALLA_MODULO_GD);
    }
}
```

> Nota: `vigilar_modo_dt` compara una sola lectura; si en banco aparecen disparos falsos por ruido, exigir varias lecturas consecutivas diferentes antes de marcar falla.

### 5.3 Aplicar el tiempo muerto

**ATmega328 (Timer1, phase-correct, 16 MHz):**
- TOP = 16 MHz / (2 · 24 kHz) ≈ **333 cuentas**; cada cuenta desplaza cada flanco **62.5 ns**.
- 500 ns = **8 cuentas**; 1 µs = **16 cuentas**.
- OC1A normal (lado alto) y OC1B invertido (lado bajo), con **OCR1B = OCR1A + DT_cuentas**.
- **Validar en cada actualización que OCR1B > OCR1A**; si no, la rama queda en conducción cruzada.

```c
#define CUENTAS_POR_NS_X1000  16      // 16 MHz → 0.016 cuentas por ns
static uint8_t dt_cuentas;

void configurar_dt_atmega(void){
    dt_cuentas = (uint8_t)((dt_ns * CUENTAS_POR_NS_X1000 + 999) / 1000);   // 500→8, 1000→16
}

static inline void cargar_duty_rama_A(uint16_t ocr_alto){
    uint16_t ocr_bajo = ocr_alto + dt_cuentas;
    if (ocr_bajo > 333) ocr_bajo = 333;      // límite del TOP
    if (ocr_bajo <= ocr_alto) return;        // nunca cargar valores que eliminen el DT
    OCR1A = ocr_alto;
    OCR1B = ocr_bajo;
}
```

> Verificar con el osciloscopio la relación exacta entre cuentas y tiempo muerto en el modo de timer usado, y ajustar `dt_cuentas` si hace falta.

**ESP32 (MCPWM):** configurar el generador de tiempo muerto del periférico MCPWM con flanco de subida y de bajada iguales a `dt_ns`, y habilitar su entrada de falla para apagado por hardware.

### 5.4 Coherencia con la compensación de tiempo muerto

El mismo dato alimenta la compensación (ver el documento "Compensación adaptativa de tiempo muerto"):

```c
float d_comp = (float)dt_ns * 1e-9f * (float)FSW_HZ;   // 0.012 con 500 ns, 0.024 con 1 µs
```

En ATmega328, convertir a cuentas enteras: `d_comp_cuentas = dt_cuentas` (aplicado como ±DT/2 en cada flanco según el signo de la corriente).

---

## 6. Procedimiento de validación en banco

1. **Sin potencia:** instalar cada módulo y verificar con el puerto serie o el LED que el firmware detecta el modo correcto.
2. **Falla simulada:** con un módulo UCC o IRS, desconectar el cable P9 de J1 → el equipo debe indicar **1 µs**.
2b. **Módulo TLP250:** antes de habilitar el PWM, confirmar que el equipo reporta **1 µs** (coincide con la serigrafía "OPERACION DT TLP250=1US").
3. **Tiempo muerto real:** con el osciloscopio, medir entre el flanco de bajada de LO y el de subida de HO (y viceversa), en el nodo de compuerta de cada MOSFET. Debe ser ≥ 500 ns o ≥ 1 µs según el modo.
4. **Con TLP250:** medir el retardo real de los dos optoacopladores de cada rama. Si la diferencia real es baja, documentar el dato: podría justificar en el futuro un modo intermedio (por ejemplo 750 ns).
5. **Cambio en operación:** con el bus en bajo voltaje (12–24 V) y limitación de corriente, aflojar un módulo → el PWM debe apagarse y marcar falla.
6. **Armónicos:** medir el 3er armónico en cada modo y compararlo contra la tabla de la sección 2.4.

---

## 7. Extensiones futuras

| Idea | Cómo | Para qué |
| --- | --- | --- |
| Más de dos modos | Pin 9 con divisor resistivo y lectura por ADC (por ejemplo GND = 500 ns, mitad de VCC = 750 ns, abierto = 1 µs) | Módulos nuevos o TLP250 seleccionados por lote |
| ID por rama independiente | Usar el DT de cada módulo en su propia rama, en lugar del mayor de los dos | Mejor calidad cuando solo una rama lleva TLP250 |
| Registro de eventos | Guardar en memoria no volátil el modo detectado y las fallas de módulo | Diagnóstico en campo |
| Compensación adaptativa | Ver documento "Compensación adaptativa de tiempo muerto" | Reducir los armónicos de 1 µs hasta casi cero |

---

## 8. Referencias

- Toshiba, hoja de datos TLP250H: retardo 100–500 ns, diferencia entre piezas ±150 ns, ±2 A recomendados.
- Infineon, hoja de datos IRS2186: retardo 250 ns máx., diferencia entre canales 35 ns máx.
- Texas Instruments, hoja de datos UCC21520 (SLUSCJ9): DT (ns) = 10 × R_DT (kΩ), interbloqueo con R_DT.
- Simulador de armónicos por tiempo muerto (artefacto del proyecto).
- Documento "Compensación adaptativa de tiempo muerto — etapa posterior" (`compensacion_adaptativa_tiempo_muerto.md`).

## 9. Bitácora de seguimiento

| Fecha | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- |
| 2026-11-02 | Definición del pin 9 como identificación del tiempo muerto | Documento creado | Confirmar INA_OUT en el módulo IRS2186; agregar pull-up en tarjeta de potencia; pista a GND en módulos IRS y UCC |
| 2026-11-02 | Se documenta el propósito del pin 9: soporte legacy para trincheras (sección 2.1) | Regla de diseño fijada: pin abierto = modo legacy 1 µs | — |
| 2026-11-02 | Módulo UCC21520ADWR: J2-9 conectado a la zona GND (mismo net que J2-6); J2-8 aislado con clearance hacia J2-7 (15V) | Módulo UCC declara 500 ns ✓ | Mismo cambio en módulo IRS2186 (confirmar INA_OUT); pull-up 10 k + serie en tarjeta de potencia |
| 2026-11-02 | Se agrega la sección 2.6: costo de la modalidad (de 4 a 5 terminales, ~1 USD) contra el costo de una ida de reparación | Argumento económico documentado | — |
| 2026-11-02 | Llave mecánica en posición 8 (sin pin/pad en módulo, tapón en tarjeta) y sección 2.7 poka-yoke | Pad 8 eliminado en módulo UCC21520ADWR | Keepout en posición 8, serigrafía, aplicar en IRS2186 y TLP250H, tapón en tarjeta del puente H |
| 2026-11-03 | P9 se ubica en **J1-5** (conector hacia la tarjeta de control); pull-up en la tarjeta de control; J5-9 queda reservado | Implementado en módulo TLP250 V.0.1.NOV.26.J.G: P9 abierto, "X" en J5-8, serigrafía "OPERACION DT TLP250=1US" y "TRENZAR H Y L CON GND" | Llevar P9 a J1 en módulos IRS2186 y UCC21520; pull-up 10 k + serie en tarjeta de control |
| 2026-11-03 | Módulo IRS2186: P9 en J1-5 unido a GND; J5 sin pad 8 con "X"; J5-9 a GND (reservado) | Módulo IRS declara 500 ns ✓; duda de INA_OUT resuelta | Falta llevar P9 a J1 en el módulo UCC21520 |
| 2026-11-05 | Nivel lógico de J1 fijado en **5 V** (ATmega328P + 74HC08); pull-up de P9 a 5 V | Un solo valor por módulo (TLP250 con 330 Ω); 3.3 V queda para una etapa posterior con 74HCT08 | — |
| | | | |
