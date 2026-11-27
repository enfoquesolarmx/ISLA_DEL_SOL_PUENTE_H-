/*
 * =============================================================================
 *  spwm_328p.cpp  -  Isla del Sol, micro CRÍTICO (A1), ATmega328P
 *  SPWM híbrida: Timer1 + interrupción de portadora
 * -----------------------------------------------------------------------------
 *  RECURSOS QUE SON DE ESTE ARCHIVO (nadie más los toca)
 *    Timer1 completo (TCCR1A/B, TCNT1, ICR1, OCR1A, OCR1B, TIMSK1)
 *    Interrupción TIMER1_OVF_vect (una vez por periodo de portadora, 41.6 µs)
 *    D9  = PB1 = OC1A -> HA   pierna A, lado alto   (24 kHz, por hardware)
 *    D10 = PB2 = OC1B -> LA   pierna A, lado bajo   (24 kHz, por hardware)
 *    D11 = PB3        -> HB   pierna B, lado alto   (60 Hz, por la ISR)
 *    D3  = PD3        -> LB   pierna B, lado bajo   (60 Hz, por la ISR)
 *    D12 = PB4        -> EN_HW, permiso del puente (sólo se BAJA aquí)
 *  Otros módulos que usen PORTB o PORTD deben escribir bit por bit
 *  (PORTD |= _BV(x) con x constante -> instrucción sbi, atómica) para no
 *  pisar los cambios que hace esta ISR.
 *
 * -----------------------------------------------------------------------------
 *  CÓMO FUNCIONA EL TIMER (modo 8: PWM Phase and Frequency Correct, TOP = ICR1)
 *
 *    TCNT1   TOP ...........    /\          /\
 *                              /  \        /  \
 *            OCR1B (LA) ----- /----\------/----\-----
 *            OCR1A (HA) ---- /------\----/------\----
 *            BOTTOM ....... /        \  /        \  /
 *                                     \/          \/
 *                          |<-- un periodo, 41.6 µs -->|
 *
 *    HA (OC1A, no invertida): ALTA mientras TCNT1 < OCR1A  (centrada en BOTTOM)
 *    LA (OC1B, invertida):    ALTA mientras TCNT1 > OCR1B  (centrada en TOP)
 *    Con OCR1B = OCR1A + DT entre las dos queda un hueco de DT cuentas en
 *    CADA flanco (subida y bajada de la rampa): ése es el tiempo muerto, y lo
 *    hace el hardware del timer, no el programa.
 *
 *    OCR1A y OCR1B tienen doble búfer: lo que la ISR escribe se aplica en el
 *    SIGUIENTE BOTTOM. La interrupción salta justo en BOTTOM, así que tiene un
 *    periodo completo (666 ciclos) para calcular el siguiente.
 *
 *  CICLO DE TRABAJO POR SEMICICLO (d = amplitud × |sen θ|, en cuentas)
 *    semiciclo + (LB encendido):  OCR1A = d          OCR1B = d + DT
 *        HA conduce 2d de cada 2·TOP cuentas -> salida = +Vbus × d / TOP
 *    semiciclo - (HB encendido):  OCR1B = TOP - d    OCR1A = TOP - d - DT
 *        LA conduce 2d de cada 2·TOP cuentas -> salida = -Vbus × d / TOP
 *    El timer y las salidas son LOS MISMOS en los dos semiciclos: sólo cambia
 *    qué lado de la pierna A lleva el pulso de potencia.
 *
 *  CRUCE POR CERO (cambio de la pierna B), muestras del semiciclo 0 .. S-1
 *  (S = 200 a 60 Hz, 240 a 50 Hz):
 *    periodo S-1: pierna A apagada (OCR1A = 0, OCR1B = TOP) y se APAGA el
 *                 interruptor de la pierna B que sale.
 *    periodo 0:   pierna A apagada y se ENCIENDE el interruptor que entra.
 *    periodo 1..: pierna A con el patrón del nuevo semiciclo.
 *    Tiempo muerto de la pierna B = 1 periodo completo (41.6 µs).
 *    PULSO SOLITARIO DE HA (medido en banco, 2026-11-12): en la muestra 1 del
 *    semiciclo -, OCR1A pasa de 0 (pierna A apagada) a TOP - DT. En ese BOTTOM
 *    el chip pone HA en el nivel que le toca ("alta mientras TCNT1 < OCR1A")
 *    aunque no haya coincidencia: HA sube en BOTTOM, baja en TOP - DT (~19.8 µs)
 *    y vuelve a subir 2 µs después. Es seguro: LA está apagada (OCR1B = TOP) y
 *    HB encendido, así que HA + HB = salida en 0 V, que es justo lo que pide el
 *    seno en ese punto. El tiempo muerto se respeta en todos los casos
 *    (pruebas/prueba_spwm.cpp ya modela esta regla).
 *    En las muestras 0 y S-1 el seno vale 0.8 % (60 Hz) o 0.65 % (50 Hz): a esa amplitud el pulso sería
 *    menor que PULSO_MIN y no se generaría de todos modos. No se pierde onda.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include "config_portadora.h"

#if ISLA_CHIP_328P

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "spwm.h"
#include "tabla_seno.h"
#include "reloj_4us.h"

// --- Enganches de medición (DC-017 / protocolo de banco, Parte C) -----------
// En producción NO EXISTEN: no hay traza_isr.h, las macros se expanden a nada
// y el código compilado queda byte por byte igual (verificado: mismo md5 del
// .o con y sin ellas).
//
// El sketch de la Parte C pone un traza_isr.h junto a este archivo y ahí las
// define. Así el módulo se compila como un .cpp NORMAL —nada de incluir un
// .cpp desde el .ino, que produce símbolos duplicados en el IDE de Arduino.
#if defined(__has_include)
#  if __has_include("traza_isr.h")
#    include "traza_isr.h"
#  endif
#endif
#ifndef SPWM_TRAZA_ENTRA
#define SPWM_TRAZA_ENTRA()
#endif
#ifndef SPWM_TRAZA_SALE
#define SPWM_TRAZA_SALE()
#endif

// --- Enganche de periodo: lo que otros módulos necesitan hacer en cada
//     desborde de la portadora ---------------------------------------------
// El ADC se dispara desde aquí (DC-020): el desborde del Timer1 es la única
// base de tiempo que queda, porque OCR1B es el tiempo muerto e ICR1 es el TOP
// del modo 8. Si existe gancho_spwm.h junto a este archivo se usa; si no, la
// macro queda vacía y este módulo sigue siendo independiente.
#if defined(__has_include)
#  if __has_include("gancho_spwm.h")
#    include "gancho_spwm.h"
#  endif
#endif
#ifndef SPWM_CADA_PERIODO
#define SPWM_CADA_PERIODO()
#endif

// ============================ PINES ==========================================
#define BIT_HA   PB1   // D9,  OC1A
#define BIT_LA   PB2   // D10, OC1B
#define BIT_HB   PB3   // D11
#define BIT_EN   PB4   // D12, EN_HW
#define BIT_LB   PD3   // D3

// ============================ ESTADO =========================================
// Compartido con loop(): volatile; los de 16 bits se leen/escriben atómicos.
static volatile uint16_t amplitudPedida = 0;   // la escribe loop(), la toma la ISR
static volatile uint16_t ciclosSalida   = 0;   // ciclos de 60 Hz completados
static volatile bool     corriendo      = false;

// Sólo de la ISR (y de spwm_arrancar con el timer detenido).
static uint8_t  muestra   = 0;                // muestra del periodo que ACABA de empezar
static uint8_t  semiciclo = 1;                // 0 = positivo, 1 = negativo
static uint16_t amplitud  = 0;                // amplitud vigente durante el ciclo completo
static uint8_t  dtCuentas = DT_LEGACY_CUENTAS;
static uint16_t amplitudMax = AMPLITUD_MAX_LEGACY;   // según el DT en uso

// Frecuencia de línea (sólo cambia con la SPWM detenida, en spwm_preparar).
//                         60 Hz   50 Hz
//   muestrasSemiciclo      200     240
//   ultimaMuestra          199     239
//   muestrasCuarto         100     120
static uint8_t         frecuenciaHz      = LINEA_60_HZ;
static uint8_t         muestrasSemiciclo = MUESTRAS_SEMICICLO_60;
static uint8_t         ultimaMuestra     = MUESTRAS_SEMICICLO_60 - 1;
static uint8_t         muestrasCuarto    = MUESTRAS_CUARTO_60;
static const uint16_t *tablaSeno         = TABLA_SENO_60_Q16;   // en flash

// ============================ INTERRUPCIÓN DE PORTADORA ======================
/*  Sello del ultimo cruce por cero del inversor (inicio del semiciclo
 *  positivo) y un contador que cambia en cada uno. Ver reloj_4us.h.
 */
