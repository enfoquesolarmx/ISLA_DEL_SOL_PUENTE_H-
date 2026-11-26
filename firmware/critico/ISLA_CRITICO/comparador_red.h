/*
 * =============================================================================
 *  comparador_red.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  El cruce por cero de la red, con el comparador analógico interno
 * -----------------------------------------------------------------------------
 *  POR QUÉ EL COMPARADOR Y NO EL ADC
 *
 *    **El cruce por cero es una medición de tiempo, y la estábamos sacando de
 *    un canal que mide amplitud.** El ADC no tiene problema de resolución:
 *    tiene problema de saber *cuándo* tomó la muestra. Con 3 003 muestras por
 *    segundo en el canal de la red, entre dos muestras pasan 333 µs = 7.2° a
 *    60 Hz, y el cruce queda en algún lugar de ese hueco. El comparador
 *    dispara **en** el cruce.
 *
 *    Esto es DC-023, y no cuesta un solo componente: las dos entradas son
 *    nodos que ya existen.
 *
 *        AIN0 = PD6 (D6)  ← el nodo del divisor AC de red, tomado en C7
 *        AIN1 = PD7 (D7)  ← NEUTRO-GRID, la referencia de bias de ese canal
 *
 *    AIN0 es la entrada POSITIVA: ACO = 1 cuando la red está arriba del bias,
 *    o sea en el semiciclo positivo.
 *
 *  ⚠️ LO QUE NO SE PUEDE HACER, Y PARECE QUE SÍ
 *
 *    El 328P puede amarrar el comparador a la captura de entrada del Timer1
 *    con el bit `ACIC`: el hardware guardaría `TCNT1` en `ICR1` en el instante
 *    exacto del flanco — 62.5 ns de resolución, latencia cero, sin software.
 *
 *    **No se puede usar: `ICR1` ES EL TOP DE LA PORTADORA.** El SPWM corre en
 *    modo 8 (fase y frecuencia correctas) con TOP = ICR1 = 333. Si el
 *    comparador escribiera ahí, cada cruce de la red cambiaría la frecuencia
 *    de conmutación. `ACIC` se queda en 0 a propósito.
 *
 *    Por eso el fechado es por software, con `micros()` dentro de la ISR: 4 µs
 *    de resolución = 0.086° a 60 Hz. Suficiente, porque la frecuencia no se
 *    saca de un semiperiodo sino del promedio de un segundo completo.
 *
 *  EL BLANQUEO, Y POR QUÉ HACE FALTA
 *
 *    El comparador del AVR **no tiene histéresis**. Pegado al cruce, la señal
 *    pasa por la referencia muy despacio —a 60 Hz, cerca del cero la pendiente
 *    es de ~0.19 mV/µs en el nodo— así que cualquier ruido produce varios
 *    flancos seguidos en vez de uno.
 *
 *    La defensa es una **ventana ciega** tras cada cruce aceptado: los flancos
 *    que llegan antes de `CMP_BLANQUEO_US` se cuentan como rechazados y no se
 *    toman. No se apaga la interrupción para hacerlo: si se apagara, un cruce
 *    de verdad que cayera ahí se perdería sin dejar rastro. Así quedan
 *    contados, y **ese contador es el diagnóstico**: `rechazados = 0` quiere
 *    decir que el cruce está limpio; si sube, el cruce tiene ruido y hay que
 *    ir a buscarlo antes de cerrar ningún lazo.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

/*  Cual de los dos sentidos de cruce se toma como referencia de fase. Depende
 *  de si A6 entra al + o al - del comparador: si el enganche se asienta cerca
 *  de 180 grados en vez de cerca de 0, se voltea este 1 por 0. Una corrida de
 *  banco lo contesta; no se adivina.
 */
#define CMP_ACO_REFERENCIA  1

#include <stdint.h>

/*  Ventana ciega tras un cruce aceptado. A 60 Hz el semiperiodo son 8 333 µs,
 *  así que 2 000 µs deja pasar hasta un 24 % de error de frecuencia sin
 *  estorbar, y tapa cualquier repique del cruce. A 50 Hz el semiperiodo son
 *  10 000 µs y sobra más.
 */
