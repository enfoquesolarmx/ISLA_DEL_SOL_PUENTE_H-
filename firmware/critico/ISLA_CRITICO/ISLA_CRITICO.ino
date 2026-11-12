/*
 * =============================================================================
 *  ISLA_CRITICO.ino  -  Isla del Sol, micro CRÍTICO (A1) de la tarjeta de control
 * -----------------------------------------------------------------------------
 *  Archivo principal: sólo llama a los módulos. La lógica vive en cada
 *  archivo .h / .cpp de esta carpeta (ver README.md).
 *
 *  ESTADO: paso 2 - SPWM híbrida, PRUEBA DE BANCO SIN POTENCIA.
 *    Al encender:
 *      1) puente apagado: D9, D10, D11, D3 en bajo y D12 (EN_HW) = 0;
 *      2) reporte por el puerto serie de los valores de la portadora;
 *      3) arranca la SPWM a la frecuencia de FRECUENCIA_LINEA_HZ (60 o 50 Hz,
 *         en config_portadora.h) con tiempo muerto de 1 µs (el seguro: todavía
 *         no se lee P9) y sube la amplitud en rampa hasta el máximo en ~1 s;
 *      4) cada 10 s imprime los ciclos generados (~600 a 60 Hz, ~500 a 50 Hz).
 *
 *    D12 SE QUEDA EN 0 TODO EL TIEMPO: las señales se ven en los pines del Nano
 *    pero la compuerta AND (74HC08) de la tarjeta de control no las deja pasar
 *    a los gate drivers. Esta versión NO puede mover el puente H.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include "config_portadora.h"
#include "config_spwm.h"
#include "spwm.h"

// Rampa de prueba de banco: cuentas que sube la amplitud en cada ciclo de línea.
// 313 cuentas / 5 por ciclo = 63 ciclos ≈ 1 s a 60 Hz (1.3 s a 50 Hz). NO es el arranque suave final
// (ése irá con el lazo de voltaje en ac_fb_inversor).
constexpr uint8_t RAMPA_BANCO_CUENTAS_POR_CICLO = 5;

// Imprime un valor en décimas, centésimas o milésimas sin usar float.
static void imprimirFijo(uint32_t v, uint16_t div) {
  Serial.print(v / div);
  Serial.print('.');
  uint32_t r = v % div;
  for (uint16_t d = div / 10; d > 0; d /= 10) { Serial.print((uint8_t)(r / d)); r %= d; }
}

static void reportePortadora() {
  Serial.println(F("=== Isla del Sol - CRITICO - SPWM hibrida (banco) ==="));
#if ISLA_CHIP_328P
  Serial.println(F("Chip: ATmega328P (Timer1 phase-correct)"));
#else
  Serial.println(F("Chip: ATmega4809 (TCA0 dual-slope)"));
#endif
  Serial.print(F("Reloj (Hz):              ")); Serial.println(RELOJ_HZ);
  Serial.print(F("TOP (cuentas):           ")); Serial.println(TOP_CUENTAS);
  Serial.print(F("Ciclos CPU por periodo:  ")); Serial.println(CICLOS_CPU_PERIODO);
  Serial.print(F("Portadora real (Hz):     ")); imprimirFijo(FSW_REAL_CHZ, 100); Serial.println();
  Serial.print(F("Frecuencia de linea:     ")); Serial.print(spwm_frecuencia_hz()); Serial.println(F(" Hz"));
  Serial.print(F("Muestras por ciclo:      ")); Serial.println(spwm_muestras_ciclo());
  Serial.print(F("Salida (Hz):             "));
  imprimirFijo(spwm_frecuencia_hz() == LINEA_50_HZ ? F_SALIDA_50_MHZ : F_SALIDA_60_MHZ, 1000);
  Serial.println();
  Serial.print(F("Error de frecuencia ppm: ")); Serial.println(ERROR_FRECUENCIA_PPM);
  Serial.print(F("ns por cuenta:           ")); imprimirFijo(NS_X10_POR_CUENTA, 10); Serial.println();
  Serial.print(F("DT 500 ns (cuentas):     ")); Serial.println(DT_MODERNO_CUENTAS);
  Serial.print(F("DT 1 us   (cuentas):     ")); Serial.println(DT_LEGACY_CUENTAS);
  Serial.print(F("Pulso minimo (cuentas):  ")); Serial.println(PULSO_MIN_CUENTAS);
  Serial.print(F("Amplitud max (cuentas):  ")); Serial.println(AMPLITUD_MAX_CUENTAS);
  Serial.print(F("Indice m maximo:         ")); imprimirFijo(M_MAX_MILESIMAS, 1000); Serial.println();
  Serial.print(F("DT en uso (cuentas):     ")); Serial.println(spwm_dt_cuentas());
  Serial.println(F("D12 (EN_HW) = 0: puente bloqueado, prueba sin potencia"));
}

void setup() {
  // 1) Puente apagado y D12 = 0 antes que nada; tiempo muerto seguro y la
  //    frecuencia de línea elegida en config_portadora.h.
  const bool frecuenciaValida = spwm_preparar(DT_LEGACY_CUENTAS, FRECUENCIA_LINEA_HZ);
  Serial.begin(115200);               // 2) Reporte (Monitor Serie a 115200)
  reportePortadora();
  if (!frecuenciaValida) Serial.println(F("AVISO: frecuencia invalida, se usa 60 Hz"));
  spwm_arrancar();                    // 3) SPWM con amplitud 0
}

void loop() {
  // Rampa de banco: sube la amplitud una vez por ciclo de línea.
  static uint16_t cicloAnterior = 0;
  static uint16_t amplitud = 0;
  const uint16_t c = spwm_ciclos();
  if (c != cicloAnterior) {
    cicloAnterior = c;
    if (amplitud < AMPLITUD_MAX_CUENTAS) {
      amplitud += RAMPA_BANCO_CUENTAS_POR_CICLO;
      spwm_fijar_amplitud(amplitud);   // se recorta solo al máximo
    }
  }

  // Verificación de frecuencia: ciclos en ventanas de 10 s
  // (600 o 601 a 60 Hz; 500 o 501 a 50 Hz).
  static uint32_t t0 = millis();
  static uint16_t c0 = 0;
  if (millis() - t0 >= 10000UL) {
    t0 += 10000UL;
    Serial.print(F("Ciclos en 10 s: "));
    Serial.println((uint16_t)(c - c0));
    c0 = c;
  }
}