/*  TOP DE LA PORTADORA, AJUSTABLE — es el volante de frecuencia del PLL.
 *
 *      f_linea = 16 MHz / (2 x TOP x 400)        TOP = 333 -> 60.060 Hz
 *
 *  Mover el TOP mueve la portadora Y la linea juntas, y por eso **las 25
 *  muestras por semiciclo de DC-023 se conservan exactas**: el disparo del
 *  ADC tambien cuelga de la portadora, asi que la razon no cambia.
 *
 *          muestras por semiciclo = (fsw/8) / (fsw/200) = 25,  sea cual sea fsw
 *
 *  Una cuenta de TOP son 0.18 Hz, demasiado grueso para enganchar fase. Se
 *  afina **dithereando** entre TOP y TOP+1 con un acumulador de 8 bits:
 *  0.70 mHz por paso, diez veces mas fino que los 7 mHz con que medimos la
 *  red. El dither mueve el flanco 62.5 ns — nada, al lado de los 41.6 us del
 *  periodo— y **no toca ni una muestra de la tabla**: la onda sigue intacta.
 */
static volatile uint16_t topEntero = TOP_CUENTAS;
static volatile uint8_t  topFrac;          // 0..255 -> fraccion de cuenta
static uint8_t           acumTop;          // solo la ISR

