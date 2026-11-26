/* =============================================================================
 *  ISLA DEL SOL — PLL de red.  Ver el .h para el porque.
 *
 *  TODO ESTE MODULO CORRE EN EL LAZO, NINGUNA LINEA EN UNA ISR.
 *
 *  Es deliberado y es la leccion mas cara de la Parte C: la estadistica del
 *  jitter metida en la ISR del comparador costo 117 ciclos y tiro 14 pasos de
 *  onda. Aqui el lazo lee dos sellos de tiempo que las ISR ya dejaron puestos
 *  y hace toda la aritmetica afuera.
 *
 *  Que el lazo se quede ciego 2.6 ms durante una transferencia de I2C no
 *  estorba: el enganche se mueve en segundos, no en milisegundos.
 * =============================================================================
 */

#include <Arduino.h>

#include "config_portadora.h"
#include "spwm.h"
#include "comparador_red.h"
#include "reloj_4us.h"
#include "pll_red.h"

/*  Cuentas de TOP por hertz:  dTOP/df = TOP/f = 333 / 60.06 = 5.544
 *  Se guarda x256 para trabajar en entero.
 */
#define PLL_TOP_X256_POR_HZ   1419L     /* 5.544 x 256 */

/*  Rango en cuentas de TOP x256, a partir de +-2 Hz. */
#define PLL_TOPE_X256   ((PLL_RANGO_MHZ * PLL_TOP_X256_POR_HZ) / 1000L)

/*  Rampa: recorrer el rango completo (de un tope al otro) en PLL_RAMPA_MS,
 *  actualizando una vez por ciclo de salida (~60/s).
 */
#define PLL_PASO_MAX_X256  \
  ((2L * PLL_TOPE_X256 * 1000L) / ((long)PLL_RAMPA_MS * 60L))

static PllEstado estado;
static bool      pedido;
static int32_t   corrX256;          /* correccion acumulada, cuentas de TOP x256 */
static int16_t   errGradosX10;
static uint8_t   dentro;            /* ciclos seguidos dentro de banda       */
static uint8_t   nCruceVisto;
static uint32_t  msInicio;
static uint16_t  msEnganche;

void pll_preparar(void)
{
  estado = PLL_LIBRE;
  pedido = false;
  corrX256 = 0;
  errGradosX10 = 0;
  dentro = 0;
  nCruceVisto = 0;
  msEnganche = 0;
  spwm_top_fijar(TOP_CUENTAS, 0);
}

void pll_pedir_enganche(bool si)
{
  if (si == pedido) return;
  pedido = si;
  if (si) { msInicio = millis(); dentro = 0; estado = PLL_ENGANCHANDO; }
  else    { estado = PLL_LIBRE; }
}

PllEstado pll_estado(void)            { return estado; }
int16_t   pll_error_grados_x10(void)  { return errGradosX10; }
uint16_t  pll_ms_para_enganchar(void) { return msEnganche; }

int16_t pll_correccion_mhz(void)
{
  return (int16_t)((-corrX256 * 1000L) / PLL_TOP_X256_POR_HZ);
}

/* --------------------------------------------------------------- el lazo */

