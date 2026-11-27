/*
 * =============================================================================
 *  comparador_red.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del cruce por cero de la red. Ver comparador_red.h.
 * -----------------------------------------------------------------------------
 *  ⚠️ LO QUE ESTA ISR **NO** HACE, Y ES LA LECCIÓN DE LA CORRIDA DEL 24-nov
 *
 *  Tuvo adentro la estadística del jitter —una multiplicación de 16×16 y dos
 *  sumas de 32 bits— y costaba **117 ciclos**. Con eso esta ISR pasó de 216 a
 *  333 ciclos, y la suma de las tres ISR del sistema de 606 a 723 contra un
 *  periodo de portadora de 666 ciclos: **de 91 % a 109 %**. El banco perdió
 *  14 pasos de la onda en 33 segundos.
 *
 *  La medición perturbó al sistema que medía. Ahora aquí sólo pasa lo que no
 *  puede pasar en otro lado:
 *
 *      · fechar el cruce            — el instante sólo existe aquí
 *      · decidir si es válido       — para no fechar basura
 *      · encolar                    — diez ciclos
 *
 *  Todo lo demás —la σ, la frecuencia, el RMS de la red— lo hace el lazo, que
 *  no tiene plazo que cumplir.
 * =============================================================================
 */

#include "comparador_red.h"
#include "reloj_4us.h"
#include "spwm.h"
#include "rms_ventana.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/*  El contador de desbordes del Timer0 del núcleo de Arduino. No está en
 *  ningún encabezado público, pero tiene enlace externo y lleva quince años
 *  llamándose igual. Si algún día cambiara, esto **no compila** — falla
 *  ruidosamente, que es la forma correcta de fallar.
 */

// --- Enganche de medición (protocolo de banco, Parte C, etapa 4) -------------
#if defined(__has_include)
#  if __has_include("traza_cruce.h")
#    include "traza_cruce.h"
#  endif
#  if __has_include("traza_cmp.h")
#    include "traza_cmp.h"
#  endif
#endif
#ifndef CRUCE_TRAZA_TOGGLE
#define CRUCE_TRAZA_TOGGLE()
#endif
#ifndef CMP_TRAZA_ENTRA
#define CMP_TRAZA_ENTRA()
#endif
#ifndef CMP_TRAZA_SALE
#define CMP_TRAZA_SALE()
#endif

// ============================== ESTADO =======================================

static volatile uint16_t cruces, rechazados, huecos, rechDir;
static volatile uint16_t semiperiodo;    // el último aceptado, en µs
static volatile uint16_t tUltimo;        // garrapatas de 4 µs del último cruce
static volatile uint8_t  acoUlt;         // ACO del último cruce aceptado
static volatile uint32_t faseCruce;      // fase del cruce EN NUESTRO CICLO
static volatile uint32_t faseVuelta;     // cuentas de una vuelta, al momento
static volatile uint16_t tCruceDir;      // sello SOLO del sentido de referencia
static volatile uint8_t  nCruceDir;      // cambia una vez por ciclo, 60/s
static volatile uint8_t  sentido;        // alternancia por TIEMPO, no por ACO
static int8_t            voto;           // ACO como ancla lenta, no rapida

/*  Vencimiento de la red, en 32 bits. Ver `reloj_4us_32()` y el defecto que
 *  lo obligó: con el reloj de 16 bits, sin red, `cmp_red_presente()` volvía a
 *  decir «hay red» 50 ms de cada 262 ms. Estas tres viven SOLO en el lazo
 *  (`cmp_atender`), nunca en una ISR, así que no llevan `volatile`.
 */
static uint32_t tCruce32  = 0;     // sello de 32 bits del ultimo cruce
static uint16_t crucesVistos = 0;  // para detectar que hubo cruce nuevo
static bool     huboCruce = false; // antes del primero no hay nada que medir

/*  Histograma de a qué distancia del cruce aceptado llegan los flancos
 *  rechazados. Es la pregunta que decide el arreglo, y se contesta desde
 *  adentro — sin analizador y sin ventana ciega.
 *
 *      si caen en los primeros ~50 µs  → REPIQUE del cruce: el comparador
 *          alterna porque no tiene histéresis y la pendiente es de
 *          0.6 mV/µs. Se arregla con histéresis, no con filtro.
 *      si caen a cientos de µs o a ms  → RECRUCE por armónicos: la red está
 *          achatada cerca del cero y de verdad cruza más de una vez.
 *
 *  ⚠️ Medido el 24-nov: **todo abajo de 100 µs y el 97 % abajo de 50**. Es
 *  repique. Y el primer cajón es ciego mientras la ISR corre, así que el
 *  repique real es más rápido de lo que estos cajones pueden resolver.
 */