void spwm_top_fijar(uint16_t entero, uint8_t frac)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { topEntero = entero; topFrac = frac; }
}

uint16_t spwm_top_actual(void) { return topEntero; }

static volatile uint16_t tCruceInv;
static volatile uint8_t  nCruceInv;

/* =============================================================================
 *  LA FASE SE MIDE CONTRA TIMER1, NO CONTRA EL RELOJ DE 4 us
 *
 *  Lo enseno el banco con un numero que no deja lugar a dudas: enganchado, la
 *  linea FASE marcaba -87.7, -88.5, -88.1, -87.9, -87.8 mientras el lazo leia
 *  0.2. No es ruido: es un OFFSET CONSTANTE de 88 grados que aparece y
 *  desaparece. Y 88.5 grados de nuestro ciclo son 4096 us, o sea **cuatro
 *  desbordes del Timer0**.
 *
 *  El reloj de 4 us se arma con `timer0_overflow_count`, que lo lleva una ISR
 *  que compite con la del SPWM al 65 % de ocupacion, con la del ADC, con TWI
 *  y con los cli() del lazo. La correccion por TOV0 solo salva UN desborde
 *  pendiente. Cuatro, no.
 *
 *  NO VOY A ADIVINAR UNA CUARTA VEZ POR QUE SE PIERDEN CUATRO. Quito la
 *  dependencia: **ya tenemos un reloj mejor y es el nuestro.**
 *
 *      posicion = semiciclo x 200 + muestra        (0..399, 0.9 grados)
 *      fino     = TCNT1 dentro del periodo         (62.5 ns)
 *
 *  Timer1 no puede perder cuentas: es el hardware que genera la onda, y su
 *  ISR es lo de mas alta prioridad del sistema. Si eso se atrasa, la onda se
 *  deforma y ya no importa la fase.
 *
 *  Y HAY ALGO MEJOR TODAVIA: **el cruce del inversor ya no necesita sello.**
 *  Nuestro cruce es `posicion = 0` por construccion. Asi que esto no mide una
 *  diferencia de dos relojes: mide DIRECTAMENTE en que punto de nuestro
 *  propio ciclo cruzo la red. Un solo numero, una sola fuente, sin resta.
 *
 *  Se llama desde la ISR del comparador, 120 veces por segundo.
 * =============================================================================
 */
/*  Posicion del ciclo AHORA, 0..399. La lee la ISR del ADC para saber a que
 *  angulo corresponde cada muestra de A6. Dos bytes y una suma.
 */
uint16_t spwm_posicion(void)
{
  return (uint16_t)semiciclo * (uint16_t)muestrasSemiciclo + (uint16_t)muestra;
}

uint16_t spwm_posiciones(void) { return (uint16_t)muestrasSemiciclo << 1; }

