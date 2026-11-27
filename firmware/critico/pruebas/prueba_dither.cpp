/* =============================================================================
 *  ISLA DEL SOL — prueba del DITHER DEL TOP contra el tiempo muerto
 *
 *  POR QUE EXISTE: el 27 de noviembre el enganche del PLL paso a ser
 *  permanente, asi que el dither del TOP dejo de correr "unos segundos" y
 *  pasa a correr 24 024 veces por segundo, siempre. Eso cambia el ancho del
 *  periodo de un ciclo a otro y toca OCR1A/OCR1B/ICR1 en cada BOTTOM.
 *
 *  `prueba_spwm.cpp` valida la onda con TOP fijo y no ejercita nada de esto.
 *  Lo unico que de verdad importa es que **HA y LA nunca conduzcan a la vez**
 *  y que el tiempo muerto se respete: eso es lo que protege el puente.
 *
 *  Se barre toda la onda con varias fracciones de dither, incluida la que mas
 *  mueve el TOP, y se exige:
 *
 *    1. HA y LA nunca en 1 al mismo tiempo.
 *    2. Entre que uno baja y el otro sube pasa al menos el tiempo muerto.
 *    3. Ningun ancho de pulso mayor que el TOP del periodo.
 * =============================================================================
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "avr_simulado/avr/io.h"

volatile uint8_t  PORTB, PORTD, DDRB, DDRD;
volatile uint8_t  TCCR1A, TCCR1B, TIMSK1, TIFR1;
volatile uint16_t TCNT1, ICR1, OCR1A, OCR1B;
volatile uint8_t  TCNT0, TIFR0, ADCSRA, ADMUX, ADCSRB, DIDR0;
volatile uint16_t ADC;
volatile unsigned long timer0_overflow_count;
volatile uint16_t adc_cuenta_desbordes, adc_perdidas_n;

#include "../ISLA_CRITICO/spwm.h"
extern "C" void TIMER1_OVF_vect();

/*  Modelo del hardware en modo 8 (phase and frequency correct, TOP = ICR1):
 *    OC1A (HA) NO invertida : alta mientras TCNT1 < OCR1A
 *    OC1B (LA) invertida    : alta mientras TCNT1 >= OCR1B
 *  El contador sube 0..TOP y baja TOP..0; ICR1/OCR1x se aplican en BOTTOM.
 */
int main()
{
  const uint8_t  DT = DT_LEGACY_CUENTAS;          /* el peor caso: 1 us */
  const uint16_t FRACS[] = { 0, 1, 64, 128, 200, 255 };
  int fallas = 0;
  double dtMin = 1e9;

  for (uint16_t fr : FRACS) {
    if (!spwm_preparar(DT, LINEA_60_HZ)) { printf("preparar fallo\n"); return 1; }
    spwm_arrancar();
    spwm_fijar_amplitud(spwm_amplitud_max());
    spwm_top_fijar(TOP_CUENTAS, (uint8_t)fr);

    uint16_t ocrABuf = 0, ocrBBuf = 0;
    double t = 0.0, tBajaHA = -1e9, tBajaLA = -1e9;
    int ha = 0, la = 0;

    for (long p = 0; p < 400L * 6; p++) {         /* seis ciclos completos */
      /*  !! EL DOBLE BUFER, Y POR ESTO LA PRIMERA VERSION DE ESTA PRUEBA
       *  DEJO PASAR UN DEFECTO DE 41.6 us.
       *
       *  `OCR1A`/`OCR1B` tienen doble bufer: lo que la ISR escribe se carga
       *  en el SIGUIENTE BOTTOM y gobierna el periodo de despues. `ICR1` no
       *  tiene bufer: gobierna el periodo que empieza ya.
       *
       *  Mi modelo aplicaba los tres en el mismo periodo, asi que nunca podia
       *  ver un OCR1B emparejado con un TOP distinto -- que es exactamente el
       *  defecto que el banco encontro. Una prueba que modela mal el hardware
       *  no es media prueba: es una que miente.
       */
      TCNT1 = 0;
      const uint16_t a = ocrABuf, b = ocrBBuf;    /* los del BOTTOM anterior */
      TIMER1_OVF_vect();                          /* escribe ICR1 YA, OCR para la otra */
      const uint16_t top = ICR1;
      ocrABuf = OCR1A; ocrBBuf = OCR1B;

      if (a > top || b > top) {
        printf("frac=%3u periodo=%ld: ANCHO MAYOR QUE EL TOP (a=%u b=%u top=%u)\n",
               fr, p, a, b, top);
        fallas++;
      }

      /*  Sube 0..top y baja top..0, en cuentas de 62.5 ns. */
      for (int dir = 0; dir < 2; dir++) {
        for (uint16_t c = 0; c <= top; c++) {
          const uint16_t n = dir ? (uint16_t)(top - c) : c;
          const int nha = (n <  a) ? 1 : 0;
          const int nla = (n >= b) ? 1 : 0;

          if (nha && nla) { printf("frac=%3u periodo=%ld cuenta=%u: HA Y LA EN 1\n",
                                   fr, p, n); fallas++; }
          if (nha && !ha) {                        /* HA sube */
            const double d = t - tBajaLA;
            if (d < dtMin) dtMin = d;
            if (d < DT * 0.0625 - 1e-9) {
              printf("frac=%3u periodo=%ld: HA sube %.3f us tras bajar LA\n", fr, p, d);
              fallas++;
            }
          }
          if (nla && !la) {                        /* LA sube */
            const double d = t - tBajaHA;
            if (d < dtMin) dtMin = d;
            if (d < DT * 0.0625 - 1e-9) {
              printf("frac=%3u periodo=%ld: LA sube %.3f us tras bajar HA\n", fr, p, d);
              fallas++;
            }
          }
          if (!nha && ha) tBajaHA = t;
          if (!nla && la) tBajaLA = t;
          ha = nha; la = nla;
          t += 0.0625;
        }
      }
    }
    printf("frac=%3u  ->  %s\n", fr, fallas ? "FALLAS" : "sin traslape, DT respetado");
    spwm_detener();
  }

  printf("\ntiempo muerto minimo observado: %.3f us   (pedido: %.3f us)\n",
         dtMin, DT * 0.0625);
  printf("RESULTADO: %s\n", fallas ? "HAY FALLAS" : "todo correcto");
  return fallas ? 1 : 0;
}
