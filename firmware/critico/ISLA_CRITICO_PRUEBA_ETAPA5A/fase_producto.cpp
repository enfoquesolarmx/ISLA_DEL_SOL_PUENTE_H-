/* =============================================================================
 *  ISLA DEL SOL — detector de fase por producto. Ver el .h para el porque.
 *
 *  REPARTO DE TRABAJO, Y ES LA REGLA DE LA CASA:
 *      la ISR guarda dos enteros        ~25 ciclos,  1.5 us,  1500/s
 *      el lazo hace tablas y productos  ~150 ciclos, sin prisa
 * =============================================================================
 */
#include <Arduino.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <math.h>

#include "fase_producto.h"
#include "spwm.h"
#include "tabla_seno.h"
#include "config_portadora.h"

/* ------------------------------------------------- el anillo ISR -> lazo */

#define ANILLO 32                      /* potencia de dos: el modulo es un AND */

static volatile uint16_t anPos[ANILLO];
static volatile uint16_t anVal[ANILLO];
static volatile uint8_t  anW, anR;
static volatile uint16_t perdidas;

volatile bool fase_activa = false;

void fase_habilitar(bool si)
{
  if (si) fase_preparar();
  fase_activa = si;
}

void fase_acumular(uint16_t cuenta)
{
  const uint8_t w = anW;
  const uint8_t sig = (uint8_t)((w + 1) & (ANILLO - 1));
  if (sig == anR) { perdidas++; return; }   /* el lazo no drenó: se tira */
  anPos[w] = spwm_posicion();
  anVal[w] = cuenta;
  anW = sig;
}

/* ------------------------------------------- seno y coseno de la tabla --- */

/*  La tabla es un CUARTO de onda: 100 entradas que cubren 0..90 grados, con
 *  tabla[k] = 65535 x sen((k+0.5) x 180/200). El ciclo son 400 posiciones, o
 *  sea 100 por cuadrante, asi que el indice cae directo.
 *
 *  Se devuelve con signo y escalado a +-16383 (Q14) para que el producto por
 *  una cuenta del ADC (+-512) quepa holgado en int32.
 */
static int16_t seno_q14(uint16_t pos, uint16_t vuelta)
{
  const uint16_t cuarto = (uint16_t)(vuelta >> 2);     /* 100 */
  uint16_t q = 0;
  while (pos >= cuarto) { pos -= cuarto; q++; }        /* 0..3, sin dividir */

  const uint16_t k = (q & 1u) ? (uint16_t)(cuarto - 1u - pos) : pos;
  const uint16_t *tab = (vuelta == 480u) ? TABLA_SENO_50_Q16 : TABLA_SENO_60_Q16;
  const uint16_t v = pgm_read_word(&tab[k]);
  const int16_t  m = (int16_t)(v >> 2);                /* 65535 -> 16383 */
  return (q >= 2u) ? (int16_t)-m : m;
}

/* ------------------------------------------------ acumuladores del lazo -- */

static int32_t  sumCos, sumSen;
static uint8_t  nMuestras;

void fase_preparar(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { anW = anR = 0; perdidas = 0; }
  sumCos = sumSen = 0;
  nMuestras = 0;
}

void fase_atender(void)
{
  const uint16_t vuelta = spwm_posiciones();
  const uint16_t cuarto = (uint16_t)(vuelta >> 2);

  /*  Un byte se lee atomico en el AVR: el cli() mas barato es el que no
   *  existe (DC-026). Misma razon que en rms_atender().
   */
  while (anR != anW) {
    const uint8_t  r   = anR;
    const uint16_t pos = anPos[r];
    const int16_t  v   = (int16_t)anVal[r] - 511;      /* fuera el bias */
    anR = (uint8_t)((r + 1) & (ANILLO - 1));

    if (pos >= vuelta) continue;                       /* por si cambio 50/60 */

    const int16_t sn = seno_q14(pos, vuelta);
    uint16_t pc = (uint16_t)(pos + cuarto);
    if (pc >= vuelta) pc -= vuelta;          /* sin modulo, aunque sea el lazo */
    const int16_t cs = seno_q14(pc, vuelta);

    sumSen += (int32_t)v * sn;
    sumCos += (int32_t)v * cs;
    if (nMuestras < 255) nMuestras++;
  }
}

bool fase_leer(int16_t *gradosX10, uint8_t *muestras)
{
  if (nMuestras < FASE_MIN_MUESTRAS) return false;

  /*  theta0 = atan2(-Sc, Ss).  El cociente NO depende de la amplitud: la A y
   *  la N se van en la division. Por eso la ganancia del detector no cambia
   *  con el voltaje de la fuente, que es algo que el cruce por cero nunca
   *  pudo dar.
   *
   *  atan2 de libm cuesta ~1000 ciclos, pero corre 60 veces por segundo EN EL
   *  LAZO: 0.4 % de un lazo que da 7680 vueltas por segundo.
   */
  const double th = atan2((double)-sumCos, (double)sumSen);
  int32_t g10 = (int32_t)lround(th * (1800.0 / M_PI));
  if (g10 >  1800) g10 -= 3600;
  if (g10 < -1800) g10 += 3600;

  *gradosX10 = (int16_t)g10;
  *muestras  = nMuestras;

  sumCos = sumSen = 0;
  nMuestras = 0;
  return true;
}

uint16_t fase_perdidas(void)
{
  uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = perdidas; } return v;
}