uint32_t spwm_fase_cuentas(void)
{
  const uint16_t pos = (uint16_t)semiciclo * (uint16_t)muestrasSemiciclo
                     + (uint16_t)muestra;
  /*  Modo 8: el contador sube y baja. Dos lecturas dicen para donde va, que
   *  es mas barato y mas seguro que deducirlo de un registro de estado.
   */
  const uint16_t a   = TCNT1;
  const uint16_t b   = TCNT1;
  const uint16_t dos = (uint16_t)(topEntero << 1);
  const uint16_t fino = (b >= a) ? a : (uint16_t)(dos - a);
  return (uint32_t)pos * dos + fino;
}

/*  Cuentas en una vuelta completa de fase. Depende del TOP, que el PLL mueve. */
uint32_t spwm_fase_vuelta(void)
{
  return (uint32_t)((uint16_t)muestrasSemiciclo << 1) * (uint32_t)(topEntero << 1);
}

uint16_t spwm_t_cruce(void)
{
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = tCruceInv; }
  return v;
}

uint8_t spwm_n_cruce(void) { return nCruceInv; }

ISR(TIMER1_OVF_vect) {
  SPWM_TRAZA_ENTRA();
  SPWM_CADA_PERIODO();   // el disparo del ADC va aquí: lo más cerca posible
                         // de BOTTOM, para que el instante sea repetible
  // ---- 1) Periodo que acaba de empezar: pierna B ---------------------------
  // La pierna A ya está apagada en estos dos periodos (se programó antes).
  if (muestra == ultimaMuestra) {
    if (semiciclo == 0) PORTD &= ~_BV(BIT_LB);    // sale LB (termina el semiciclo +)
    else                PORTB &= ~_BV(BIT_HB);    // sale HB (termina el semiciclo -)
  } else if (muestra == 0) {
    if (semiciclo == 0) PORTD |= _BV(BIT_LB);     // entra LB (empieza el semiciclo +)
    else                PORTB |= _BV(BIT_HB);     // entra HB (empieza el semiciclo -)
  }

  /*  ---- EL TOP QUE VA A REGIR ESTE PERIODO, ANTES DE CALCULAR ANCHOS -----
   *
   *  !! Esto estaba mal y lo encontre revisando mi propio trabajo. El dither
   *  escribia `ICR1 = topEntero + 1` DESPUES de haber calculado los anchos
   *  con `topEntero` pelado. En el periodo del acarreo, OC1B --que esta en
   *  modo INVERTIDO, asi que OCR1B = TOP significa "siempre baja"-- quedaba
   *  en TOP-1: **LA recibia un pulso espurio de una cuenta, 62.5 ns**, justo
   *  en el periodo donde debia estar apagada del todo.
   *
   *  Son 62.5 ns y seguramente no pasan el driver de compuerta, pero es un
   *  pulso que nadie pidio en la pierna baja. Ahora el acarreo se calcula
   *  PRIMERO y el mismo numero rige los anchos y el ICR1, que es lo correcto:
   *  en modo 8 los dos se actualizan en BOTTOM y tienen que ser coherentes.
   *
   *  Costo en el camino comun (PLL suelto, topFrac = 0): el mismo de antes,
   *  una lectura y un salto.
   */
  /*  !! EL DITHER NO CORRE EN LOS DOS PERIODOS DEL CRUCE, Y ESTA ES LA RAZON.
   *
   *  `OCR1x` TIENE DOBLE BUFER: se carga en el siguiente BOTTOM, asi que lo
   *  que se escribe aqui gobierna el periodo N+1. `ICR1` NO tiene bufer:
   *  gobierna el periodo N. Son dos periodos distintos.
   *
   *  Mi arreglo de la manana escribia `OCR1B = ICR1` creyendo que eran el
   *  mismo, y empeoro el defecto en vez de quitarlo: si este periodo lleva
   *  acarreo (TOP=334) y el siguiente no (TOP=333), queda OCR1B > TOP, la
   *  comparacion NUNCA ocurre y LA se queda en su estado anterior **un
   *  periodo entero: 41.6 us**. Antes eran 62.5 ns. El banco lo vio en tres
   *  horas, y vio tambien la pista que lo delata: con el PLL suelto --sin
   *  dither-- el pulso no esta.
   *
   *  La solucion es hacer que los dos coincidan por construccion: en los dos
   *  periodos del cruce, donde la pierna A va apagada y OCR1B tiene que valer
   *  TOP exacto, **el TOP no se dithierea**. Asi `OCR1B = topEntero` siempre
   *  es el valor correcto.
   *
   *  Cuesta 2 de cada 400 pasos de dither: 0.5 % de la correccion, o sea
   *  0.24 mHz sobre los 48 que corregimos. Nada.
   */
  if (topFrac) {
    if (muestra != 0 && muestra != ultimaMuestra) {
      const uint8_t prev = acumTop;
      acumTop = (uint8_t)(prev + topFrac);
      ICR1 = (uint16_t)(topEntero + ((acumTop < prev) ? 1u : 0u));
    } else {
      ICR1 = topEntero;        /* cruce: TOP exacto, para que OCR1B cuadre */
    }
  }

  /*  !! SIN VARIABLE VIVA, Y ESO IMPORTA. La primera version de este arreglo
   *  guardaba el TOP en un `uint16_t topAhora` que vivia toda la ISR: el
   *  compilador tuvo que salvar un registro mas y el prologo paso de 15 a 17
   *  push. Son 8 ciclos en CADA interrupcion a 24 kHz: 1.2 % del CPU
   *  permanente en la ISR mas critica, por arreglar un pulso de 62.5 ns.
   *
   *  En vez de eso, el dither sube aqui arriba y quien necesite el TOP de
   *  este periodo LEE ICR1, que es el hardware y ya lo tiene. Dos lecturas de
   *  registro, cero registros vivos.
   */

  // ---- 2) Siguiente periodo: ¿qué muestra y qué semiciclo? -----------------
  uint8_t sig = muestra + 1;
  uint8_t sem = semiciclo;
  if (sig == muestrasSemiciclo) {
    sig = 0;
    sem ^= 1;
    if (sem == 0) {                // empieza un ciclo nuevo (semiciclo positivo):
      amplitud = amplitudPedida;   // la amplitud sólo cambia aquí -> los dos
      ciclosSalida++;              // semiciclos del ciclo son iguales (sin CD)

      /*  EL CRUCE POR CERO DEL INVERSOR ES EXACTAMENTE AQUI, y esta rama ya
       *  existia para otra cosa: corre 60 veces por segundo, no 24 024. El
       *  sello cuesta ~10 ciclos y los otros 24 023 periodos no pagan nada.
       *
       *  Mismo `reloj_4us()` que la ISR del comparador usa para sellar el
       *  cruce de la red: la resta de los dos sellos ES la fase, y usando la
       *  misma secuencia de instrucciones cualquier sesgo se cancela.
       */
      tCruceInv = reloj_4us();
      nCruceInv++;
    }
  }

  // ---- 3) Comparadores del siguiente periodo -------------------------------
  if (sig == 0 || sig == ultimaMuestra) {
    OCR1A = 0;                     // HA siempre baja
    OCR1B = topEntero;             // LA siempre baja. En el cruce el TOP NO
                                   // se dithierea, asi que esto ES el TOP.
  } else {
    // Cuarto de onda en espejo: 0..C-1 tal cual, C..S-1 al revés
    // (60 Hz: C = 100, S = 200;  50 Hz: C = 120, S = 240).
    const uint8_t q = (sig < muestrasCuarto) ? sig : (uint8_t)(ultimaMuestra - sig);
    uint16_t d = (uint16_t)(((uint32_t)pgm_read_word(&tablaSeno[q]) * amplitud) >> 16);
    if (d < PULSO_MIN_CUENTAS) d = 0;          // pulso demasiado angosto: no se genera
    if (sem == 0) {                            // semiciclo +: HA lleva la potencia
      OCR1A = d;
      OCR1B = d + dtCuentas;
    } else {                                   // semiciclo -: LA lleva la potencia
      /*  Aqui `topEntero` basta aunque el dither haya sumado uno: el ANCHO del
       *  pulso es d + dtCuentas y no depende del TOP. Lo unico que se corre
       *  es la posicion dentro del periodo, 62.5 ns, y eso no cambia el area.
       */
      const uint16_t b = topEntero - d;
      OCR1B = b;
      OCR1A = b - dtCuentas;
    }
  }


  muestra   = sig;
  semiciclo = sem;
  SPWM_TRAZA_SALE();
}

