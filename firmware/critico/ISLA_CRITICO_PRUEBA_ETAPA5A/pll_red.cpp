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
 *
 *  !! La rampa limita el VALOR APLICADO, no el paso del integrador. Es la
 *  lectura correcta de "no forzar": lo que no debe saltar es la frecuencia que
 *  ve la casa. Y de paso el mismo limitador sirve para soltar, que tambien
 *  tiene que ser por rampa.
 */
#define PLL_PASO_MAX_X256  \
  ((2L * PLL_TOPE_X256 * 1000L) / ((long)PLL_RAMPA_MS * 60L))

/*  LAS DOS GANANCIAS, Y POR QUE HACEN FALTA LAS DOS.
 *
 *  El primer intento tenia solo el acumulador:  corr += Ki * error.  Lo llame
 *  "proporcional" en los comentarios y estaba mal nombrado y mal planteado:
 *  si la salida es la INTEGRAL del error de fase, el lazo es un oscilador sin
 *  amortiguamiento. El banco lo mostro -- el error no divergia ni convergia,
 *  se paseaba.
 *
 *  La cuenta, con una cuenta de TOP = 0.18 Hz y 60 ciclos por segundo:
 *
 *      a = 0.18 Hz/cuenta x 60 decimas-de-grado/ciclo por Hz = 10.8
 *      Ki = 1419/(2000 x 256) = 0.002771 cuentas por decima de grado
 *      wn = raiz(a x Ki) = 0.173 rad/ciclo  ->  36 ciclos, 0.6 s
 *
 *  Sin termino proporcional, zeta = 0: ringing eterno, y el criterio de
 *  "medio segundo dentro de banda" nunca se cumple. Con
 *
 *      Kp = 2 x zeta x wn / a = 2 x 0.7 x 0.173 / 10.8 = 0.0224 cuentas/decima
 *         = 5.75 en unidades x256   ->   e x 23 / 4
 *
 *  sale zeta = 0.70, que es el amortiguamiento que uno quiere: entra rapido y
 *  no rebota. El integrador se queda para matar el error de frecuencia en
 *  regimen; el proporcional es el que amortigua.
 */
#define PLL_KP_NUM   23L
#define PLL_KP_DEN    4L

static PllEstado estado;
static bool      pedido;
static int32_t   corrX256;          /* correccion acumulada, cuentas de TOP x256 */
static int16_t   errGradosX10;
static uint8_t   dentro;            /* ciclos seguidos dentro de banda       */
static uint8_t   nCruceVisto;
static uint8_t   nDirVisto;         /* para medir el periodo de la red        */
static uint16_t  tDirAnt;
static bool      hayPeriodo;
static uint8_t   fuera;             /* ciclos seguidos fuera de rango         */
static int32_t   aplicadoX256;      /* lo que de veras se escribio            */
static int32_t   redMhz;            /* ultima medida valida, latcheada        */
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
  nDirVisto = 0;
  hayPeriodo = false;
  fuera = 0;
  aplicadoX256 = 0;
  redMhz = 0;
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

/*  La deriva de la red contra nuestros 60.060 Hz, en mHz. Se publica desde
 *  aqui porque aqui es donde se mide bien: una vez por ciclo, de dos sellos
 *  consecutivos. El reporte no puede medirla por su cuenta -- entre dos
 *  impresiones pasa un segundo y la resta de sellos envuelve.
 */
int32_t pll_red_mhz(void) { return redMhz; }

int16_t pll_correccion_mhz(void)
{
  return (int16_t)((-aplicadoX256 * 1000L) / PLL_TOP_X256_POR_HZ);
}

/* --------------------------------------------------------------- el lazo */

