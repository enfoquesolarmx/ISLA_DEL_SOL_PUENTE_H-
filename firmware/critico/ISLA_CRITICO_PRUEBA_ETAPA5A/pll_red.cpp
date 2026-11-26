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

/*  LA LEY DE FRENADO, Y SALE DE LA RAMPA QUE PIDIO EL ARQUITECTO
 *
 *  Los dos intentos anteriores fallaron por la misma razon de fondo: trataban
 *  el lazo como si la frecuencia se pudiera poner donde uno quiera. No se
 *  puede -- la rampa limita cuanto se mueve POR CICLO, y eso convierte el
 *  problema en otro: no es un lazo de fase, es **frenar algo que tiene inercia
 *  para que se detenga justo en cero**.
 *
 *  Planteado asi se resuelve solo. Con `e` el error de fase en decimas y
 *  `v = de/ciclo`:
 *
 *      de/ciclo = 60 (f_nuestra - f_red) = 60 (60.060 - 0.18 c - f_red)
 *
 *  o sea  v = (sesgo de la red) - 10.8 c,  con c la correccion en cuentas de
 *  TOP. Y la rampa limita  dc/ciclo <= 0.1848 cuentas, o sea
 *
 *      A = 10.8 x 0.1848 = 2.0 decimas por ciclo al cuadrado
 *
 *  La aceleracion maxima NO es un numero que yo escogi: es la rampa de 2 s,
 *  traducida. Y con una aceleracion maxima conocida, la velocidad con la que
 *  uno puede ir y todavia alcanzar a parar en cero es la parabola de frenado
 *  de toda la vida:
 *
 *      |v| = raiz(2 A |e|)      ->      c = signo(e) x 0.1852 x raiz(|e|)
 *
 *  En x256 eso es  signo(e) x 47 x raiz(|e|),  y 47 resulta ser el paso de la
 *  rampa. No es casualidad: los dos salen del mismo limite.
 *
 *  LO QUE ESTO ARREGLA, QUE NO ES POCO:
 *
 *    - No hay modos. Se acabaron busqueda, banda de captura y la entrega
 *      entre las dos, que es donde el lazo se desbarrancaba.
 *    - No hay windup. No hay integrador en la ley de adquisicion.
 *    - Es de tiempo minimo. Nada que respete la rampa puede llegar antes.
 *    - Nunca pide mas de lo que puede dar: con 180 grados pide 1.41 Hz, que
 *      cabe en el rango de +-2 Hz sin recortarse.
 *
 *  Tiempos: 180 grados en ~84 ciclos (1.4 s), 30 grados en ~24 ciclos (0.4 s).
 *
 *  LA FRECUENCIA NO SE DESCUBRE INTEGRANDO: YA LA MEDIMOS.
 *
 *  La parabola lleva el error a cero pero no sabe nada del sesgo de la fuente
 *  -- los 48 mHz que nos separan de la red, o los 800 de un generador viejo.
 *  El primer intento dejaba que un integrador lo descubriera, y la simulacion
 *  mostro el hueco: con el integrador encerrado en +-5 grados y una fuente a
 *  0.8 Hz, el error se queda clavado en 62 grados PARA SIEMPRE, porque nunca
 *  entra a la ventana donde el integrador vive.
 *
 *  Pero no hace falta descubrir nada: `redMhz` ya trae la deriva medida. El
 *  corrimiento que iguala las dos frecuencias es directo,
 *
 *      ff = -redMhz x 1419 / 1000     (cuentas de TOP x256)
 *
 *  y la parabola trabaja encima de el. Eso es prealimentacion, y es mejor que
 *  un integrador por la razon de siempre: no tiene que esperar a que el error
 *  se acumule para enterarse.
 *
 *  EL INTEGRADOR SE QUEDA, PERO SOLO COMO AFINADOR. `redMhz` tiene 14 mHz de
 *  cuantizacion y la media de ocho deja unos 5 mHz de residuo; eso lo limpia
 *  un integrador chiquito que corre dentro de +-5 grados, donde la parabola
 *  ya casi no manda y no hay con que pelearse.
 *
 *  Simulado con el jitter medido (0.55 grados) y la cuantizacion real: engancha
 *  entre 0.8 y 2.3 s y se sostiene dentro de +-2.1 grados, con la red a 48 mHz
 *  y con generadores a -0.86 y +1.44 Hz.
 *
 *  Y UNA ZONA MUERTA DE 0.3 GRADOS. La pendiente de la raiz es infinita en
 *  cero, asi que sin ella la ley estaria pidiendo +-33 mHz alternados para
 *  siempre. Con 0.3 grados el integrador sostiene y la parabola se calla.
 */
