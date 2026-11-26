/* =============================================================================
 *  ISLA DEL SOL — el reloj de 4 µs, compartido
 *
 *  Timer0 corre con el preescalador /64 que pone el núcleo de Arduino, así que
 *  desborda cada 1024 µs y cada cuenta vale 4 µs. Esta lectura arma un sello
 *  de 16 bits con el contador y el byte bajo de los desbordes: **262 ms de
 *  alcance**, de sobra para un ciclo de 16.65 ms.
 *
 *  POR QUE ES UNA SOLA FUNCION Y NO DOS COPIAS
 *
 *    La usan dos sitios: la ISR del comparador, que sella el cruce de la RED,
 *    y la ISR del SPWM, que sella el cruce del INVERSOR. La diferencia entre
 *    esos dos sellos **es** la fase, y es lo único que nos interesa.
 *
 *    Si cada sitio tuviera su propia copia del código, bastaría que el
 *    compilador ordenara las instrucciones distinto para meter un **sesgo
 *    constante** entre los dos sellos — y un sesgo constante en una medición
 *    de fase es indistinguible de fase de verdad. Siendo la misma secuencia de
 *    instrucciones, lo que sobre o falte **se cancela exacto en la resta**.
 *
 *    Es la misma idea del espejo del cuarto de onda: una sola pieza sirviendo
 *    los dos lados, y lo que se ahorra no es sólo flash — es que los dos lados
 *    no puedan discrepar.
 *
 *  ⚠️ `timer0_overflow_count` no está en ningún encabezado público del núcleo,
 *  pero tiene enlace externo y se llama igual desde hace quince años. Si algún
 *  día cambiara, **esto no compila** — que es la forma correcta de enterarse.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

extern volatile unsigned long timer0_overflow_count;

/*  Cuentas de 4 µs. ~10 ciclos. Se lee TCNT0 primero y la bandera después:
 *  si el contador desbordó entre las dos lecturas, `t0` ya viene del ciclo
 *  nuevo y hay que sumar el desborde a mano, porque la ISR de Timer0 no pudo
 *  correr —estamos dentro de otra ISR.
 */
static inline uint16_t reloj_4us(void)
{
  const uint8_t t0 = TCNT0;
  uint8_t       m8 = (uint8_t)timer0_overflow_count;
  if ((TIFR0 & _BV(TOV0)) && (t0 < 255)) m8++;
  return (uint16_t)(((uint16_t)m8 << 8) | t0);
}

/*  Cuentas de 4 µs en un ciclo de línea completo, para desenrollar la fase.
 *      60.06 Hz → 16 650 µs → 4162.5 cuentas
 *  Se guarda ×2 para no perder el medio.
 */
#define RELOJ_CICLO_X2   8325u

/*  Grados ×100 por cuenta de 4 µs:  360 / 4162.5 = 0.08648°
 */
#define RELOJ_GRADOS_X100_NUM   8648UL
#define RELOJ_GRADOS_X100_DEN    100UL