void pll_atender(void)
{
  /* ---- la frecuencia de la red, medida POR NOSOTROS ---------------------
   *
   *  !! Esto antes llamaba a `cmp_frecuencia_mhz()`, y era un error grave que
   *  el banco destapo: ese promedio se pone en cero en cada `cmp_reiniciar()`,
   *  o sea cada vez que el reporte imprime. Mientras se vuelve a llenar
   *  devuelve 0, el PLL leia "la red esta a 0 Hz", declaraba fuera de rango y
   *  **se desandaba un pedazo una vez por segundo**. El lazo no podia
   *  enganchar aunque todo lo demas estuviera bien.
   *
   *  El periodo sale de los dos ultimos sellos del MISMO sentido, que es un
   *  dato nuestro y nadie lo reinicia. Una cuenta de 4 us sobre 16.6 ms son
   *  14 mHz de resolucion: de sobra para una compuerta de +-2 Hz.
   */
  const uint8_t nd = cmp_n_cruce_dir();
  int32_t dmhz = 0;
  bool    hayMedida = false;
  if (nd != nDirVisto) {
    const uint16_t t = cmp_t_cruce_dir();
    if (hayPeriodo) {
      const uint16_t per = (uint16_t)(t - tDirAnt);
      /*  Un periodo plausible y nada mas: si falto un cruce, el numero mide
       *  que falto un cruce, no la frecuencia.
       */
      if (per > 3800u && per < 4600u) {
        dmhz = (int32_t)(250000000L / (int32_t)per) - 60060L;
        hayMedida = true;
        redMhz = dmhz;
      }
    }
    tDirAnt = t;
    hayPeriodo = true;
    nDirVisto = nd;
  }

  /*  Una vez por ciclo del inversor: el contador de cruces lo mueve la ISR
   *  del SPWM en la rama que ya existia.
   */
  const uint8_t n = spwm_n_cruce();
  if (n == nCruceVisto) return;
  nCruceVisto = n;

  if (!pedido) {
    estado = PLL_LIBRE;
    /*  Soltar tambien es por rampa. Un escalon al soltar es el mismo escalon
     *  que la rampa existe para evitar, nada mas que al reves.
     */
    if (aplicadoX256 != 0) {
      if      (aplicadoX256 >  PLL_PASO_MAX_X256) aplicadoX256 -= PLL_PASO_MAX_X256;
      else if (aplicadoX256 < -PLL_PASO_MAX_X256) aplicadoX256 += PLL_PASO_MAX_X256;
      else                                        aplicadoX256  = 0;
      corrX256 = aplicadoX256;
      const int32_t t0 = ((int32_t)TOP_CUENTAS << 8) + aplicadoX256;
      spwm_top_fijar((uint16_t)(t0 >> 8), (uint8_t)(t0 & 0xFF));
    }
    return;
  }

  int32_t deseadoX256 = corrX256;

  if (!cmp_red_presente()) { estado = PLL_ENGANCHANDO; dentro = 0; deseadoX256 = 0; }
  else {

    /* ---- 1) la compuerta de rango, con memoria --------------------------
     *
     *  Hacen falta varios ciclos seguidos afuera para soltar. Un semiperiodo
     *  con jitter de 36 us vale 130 mHz de error en UNA medida; soltar por una
     *  sola seria soltar por ruido. Ocho seguidos no son ruido.
     */
    if (hayMedida) {
      if (dmhz > PLL_RANGO_MHZ || dmhz < -PLL_RANGO_MHZ) { if (fuera < 8) fuera++; }
      else                                                fuera = 0;
    }

    if (fuera >= 8) {
      estado = PLL_FUERA_RANGO;
      dentro = 0;
      deseadoX256 = 0;          /* de vuelta al cristal, y por la rampa */
      corrX256    = 0;
    } else {

      /* ---- 2) el error de fase, de los dos sellos del MISMO sentido ----
       *
       *  La resta la hace `reloj_fase_x10()`, que vive junto al reloj: una
       *  sola pieza para el lazo y para el reporte. El primer intento restaba
       *  en uint16 y reducia modulo 4162 -- mal, porque 65536 no es multiplo
       *  de 4162 y la mitad de las diferencias son negativas. Ver reloj_4us.h.
       */
      const int16_t e = reloj_fase_x10(cmp_t_cruce_dir(), spwm_t_cruce());
      errGradosX10 = e;

      /* ---- 3) integral + proporcional ---------------------------------- */

      corrX256 += ((int32_t)e * PLL_TOP_X256_POR_HZ) / 2000L;
      if      (corrX256 >  PLL_TOPE_X256) corrX256 =  PLL_TOPE_X256;
      else if (corrX256 < -PLL_TOPE_X256) corrX256 = -PLL_TOPE_X256;

      deseadoX256 = corrX256 + (((int32_t)e * PLL_KP_NUM) / PLL_KP_DEN);
      if      (deseadoX256 >  PLL_TOPE_X256) deseadoX256 =  PLL_TOPE_X256;
      else if (deseadoX256 < -PLL_TOPE_X256) deseadoX256 = -PLL_TOPE_X256;

      /* ---- 4) engancho? ------------------------------------------------ */

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
  }

  /* ---- 5) LA RAMPA, sobre el valor aplicado ------------------------------ */

  {
    const int32_t falta = deseadoX256 - aplicadoX256;
    if      (falta >  PLL_PASO_MAX_X256) aplicadoX256 += PLL_PASO_MAX_X256;
    else if (falta < -PLL_PASO_MAX_X256) aplicadoX256 -= PLL_PASO_MAX_X256;
    else                                 aplicadoX256  = deseadoX256;
  }

  const int32_t topX256 = ((int32_t)TOP_CUENTAS << 8) + aplicadoX256;
  spwm_top_fijar((uint16_t)(topX256 >> 8), (uint8_t)(topX256 & 0xFF));

  /*  !! ACOPLAMIENTO CONOCIDO Y NO COMPENSADO AQUI, a proposito.
   *
   *  Al subir el TOP, el mismo `d` de la tabla vale menos ciclo de trabajo:
   *  la amplitud de salida baja en la misma proporcion. En el extremo del
   *  rango (TOP 344, o sea -2 Hz) son **-3.3 %**.
   *
   *  No se corrige aqui por dos razones:
   *
   *    - Compensar desde este modulo exigiria leer la amplitud, escalarla y
   *      volverla a escribir, y la siguiente vuelta leeria la ya escalada y
   *      la escalaria otra vez. Es una realimentacion sobre si misma.
   *
   *    - **El lazo de voltaje de salida lo va a corregir solo.** Ese lazo mide
   *      el RMS real en A0 y ajusta la amplitud; cualquier error de ganancia,
   *      venga de donde venga, le queda adentro.
   *
   *  En enganche normal son 0.1 %.
   */
}
