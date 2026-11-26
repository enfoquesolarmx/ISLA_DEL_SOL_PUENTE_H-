/*
 * =============================================================================
 *  traza_cmp.h  -  Isla del Sol, protocolo de banco Parte C, etapa 4
 *  Traza de la ISR del comparador, en D13 (CH8 de las ocho puntas)
 * -----------------------------------------------------------------------------
 *  comparador_red.cpp busca este archivo con __has_include. En la carpeta de
 *  producción NO EXISTE, así que las macros quedan vacías.
 *
 *  ⚠️ D13 es PB5 = `TN_CMD`, la orden de unir N–PE (relé K5). Esto sólo se
 *  puede usar como traza con **J15 DESCONECTADO**, que es la condición de
 *  toda la Parte C (sección C0). Con la tarjeta de transferencia puesta,
 *  alternar D13 castañetearía K5.
 *
 *  A diferencia de la ISR del SPWM, aquí la traza sí sirve sola: el
 *  comparador corre 120 veces por segundo, no 24 024, así que lo que importa
 *  es CUÁNDO cae —si encima del SPWM o del ADC— y eso el pin lo dice bien.
 *  El costo propio se mide con el analizador, y los 74 ciclos de prólogo que
 *  el pin no ve son despreciables a 120 Hz: 0.06 % del CPU.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>

#define TRZ_CMP_BIT   PB5          // D13 = TN_CMD con J15 puesto

#define CMP_TRAZA_ENTRA()   do { PORTB |=  _BV(TRZ_CMP_BIT); } while (0)
#define CMP_TRAZA_SALE()    do { PORTB &= ~_BV(TRZ_CMP_BIT); } while (0)
