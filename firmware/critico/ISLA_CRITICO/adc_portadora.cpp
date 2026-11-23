/*
 * =============================================================================
 *  adc_portadora.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del ADC amarrado a la portadora. Ver adc_portadora.h.
 * =============================================================================
 */

#include "adc_portadora.h"

#include <avr/interrupt.h>
#include <util/atomic.h>

// --- Enganches de medición (protocolo de banco, Parte C, etapa 2) -----------
// En producción NO EXISTEN: no hay traza_adc.h, las macros se expanden a nada
// y el código compilado queda byte por byte igual. El sketch de la etapa 2
// pone un traza_adc.h junto a este archivo y ahí las define.
#if defined(__has_include)
#  if __has_include("traza_adc.h")
#    include "traza_adc.h"
#  endif
#endif
#ifndef ADC_TRAZA_ENTRA
#define ADC_TRAZA_ENTRA()
#endif
#ifndef ADC_TRAZA_SALE
#define ADC_TRAZA_SALE()
#endif

// ============================ ESTADO INTERNO =================================

/*  La rotación de DC-025. Cuatro pasos, tres ranuras: el inversor aparece dos
 *  veces porque es el único que cierra un lazo dentro del periodo.
 *
 *      paso:      0      1      2      3
 *      canal:    A0     A6     A0     A3
 *      ranura:  INV    RED    INV    BAT
 */
static const uint8_t CANAL[4]  = { 0, 6, 0, 3 };
static const uint8_t RANURA[4] = { ADC_RANURA_INV, ADC_RANURA_RED,
                                   ADC_RANURA_INV, ADC_RANURA_BAT };

static volatile uint8_t  paso;                      // dónde va la rotación
static volatile uint16_t cuentas[ADC_RANURAS];
static volatile uint32_t muestras[ADC_RANURAS];

volatile uint8_t  adc_cuenta_desbordes;
volatile uint16_t adc_perdidas_n;

// Referencia AVCC (REFS0) y el canal que toque.
static inline uint8_t admux_de(uint8_t p) { return _BV(REFS0) | CANAL[p]; }

// ================================ LA ISR =====================================
//
// Corre al terminar cada conversión, 104 µs después del disparo. Lo único
// urgente que hace es leer ADC y dejar el multiplexor en el canal siguiente:
// así el canal lleva más de 150 µs asentado cuando llegue el próximo disparo.
//
// ADC se lee como un solo uint16: el compilador genera la pareja ADCL/ADCH en
// el orden correcto, que es lo que libera el registro de datos.

ISR(ADC_vect)
{
  ADC_TRAZA_ENTRA();

  const uint8_t  p = paso;
  const uint8_t  r = RANURA[p];

  cuentas[r] = ADC;
  muestras[r]++;

  const uint8_t sig = (uint8_t)((p + 1) & 3);
  paso  = sig;
  ADMUX = admux_de(sig);          // listo con 150 µs de sobra

  ADC_TRAZA_SALE();
}

// ========================== FUNCIONES PÚBLICAS ===============================

void adc_preparar(void)
{
  ADCSRA = 0;                     // apagado mientras se configura

  paso = 0;
  adc_cuenta_desbordes = 0;
  adc_perdidas_n = 0;
  for (uint8_t i = 0; i < ADC_RANURAS; i++) { cuentas[i] = 0; muestras[i] = 0; }

  ADMUX  = admux_de(0);
  ADCSRB = 0;                     // sin auto-disparo: el disparo es por software

  /*  Entradas digitales apagadas en los pines que usamos. Ahorra consumo y
   *  quita una fuente de ruido en el pin. A6 no lleva: en el 328P los canales
   *  6 y 7 no tienen buffer digital que apagar.
   */
  DIDR0 |= _BV(ADC0D) | _BV(ADC3D);

  /*  Preescalador /128 → reloj de ADC = 125 kHz.
   *
   *  No es negociable mientras el CPU corra a 16 MHz: la hoja de datos pide
   *  el reloj del ADC entre 50 y 200 kHz para los 10 bits completos, y /128
   *  es el único divisor que cae dentro. Una conversión son 13 ciclos de ADC
   *  = 104 µs.
   */
  ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

  /*  La primera conversión después de encender el ADC tarda 25 ciclos en vez
   *  de 13 porque inicializa la lógica analógica. Se hace aquí, a mano y sin
   *  interrupciones, para que no le cueste ese tiempo extra al lazo ya
   *  corriendo. La descartamos: sólo sirve para dejar el ADC despierto.
   */
  ADCSRA &= ~_BV(ADIE);
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) { }
  (void)ADC;
  ADCSRA |= _BV(ADIF);            // limpia la bandera (se limpia escribiendo 1)
  ADCSRA |= _BV(ADIE);
}

uint16_t adc_cuentas(uint8_t ranura)
{
  if (ranura >= ADC_RANURAS) return 0;
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = cuentas[ranura]; }
  return v;
}

uint32_t adc_muestras(uint8_t ranura)
{
  if (ranura >= ADC_RANURAS) return 0;
  uint32_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = muestras[ranura]; }
  return v;
}

uint16_t adc_perdidas(void)
{
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = adc_perdidas_n; }
  return v;
}

void adc_reiniciar_contadores(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for (uint8_t i = 0; i < ADC_RANURAS; i++) muestras[i] = 0;
    adc_perdidas_n = 0;
  }
}
