/*
 * =============================================================================
 *  genera_vcd_spwm.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Genera una traza VCD de la SPWM para verla en GTKWave o PulseView
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Corre el MISMO código del firmware (spwm_328p.cpp) sobre el modelo de
 *    registros de la PC, cuenta por cuenta, y escribe un archivo .vcd con las
 *    cuatro salidas del puente y con la salida promedio por periodo.
 *
 *    Sirve para comparar contra una captura real del analizador lógico: lo que
 *    sale aquí es la onda IDEAL que el firmware pide, sin jitter ni retardos
 *    de los gate drivers.
 *
 *    NO se usa simavr: su Timer1 en modo 8 corre a otra frecuencia (medido:
 *    la pierna B sale a 1197 Hz en vez de 60). El modelo de registros de
 *    prueba_spwm.cpp sí está validado contra el banco.
 *
 *  SEÑALES DEL ARCHIVO
 *    HA_D9, LA_D10   pierna A (24 kHz), las genera el hardware del timer
 *    HB_D11, LB_D3   pierna B (frecuencia de línea), las mueve la interrupción
 *    salida          promedio por periodo de (VA − VB), de −1 a +1 (analógica)
 *
 *  COMPILAR Y CORRER (desde firmware/critico)
 *    g++ -std=gnu++11 -O2 -D__AVR_ATmega328P__ -DF_CPU=16000000UL \
 *        -Ipruebas/avr_simulado -IISLA_CRITICO \
 *        pruebas/genera_vcd_spwm.cpp ISLA_CRITICO/spwm_328p.cpp -o genera_vcd
 *    ./genera_vcd  [dt_cuentas] [amplitud] [ciclos] [hz] [archivo.vcd]
 *
 *  EJEMPLOS
 *    ./genera_vcd                       1 µs, amplitud máxima, 2 ciclos, 60 Hz
 *    ./genera_vcd 8 316 2 60 dt500.vcd  500 ns
 *    ./genera_vcd 16 50 2 60 bajo.vcd   amplitud baja (arranque)
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include <cstdio>
#include <cstdlib>
#include "avr/io.h"
#include "spwm.h"

volatile uint8_t  PORTB, PORTD, DDRB, DDRD;
volatile uint8_t  TCCR1A, TCCR1B, TIMSK1, TIFR1;
volatile uint16_t TCNT1, ICR1, OCR1A, OCR1B;
extern "C" void TIMER1_OVF_vect(void);

// Cuentas desde BOTTOM hasta que la interrupción mueve la pierna B (igual que
// en prueba_spwm.cpp). En el chip real varía unas cuentas: ése es el jitter
// que se ve en el analizador y que esta traza ideal no tiene.
static const int RETARDO_ISR = 40;

// Una cuenta = 62.5 ns = 625 unidades de 100 ps.
static const long UNIDADES_POR_CUENTA = 625;

int main(int argc, char **argv) {
  const uint8_t dt  = (uint8_t)(argc > 1 ? atoi(argv[1]) : DT_LEGACY_CUENTAS);
  const uint8_t hz  = (uint8_t)(argc > 4 ? atoi(argv[4]) : 60);
  const int ciclos  = argc > 3 ? atoi(argv[3]) : 2;
  const char *nom   = argc > 5 ? argv[5] : "spwm.vcd";

  if (!spwm_preparar(dt, hz)) { printf("Frecuencia invalida, se usa 60 Hz\n"); }
  const uint16_t amp = (uint16_t)(argc > 2 ? atoi(argv[2]) : spwm_amplitud_max());
  const int TOP = TOP_CUENTAS, N = spwm_muestras_ciclo();

  FILE *f = fopen(nom, "w");
  if (!f) { printf("No se pudo crear %s\n", nom); return 1; }
  fprintf(f, "$date Isla del Sol - SPWM ideal del firmware $end\n");
  fprintf(f, "$comment DT %u cuentas (%u ns), amplitud %u de %u, %u Hz, %d ciclos $end\n",
          dt, (unsigned)(dt * 625 / 10), amp, spwm_amplitud_max(), hz, ciclos);
  fprintf(f, "$timescale 100ps $end\n$scope module spwm $end\n");
  fprintf(f, "$var wire 1 ! HA_D9 $end\n$var wire 1 \" LA_D10 $end\n");
  fprintf(f, "$var wire 1 # HB_D11 $end\n$var wire 1 $ LB_D3 $end\n");
  fprintf(f, "$var real 64 %% salida $end\n");
  fprintf(f, "$upscope $end\n$enddefinitions $end\n#0\n$dumpvars\n0!\n0\"\n0#\n0$\nr0 %%\n$end\n");

  spwm_arrancar();
  uint16_t actA = OCR1A, actB = OCR1B;
  bool ha = false, la = false, hb = false, lb = false;
  long t = 0, tUltimo = -1;
  const int periodos = ciclos * N + 2;
  bool ampFijada = false;

  for (int p = 0; p < periodos; p++) {
    if (p > 0) {                                  // BOTTOM: búfer -> vigente, y la ISR
      actA = OCR1A; actB = OCR1B;
      TIMER1_OVF_vect();
      if (!ampFijada) { spwm_fijar_amplitud(amp); ampFijada = true; }
    }
    const bool hbN = PORTB & _BV(PB3), lbN = PORTD & _BV(PD3);
    double suma = 0;
    for (int c = 0; c < 2 * TOP; c++, t++) {
      bool haN = ha, laN = la, hbX = hb, lbX = lb;
      if (c == RETARDO_ISR || (p == 0 && c == 0)) { hbX = hbN; lbX = lbN; }
      const bool subiendo = c < TOP;
      const int tcnt = subiendo ? c : 2 * TOP - c;
      if (c == 0 && p > 0) { haN = actA > 0; laN = actB == 0; }   // nivel en BOTTOM
      if (actA == 0) haN = false;
      else if (actA >= TOP) haN = true;
      else if (tcnt == actA) haN = !subiendo;
      if (actB >= TOP) laN = false;
      else if (actB == 0) laN = true;
      else if (tcnt == actB) laN = subiendo;

      if (haN != ha || laN != la || hbX != hb || lbX != lb) {
        const long u = t * UNIDADES_POR_CUENTA;
        if (u != tUltimo) { fprintf(f, "#%ld\n", u); tUltimo = u; }
        if (haN != ha) fprintf(f, "%d!\n", haN);
        if (laN != la) fprintf(f, "%d\"\n", laN);
        if (hbX != hb) fprintf(f, "%d#\n", hbX);
        if (lbX != lb) fprintf(f, "%d$\n", lbX);
        ha = haN; la = laN; hb = hbX; lb = lbX;
      }
      // Salida del puente: +1 con HA y LB, −1 con LA y HB, 0 en los demás casos.
      suma += (ha && lb) ? 1.0 : (la && hb) ? -1.0 : 0.0;
    }
    const long u = t * UNIDADES_POR_CUENTA;       // promedio del periodo que acaba
    if (u != tUltimo) { fprintf(f, "#%ld\n", u); tUltimo = u; }
    fprintf(f, "r%.5f %%\n", suma / (2 * TOP));
  }
  fclose(f);
  printf("Listo: %s\n", nom);
  printf("  DT %u cuentas (%u ns) | amplitud %u de %u (m = %.3f) | %u Hz | %d ciclos\n",
         dt, (unsigned)(dt * 625 / 10), amp, spwm_amplitud_max(), (double)amp / TOP, hz, ciclos);
  printf("  %d periodos de portadora, %.3f ms de traza\n", periodos, t * 0.0625 / 1000.0);
  return 0;
}
