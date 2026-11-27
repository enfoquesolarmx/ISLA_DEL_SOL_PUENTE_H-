/*
 * =============================================================================
 *  senal_red.cpp  -  el veredicto de la red, manejado a D13
 *  Ver senal_red.h: que es, para que, y lo que debe salir en el analizador.
 * =============================================================================
 */

#include <avr/io.h>

#include "senal_red.h"
#include "comparador_red.h"
#include "reloj_4us.h"
#include "traza_cmp.h"        /* de aqui sale TRZ_CMP_BIT = PB5 = D13 */

/*  Estado. Nada de esto lo toca una ISR, asi que no hace falta `volatile`
 *  ni ATOMIC_BLOCK: vive y muere en el lazo.
 */
static bool     nivel        = false;
static bool     arrancado    = false;
static uint16_t cambios      = 0;
static uint8_t  ultSubio     = 0;
static uint16_t ultLatenciaUs = 0;
static bool     hayUltima    = false;

void senal_red_preparar(void)
{
#if SENAL_RED_ACTIVA
  PORTB &= ~_BV(TRZ_CMP_BIT);     /* 0 = sin red, y es el estado seguro  */
  DDRB  |=  _BV(TRZ_CMP_BIT);     /* manejar DESPUES de fijar el valor   */
#endif
  nivel     = false;
  arrancado = false;
  cambios   = 0;
  hayUltima = false;
}

void senal_red_atender(void)
{
#if SENAL_RED_ACTIVA
  const bool hay = cmp_red_presente();

  if (hay == nivel && arrancado) return;    /* el caso comun: nada que hacer */

  /*  El pin primero: una instruccion, 62.5 ns, antes de gastar tiempo en la
   *  contabilidad. Lo que se esta midiendo es CUANDO cambia el pin.
   */
  if (hay) PORTB |=  _BV(TRZ_CMP_BIT);
  else     PORTB &= ~_BV(TRZ_CMP_BIT);

  if (arrancado) {
    /*  A cuantos microsegundos del cruce MAS RECIENTE cambio el veredicto.
     *
     *  Al caer, el cruce mas reciente es el ULTIMO cruce real de la red, asi
     *  que esto es el tiempo que tardo el firmware en darse cuenta: debe
     *  salir ~CMP_SIN_RED_MS, porque el criterio es un puro vencimiento.
     *
     *  Al subir, el cruce mas reciente es el que acaba de llegar, asi que
     *  esto es lo que tarda el LAZO en reaccionar: debe ser < 1 ms.
     */
    const uint16_t t     = cmp_t_ultimo_cruce();
    const uint16_t ahora = reloj_4us();
    const uint16_t tics  = (uint16_t)(ahora - t);

    /*  Garrapatas de 4 us a us, con tope: 16 383 tics son 65 532 us y de ahi
     *  no cabe en 16 bits. Mas de eso ya no es un numero util, es un vuelco
     *  del reloj (envuelve cada 262 ms).
     */
    ultLatenciaUs = (tics > 16383u) ? 0xFFFFu : (uint16_t)(tics << 2);
    ultSubio      = hay ? 1u : 0u;
    hayUltima     = true;
    if (cambios != 0xFFFFu) cambios++;
  }

  nivel     = hay;
  arrancado = true;
#endif
}

bool     senal_red_nivel(void)   { return nivel; }
uint16_t senal_red_cambios(void) { return cambios; }

bool senal_red_ultima(uint8_t *subio, uint16_t *latenciaUs)
{
  if (!hayUltima) return false;
  if (subio)      *subio      = ultSubio;
  if (latenciaUs) *latenciaUs = ultLatenciaUs;
  return true;
}
