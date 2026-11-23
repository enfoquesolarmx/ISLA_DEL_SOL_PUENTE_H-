/*
 * =============================================================================
 *  comparador_red.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del cruce por cero de la red. Ver comparador_red.h.
 * =============================================================================
 */

#include "comparador_red.h"
#include "rms_ventana.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// --- Enganche de medición (protocolo de banco, Parte C, etapa 4) -------------
// En producción no existe traza_cmp.h: las macros quedan vacías y el código
// compilado es el mismo. El sketch de la etapa 4 pone el archivo al lado.
#if defined(__has_include)
#  if __has_include("traza_cmp.h")
#    include "traza_cmp.h"
#  endif
#endif
#ifndef CMP_TRAZA_ENTRA
#define CMP_TRAZA_ENTRA()
#endif
#ifndef CMP_TRAZA_SALE
#define CMP_TRAZA_SALE()
#endif

// ============================== ESTADO =======================================

static volatile uint16_t cruces, rechazados, huecos;
static volatile uint16_t semiperiodo;
static volatile uint32_t tUltimo;        // µs del último cruce aceptado
static volatile uint32_t tPrimero;       // µs del primero de esta ventana
static volatile uint16_t crucesVentana;  // aceptados desde cmp_reiniciar()

// ================================ LA ISR =====================================
//
// Corre en cada flanco del comparador: subida Y bajada (ACIS = 00, alternar),
// o sea los dos cruces de cada ciclo. A 60 Hz son 120 por segundo.
//
// Lo único que hace es fechar y decidir si el cruce es bueno. El RMS de la red
// se cierra aquí porque este es el instante que define la ventana: medio ciclo
// va de un cruce al siguiente, y nadie más sabe cuándo ocurrió.

ISR(ANALOG_COMP_vect)
{
  CMP_TRAZA_ENTRA();

  const uint32_t t  = micros();
  const uint32_t dt = t - tUltimo;

  /*  Repique del cruce: llegó demasiado pronto para ser real. Se cuenta y se
   *  descarta, pero NO se apaga la interrupción — un cruce bueno que cayera
   *  en la ventana ciega se perdería sin dejar rastro.
   */
  if (dt < CMP_BLANQUEO_US) {
    rechazados++;
    CMP_TRAZA_SALE();
    return;
  }

  if (dt > CMP_HUECO_US) huecos++;   // se acepta igual, pero queda anotado

  tUltimo     = t;
  semiperiodo = (uint16_t)dt;        // < 65 ms, cabe de sobra
  cruces++;

  if (crucesVentana == 0) tPrimero = t;
  crucesVentana++;

  rms_cerrar_red();                  // aquí termina una ventana y empieza otra

  CMP_TRAZA_SALE();
}

// ============================ FUNCIONES PÚBLICAS =============================

void cmp_preparar(void)
{
  ACSR = _BV(ACD);                 // comparador apagado mientras se configura

  /*  D6 y D7 como entradas sin pull-up. Son nodos analógicos: un pull-up de
   *  20–50 kΩ sobre el divisor movería el bias y el cruce con él.
   */
  DDRD  &= (uint8_t)~(_BV(PD6) | _BV(PD7));
  PORTD &= (uint8_t)~(_BV(PD6) | _BV(PD7));

  /*  Apaga los buffers digitales de los dos pines. Ahorra consumo y, sobre
   *  todo, quita una fuente de ruido justo en el nodo que vamos a comparar.
   */
  DIDR1 |= _BV(AIN0D) | _BV(AIN1D);

  cmp_reiniciar();

  /*  ACSR, bit por bit, porque cada uno importa:
   *    ACD  = 0   comparador encendido
   *    ACBG = 0   la entrada positiva es AIN0 (el divisor), no el bandgap
   *    ACIC = 0   ⚠️ NO amarrar a la captura del Timer1: ICR1 es el TOP de la
   *               portadora, y escribirlo cambiaría la frecuencia de
   *               conmutación en cada cruce de la red. Ver comparador_red.h.
   *    ACIS = 00  interrumpe en CUALQUIER flanco: los dos cruces del ciclo
   *    ACIE = 0   todavía no; lo prende cmp_arrancar()
   */
  ACSR = 0;
  ACSR |= _BV(ACI);                // limpia la bandera (se limpia escribiendo 1)
}

void cmp_arrancar(void)
{
  ACSR |= _BV(ACI);                // sin basura pendiente del arranque
  ACSR |= _BV(ACIE);
}

void cmp_detener(void)
{
  ACSR &= (uint8_t)~_BV(ACIE);
}

uint16_t cmp_cruces(void)      { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = cruces; }      return v; }
uint16_t cmp_rechazados(void)  { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rechazados; }  return v; }
uint16_t cmp_huecos(void)      { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = huecos; }      return v; }
uint16_t cmp_semiperiodo_us(void) { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = semiperiodo; } return v; }

bool cmp_red_presente(void)
{
  uint32_t t;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = tUltimo; }
  return (micros() - t) < ((uint32_t)CMP_SIN_RED_MS * 1000UL);
}

uint32_t cmp_frecuencia_mhz(void)
{
  uint32_t t0, t1;
  uint16_t n;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t0 = tPrimero; t1 = tUltimo; n = crucesVentana; }

  if (n < 3) return 0;             // hacen falta al menos dos semiperiodos

  const uint32_t lapso = t1 - t0;  // µs entre el primer y el último cruce
  if (lapso == 0) return 0;

  /*  n cruces aceptados dejan n-1 semiperiodos en `lapso`. Entonces
   *
   *      periodo = 2 × lapso / (n-1)        f = (n-1) / (2 × lapso)
   *
   *  y en milihertz, con lapso en µs:
   *
   *      f_mHz = (n-1) × 1e6 × 1000 / (2 × lapso) = (n-1) × 5e8 / lapso
   *
   *  Se hace en 64 bits porque 120 × 5e8 ya no cabe en 32. Corre una vez por
   *  reporte, en el lazo, así que el costo da igual.
   */
  return (uint32_t)(((uint64_t)(n - 1) * 500000000ULL) / lapso);
}

void cmp_reiniciar(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    cruces = rechazados = huecos = 0;
    crucesVentana = 0;
    semiperiodo = 0;
  }
}
