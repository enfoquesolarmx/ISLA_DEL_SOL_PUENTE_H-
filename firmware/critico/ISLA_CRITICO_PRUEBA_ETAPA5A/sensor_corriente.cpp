/* =============================================================================
 *  ISLA DEL SOL — sensor de corriente AC (SCT-013) en A2.  Ver el .h.
 *
 *  LA VENTANA SON 25 MUESTRAS, Y SON DOS CICLOS EXACTOS
 *
 *    Mismo truco que el canal de la red (DC-028): la ventana se cierra por
 *    cuenta de muestras y cae en un número entero de ciclos, así que no hace
 *    falta saber dónde está el cruce por cero.
 *
 *        6 006 disparos de ADC/s / 8 ranuras = 750.75 muestras/s en A2
 *        línea de 60.06 Hz                   = 12.5 muestras por ciclo
 *        25 muestras                         = 2.000 ciclos EXACTOS
 *
 *    ⚠️ Frágil igual que los otros: sale del mismo cristal, y cambiar el TOP
 *    de la portadora, el preescalador del ADC o el largo de la rotación lo
 *    rompe sin avisar.
 *
 *    Y por eso se acumula **dentro de la ISR**: el lazo se queda ciego durante
 *    los 2.6 ms de una transferencia de I2C, y un RMS al que le faltan
 *    muestras cuando hay bus no es un RMS, es un número que depende del
 *    tráfico (lección de DC-026).
 * =============================================================================
 */

#include <Arduino.h>
#include <util/atomic.h>

#include "adc_portadora.h"
#include "sensor_corriente.h"

#define SCT_MUESTRAS   25        // 2 ciclos exactos

/*  ⚠️ LA CALIBRACIÓN LLEVA AVCC ADENTRO (DC-029). El ADC mide la razón entre
 *  la entrada y su referencia, y la referencia es AVCC. Con los 4.76 V del
 *  USB en vez de los 5.00 del regulador, esta constante se corre un 5 %.
 *  Cuando se implemente la corrección por bandgap, este número sale de ahí.
 */
#define SCT_AVCC_MV  5000

/*  mA de cable por cuenta de ADC:
 *      1 V RMS de salida  =  SCT_FONDO_A ampere-vuelta  =  FONDO/VUELTAS A de cable
 *      1 cuenta           =  AVCC_mV / 1023  mV
 *  →   mA por cuenta      =  (AVCC_mV / 1023) × FONDO_A / VUELTAS
 *
 *  Se guarda ×100 para no perder resolución en entero.
 */
#define SCT_MA_POR_CUENTA_X100  \
  ((uint32_t)(((uint32_t)SCT_AVCC_MV * 100UL * SCT_FONDO_A) / (1023UL * SCT_VUELTAS)))

/* ------------------------------------------------------- estado, en la ISR */

static volatile uint32_t sumaCuad;      // Σx²  — cabe: 25 × 1023² = 2.6e7
static volatile uint16_t suma;          // Σx   — cabe: 25 × 1023  = 25 575
static volatile uint8_t  n;

static volatile uint32_t pendCuad;      // ventana cerrada, esperando al lazo
static volatile uint16_t pendSuma;
static volatile bool     pendiente;

static uint16_t rmsCuentas;             // resultado: lo escribe sólo el lazo
static uint16_t cdCuentas;              // nivel de CD: dice si hay pinza
static bool     hayVentana;

/* ----------------------------------------------------------------- utilería */

static uint16_t isqrt32(uint32_t v)
{
  uint32_t r = 0, b = 1UL << 30;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
    else            { r >>= 1; }
    b >>= 2;
  }
  return (uint16_t)r;
}

/* ------------------------------------------------ lo que corre en la ISR ---
 *
 *  Sólo se llama en la ranura de la pinza: el filtro de ranura es un `inline`
 *  del encabezado, para que las otras 7 de 8 no paguen el prólogo.
 */
void sct_acumular(uint16_t cuenta)
{
  sumaCuad += (uint32_t)cuenta * cuenta;
  suma     += cuenta;

  if (++n < SCT_MUESTRAS) return;

  /*  ⚠️ AQUI NO SE CALCULA NADA. La ventana se cierra copiando las sumas y
   *  levantando una bandera; la raiz y las divisiones las hace el lazo en
   *  `sct_atender()`.
   *
   *  La primera version de este archivo hacia la cuenta completa aqui adentro
   *  —una raiz entera iterativa y dos divisiones de 32 bits, del orden de
   *  cientos de ciclos— dentro de una ISR que comparte un periodo de 666
   *  ciclos con otras tres. Es el mismo error que costo 14 pasos de onda en la
   *  etapa 4 cuando la estadistica del jitter se metio en la ISR del
   *  comparador: **la medicion perturbando lo que mide**.
   */
  pendCuad = sumaCuad;
  pendSuma = suma;
  pendiente = true;

  sumaCuad = 0;
  suma     = 0;
  n        = 0;
}

/* --------------------------------------------- lo que corre en el lazo --- */

void sct_atender(void)
{
  uint32_t sc;
  uint16_t sm;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (!pendiente) return;
    sc = pendCuad;
    sm = pendSuma;
    pendiente = false;
  }

  /*  Forma escalada, sin dividir hasta el final: promediar y luego restar
   *  pierde hasta una cuenta al truncar, y al elevarla al cuadrado el error
   *  se multiplica por 2×511 (la leccion de DC-028).
   */
  const uint32_t num = (uint32_t)SCT_MUESTRAS * sc - (uint32_t)sm * sm;
  const uint16_t r   = isqrt32(num / ((uint32_t)SCT_MUESTRAS * SCT_MUESTRAS));
  const uint16_t cd  = (uint16_t)(sm / SCT_MUESTRAS);

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    rmsCuentas = r;
    cdCuentas  = cd;
    hayVentana = true;
  }
}

/* -------------------------------------------------------------- públicas --- */

void sct_preparar(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    sumaCuad = 0; suma = 0; n = 0; pendiente = false;
  }
  rmsCuentas = 0; cdCuentas = 0; hayVentana = false;
  {
  }
}

bool sct_pinza_presente(void)
{
  uint16_t cd;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { cd = cdCuentas; }

  /*  Sin ninguna ventana cerrada todavía no se puede afirmar nada; se reporta
   *  "no hay pinza" porque es el lado seguro: mejor una alarma de más que una
   *  corriente inventada.
   */
  if (!hayVentana) return false;

  const int16_t d = (int16_t)cd - (int16_t)SCT_CD_ESPERADO;
  return (d >= -(int16_t)SCT_CD_TOLERANCIA) && (d <= (int16_t)SCT_CD_TOLERANCIA);
}

uint16_t sct_cd_cuentas(void)
{
  uint16_t cd;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { cd = cdCuentas; }
  return cd;
}

uint16_t sct_corriente_ma(void)
{
  if (!sct_pinza_presente()) return 0;

  uint16_t r;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { r = rmsCuentas; }

  /*  Abajo del piso de ruido no se reporta una corriente: se reporta cero.
   *  El RMS de cero NO es cero —converge al ruido propio del canal—, así que
   *  dejarlo pasar sería inventar una carga que no existe.
   */
  if (r <= SCT_PISO_CUENTAS) return 0;

  return (uint16_t)(((uint32_t)r * SCT_MA_POR_CUENTA_X100) / 100UL);
}