/*  Dos cajones nuevos, 4 ms y 6 ms, para VER lo que el blanqueo viejo dejaba
 *  pasar. Si ahi aparecen cuentas, eran cruces falsos del rizado de la fuente
 *  que antes se colaban y torcian la alternancia.
 */
#define CMP_CAJONES  9
static const uint16_t CMP_TOPE[CMP_CAJONES] = { 20, 50, 100, 200, 500, 1000, 2000, 4000, 6000 };
static volatile uint16_t cmp_histo[CMP_CAJONES];

// Anillo de semiperiodos: la ISR encola, el lazo saca la σ y la frecuencia.
#define CMP_SEMI_ANILLO  8
static volatile uint16_t semiAnillo[CMP_SEMI_ANILLO];
static volatile uint8_t  semiW;

// ================================ LA ISR =====================================

ISR(ANALOG_COMP_vect)
{
  CMP_TRAZA_ENTRA();

  /*  EL FECHADO, SIN micros().
   *
   *  `micros()` cuesta ~50 ciclos: lee el contador de desbordes de 32 bits, lo
   *  corrige si hay uno pendiente, y lo multiplica por 4 para dar
   *  microsegundos. Aquí no hace falta nada de eso.
   *
   *  Se guardan las **garrapatas crudas del Timer0**: un byte del contador de
   *  desbordes y `TCNT0`, que juntos dan 16 bits en unidades de 4 µs. Eso
   *  envuelve cada 262 ms, de sobra para un semiperiodo de 8.3 ms.
   *
   *  La corrección del desborde pendiente es la misma que hace `micros()` y
   *  hay que hacerla igual: si TOV0 está puesto, el contador se quedó atrás.
   */
  /*  Mismo reloj que usa el SPWM para sellar SU cruce: la resta de los dos
   *  sellos es la fase, y con la misma secuencia de instrucciones cualquier
   *  sesgo se cancela exacto. Ver reloj_4us.h.
   */
  const uint16_t t = reloj_4us();

  const uint16_t dt  = (uint16_t)((uint16_t)(t - tUltimo) << 2);   // a µs
  const uint8_t  aco = (uint8_t)((ACSR & _BV(ACO)) ? 1 : 0);

  /*  Repique del cruce: llegó demasiado pronto para ser real. Se cuenta y se
   *  descarta, pero NO se apaga la interrupción — un cruce bueno que cayera
   *  en la ventana ciega se perdería sin dejar rastro.
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
   *  cruce de verdad la salida queda del lado contrario al del cruce anterior.
   *  Un flanco que llega con el MISMO lado es repique que fue y volvió.
   *
   *  ⚠️ Medido: atrapa 2 a 10 por segundo de los ~80 rechazos. Sirve poco,
   *  porque el repique es más rápido que la entrada a la ISR y `ACO` ya volvió
   *  cuando lo leemos. Se queda porque cuesta cuatro ciclos y nunca se
   *  equivoca en contra.
   */
  if (cruces != 0 && aco == acoUlt) {
    rechDir++;
    CMP_TRAZA_SALE();
    return;
  }
  acoUlt = aco;

  /*  EL SELLO CALIFICADO POR SENTIDO, Y POR QUE HACE FALTA.
   *
   *  `tUltimo` se pone en LOS DOS cruces del ciclo (ACIS=00, cualquier flanco):
   *  son 120/s. Pero el inversor solo sella el arranque del semiciclo POSITIVO:
   *  60/s. Restar uno contra otro da un numero **ambiguo en 180 grados**, y el
   *  banco lo mostro limpio: el error del PLL se agrupaba en +-100 a 180 en vez
   *  de converger a cero.
   *
   *  Aqui se guarda el sello de UN solo sentido. Cuesta una comparacion y dos
   *  guardados, 120 veces por segundo.
   *
   *  !! CUAL de los dos sentidos es el positivo depende de si A6 entra al + o
   *  al - del comparador, y eso lo contesta el banco en una corrida: si el
   *  enganche se asienta cerca de 180 en vez de cerca de 0, se voltea
   *  CMP_ACO_REFERENCIA. No lo adivino aqui.
   */
  /*  EL SENTIDO SALE DE LA ALTERNANCIA, NO DE LEER ACO. Y ESTO LO ENSENO EL
   *  BANCO CON 417 MUESTRAS IMPOSIBLES EN UNA CORRIDA.
   *
   *  Arriba, en la prueba de direccion, esta escrito desde hace semanas:
   *  "el repique es mas rapido que la entrada a la ISR y ACO ya volvio cuando
   *  lo leemos". Es cierto, y yo lo use de todas formas para decidir el
   *  sentido del sello de fase. Decidir con una lectura que sabemos tardia,
   *  justo donde la senal es mas lenta y el repique manda, es una moneda al
   *  aire: salian entre 10 y 20 muestras imposibles por segundo, de 60.
   *
   *  LA ALTERNANCIA, EN CAMBIO, ES DETERMINISTA. Los cruces se turnan: subida,
   *  bajada, subida. No hay que medir nada para saberlo, solo contar. Y cuando
   *  falta un cruce la alternancia se rompe -- pero eso se ve en el TIEMPO,
   *  que es lo que medimos bien:
   *
   *      semis = redondeo(dt / 8330 us)    semiciclos que de veras pasaron
   *      si semis es par, el sentido NO cambio
   *
   *  Asi un cruce perdido se corrige solo, y dos tambien, sin leer ACO.
   *
   *  ACO SE QUEDA, PERO COMO ANCLA LENTA. La alternancia es exacta pero
   *  relativa: alguien tiene que decir cual de los dos sentidos es cual. Ese
   *  alguien es ACO, que en UNA lectura es poco de fiar pero EN PROMEDIO no se
   *  equivoca. Un voto que sube con cada acuerdo y baja con cada desacuerdo:
   *  si se va muy negativo, la bandera estaba volteada y se voltea. Verdad
   *  rapida de la alternancia, verdad lenta del sensor ruidoso.
   */
  {
    /*  !! SIN DIVISION. La primera version de esto escribio
     *  `(dt + 4165) / 8330` -- una division de 16 bits, ~200 ciclos, DENTRO DE
     *  LA ISR. Es exactamente el error de la Parte C que costo 14 pasos de
     *  onda, cometido otra vez. Con comparaciones el caso normal (dt de un
     *  semiciclo) no entra al lazo ni una vez: cuesta una comparacion.
     */
    uint8_t  impares = 1;
    uint16_t tope    = 12495u;            /* 1.5 semiciclos */
    while (dt >= tope && impares < 8) { impares ^= 1u; tope += 8330u; }
    if (impares) sentido = (uint8_t)!sentido;

    if (aco == sentido) { if (voto <  60) voto++; }
    else                { if (voto > -60) voto--; }
    if (voto < -30)     { sentido = (uint8_t)!sentido; voto = 0; }

    if (sentido == CMP_ACO_REFERENCIA) {
      /*  LA FASE, CONTRA TIMER1. El sello de 4 us se queda para el
       *  semiperiodo y el jitter, donde funciona bien; para la FASE no
       *  sirve, porque pierde desbordes del Timer0 y mete offsets de 88
       *  grados. Ver spwm_328p.cpp.
       */
      faseCruce  = spwm_fase_cuentas();
      faseVuelta = spwm_fase_vuelta();
      tCruceDir  = t;
      nCruceDir++;
    }
  }

  if (dt > CMP_HUECO_US) huecos++;   // se acepta igual, pero queda anotado

  tUltimo     = t;
  semiperiodo = dt;
  cruces++;

  /*  D4 conmuta aqui: UNA instruccion, y es el unico sitio donde se sabe que
   *  el cruce fue aceptado. Ver traza_cruce.h -- es lo que deja medir la
   *  demora directo en el analizador, sin inferirla de la pierna B.
   */
  CRUCE_TRAZA_TOGGLE();

  /*  El jitter, encolado. Sólo entran los semiperiodos plausibles: el primero
   *  de todos y los que vienen después de un hueco no son medidas de jitter,
   *  son medidas de que faltó un cruce.
   */
  if (cruces > 1 && dt < CMP_HUECO_US) {
    semiAnillo[semiW & (CMP_SEMI_ANILLO - 1)] = dt;
    semiW++;
  }

  rms_marca_red();                   // aquí termina una ventana y empieza otra

  CMP_TRAZA_SALE();
}

