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

static volatile uint16_t cruces, rechazados, huecos, rechDir;

/*  ESTADÍSTICA DEL SEMIPERIODO — la medición que sustenta el criterio nuevo.
 *
 *  Pedir `rechazados = 0` con un comparador sin histéresis es pedir lo
 *  imposible: la pendiente en el cruce es de 0.6 mV/µs, así que unos
 *  milivolts de ruido alternan la salida. Lo que de verdad le importa al PLL
 *  no es cuántos flancos se descartaron, sino **cuánto se mueve el cruce que
 *  sí se acepta**.
 *
 *  Se acumula la desviación contra un ancla de 8 192 µs (una potencia de dos,
 *  para que la resta no cueste) y de ahí sale σ sin manejar números grandes:
 *  la desviación anda en ±300, su cuadrado en 9e4, y 120 de ellos caben de
 *  sobra en 32 bits.
 *
 *  ⚠️ σ del SEMIPERIODO no es σ del cruce: un semiperiodo es la diferencia de
 *  dos cruces, cada uno con su propio jitter, así que σ_cruce = σ_semi / √2.
 *  El sketch hace esa conversión al reportar.
 */
#define CMP_ANCLA_US   8192
static volatile uint16_t semiN, semiMin, semiMax;
static volatile int32_t  semiSumaDev;
static volatile uint32_t semiSumaDev2;
static volatile uint8_t  acoUlt;          // ACO del último cruce aceptado

/*  Histograma de a qué distancia del cruce aceptado llegan los flancos
 *  rechazados. Es la pregunta que decide el arreglo, y se contesta desde
 *  adentro — sin analizador y sin ventana ciega.
 *
 *      si caen en los primeros ~50 µs  → REPIQUE del cruce: el comparador
 *          alterna porque no tiene histéresis y la pendiente es de
 *          0.6 mV/µs. Se arregla con histéresis (por software o por un pin).
 *      si caen a cientos de µs o a ms  → RECRUCE por armónicos: la red está
 *          achatada cerca del cero y de verdad cruza el umbral más de una
 *          vez. Ningún filtro lo quita; se arregla con el blanqueo, que ya
 *          está, y la pregunta pasa a ser si el blanqueo es suficiente.
 */
#define CMP_CAJONES  7
static const uint16_t CMP_TOPE[CMP_CAJONES] = { 20, 50, 100, 200, 500, 1000, 2000 };
static volatile uint16_t cmp_histo[CMP_CAJONES];
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
  const uint8_t  aco = (uint8_t)((ACSR & _BV(ACO)) ? 1 : 0);

  /*  Repique del cruce: llegó demasiado pronto para ser real. Se cuenta y se
   *  descarta, pero NO se apaga la interrupción — un cruce bueno que cayera
   *  en la ventana ciega se perdería sin dejar rastro.
   *
   *  Y se anota EN QUÉ CAJÓN cayó, que es lo que dice qué clase de ruido es.
   */
  if (dt < CMP_BLANQUEO_US) {
    rechazados++;
    for (uint8_t k = 0; k < CMP_CAJONES; k++) {
      if (dt <= CMP_TOPE[k]) { cmp_histo[k]++; break; }
    }
    CMP_TRAZA_SALE();
    return;
  }

  /*  LA HISTÉRESIS QUE EL HARDWARE NO DA.
   *
   *  El comparador del AVR no tiene histéresis y su salida no sale a ningún
   *  pin, así que no se puede realimentar. Pero sí se puede leer `ACO`: en un
   *  cruce de verdad la salida queda del lado contrario al del cruce
   *  anterior. Un flanco que llega con el MISMO lado que el anterior no es un
   *  cruce nuevo — es repique que alcanzó a ir y volver.
   *
   *  Cuesta cuatro ciclos y no puede equivocarse en contra: si el repique es
   *  más rápido que la entrada a la ISR (~3 µs), `ACO` ya volvió y el flanco
   *  se descarta, que es lo que queremos.
   */
  if (cruces != 0 && aco == acoUlt) {
    rechDir++;
    CMP_TRAZA_SALE();
    return;
  }
  acoUlt = aco;

  if (dt > CMP_HUECO_US) huecos++;   // se acepta igual, pero queda anotado

  tUltimo     = t;
  semiperiodo = (uint16_t)dt;        // < 65 ms, cabe de sobra
  cruces++;

  /*  El jitter. Sólo entran los semiperiodos plausibles: el primero de todos
   *  y los que vienen después de un hueco no son medidas de jitter, son
   *  medidas de que faltó un cruce.
   */
  if (cruces > 1 && dt < CMP_HUECO_US) {
    const int16_t dev = (int16_t)((int32_t)dt - CMP_ANCLA_US);
    semiSumaDev  += dev;
    semiSumaDev2 += (uint32_t)((int32_t)dev * dev);
    if (semiN == 0 || (uint16_t)dt < semiMin) semiMin = (uint16_t)dt;
    if (semiN == 0 || (uint16_t)dt > semiMax) semiMax = (uint16_t)dt;
    semiN++;
  }

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
uint16_t cmp_rech_direccion(void) { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rechDir; } return v; }

bool cmp_jitter(uint16_t *minUs, uint16_t *promUs, uint16_t *maxUs, uint16_t *sigmaUs)
{
  uint16_t n, mn, mx;
  int32_t  sd;
  uint32_t sd2;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    n = semiN; mn = semiMin; mx = semiMax; sd = semiSumaDev; sd2 = semiSumaDev2;
  }
  if (n < 8) return false;

  const int32_t promDev = sd / (int32_t)n;

  /*  σ² = ⟨d²⟩ − ⟨d⟩², en la forma escalada de siempre para no truncar antes
   *  de tiempo: (n·Σd² − (Σd)²) / n².
   */
  const uint64_t num = (uint64_t)n * sd2 - (uint64_t)((int64_t)sd * sd);
  const uint32_t var = (uint32_t)(num / ((uint32_t)n * n));

  if (minUs)   *minUs   = mn;
  if (maxUs)   *maxUs   = mx;
  if (promUs)  *promUs  = (uint16_t)(CMP_ANCLA_US + promDev);
  if (sigmaUs) *sigmaUs = rms_isqrt32(var);
  return true;
}

uint16_t cmp_histograma(uint8_t k)
{
  if (k >= CMP_CAJONES) return 0;
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = cmp_histo[k]; }
  return v;
}

uint16_t cmp_histograma_tope(uint8_t k)
{
  return (k < CMP_CAJONES) ? CMP_TOPE[k] : 0;
}
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
    cruces = rechazados = huecos = rechDir = 0;
    crucesVentana = 0;
    semiperiodo = 0;
    semiN = 0; semiMin = 0; semiMax = 0;
    semiSumaDev = 0; semiSumaDev2 = 0;
    for (uint8_t k = 0; k < CMP_CAJONES; k++) cmp_histo[k] = 0;
  }
}