#define PLL_PARAB_X256      47L   /* signo(e) x 47 x raiz(|e|)               */
#define PLL_INTEGRA_X10      50   /* el integrador solo adentro de +-5.0     */
#define PLL_MUERTA_X10        3   /* 0.3 grados: abajo de eso, no se empuja  */

/*  Raiz entera, en el lazo. |e| <= 1800, asi que el resultado cabe en 6 bits.
 */
static uint8_t raiz_entera(uint16_t v)
{
  uint16_t r = 0, b = 1u << 10;   /* 1024: raiz(1800) < 43, basta  */
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
    else            { r >>= 1; }
    b >>= 2;
  }
  return (uint8_t)r;
}

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
static uint8_t   mudos;             /* ciclos seguidos sin sello nuevo de red  */
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
  mudos = 0;
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
        /*  Promediada para mostrar y para la compuerta: una sola medida de
         *  periodo tiene 14 mHz de cuantizacion y el jitter del cruce la
         *  esparce +-180 mHz. En el reporte se veia saltar de -8 a -195
         *  cuando la deriva real era -48. Con esta media la compuerta de
         *  +-2 Hz tampoco se pone nerviosa.
         */
        redMhz += (dmhz - redMhz) / 8;
        dmhz = redMhz;
      }
    }
    tDirAnt = t;
    hayPeriodo = true;
    nDirVisto = nd;
    mudos = 0;
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

  /* ---- 0) LA RED SIGUE AHI? SE PREGUNTA CADA CICLO, NO UNA VEZ ----------
   *
   *  !! Lo encontro el banco: al cortar la red el lazo dejaba de corregir pero
   *  seguia reportando ENGANCHANDO para siempre. Un estado que miente es peor
   *  que no tenerlo, porque quien decida cerrar el relevador le va a preguntar
   *  a `pll_estado()`.
   *
   *  Y el guardia NO puede ser solo `cmp_red_presente()`. Con la red cortada
   *  la entrada queda al aire y el comparador puede castanetear con el ruido:
   *  `cmp_red_presente()` diria que si hay red, y el lazo se engancharia AL
   *  RUIDO. Se exige que **el sello calificado siga avanzando**, que es el
   *  dato que el lazo de veras usa, y ademas que el periodo sea plausible.
   *  Tres ciclos sin sello nuevo (50 ms) y se suelta.
   */
  if (mudos < 255) mudos++;

  if (mudos > 3 || !cmp_red_presente()) {
    estado      = PLL_SIN_RED;
    dentro      = 0;
    fuera       = 0;
    msEnganche  = 0;          /* el enganche anterior ya no vale nada */
    msInicio    = millis();   /* y el cronometro arranca de nuevo cuando vuelva */
    corrX256    = 0;
    deseadoX256 = 0;          /* de vuelta al cristal, y por la rampa */
  }
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
      const int16_t ae = (e < 0) ? (int16_t)-e : e;

      /* ---- 3) LA PARABOLA DE FRENADO + el integrador chiquito -------- */

      int32_t par = 0;
      if (ae > PLL_MUERTA_X10) {
        par = PLL_PARAB_X256 * (int32_t)raiz_entera((uint16_t)ae);
        if (e < 0) par = -par;
      }

      /*  El integrador solo cerca de cero, y con anti-windup: si la salida
       *  esta pegada al limitador, integrar es acumular una deuda que luego
       *  se paga con sobretiro. Esa fue la leccion de la corrida pasada.
       */
      /*  Prealimentacion: la frecuencia medida, no adivinada. */
      const int32_t ff = -(redMhz * PLL_TOP_X256_POR_HZ) / 1000L;

      deseadoX256 = ff + corrX256 + par;
      if      (deseadoX256 >  PLL_TOPE_X256) deseadoX256 =  PLL_TOPE_X256;
      else if (deseadoX256 < -PLL_TOPE_X256) deseadoX256 = -PLL_TOPE_X256;

      if (ae <= PLL_INTEGRA_X10) {
        const int32_t sobra = deseadoX256 - aplicadoX256;
        if (sobra <= PLL_PASO_MAX_X256 && sobra >= -PLL_PASO_MAX_X256) {
          corrX256 += (int32_t)e / 2;
          if      (corrX256 >  PLL_TOPE_X256) corrX256 =  PLL_TOPE_X256;
          else if (corrX256 < -PLL_TOPE_X256) corrX256 = -PLL_TOPE_X256;
        }
      }

      /* ---- 4) engancho? ---------------------------------------------- */

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