// ======================= LA ESTADÍSTICA, EN EL LAZO ==========================

static uint16_t jN, jMin, jMax;
static uint8_t  jRead;
static int32_t  jSumaDev;
static uint32_t jSumaDev2;

void cmp_atender(void)
{
  /*  EL VENCIMIENTO DE LA RED, pasado a 32 bits.
   *
   *  `cruces` dice SI hubo cruce nuevo (es monotono, no tiene aliasing) y
   *  `tUltimo` dice CUANDO con 4 us. Se juntan aqui, en el lazo, para armar
   *  un sello de 32 bits que no envuelve en 4.77 horas. A partir de ahi la
   *  ausencia se mide restando, y la resta ya no puede dar la vuelta.
   *
   *  La ISR no se toca: sigue costando 232 instrucciones y 19 push.
   */
  {
    uint16_t n, t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { n = cruces; t = tUltimo; }

    if (n != crucesVistos) {
      crucesVistos = n;
      /*  El cruce fue hace `atraso` garrapatas, y eso SI cabe en 16 bits
       *  porque acabamos de verlo: el lazo pasa por aqui muchas veces por
       *  milisegundo. Se resta del reloj largo para anclarlo.
       */
      const uint32_t ahora32 = reloj_4us_32();
      const uint16_t atraso  = (uint16_t)((uint16_t)ahora32 - t);
      tCruce32  = ahora32 - (uint32_t)atraso;
      huboCruce = true;
    }
  }

  // Un byte, y en el AVR eso se lee atómico: el cli() más barato es el que no
  // existe (DC-026). Misma razón que en rms_atender().
  const uint8_t w = semiW;

  while (jRead != w) {
    const uint16_t dt = semiAnillo[jRead & (CMP_SEMI_ANILLO - 1)];
    jRead++;

    const int16_t dev = (int16_t)((int32_t)dt - CMP_ANCLA_US);
    jSumaDev  += dev;
    jSumaDev2 += (uint32_t)((int32_t)dev * dev);
    if (jN == 0 || dt < jMin) jMin = dt;
    if (jN == 0 || dt > jMax) jMax = dt;
    jN++;
  }
}