void pll_atender(void)
{
  /*  Una vez por ciclo del inversor: el contador de cruces lo mueve la ISR
   *  del SPWM en la rama que ya existia.
   */
  const uint8_t n = spwm_n_cruce();
  if (n == nCruceVisto) return;
  nCruceVisto = n;

  if (!pedido) { estado = PLL_LIBRE; return; }

  /* ---- 1) ¿la fuente esta dentro de rango? ------------------------------ */

  if (!cmp_red_presente()) { estado = PLL_ENGANCHANDO; dentro = 0; return; }

  const int32_t dmhz = (int32_t)cmp_frecuencia_mhz() - 60060L;
  if (dmhz > PLL_RANGO_MHZ || dmhz < -PLL_RANGO_MHZ) {
    /*  Fuera de +-2 Hz: se suelta y se vuelve al cristal POR LA RAMPA, no de
     *  golpe. Soltar de golpe seria el mismo escalon que la rampa existe para
     *  evitar, nada mas que al reves.
     */
    estado = PLL_FUERA_RANGO;
    dentro = 0;
    if      (corrX256 >  PLL_PASO_MAX_X256) corrX256 -= PLL_PASO_MAX_X256;
    else if (corrX256 < -PLL_PASO_MAX_X256) corrX256 += PLL_PASO_MAX_X256;
    else                                    corrX256  = 0;
  } else {

    /* ---- 2) el error de fase, de los dos sellos ------------------------- */

    uint16_t d = (uint16_t)(cmp_t_ultimo_cruce() - spwm_t_cruce());
    while (d >= 4163u) d -= 4163u;              /* un ciclo = 4162.5 cuentas */

    /*  Se lleva a -180..+180 grados x10. 4163 cuentas = 3600 decimas.      */
    int16_t e = (int16_t)(((uint32_t)d * 3600UL) / 4163UL);
    if (e > 1800) e -= 3600;
    errGradosX10 = e;

    /* ---- 3) correccion, con la rampa como limite ------------------------ */

    /*  Proporcional simple sobre el error de fase. El termino integral no
     *  hace falta todavia: el error de frecuencia es un escalon lento y la
     *  rampa ya domina el transitorio. Cuando haya banco se decide con datos.
     */
    int32_t paso = ((int32_t)e * PLL_TOP_X256_POR_HZ) / 2000L;

    if      (paso >  PLL_PASO_MAX_X256) paso =  PLL_PASO_MAX_X256;
    else if (paso < -PLL_PASO_MAX_X256) paso = -PLL_PASO_MAX_X256;

    corrX256 += paso;
    if      (corrX256 >  PLL_TOPE_X256) corrX256 =  PLL_TOPE_X256;
    else if (corrX256 < -PLL_TOPE_X256) corrX256 = -PLL_TOPE_X256;

    /* ---- 4) ¿engancho? -------------------------------------------------- */

    const int16_t ae = (e < 0) ? (int16_t)-e : e;
    if (ae <= PLL_BANDA_GRADOS_X10) {
      if (dentro < PLL_SOSTENIDO_CICLOS) {
        if (++dentro == PLL_SOSTENIDO_CICLOS) {
          estado = PLL_ENGANCHADO;
          msEnganche = (uint16_t)(millis() - msInicio);
        }
      }
    } else {
      dentro = 0;
      estado = PLL_ENGANCHANDO;
    }
  }

  /* ---- 5) aplicar: TOP entero + fraccion, y compensar la amplitud ------- */

  const int32_t topX256 = ((int32_t)TOP_CUENTAS << 8) + corrX256;
  const uint16_t ent    = (uint16_t)(topX256 >> 8);
  const uint8_t  frac   = (uint8_t)(topX256 & 0xFF);
  spwm_top_fijar(ent, frac);

  /*  !! ACOPLAMIENTO CONOCIDO Y NO COMPENSADO AQUI, a proposito.
   *
   *  Al subir el TOP, el mismo `d` de la tabla vale menos ciclo de trabajo:
   *  la amplitud de salida baja en la misma proporcion. En el extremo del
   *  rango (TOP 344, o sea -2 Hz) son **-3.3 %**.
   *
   *  No se corrige aqui por dos razones:
   *
   *    · Compensar desde este modulo exigiria leer la amplitud, escalarla y
   *      volverla a escribir — y la siguiente vuelta leeria la ya escalada y
   *      la escalaria otra vez. Es una realimentacion sobre si misma.
   *
   *    · **El lazo de voltaje de salida lo va a corregir solo.** Ese lazo mide
   *      el RMS real en A0 y ajusta la amplitud; cualquier error de ganancia,
   *      venga de donde venga, le queda adentro. Corregirlo dos veces es
   *      pelearse con el.
   *
   *  En enganche normal esto ni se nota: la red a 60.00 contra nuestros 60.06
   *  son 0.33 cuentas de TOP, un 0.1 %. El 3.3 % solo aparece con un generador
   *  dos hertz abajo, y ahi hay cosas peores de que preocuparse.
   */
}
