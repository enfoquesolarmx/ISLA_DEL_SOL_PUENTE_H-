/*
 * =============================================================================
 *  traza_isr.h  -  Isla del Sol, protocolo de banco Parte C, etapa 1
 *  v4 — histograma de latencia de entrada, para identificar a los ladrones
 *       por el ANCHO de su bloqueo y no por descarte
 * -----------------------------------------------------------------------------
 *  spwm_328p.cpp busca este archivo con __has_include y, si lo encuentra, lo
 *  incluye. En la carpeta de producción (ISLA_CRITICO/) NO EXISTE, así que las
 *  macros quedan vacías y el código compilado es byte por byte el mismo.
 *  Verificado: el .o da el mismo md5 con y sin este archivo presente.
 *
 *  QUÉ MIDE CADA COSA
 *
 *    isr_t0 = TCNT1 al entrar.
 *        TOV1 se levanta en BOTTOM con TCNT1 = 0, así que esto es el tiempo
 *        desde que la interrupción se disparó hasta la primera instrucción
 *        útil. Son dos cosas sumadas:
 *            el piso fijo = vector (7) + prólogo (32) + traza (~4) = 43
 *            la ESPERA    = lo que tardó otro en soltar las interrupciones
 *        Medido en banco: el piso es 43, clavado en todos los reportes.
 *
 *    isr_t1 - isr_t0 = el CUERPO. Costo propio del SPWM, no depende de quién
 *        más esté corriendo. Medido: 49/116/126 ciclos, idéntico siempre.
 *
 *    isr_sobrepasos: el AVR limpia TOV1 al entrar a la ISR. Si al SALIR está
 *        puesto otra vez, hubo un segundo desborde mientras estábamos adentro
 *        y se perdió una actualización de OCR. Exacto, sin umbral.
 *
 *    isr_n: cuenta TODAS las ISR; el lazo sólo alcanza a muestrear ~99 %.
 *
 *  ⚠️ La instrumentación que corre DESPUÉS de leer isr_t1 (el contador y la
 *  comprobación de TOV1, ~23 ciclos) alarga la ISR real pero NO entra en el
 *  número reportado. Por eso el detector de sobrepasos es algo pesimista.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>
#include "config_portadora.h"   // por CICLOS_CPU_PERIODO = 2 × TOP

// Pin de traza para el analizador lógico: PD4 = D4 (INV_CMD con J15 puesto).
// ⚠️ J15 DESCONECTADO durante toda la prueba — ver Parte C, sección C0.
#define TRZ_BIT    PD4

/*  1 = la ISR del SPWM tambien mueve el pin D4.  0 = solo la contabilidad.
 *
 *  Se pone en 0 mientras D4 lleva los cruces de la red (traza_cruce.h). Lo
 *  que se apaga es UNICAMENTE el toggleo del pin: `isr_t0`, `isr_t0_max`,
 *  `isr_n` y sobre todo `isr_sobrepasos` se siguen midiendo igual, asi que
 *  PROPIO, t0max y SOBREPASOS del reporte NO cambian.
 */
#define TRZ_SPWM_PIN   0

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint16_t isr_t0;          // TCNT1 al entrar  (piso + espera)
extern volatile uint16_t isr_t1;          // TCNT1 al salir, antes del epílogo
extern volatile uint8_t  isr_lista;       // bandera: hay una muestra nueva
extern volatile uint32_t isr_n;           // TODAS las ISR, no sólo las recogidas
extern volatile uint16_t isr_sobrepasos;  // no terminó antes del siguiente TOV
extern volatile uint16_t isr_t0_max;      // el PEOR caso de entrada, sin ceguera

#ifdef __cplusplus
}
#endif

/*  ⚠️ EL MÁXIMO SE GUARDA AQUÍ ADENTRO, Y HAY QUE EXPLICAR POR QUÉ.
 *
 *  El histograma lo arma el lazo, que sólo recoge una muestra por vuelta. Con
 *  el I2C corriendo, el lazo se queda detenido 2.7 ms y **las ~63 ISR de esa
 *  ventana no se recogen**. La corrida del 24-nov lo dejó en evidencia:
 *
 *      entrada máxima que reportó el histograma    269 ciclos
 *      entrada mínima que SOBREPASOS demuestra     > 491 ciclos, 14 veces
 *
 *  O sea que el máximo muestreado subestimaba el real **a la mitad**.
 *
 *  ⚠️⚠️ Y LA PRIMERA VERSIÓN DE ESTE MÁXIMO SALIÓ SATURADA, TAMBIÉN POR MÍ.
 *
 *  Leía `TCNT1` crudo, y en modo 8 el contador sube 0→TOP y baja TOP→0: **no
 *  puede leer más que TOP**. La corrida reportó `t0max = 333` en varios
 *  reportes, que es TOP exacto — o sea «333 o más», no 333.
 *
 *  EL ARREGLO: dos lecturas seguidas dicen la dirección.
 *
 *  Una lectura de 16 bits son dos instrucciones, cuatro ciclos, así que la
 *  segunda lectura queda cuatro cuentas adelante si el contador sube, y
 *  cuatro atrás si baja. Si baja, el tiempo real desde BOTTOM no es `TCNT1`
 *  sino `2·TOP − TCNT1`.
 *
 *  Con eso el alcance pasa de 333 a 666 ciclos — justo el periodo completo, y
 *  arriba de los 491 donde se pierde un paso. **Se acaba la ventana ciega de
 *  207 a 245 que arrastrábamos desde la etapa 1**, donde ningún umbral podía
 *  distinguir una lectura doblada de una buena.
 *
 *  Cuesta ocho ciclos por punta. Es instrumentación: en producción no existe.
 */
#define SPWM_TCNT_DIR(dest)                                       \
  do {                                                            \
    const uint16_t a_ = TCNT1;                                    \
    const uint16_t b_ = TCNT1;                                    \
    (dest) = (b_ >= a_) ? a_                                      \
                        : (uint16_t)(CICLOS_CPU_PERIODO - a_);    \
  } while (0)

#define SPWM_TRAZA_ENTRA()                      \
  do {                                          \
    if (TRZ_SPWM_PIN) PORTD |= _BV(TRZ_BIT);      \
    uint16_t t;                                 \
    SPWM_TCNT_DIR(t);                           \
    isr_t0 = t;                                 \
    if (t > isr_t0_max) isr_t0_max = t;         \
  } while (0)

#define SPWM_TRAZA_SALE()                       \
  do {                                          \
    SPWM_TCNT_DIR(isr_t1);                      \
    isr_n++;                                    \
    if (TIFR1 & _BV(TOV1)) isr_sobrepasos++;    \
    isr_lista = 1;                              \
    if (TRZ_SPWM_PIN) PORTD &= ~_BV(TRZ_BIT);     \
  } while (0)
