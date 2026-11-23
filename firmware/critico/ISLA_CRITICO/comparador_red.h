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
uint16_t cmp_semiperiodo_us(void);// el último aceptado
bool     cmp_red_presente(void);

/*  Frecuencia en milihertz (60043 = 60.043 Hz), promediada sobre todos los
 *  cruces desde el último `cmp_reiniciar()`. Devuelve 0 si no hay suficientes.
 *
 *  No se saca de un semiperiodo: se saca del tiempo entre el PRIMER y el
 *  ÚLTIMO cruce de la ventana, dividido entre los semiperiodos que hubo. Así
 *  los 4 µs de `micros()` se reparten entre un segundo entero y el error de
 *  cuantización queda en ~0.0005 Hz.
 */
uint32_t cmp_frecuencia_mhz(void);

void cmp_reiniciar(void);
