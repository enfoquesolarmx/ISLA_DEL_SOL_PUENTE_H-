/*
 * =============================================================================
 *  prueba_p9.cpp  -  Isla del Sol, firmware del CRÍTICO
 *  Prueba en la PC del MISMO dt_p9.cpp que va al Nano.
 * -----------------------------------------------------------------------------
 *  Verifica:
 *    1. Umbrales: las 4 combinaciones de P9 se clasifican bien con TODAS las
 *       combinaciones de tolerancia de R12, R13, R14 (±1 % y ±5 %) y ±2 cuentas
 *       de error del ADC.
 *    2. Regla de seguridad: 500 ns SOLO con las dos ramas a GND.
 *    3. Arranque: lecturas que no coinciden -> INESTABLE -> 1 µs.
 *    4. Fuera de rango -> no arranca.
 *    5. Vigilancia: 1 o 2 lecturas distintas (ruido) no disparan; 3 seguidas sí.
 *
 *  Compilar y correr (desde firmware/critico):
 *    g++ -std=gnu++11 -O2 -D__AVR_ATmega328P__ -DF_CPU=16000000UL \
 *        -Ipruebas/avr_simulado -IISLA_CRITICO \
 *        pruebas/prueba_p9.cpp ISLA_CRITICO/dt_p9.cpp -o prueba_p9
 *    ./prueba_p9
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include <cmath>
#include <cstdio>
#include <vector>
#include "Arduino.h"
#include "config_portadora.h"
#include "dt_p9.h"

// ---- Arduino simulado: lecturas de A7 programadas y reloj ----
static std::vector<uint16_t> lecturas;
static size_t indice = 0;
static uint32_t ahora = 0;
uint16_t analogRead(uint8_t) { return lecturas.empty() ? 1023 : lecturas[indice < lecturas.size() ? indice++ : lecturas.size() - 1]; }
uint32_t millis() { return ahora; }
void delay(uint32_t ms) { ahora += ms; }

static int fallas = 0;
#define FALLA(...) do { printf("  FALLA: " __VA_ARGS__); printf("\n"); fallas++; } while (0)

// Voltaje en A7 para cada combinación (true = rama a GND).
static double vA7(bool aGnd, bool bGnd, double r12, double r13, double r14) {
  double g = 0;                       // conductancia a GND
  if (aGnd) g += 1.0 / r13;
  if (bGnd) g += 1.0 / r14;
  if (g == 0) return 5.0;
  const double rAbajo = 1.0 / g;
  return 5.0 * rAbajo / (r12 + rAbajo);
}

int main() {
  // ---------------- 1. Umbrales con tolerancias ----------------
  struct Caso { bool a, b; EstadoP9 esperado; const char *nombre; } casos[] = {
    {true,  true,  P9_AMBOS_GND,       "A GND, B GND"},
    {true,  false, P9_A_GND_B_ABIERTO, "A GND, B abierto"},
    {false, true,  P9_A_ABIERTO_B_GND, "A abierto, B GND"},
    {false, false, P9_AMBOS_ABIERTOS,  "A abierto, B abierto"},
  };
  printf("1. Umbrales: %u / %u / %u / %u cuentas\n", P9_UMBRAL_FALLA, P9_UMBRAL_AMBOS_GND, P9_UMBRAL_A_GND, P9_UMBRAL_B_GND);
  for (double tol : {0.01, 0.05}) {
    printf("   Resistencias ±%.0f %%:\n", tol * 100);
    for (auto &c : casos) {
      int mn = 2000, mx = -1;
      for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) {
        const double v = vA7(c.a, c.b, 10e3 * (1 + i * tol), 10e3 * (1 + j * tol), 20e3 * (1 + k * tol));
        const int adc0 = (int)lround(v / 5.0 * 1023);
        for (int e = -2; e <= 2; e++) {               // error del ADC
          int adc = adc0 + e; if (adc > 1023) adc = 1023; if (adc < 0) adc = 0;
          if (adc < mn) mn = adc;
          if (adc > mx) mx = adc;
          if (p9_clasificar((uint16_t)adc) != c.esperado) FALLA("%s con ADC %d se clasifica mal", c.nombre, adc);
        }
      }
      printf("     %-22s ADC %4d … %4d  ->  %s\n", c.nombre, mn, mx, (const char *)p9_texto(c.esperado));
    }
  }

  // ---------------- 2. Regla de seguridad ----------------
  printf("\n2. Regla de seguridad (500 ns sólo con las dos ramas a GND)\n");
  for (int e = P9_AMBOS_GND; e <= P9_INESTABLE; e++) {
    const uint8_t dt = p9_dt_cuentas((EstadoP9)e);
    const uint8_t esperado = (e == P9_AMBOS_GND) ? DT_MODERNO_CUENTAS : DT_LEGACY_CUENTAS;
    printf("   %-48s -> %2u cuentas  arranca: %s\n", (const char *)p9_texto((EstadoP9)e), dt,
           p9_permite_arrancar((EstadoP9)e) ? "sí" : "NO");
    if (dt != esperado) FALLA("estado %d da %u cuentas", e, dt);
  }
  if (p9_permite_arrancar(P9_FUERA_DE_RANGO)) FALLA("fuera de rango permite arrancar");

  // ---------------- 3. Arranque estable e inestable ----------------
  printf("\n3. Detección al arrancar\n");
  uint16_t adc;
  lecturas.assign(16, 409); indice = 0;
  EstadoP9 e = p9_detectar(&adc);
  printf("   16 lecturas de 409            -> %s\n", (const char *)p9_texto(e));
  if (e != P9_AMBOS_GND) FALLA("16 lecturas iguales no dieron AMBOS_GND");
  lecturas.assign(16, 409); lecturas[7] = 1023; indice = 0;
  e = p9_detectar(&adc);
  printf("   15 de 409 y una de 1023       -> %s\n", (const char *)p9_texto(e));
  if (e != P9_INESTABLE || p9_dt_cuentas(e) != DT_LEGACY_CUENTAS) FALLA("lectura distinta no dio INESTABLE con 1 µs");
  lecturas.assign(16, 150); indice = 0;
  e = p9_detectar(&adc);
  printf("   16 lecturas de 150            -> %s\n", (const char *)p9_texto(e));
  if (e != P9_FUERA_DE_RANGO || p9_permite_arrancar(e)) FALLA("A7 bajo no bloqueó el arranque");

  // ---------------- 4. Vigilancia ----------------
  printf("\n4. Vigilancia (arranque con las dos ramas a GND; lectura cada %u ms)\n", P9_PERIODO_VIGILANCIA_MS);
  lecturas.assign(16, 409); indice = 0; p9_detectar(&adc);
  auto correrVigilancia = [&](std::vector<uint16_t> seq) {
    lecturas = seq; indice = 0;
    EstadoP9 est; uint16_t a;
    for (size_t i = 0; i < seq.size(); i++) {
      ahora += P9_PERIODO_VIGILANCIA_MS;
      if (p9_vigilar(&a, &est)) return (int)i + 1;
    }
    return 0;
  };
  int n = correrVigilancia({409, 1023, 409, 409, 512, 512, 409, 409});
  printf("   ruido de 1 y 2 lecturas       -> %s\n", n ? "DISPARA" : "no dispara (correcto)");
  if (n) FALLA("la vigilancia disparó con ruido");
  n = correrVigilancia({409, 409, 512, 512, 512, 512});
  printf("   cambio a 512 (rama B suelta)  -> dispara en la lectura %d (%d ms)\n", n, n * P9_PERIODO_VIGILANCIA_MS);
  if (n != 5) FALLA("la vigilancia no disparó en la 3.ª lectura distinta");

  printf(fallas ? "\nRESULTADO: %d FALLAS\n" : "\nRESULTADO: todo correcto\n", fallas);
  return fallas ? 1 : 0;
}
