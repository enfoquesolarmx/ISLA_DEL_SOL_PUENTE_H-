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

/*  Lo mismo, pero de 32 bits.
 *
 *  ⚠️ POR QUE EXISTE. El de 16 bits envuelve cada 65 536 × 4 µs = 262.144 ms.
 *  Para la fase eso da igual —las diferencias son de microsegundos— pero para
 *  un VENCIMIENTO es una trampa: si el sello de referencia se queda quieto
 *  (no hay red), la resta `ahora - t` recorre todo el rango y vuelve a caer
 *  debajo del umbral una vez por vuelta. El 27 de noviembre de 2026 eso hizo
 *  que `cmp_red_presente()` dijera «hay red» durante 50 ms de cada 262 ms con
 *  la red apagada. Lo encontró el analizador lógico con D13 afuera; el serial
 *  no lo mostraba. Medido: 261.9 ms de periodo contra 262.144 predichos.
 *
 *  Este envuelve en 2^32 garrapatas = 4.77 horas, así que sirve para medir
 *  ausencias. Para fase se sigue usando el de 16 bits, que es más barato.
 *
 *  ⚠️ SÓLO PARA EL LAZO. Lleva un `cli()` de ~10 ciclos porque
 *  `timer0_overflow_count` son cuatro bytes y no se lee atómico. No meterlo
 *  en una ISR: ver la regla del `cli()` en DC-026.
 */
static inline uint32_t reloj_4us_32(void)
{
  const uint8_t sreg = SREG;
  cli();
  const uint8_t t0 = TCNT0;
  uint32_t      m  = timer0_overflow_count;
  if ((TIFR0 & _BV(TOV0)) && (t0 < 255)) m++;
  SREG = sreg;
  return (m << 8) | (uint32_t)t0;
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

/* ===========================================================================
 *  LA RESTA DE FASE, EN UN SOLO LUGAR -- Y ESTO SE APRENDIO EN BANCO
 *
 *  El primer intento restaba los dos sellos en `uint16_t` y luego reducia el
 *  resultado con  `while (d >= 4163) d -= 4163`.  Eso esta MAL, y falla la
 *  mitad de las veces:
 *
 *    El reloj de 4 us envuelve en 65536, y **65536 no es multiplo de 4162**.
 *    Cuando el cruce de la red cae ANTES que el del inversor, la diferencia
 *    verdadera es negativa; en uint16 eso aparece como 65535, 65534..., y
 *    restarle 4162 repetidas veces no da -1, da 3091. Un cruce que estaba a
 *    un pelo de cero se reportaba a 267 grados.
 *
 *  La forma correcta: la diferencia de dos `uint16_t` **casteada a int16_t**
 *  ES la diferencia firmada exacta, siempre que el valor real quepa en
 *  +-32768 cuentas (131 ms). El nuestro es menor a 17 ms. Y el doblez se hace
 *  sobre ese numero firmado, no sobre el crudo.
 *
 *  Vive aqui, junto al reloj, por la misma razon que el reloj: una sola pieza
 *  sirviendo al lazo y al reporte, para que no puedan discrepar.
 * ===========================================================================
 */

#define RELOJ_CICLO     (RELOJ_CICLO_X2 / 2u)   /* 4162 cuentas = un ciclo   */
#define RELOJ_MEDIO     (RELOJ_CICLO_X2 / 4u)   /* 2081 = medio ciclo        */

/*  Fase de tA respecto a tB, en decimas de grado, en -1800..+1800.
 *  Los dos sellos tienen que venir del MISMO sentido de cruce: si uno es de
 *  120/s y el otro de 60/s, el numero sale ambiguo en 180 grados.
 */
static inline int16_t reloj_fase_x10(uint16_t tA, uint16_t tB)
{
  int16_t raw = (int16_t)((uint16_t)(tA - tB));    /* firmada, exacta */
  while (raw >  (int16_t)RELOJ_MEDIO) raw -= (int16_t)RELOJ_CICLO;
  while (raw < -(int16_t)RELOJ_MEDIO) raw += (int16_t)RELOJ_CICLO;
  return (int16_t)(((int32_t)raw * 3600L) / (int32_t)RELOJ_CICLO);
}