#define CMP_BLANQUEO_US     2000

/*  Un hueco: pasó más de esto sin un cruce. A 60 Hz el semiperiodo son
 *  8 333 µs; 12 000 permite bajar hasta 41.7 Hz antes de contar hueco.
 */
#define CMP_HUECO_US        12000

/*  Sin cruces por más de esto, se declara que no hay red. Tres semiciclos.
 */
#define CMP_SIN_RED_MS      50

void cmp_preparar(void);     // configura los pines y el comparador, apagado
void cmp_arrancar(void);     // habilita la interrupción
void cmp_detener(void);

uint16_t cmp_cruces(void);        // aceptados: deben ser 120/s a 60 Hz
uint16_t cmp_rechazados(void);    // llegaron dentro de la ventana ciega
uint16_t cmp_huecos(void);        // tardaron más de CMP_HUECO_US
uint16_t cmp_rech_direccion(void);// mismo lado que el anterior: repique ida y vuelta

/*  Histograma de la distancia al cruce de los flancos rechazados. Contesta
 *  qué clase de ruido es, desde adentro del micro:
 *      concentrado abajo de ~50 µs  → repique del cruce (falta histéresis)
 *      repartido en cientos de µs   → recruce por armónicos de la red
 */
#define CMP_ANCLA_US   8192   // ancla de la desviación: potencia de dos
#define CMP_CAJONES_N  7
uint16_t cmp_histograma(uint8_t k);
uint16_t cmp_histograma_tope(uint8_t k);

/*  EL JITTER DEL CRUCE ACEPTADO — el criterio que de verdad importa.
 *
 *  Devuelve el mínimo, el promedio, el máximo y la σ del semiperiodo, en µs.
 *  false si todavía no hay suficientes.
 *
 *  ⚠️ σ del semiperiodo no es σ del cruce: el semiperiodo es la diferencia de
 *  dos cruces independientes, así que σ_cruce = σ_semi / √2.
 */
bool cmp_jitter(uint16_t *minUs, uint16_t *promUs, uint16_t *maxUs, uint16_t *sigmaUs);
uint16_t cmp_semiperiodo_us(void);// el último aceptado
bool     cmp_red_presente(void);
uint16_t cmp_t_ultimo_cruce(void);   // sello de 4 us de CUALQUIER cruce, 120/s

/*  EL SELLO QUE SIRVE PARA FASE: un solo sentido de cruce, 60/s.
 *
 *  `cmp_t_ultimo_cruce()` se mueve en los dos cruces del ciclo. Restarlo
 *  contra el del inversor —que solo sella el semiciclo positivo— da un numero
 *  ambiguo en 180 grados; el banco lo mostro como errores agrupados en +-150
 *  en vez de converger. Para fase hay que usar estos dos.
 *
 *  Ni el sello ni el contador los borra `cmp_reiniciar()`: el PLL no puede
 *  depender de un instrumento que el reporte pone en cero cada segundo.
 */
uint16_t cmp_t_cruce_dir(void);
uint8_t  cmp_n_cruce_dir(void);

/*  Hay que llamarla en cada vuelta del lazo: drena el anillo de semiperiodos
 *  y acumula la estadística. Lo que antes costaba 117 ciclos dentro de la ISR
 *  —y hacía perder pasos de la onda— aquí no le cuesta nada a nadie.
 */
void cmp_atender(void);

/*  Frecuencia en milihertz (60043 = 60.043 Hz), del promedio de los
 *  semiperiodos desde el último `cmp_reiniciar()`. 0 si no hay suficientes.
 *
 *  El promedio de N semiperiodos ES (t_último − t_primero)/N, así que da lo
 *  mismo que medir el lapso completo — sin necesitar un reloj de 32 bits, y
 *  dejando fuera los semiperiodos de los huecos. Se calcula en 256-avos de µs
 *  para que el redondeo valga 0.028 mHz contra un criterio de 10.
 */
uint32_t cmp_frecuencia_mhz(void);

void cmp_reiniciar(void);
