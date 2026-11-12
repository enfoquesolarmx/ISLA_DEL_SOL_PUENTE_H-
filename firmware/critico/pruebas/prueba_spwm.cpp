/*
 * =============================================================================
 *  prueba_spwm.cpp  -  Isla del Sol, firmware del CRÍTICO
 *  Prueba en la PC del MISMO spwm_328p.cpp que va al Nano.
 * -----------------------------------------------------------------------------
 *  Cómo funciona
 *    - Los registros del ATmega328P se simulan con variables (avr_simulado/).
 *    - Cada periodo de portadora se recorre cuenta por cuenta (1 cuenta = 1 ciclo
 *      de reloj = 62.5 ns) aplicando la regla del comparador de la hoja de datos
 *      del ATmega328P, modo 8 (Phase and Frequency Correct):
 *        OC1A no invertida: se APAGA al coincidir subiendo, se PRENDE bajando.
 *        OC1B invertida:    se PRENDE al coincidir subiendo, se APAGA bajando.
 *        OCR = 0 o TOP: salida fija (sin pulsos).
 *        OCR1A/OCR1B con doble búfer: lo escrito se aplica en el siguiente BOTTOM.
 *    - La ISR se llama en cada BOTTOM. La pierna B cambia RETARDO_ISR cuentas
 *      después (latencia de la interrupción).
 *
 *  Qué verifica (si algo falla, termina con código 1)
 *    1. HA y LA nunca encendidos a la vez (corto de la pierna A).
 *    2. HB y LB nunca encendidos a la vez (corto de la pierna B).
 *    3. Tiempo muerto en cada flanco de la pierna A >= DT pedido.
 *    4. Tiempo muerto de la pierna B >= 1 periodo.
 *    5. Ningún pulso de la pierna A más angosto que el pulso mínimo.
 *    6. Sin componente de CD por ciclo (los dos semiciclos iguales).
 *    7. Fundamental de la salida = amplitud esperada (error < 1 %).
 *    8. Apagado: D12 = 0 y las 4 salidas en bajo.
 *    Todo con 60 Hz (400 muestras) y con 50 Hz (480 muestras); una frecuencia
 *    inválida debe rechazarse y caer en 60 Hz.
 *
 *  Compilar y correr (desde firmware/critico):
 *    g++ -std=gnu++11 -O2 -D__AVR_ATmega328P__ -DF_CPU=16000000UL \
 *        -Ipruebas/avr_simulado -IISLA_CRITICO \
 *        pruebas/prueba_spwm.cpp ISLA_CRITICO/spwm_328p.cpp -o prueba_spwm
 *    ./prueba_spwm
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "avr/io.h"
#include "spwm.h"

volatile uint8_t  PORTB, PORTD, DDRB, DDRD;
volatile uint8_t  TCCR1A, TCCR1B, TIMSK1, TIFR1;
volatile uint16_t TCNT1, ICR1, OCR1A, OCR1B;
extern "C" void TIMER1_OVF_vect(void);

static const int RETARDO_ISR = 40;   // cuentas desde BOTTOM hasta que la ISR mueve la pierna B
static int fallas = 0;
#define FALLA(...) do { if (fallas < 10) { printf("  FALLA: " __VA_ARGS__); printf("\n"); } fallas++; } while (0)

struct Resultado { int dtMinA; int dtMinB; int pulsoMin; double cdMax; double errFund; };

// Corre la SPWM 'ciclos' ciclos de línea con amplitud fija y revisa todo.
static Resultado correr(uint8_t dtPedido, uint16_t amp, int ciclos, uint8_t hz) {
  const int TOP = TOP_CUENTAS;
  if (!spwm_preparar(dtPedido, hz)) FALLA("spwm_preparar rechazó %u Hz", hz);
  const int N = spwm_muestras_ciclo();
  if (N != (hz == 50 ? 480 : 400)) FALLA("%u Hz con %d muestras por ciclo", hz, N);
  spwm_arrancar();
  uint16_t actA = OCR1A, actB = OCR1B;        // valores vigentes del periodo
  bool ha = false, la = false, hb = false, lb = false;
  long t = 0;
  long ultBajaHA = -100000, ultBajaLA = -100000, ultBajaHB = -100000, ultBajaLB = -100000;
  long subeHA = -1, subeLA = -1;
  Resultado r = {1 << 30, 1 << 30, 1 << 30, 0, 0};
  // Promedio de salida por periodo (VA - VB, en fracción de Vbus).
  const int periodos = ciclos * N + 2 * N;
  double *vper = new double[periodos];
  bool ampFijada = false;

  for (int p = 0; p < periodos; p++) {
    if (p > 0) {                               // BOTTOM: búfer -> vigente, y la ISR
      actA = OCR1A; actB = OCR1B;
      TIMER1_OVF_vect();
      if (!ampFijada) { spwm_fijar_amplitud(amp); ampFijada = true; }
    }
    const bool hbN = PORTB & _BV(PB3), lbN = PORTD & _BV(PD3);
    double vSuma = 0;
    for (int c = 0; c < 2 * TOP; c++, t++) {
      // Pierna B (software): cambia RETARDO_ISR cuentas después de BOTTOM.
      if (c == RETARDO_ISR || (p == 0 && c == 0)) {
        if (hb && !hbN) ultBajaHB = t;
        if (lb && !lbN) ultBajaLB = t;
        if (!hb && hbN && t - ultBajaLB < r.dtMinB) r.dtMinB = t - ultBajaLB;
        if (!lb && lbN && t - ultBajaHB < r.dtMinB) r.dtMinB = t - ultBajaHB;
        hb = hbN; lb = lbN;
      }
      // Pierna A (hardware del timer), regla del comparador.
      const bool subiendo = c < TOP;
      const int tcnt = subiendo ? c : 2 * TOP - c;
      bool haN = ha, laN = la;
      if (actA == 0) haN = false;
      else if (actA >= TOP) haN = true;
      else if (tcnt == actA) haN = !subiendo;            // subiendo: apaga; bajando: prende
      if (actB >= TOP) laN = false;
      else if (actB == 0) laN = true;
      else if (tcnt == actB) laN = subiendo;             // invertida: subiendo prende
      if (ha && !haN) { ultBajaHA = t; if (t - subeHA < r.pulsoMin) r.pulsoMin = t - subeHA; }
      if (la && !laN) { ultBajaLA = t; if (t - subeLA < r.pulsoMin) r.pulsoMin = t - subeLA; }
      if (!ha && haN) { subeHA = t; if (t - ultBajaLA < r.dtMinA) r.dtMinA = t - ultBajaLA; }
      if (!la && laN) { subeLA = t; if (t - ultBajaHA < r.dtMinA) r.dtMinA = t - ultBajaHA; }
      ha = haN; la = laN;
      if (ha && la) FALLA("HA y LA encendidos juntos (periodo %d, cuenta %d)", p, c);
      if (hb && lb) FALLA("HB y LB encendidos juntos (periodo %d, cuenta %d)", p, c);
      // Voltaje de salida con carga resistiva (corriente en fase con el voltaje):
      //   pierna A: 1 con HA, 0 con LA. Con los dos apagados (tiempo muerto)
      //   conduce el diodo del lado contrario a la corriente: en el semiciclo +
      //   (LB encendido) la corriente sale de la pierna A -> diodo de abajo -> 0;
      //   en el semiciclo - (HB encendido) entra -> diodo de arriba -> 1.
      //   Pierna B abierta (cruce por cero): sin corriente -> salida 0.
      double v = 0;
      if (hb || lb) {
        const double vb = hb ? 1.0 : 0.0;
        const double va = ha ? 1.0 : (la ? 0.0 : vb);
        v = va - vb;
      }
      vSuma += v;
    }
    vper[p] = vSuma / (2 * TOP);
  }

  // Ciclos completos con la amplitud ya aplicada: desde el 2º ciclo.
  double cdMax = 0, a1 = 0, b1 = 0;
  const int ini = 2 * N + 1;   // +1: el ciclo empieza en el periodo 0 del semiciclo +
  for (int k = 0; k < ciclos - 1; k++) {
    double s = 0;
    for (int i = 0; i < N; i++) s += vper[ini + k * N + i];
    if (std::fabs(s / N) > cdMax) cdMax = std::fabs(s / N);
  }
  for (int i = 0; i < N; i++) {
    const double th = 2 * M_PI * (i + 0.5) / N;
    a1 += vper[ini + i] * std::sin(th);
    b1 += vper[ini + i] * std::cos(th);
  }
  const double fund = 2.0 * std::sqrt(a1 * a1 + b1 * b1) / N;
  const double esperado = (double)(amp > AMPLITUD_MAX_CUENTAS ? AMPLITUD_MAX_CUENTAS : amp) / TOP;
  r.cdMax = cdMax;
  r.errFund = esperado > 0 ? (fund - esperado) / esperado : fund;
  delete[] vper;

  spwm_detener();
  if (PORTB & (_BV(PB1) | _BV(PB2) | _BV(PB3) | _BV(PB4))) FALLA("apagado: algún pin de PORTB quedó en alto");
  if (PORTD & _BV(PD3)) FALLA("apagado: LB quedó en alto");
  if (TCCR1A != 0 || TCCR1B != 0) FALLA("apagado: el timer sigue conectado o corriendo");
  return r;
}

int main() {
  printf("Prueba de spwm_328p.cpp - TOP %d, DT %d/%d cuentas, pulso min %d, amplitud max %d\n",
         TOP_CUENTAS, DT_MODERNO_CUENTAS, DT_LEGACY_CUENTAS, PULSO_MIN_CUENTAS, AMPLITUD_MAX_CUENTAS);
  printf("%-5s %-8s %-6s %-9s %-9s %-10s %-10s %-9s\n",
         "Hz", "DT", "amp", "DTmin A", "DTmin B", "pulso min", "CD max", "err fund");
  const uint8_t dts[] = {DT_MODERNO_CUENTAS, DT_LEGACY_CUENTAS, 3};   // 3 = valor inválido -> legacy
  const uint16_t amps[] = {0, 20, 100, 200, AMPLITUD_MAX_CUENTAS, 400};
  const uint8_t hzs[] = {LINEA_60_HZ, LINEA_50_HZ};
  for (uint8_t hz : hzs)
  for (uint8_t dt : dts) {
    for (uint16_t amp : amps) {
      Resultado r = correr(dt, amp, 4, hz);
      const int dtEsperado = (dt == DT_MODERNO_CUENTAS) ? DT_MODERNO_CUENTAS : DT_LEGACY_CUENTAS;
      printf("%-5u %-8d %-6u %-9d %-9d %-10d %-10.5f %+.3f %%\n", hz, dt, amp, r.dtMinA, r.dtMinB,
             r.pulsoMin, r.cdMax, 100 * r.errFund);
      if (r.dtMinA < dtEsperado) FALLA("tiempo muerto de la pierna A de %d cuentas (< %d)", r.dtMinA, dtEsperado);
      if (r.dtMinB < 2 * TOP_CUENTAS - 1) FALLA("tiempo muerto de la pierna B de %d cuentas", r.dtMinB);
      if (r.pulsoMin < PULSO_MIN_CUENTAS) FALLA("pulso de %d cuentas (< %d)", r.pulsoMin, PULSO_MIN_CUENTAS);
      if (r.cdMax > 1e-3) FALLA("componente de CD de %.5f", r.cdMax);
      if (amp >= 100 && std::fabs(r.errFund) > 0.01) FALLA("fundamental con error de %.2f %%", 100 * r.errFund);
    }
  }
  // Frecuencia inválida: debe rechazarla y quedarse en 60 Hz.
  if (spwm_preparar(DT_LEGACY_CUENTAS, 55) || spwm_frecuencia_hz() != LINEA_60_HZ || spwm_muestras_ciclo() != 400)
    FALLA("55 Hz no se rechazó o no cayó en 60 Hz");
  else printf("\nFrecuencia inválida (55 Hz): rechazada, queda en 60 Hz (correcto)\n");
  printf(fallas ? "\nRESULTADO: %d FALLAS\n" : "\nRESULTADO: todo correcto\n", fallas);
  return fallas ? 1 : 0;
}