bool cmp_jitter(uint16_t *minUs, uint16_t *promUs, uint16_t *maxUs, uint16_t *sigmaUs)
{
  if (jN < 8) return false;

  const int32_t promDev = jSumaDev / (int32_t)jN;

  /*  σ² = ⟨d²⟩ − ⟨d⟩², en la forma escalada para no truncar antes de tiempo:
   *  (n·Σd² − (Σd)²) / n².
   */
  const uint64_t num = (uint64_t)jN * jSumaDev2
                     - (uint64_t)((int64_t)jSumaDev * jSumaDev);
  const uint32_t var = (uint32_t)(num / ((uint32_t)jN * jN));

  if (minUs)   *minUs   = jMin;
  if (maxUs)   *maxUs   = jMax;
  if (promUs)  *promUs  = (uint16_t)(CMP_ANCLA_US + promDev);
  if (sigmaUs) *sigmaUs = rms_isqrt32(var);
  return true;
}

uint32_t cmp_frecuencia_mhz(void)
{
  if (jN < 8) return 0;

  /*  ⚠️ LA FRECUENCIA SALE DEL PROMEDIO DEL SEMIPERIODO, Y NO HACE FALTA
   *  NINGÚN RELOJ LARGO.
   *
   *  Antes se medía el lapso entre el primer y el último cruce del reporte y
   *  se dividía entre los semiperiodos — lo que pedía un reloj de 32 bits,
   *  porque un segundo son 250 000 garrapatas y en 16 bits sólo caben 65 536.
   *
   *  Pero el promedio de N semiperiodos **es** (t_último − t_primero) / N: el
   *  mismo número, calculado con lo que ya se está acumulando. Y mejor, porque
   *  deja fuera los semiperiodos de los huecos.
   *
   *  Se trabaja en 256-avos de µs para no perder resolución en la división:
   *  un µs de redondeo en el promedio serían 7.2 mHz, contra un criterio de
   *  10 mHz. A 1/256 de µs el redondeo vale 0.028 mHz.
   *
   *      f = 1 / (2 · T_semi)      →     f_mHz = 5e8 / T_semi[µs]
   */
  const int32_t  devX256   = (int32_t)((jSumaDev * 256) / (int32_t)jN);
  const uint32_t semiX256  = (uint32_t)((int32_t)CMP_ANCLA_US * 256 + devX256);
  if (semiX256 == 0) return 0;

  return (uint32_t)((500000000ULL * 256ULL) / semiX256);
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
   *               conmutación en cada cruce de la red.
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

uint16_t cmp_cruces(void)          { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = cruces; }      return v; }
uint16_t cmp_rechazados(void)      { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rechazados; }  return v; }
uint16_t cmp_huecos(void)          { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = huecos; }      return v; }
uint16_t cmp_rech_direccion(void)  { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rechDir; }     return v; }
uint16_t cmp_semiperiodo_us(void)  { uint16_t v; ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = semiperiodo; } return v; }

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