// ============================ FUNCIONES PÚBLICAS =============================
void spwm_detener() {
  PORTB &= ~_BV(BIT_EN);           // 1) D12 = 0 PRIMERO: la AND corta en ns
  DDRB  |=  _BV(BIT_EN);
  TCCR1A = 0;                      // 2) OC1A/OC1B se sueltan del timer: el pin
                                   //    vuelve a valer lo que diga PORTB (0)
  PORTB &= ~(_BV(BIT_HA) | _BV(BIT_LA) | _BV(BIT_HB));
  PORTD &= ~_BV(BIT_LB);
  DDRB  |=  _BV(BIT_HA) | _BV(BIT_LA) | _BV(BIT_HB);
  DDRD  |=  _BV(BIT_LB);
  TIMSK1 = 0;                      // 3) Sin interrupción y timer detenido
  TCCR1B = 0;
  TIFR1  = _BV(TOV1);
  corriendo = false;
}

bool spwm_preparar(uint8_t dt_cuentas, uint8_t frecuencia_hz) {
  spwm_detener();
  // Sólo se aceptan los dos valores de P9; cualquier otro -> el seguro (1 µs).
  dtCuentas = (dt_cuentas == DT_MODERNO_CUENTAS) ? DT_MODERNO_CUENTAS : DT_LEGACY_CUENTAS;
  // Techo de amplitud: 95 % o el límite físico de ese tiempo muerto, el menor.
  amplitudMax = (dtCuentas == DT_MODERNO_CUENTAS) ? AMPLITUD_MAX_MODERNO : AMPLITUD_MAX_LEGACY;
  // Frecuencia de línea: 50 Hz sólo si se pide explícitamente; si no, 60 Hz.
  const bool valida = (frecuencia_hz == LINEA_60_HZ || frecuencia_hz == LINEA_50_HZ);
  if (frecuencia_hz == LINEA_50_HZ) {
    frecuenciaHz      = LINEA_50_HZ;
    muestrasSemiciclo = MUESTRAS_SEMICICLO_50;
    muestrasCuarto    = MUESTRAS_CUARTO_50;
    tablaSeno         = TABLA_SENO_50_Q16;
  } else {
    frecuenciaHz      = LINEA_60_HZ;
    muestrasSemiciclo = MUESTRAS_SEMICICLO_60;
    muestrasCuarto    = MUESTRAS_CUARTO_60;
    tablaSeno         = TABLA_SENO_60_Q16;
  }
  ultimaMuestra = muestrasSemiciclo - 1;
  return valida;
}

