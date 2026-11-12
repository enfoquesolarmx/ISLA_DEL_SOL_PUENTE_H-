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
 *  CRUCE POR CERO (cambio de la pierna B), muestras del semiciclo 0 .. 199:
 *    periodo 199: pierna A apagada (OCR1A = 0, OCR1B = TOP) y se APAGA el
 *                 interruptor de la pierna B que sale.
 *    periodo 0:   pierna A apagada y se ENCIENDE el interruptor que entra.
 *    periodo 1..: pierna A con el patrón del nuevo semiciclo.
 *    Tiempo muerto de la pierna B = 1 periodo completo (41.6 µs).
 *    En las muestras 0 y 199 el seno vale 0.8 %: a esa amplitud el pulso sería
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

// ============================ PINES ==========================================
#define BIT_HA   PB1   // D9,  OC1A
#define BIT_LA   PB2   // D10, OC1B
#define BIT_HB   PB3   // D11
#define BIT_EN   PB4   // D12, EN_HW
#define BIT_LB   PD3   // D3

// ============================ CONSTANTES =====================================
constexpr uint8_t ULTIMA_MUESTRA = MUESTRAS_SEMICICLO - 1;   // 199
static_assert(MUESTRAS_SEMICICLO <= 255, "El índice de muestra es de 8 bits");

// ============================ ESTADO =========================================
// Compartido con loop(): volatile; los de 16 bits se leen/escriben atómicos.
static volatile uint16_t amplitudPedida = 0;   // la escribe loop(), la toma la ISR
static volatile uint16_t ciclosSalida   = 0;   // ciclos de 60 Hz completados
static volatile bool     corriendo      = false;

// Sólo de la ISR (y de spwm_arrancar con el timer detenido).
static uint8_t  muestra   = ULTIMA_MUESTRA;   // muestra del periodo que ACABA de empezar
static uint8_t  semiciclo = 1;                // 0 = positivo, 1 = negativo
static uint16_t amplitud  = 0;                // amplitud vigente durante el ciclo completo
static uint8_t  dtCuentas = DT_LEGACY_CUENTAS;

// ============================ INTERRUPCIÓN DE PORTADORA ======================
ISR(TIMER1_OVF_vect) {
  // ---- 1) Periodo que acaba de empezar: pierna B ---------------------------
  // La pierna A ya está apagada en estos dos periodos (se programó antes).
  if (muestra == ULTIMA_MUESTRA) {
    if (semiciclo == 0) PORTD &= ~_BV(BIT_LB);    // sale LB (termina el semiciclo +)
    else                PORTB &= ~_BV(BIT_HB);    // sale HB (termina el semiciclo -)
  } else if (muestra == 0) {
    if (semiciclo == 0) PORTD |= _BV(BIT_LB);     // entra LB (empieza el semiciclo +)
    else                PORTB |= _BV(BIT_HB);     // entra HB (empieza el semiciclo -)
  }

  // ---- 2) Siguiente periodo: ¿qué muestra y qué semiciclo? -----------------
  uint8_t sig = muestra + 1;
  uint8_t sem = semiciclo;
  if (sig == MUESTRAS_SEMICICLO) {
    sig = 0;
    sem ^= 1;
    if (sem == 0) {                // empieza un ciclo nuevo (semiciclo positivo):
      amplitud = amplitudPedida;   // la amplitud sólo cambia aquí -> los dos
      ciclosSalida++;              // semiciclos del ciclo son iguales (sin CD)
    }
  }

  // ---- 3) Comparadores del siguiente periodo -------------------------------
  if (sig == 0 || sig == ULTIMA_MUESTRA) {
    OCR1A = 0;                     // HA siempre baja
    OCR1B = TOP_CUENTAS;           // LA siempre baja
  } else {
    // Cuarto de onda en espejo: 0..99 tal cual, 100..198 al revés.
    const uint8_t q = (sig < MUESTRAS_CUARTO) ? sig : (uint8_t)(ULTIMA_MUESTRA - sig);
    uint16_t d = (uint16_t)(((uint32_t)pgm_read_word(&TABLA_SENO_Q16[q]) * amplitud) >> 16);
    if (d < PULSO_MIN_CUENTAS) d = 0;          // pulso demasiado angosto: no se genera
    if (sem == 0) {                            // semiciclo +: HA lleva la potencia
      OCR1A = d;
      OCR1B = d + dtCuentas;
    } else {                                   // semiciclo -: LA lleva la potencia
      const uint16_t b = TOP_CUENTAS - d;
      OCR1B = b;
      OCR1A = b - dtCuentas;
    }
  }

  muestra   = sig;
  semiciclo = sem;
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

void spwm_preparar(uint8_t dt_cuentas) {
  spwm_detener();
  // Sólo se aceptan los dos valores de P9; cualquier otro -> el seguro (1 µs).
  dtCuentas = (dt_cuentas == DT_MODERNO_CUENTAS) ? DT_MODERNO_CUENTAS : DT_LEGACY_CUENTAS;
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
    muestra        = ULTIMA_MUESTRA;
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
  if (cuentas > AMPLITUD_MAX_CUENTAS) cuentas = AMPLITUD_MAX_CUENTAS;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { amplitudPedida = cuentas; }
}

uint16_t spwm_ciclos() {
  uint16_t c;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { c = ciclosSalida; }
  return c;
}

bool spwm_activo()       { return corriendo; }
uint8_t spwm_dt_cuentas() { return dtCuentas; }

#endif  // ISLA_CHIP_328P
