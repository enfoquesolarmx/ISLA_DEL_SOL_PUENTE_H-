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
 *
 *  🛑 2026-11-27: la traza queda **APAGADA por omisión** (`CMP_TRAZA_ACTIVA 0`).
 *  No se borra —sirvió para encontrar el acoplamiento de la portadora y el
 *  pulso pegado del dither— pero se desarma, por dos razones:
 *
 *    1. **D13 ya tiene dueño.** Es `TN_CMD`, la orden de unión N–PE. Un pin
 *       con dos trabajos es un pin sin dueño, y el segundo trabajo de este
 *       es el que castañetea K5 si alguien conecta J15 sin leer esto.
 *    2. En reposo el pin queda en **0 = sin unión**, que es el estado seguro:
 *       con la red conectada la unión N–PE la hace la acometida, y una
 *       segunda unión es lo que prohíben NOM-001-SEDE 250-24(a)(5) y
 *       250-30(a) («reconectar a tierra del lado carga»).
 *
 *  Para volver a medir el reparto de las ISR: poner `CMP_TRAZA_ACTIVA` en 1
 *  **con J15 desconectado**, medir, y regresarlo a 0. No se sube en 1.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>

#define TRZ_CMP_BIT   PB5          // D13 = TN_CMD con J15 puesto

/*  0 = apagada (valor que se sube).  1 = midiendo, sólo con J15 desconectado. */
#define CMP_TRAZA_ACTIVA   0

#if CMP_TRAZA_ACTIVA
#define CMP_TRAZA_ENTRA()   do { PORTB |=  _BV(TRZ_CMP_BIT); } while (0)
#define CMP_TRAZA_SALE()    do { PORTB &= ~_BV(TRZ_CMP_BIT); } while (0)
#else
#define CMP_TRAZA_ENTRA()   do { } while (0)
#define CMP_TRAZA_SALE()    do { } while (0)
#endif
