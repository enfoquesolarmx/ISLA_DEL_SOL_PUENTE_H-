/*
 * =============================================================================
 *  traza_cruce.h  -  Isla del Sol, protocolo de banco Parte C
 *  Los cruces ACEPTADOS de la red, sacados a D4, para medir la demora directo
 * -----------------------------------------------------------------------------
 *  PARA QUE. Para medir cuanto tarda el firmware en darse cuenta de que se
 *  fue la red hacen falta DOS instantes en la misma pantalla:
 *
 *      1. el ultimo cruce real de la red     <- esto no se veia
 *      2. la caida de D13                    <- esto ya se veia
 *
 *  Sin el primero hay que inferirlo de la pierna B, suponiendo que el PLL
 *  estaba enganchado, y eso obliga a contar semiciclos y a adivinar cual
 *  flanco es cual. En el banco del 27 de noviembre de 2026 eso hizo que tres
 *  mediciones seguidas cayeran en el flanco equivocado. **El instrumento
 *  estaba mal, no quien medía.**
 *
 *  Con esto, D4 CONMUTA en cada cruce aceptado: una onda cuadrada de 60 Hz
 *  amarrada a la red. Cuando la red se va, D4 **se congela**. Entonces:
 *
 *      D4   ⎍⎍⎍⎍⎍⎍⎍⎍⎍⎍⎍⎍⎍  ... ultima transicion AQUI
 *      D13  ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔╲______
 *                            └─ la demora, LEIDA, sin inferir nada
 *
 *  Se dispara en flanco de BAJADA de D13 y se mide hacia atras hasta la
 *  ultima transicion de D4. No hay que saber que es la pierna B, ni contar
 *  semiciclos, ni suponer que el PLL estaba enganchado.
 *
 *  POR QUE D4, y que se pierde. D4 es `INV_CMD` y hoy lleva la traza de la
 *  ISR del SPWM. En el CRITICO **no sobra un solo pin** (seccion C0 del
 *  protocolo), asi que el pin de traza sale de las ordenes a la tarjeta de
 *  transferencia. Se apaga SOLO el toggleo del pin (`TRZ_SPWM_PIN 0` en
 *  traza_isr.h): **la contabilidad de la ISR del SPWM se queda intacta**, asi
 *  que `PROPIO`, `t0max` y sobre todo `SOBREPASOS` siguen midiendose igual.
 *  Lo unico que se pierde es poder VER en el analizador cuando corre la ISR
 *  del SPWM, que ahora mismo no hace falta.
 *
 *  ⚠️ SOLO CON J15 DESCONECTADO, como toda la Parte C. D4 es `INV_CMD`, la
 *  orden de pasar la casa al inversor (K4 + K2).
 *
 *  COSTO, desensamblado y no supuesto. En el AVR, escribir un 1 en un bit de
 *  `PINx` CONMUTA el bit correspondiente de `PORTx` -- no es una lectura, es
 *  la forma barata de hacer toggle. GCC lo resuelve en DOS instrucciones:
 *
 *      ldi  r24, 0x10
 *      out  0x09, r24      ; PIND
 *
 *  Son 2 ciclos. (Esperaba un solo `sbi`; no fue. Se mide, no se supone.)
 *  A 120 cruces por segundo son 240 ciclos/s: **0.0015 % del CPU.** Medido
 *  en la ISR del comparador: 244 -> 246 instrucciones, los mismos 19 push.
 *
 *  Y del otro lado se GANA: apagar el toggleo del pin en la ISR del SPWM
 *  —la que corre 24 024 veces por segundo— la deja en 257 instrucciones en
 *  vez de 258, con los mismos 15 push.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>

/*  1 = D4 conmuta en cada cruce aceptado.  0 = D4 queda quieto.
 *  Se sube en 1 porque es la medicion en curso. Al cerrarla, a 0 y se
 *  devuelve el pin a la traza del SPWM (TRZ_SPWM_PIN en traza_isr.h).
 */
#define CRUCE_TRAZA_ACTIVA   1

#define CRUCE_TRAZA_BIT      PD4      /* D4 = INV_CMD con J15 puesto */

#if CRUCE_TRAZA_ACTIVA
/*  Escribir 1 en PINx conmuta PORTx: un `sbi`, 2 ciclos. */
#define CRUCE_TRAZA_TOGGLE()  do { PIND = _BV(CRUCE_TRAZA_BIT); } while (0)
#else
#define CRUCE_TRAZA_TOGGLE()  do { } while (0)
#endif
