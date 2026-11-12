/*
 * =============================================================================
 *  ISLA_CRITICO.ino  -  Isla del Sol, micro CRÍTICO (A1) de la tarjeta de control
 * -----------------------------------------------------------------------------
 *  Archivo principal: sólo llama a los módulos. La lógica vive en cada
 *  archivo .h / .cpp de esta carpeta (ver README.md).
 *
 *  ESTADO: paso 1 - variables de la portadora (config_portadora.h).
 *    Todavía NO genera PWM. Al encender:
 *      1) pone las 4 salidas hacia los gate drivers en bajo (puente apagado);
 *      2) imprime por el puerto serie los valores calculados de la portadora
 *         para comprobarlos contra la tabla del README antes de seguir.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include "config_portadora.h"

// Salidas hacia los módulos gate driver (tarjeta de control, CRÍTICO).
// Mientras no exista spwm.cpp se fuerzan en bajo: puente H apagado.
constexpr uint8_t PIN_HA = 9;    // pierna A, lado alto (24 kHz)
constexpr uint8_t PIN_LA = 10;   // pierna A, lado bajo (24 kHz)
constexpr uint8_t PIN_HB = 11;   // pierna B, lado alto (60 Hz)
constexpr uint8_t PIN_LB = 3;    // pierna B, lado bajo (60 Hz)

static void salidasSeguras() {
  // Primero el nivel, luego la dirección: el pin nunca sale en alto.
  digitalWrite(PIN_HA, LOW); pinMode(PIN_HA, OUTPUT);
  digitalWrite(PIN_LA, LOW); pinMode(PIN_LA, OUTPUT);
  digitalWrite(PIN_HB, LOW); pinMode(PIN_HB, OUTPUT);
  digitalWrite(PIN_LB, LOW); pinMode(PIN_LB, OUTPUT);
}

// Imprime un valor en centésimas o milésimas como decimal, sin usar float.
static void imprimirFijo(uint32_t v, uint16_t div) {
  Serial.print(v / div);
  Serial.print('.');
  uint32_t r = v % div;
  for (uint16_t d = div / 10; d > 0; d /= 10) { Serial.print((uint8_t)(r / d)); r %= d; }
}

static void reportePortadora() {
  Serial.println(F("=== Isla del Sol - CRITICO - portadora SPWM ==="));
#if ISLA_CHIP_328P
  Serial.println(F("Chip: ATmega328P (Timer1 phase-correct)"));
#else
  Serial.println(F("Chip: ATmega4809 (TCA0 dual-slope)"));
#endif
  Serial.print(F("Reloj (Hz):              ")); Serial.println(RELOJ_HZ);
  Serial.print(F("TOP (cuentas):           ")); Serial.println(TOP_CUENTAS);
  Serial.print(F("Ciclos CPU por periodo:  ")); Serial.println(CICLOS_CPU_PERIODO);
  Serial.print(F("Portadora real (Hz):     ")); imprimirFijo(FSW_REAL_CHZ, 100); Serial.println();
  Serial.print(F("Muestras por ciclo:      ")); Serial.println(MUESTRAS_CICLO);
  Serial.print(F("Salida (Hz):             ")); imprimirFijo(F_SALIDA_MHZ, 1000); Serial.println();
  Serial.print(F("Error de frecuencia ppm: ")); Serial.println(ERROR_FRECUENCIA_PPM);
  Serial.print(F("ns por cuenta:           ")); imprimirFijo(NS_X10_POR_CUENTA, 10); Serial.println();
  Serial.print(F("DT 500 ns (cuentas):     ")); Serial.println(DT_MODERNO_CUENTAS);
  Serial.print(F("DT 1 us   (cuentas):     ")); Serial.println(DT_LEGACY_CUENTAS);
  Serial.print(F("Comparador alto max:     ")); Serial.println(COMPARADOR_ALTO_MAX);
}

void setup() {
  salidasSeguras();          // 1) Puente apagado antes que nada
  Serial.begin(115200);      // 2) Reporte de arranque (Monitor Serie a 115200)
  reportePortadora();
}

void loop() {
}
