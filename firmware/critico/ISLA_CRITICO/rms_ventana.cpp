/*
 * =============================================================================
 *  rms_ventana.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del RMS de medio ciclo. Ver rms_ventana.h.
 * =============================================================================
 */

#include "rms_ventana.h"

#include <util/atomic.h>

// --------------------------- el anillo --------------------------------------

volatile uint16_t rms_anillo[RMS_ANILLO];
volatile uint8_t  rms_w, rms_r;
volatile uint16_t rms_anillo_lleno;
volatile uint16_t rms_fronteras_perdidas;
volatile uint8_t  rms_cuenta_inv;

// ------------------- lo que el lazo acumula (no volátil) --------------------
//
// Nada de esto lo toca una interrupción: el lazo es el único que suma.

struct Acum { uint32_t sumaCuad; uint16_t suma; uint8_t n; };

static Acum    vivo[2];      // la ventana que se está llenando
static Acum    anterior[2];  // la ventana previa, para formar el par
static uint16_t resultado[2], bias[2];
static uint8_t  muestrasPar[2];
static bool     hayNuevo[2];
static uint16_t desbordesRed;

/*  ⚠️ VENTANAS SUCIAS — el arreglo correcto, después de intentar el otro.
 *
 *  Primero agrandé el anillo y le puse drenajes para NO perder eventos. Bajó
 *  de 79 a 15 por segundo, y eso no alcanza: los 15 que quedan corrompen una
 *  ventana por reporte, que se veía como picos de σ de hasta 11 cuentas y el
 *  RMS cayendo de 227.0 a 225.9.
 *
 *  Una ventana a la que le faltan muestras **no es una medición**. Así que en
 *  vez de pelear por no perder, se detecta la pérdida y se descarta la
 *  ventana. El par abarca dos ventanas, así que se descartan dos.
 */
static uint8_t  sucio[2];
static uint16_t descartadas;
static uint16_t llenoPrev;

// =============================== CUENTAS =====================================

/*  Raíz cuadrada entera por Newton, con semilla por bits. Converge en 5 o 6
 *  vueltas para los valores que nos tocan, y cada vuelta es una división de
 *  32 bits: ~600 ciclos en total, 240 veces por segundo en el LAZO.
 */
uint16_t rms_isqrt32(uint32_t v)
{
  if (v == 0) return 0;

  uint32_t x = v, semilla = 1;
  while (x > 3) { x >>= 2; semilla <<= 1; }

  uint32_t r = semilla;
  for (uint8_t k = 0; k < 8; k++) {
    const uint32_t sig = (r + v / r) >> 1;
    if (sig == r || sig == r + 1) { r = sig; break; }
    r = sig;
  }
  while (r * r > v) r--;          // Newton puede quedar uno arriba
  return (uint16_t)r;
}

/*  Cierra una ventana: la empareja con la anterior para cubrir un ciclo
 *  completo y calcula el RMS verdadero.
 */
static void cerrar(uint8_t i)
{
  const uint8_t nPrev = anterior[i].n;

  const uint32_t scTot = vivo[i].sumaCuad + anterior[i].sumaCuad;
  const uint32_t sTot  = (uint32_t)vivo[i].suma + anterior[i].suma;
  const uint16_t nTot  = (uint16_t)vivo[i].n + nPrev;

  anterior[i] = vivo[i];
  vivo[i].sumaCuad = 0; vivo[i].suma = 0; vivo[i].n = 0;

  if (nPrev == 0 || nTot < 8) return;   // la primera ventana no tiene par

  /*  Si se perdieron eventos, esta ventana y la anterior están incompletas.
   *  No se publica: se cuenta y se sigue.
   */
  if (sucio[i]) { sucio[i]--; descartadas++; return; }

  /*  La forma escalada, y no es estilo: dividir primero trunca ⟨x⟩ hasta en
   *  una cuenta, y al elevarla al cuadrado el error se multiplica por
   *  2⟨x⟩ ≈ 1 022. Son 0.4 % en el RMS = medio volt a 127 V, contra un
   *  criterio de ±0.3 V.
   *
   *      varianza = (N·Σx² − (Σx)²) / N²
   *
   *  En 64 bits: con 50 muestras N·Σx² llega a 2.6e9 y (Σx)² a 2.6e9. Las dos
   *  cabrían en 32 bits por los pelos; en 64 no hay que pensarlo.
   */
  const uint64_t numerador = (uint64_t)nTot * scTot - (uint64_t)sTot * sTot;
  const uint32_t n2        = (uint32_t)nTot * nTot;

  resultado[i]   = rms_isqrt32((uint32_t)(numerador / n2));
  bias[i]        = (uint16_t)(sTot / nTot);   // sobre un ciclo, ⟨x⟩ ES el bias
  muestrasPar[i] = (uint8_t)nTot;
  hayNuevo[i]    = true;
}