void spwm_arrancar() {
  if (corriendo) return;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR1B = 0;                    // timer detenido
    TCCR1A = 0;                    // modo normal: OCR1A/B se escriben DIRECTO
    TCNT1  = 0;                    //   (en modo 8 irían al búfer y el primer
    OCR1A  = 0;                    //   periodo usaría valores viejos)
    OCR1B  = TOP_CUENTAS;          // primer periodo: pierna A apagada
    ICR1   = TOP_CUENTAS;

    // Estado inicial: "última muestra del semiciclo negativo". La primera
    // interrupción hace el cruce por cero normal hacia el semiciclo positivo.
    muestra        = ultimaMuestra;
    semiciclo      = 1;
    amplitud       = 0;
    amplitudPedida = 0;
    ciclosSalida   = 0;

    TCCR1A = _BV(COM1A1)                  // OC1A (HA) no invertida
           | _BV(COM1B1) | _BV(COM1B0);   // OC1B (LA) invertida; WGM11:10 = 00
    TIFR1  = _BV(TOV1);
    TIMSK1 = _BV(TOIE1);                  // interrupción en BOTTOM
    corriendo = true;
    TCCR1B = _BV(WGM13) | _BV(CS10);      // modo 8, sin preescalador: arranca
  }
}

void spwm_fijar_amplitud(uint16_t cuentas) {
  if (cuentas > amplitudMax) cuentas = amplitudMax;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { amplitudPedida = cuentas; }
}

uint16_t spwm_ciclos() {
  uint16_t c;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { c = ciclosSalida; }
  return c;
}

bool spwm_activo()       { return corriendo; }
uint8_t spwm_dt_cuentas() { return dtCuentas; }
uint16_t spwm_amplitud_max() { return amplitudMax; }
uint8_t spwm_frecuencia_hz() { return frecuenciaHz; }
uint16_t spwm_muestras_ciclo() { return 2u * muestrasSemiciclo; }

#endif  // ISLA_CHIP_328P