bool cmp_red_presente(void)
{
  /*  La resta se hace en 32 bits a proposito. En 16 envolvia cada 262.144 ms
   *  y, sin red, volvia a decir «hay red» durante CMP_SIN_RED_MS de cada
   *  vuelta: un tren de pulsos de 50 ms cada 262 ms en D13. Medido con el
   *  analizador el 27 de noviembre de 2026 (261.9 ms contra 262.144
   *  predichos), y el serial no lo mostraba.
   *
   *  ⚠️ Depende de que `cmp_atender()` se llame. Si el lazo se atora, el
   *  sello se queda viejo y esto dice «no hay red»: falla hacia el lado
   *  seguro, que es el que queremos.
   */
  if (!huboCruce) return false;

  const uint32_t ahora = reloj_4us_32();
  return (uint32_t)(ahora - tCruce32) < ((CMP_SIN_RED_MS * 1000UL) / 4);
}

/*  Sello de 4 us del ultimo cruce ACEPTADO de la red. Se resta contra
 *  `spwm_t_cruce()` para sacar la fase; los dos usan `reloj_4us()`.
 */
uint16_t cmp_t_ultimo_cruce(void)
{
  uint16_t t;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = tUltimo; }
  return t;
}

/*  Sello y contador del cruce de UN SOLO SENTIDO, 60/s. Es lo que el PLL tiene
 *  que usar: `cmp_t_ultimo_cruce()` es de 120/s y da fase ambigua en 180.
 *
 *  !! Estos dos NO los toca `cmp_reiniciar()`, a proposito: el PLL no puede
 *  depender de un instrumento que el reporte pone en cero cada segundo.
 */
uint16_t cmp_t_cruce_dir(void)
{
  uint16_t t;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = tCruceDir; }
  return t;
}

uint8_t cmp_n_cruce_dir(void) { return nCruceDir; }

/*  Fase del ultimo cruce de referencia, en decimas de grado, 0..3599, medida
 *  DENTRO de nuestro propio ciclo. Sin resta y sin Timer0.
 */
uint16_t cmp_fase_x10(void)
{
  uint32_t c, v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { c = faseCruce; v = faseVuelta; }
  if (v == 0) return 0;
  return (uint16_t)((c * 3600UL) / v);
}   /* un byte: atomico */

/*  Acuerdo entre la alternancia y ACO, -60..+60. Cerca de +60 el detector de
 *  sentido esta sano; cerca de 0 o negativo, ACO esta inservible y hay que ir
 *  a ver el hardware del cruce, no el firmware.
 */
int8_t cmp_voto_sentido(void) { return voto; }

void cmp_reiniciar(void)
{
  jN = jMin = jMax = 0;
  jRead = 0;
  jSumaDev = 0;
  jSumaDev2 = 0;

  tCruce32     = 0;
  crucesVistos = 0;
  huboCruce    = false;   /* sin cruces todavia: no hay red hasta que llegue uno */

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    cruces = rechazados = huecos = rechDir = 0;
    semiperiodo = 0;
    semiW = 0;
    for (uint8_t k = 0; k < CMP_CAJONES; k++) cmp_histo[k] = 0;
  }
}