// ============================ EL DRENAJE =====================================

void rms_atender(void)
{
  /*  `rms_w` lo mueven dos interrupciones distintas, así que se lee una vez,
   *  y de un byte: en el AVR eso es atómico y no hace falta apagar nada
   *  (DC-026 — el `cli()` más barato es el que no existe).
   */
  /*  ¿Se perdió algún evento desde la última vuelta? Entonces las ventanas
   *  que están abiertas en los dos canales quedaron incompletas.
   */
  const uint16_t lleno = rms_anillo_lleno;
  if (lleno != llenoPrev) {
    llenoPrev = lleno;
    sucio[RMS_INV] = 2;    // la que está abierta y la que forma el par
    sucio[RMS_RED] = 2;
  }

  uint8_t w = rms_w;

  while (rms_r != w) {
    const uint16_t ev = rms_anillo[rms_r & RMS_ANILLO_MSK];
    rms_r++;

    const uint8_t i = (ev & RMS_EV_CANAL) ? RMS_RED : RMS_INV;

    if (ev & RMS_EV_FRONTERA) {
      cerrar(i);
      continue;
    }

    const uint16_t cuenta = ev & 0x03FFu;
    vivo[i].suma     += cuenta;
    vivo[i].sumaCuad += (uint32_t)cuenta * cuenta;

    /*  Tope de seguridad: si el comparador no está dando cruces, la ventana de
     *  la red crecería sin freno hasta desbordar los acumuladores.
     */
    if (++vivo[i].n >= RMS_TOPE_RED) { desbordesRed++; cerrar(i); }
  }
}

// ========================== FUNCIONES PÚBLICAS ===============================

void rms_preparar(void) { rms_reiniciar(); }

void rms_reiniciar(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    rms_w = rms_r = 0;
    rms_anillo_lleno = 0;
    rms_cuenta_inv = 0;
  }
  for (uint8_t i = 0; i < 2; i++) {
    vivo[i].sumaCuad = 0;     vivo[i].suma = 0;     vivo[i].n = 0;
    anterior[i].sumaCuad = 0; anterior[i].suma = 0; anterior[i].n = 0;
    hayNuevo[i] = false;
  }
  desbordesRed = 0;

  /*  ⚠️ `descartadas`, `llenoPrev` y `sucio` NO se borran aquí, y es a
   *  propósito. En la v7 sí se borraban, y el resultado fue que
   *  `descartadas` imprimía 0 en todos los reportes, funcionara o no el
   *  descarte: el contador se imprime en la línea 2, los descartes ocurren
   *  8.3 ms después —cuando cierra la ventana— y el reinicio del final del
   *  reporte los borraba antes de que nadie los viera.
   *
   *  Un contador que se reinicia antes de poder contar no mide nada.
   */
}

bool rms_recoger(uint8_t i, uint16_t *rmsCuentas, uint16_t *cd, uint8_t *n)
{
  if (i > RMS_RED || !hayNuevo[i]) return false;
  hayNuevo[i] = false;

  if (rmsCuentas) *rmsCuentas = resultado[i];
  if (cd)         *cd = bias[i];
  if (n)          *n  = muestrasPar[i];
  return true;
}

uint16_t rms_desbordes_red(void)  { return desbordesRed; }
uint16_t rms_descartadas(void)    { return descartadas; }

uint16_t rms_fronteras_perdidas_n(void)
{
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rms_fronteras_perdidas; }
  return v;
}

uint16_t rms_anillo_perdidos(void)
{
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rms_anillo_lleno; }
  return v;
}
