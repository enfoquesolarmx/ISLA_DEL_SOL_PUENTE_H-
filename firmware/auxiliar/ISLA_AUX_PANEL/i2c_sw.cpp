/*
 *  i2c_sw.cpp  -  Isla del Sol, AUX: maestro I2C por software (ver i2c_sw.h)
 *  Licencia: MIT. Escrito desde cero.
 */
#include <Arduino.h>
#include "config_panel.h"
#include "i2c_sw.h"

static inline void espera() { delayMicroseconds(I2C_SW_MEDIO_PERIODO_US); }
// "0": primero PORT en bajo (sin pull-up) y luego salida. "1": soltar.
static inline void bajar(uint8_t p)  { digitalWrite(p, LOW); pinMode(p, OUTPUT); }
static inline void soltar(uint8_t p) { pinMode(p, INPUT_PULLUP); }

// Suelta SCL y espera a que de verdad suba (el esclavo puede estirar el reloj).
static bool subirScl() {
  soltar(PIN_SCL_LCD);
  for (uint16_t t = 0; t < I2C_SW_ESPERA_SCL_US; t++) {
    if (digitalRead(PIN_SCL_LCD)) return true;
    delayMicroseconds(1);
  }
  return false;                       // SCL pegado a GND
}

static void inicio() {               // START: SDA baja con SCL arriba
  soltar(PIN_SDA_LCD); subirScl(); espera();
  bajar(PIN_SDA_LCD); espera();
  bajar(PIN_SCL_LCD); espera();
}
static void fin() {                  // STOP: SDA sube con SCL arriba
  bajar(PIN_SDA_LCD); espera();
  subirScl(); espera();
  soltar(PIN_SDA_LCD); espera();
}
static bool byteOut(uint8_t b) {     // MSB primero; devuelve true si hubo ACK
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80) soltar(PIN_SDA_LCD); else bajar(PIN_SDA_LCD);
    b <<= 1;
    espera();
    if (!subirScl()) return false;
    espera();
    bajar(PIN_SCL_LCD);
  }
  soltar(PIN_SDA_LCD); espera();      // noveno pulso: el esclavo contesta
  if (!subirScl()) return false;
  const bool ack = !digitalRead(PIN_SDA_LCD);
  espera();
  bajar(PIN_SCL_LCD); espera();
  return ack;
}

void i2c_sw_iniciar() { soltar(PIN_SDA_LCD); soltar(PIN_SCL_LCD); i2c_sw_recuperar(); }

void i2c_sw_recuperar() {
  soltar(PIN_SDA_LCD);
  for (uint8_t i = 0; i < 9 && !digitalRead(PIN_SDA_LCD); i++) {
    bajar(PIN_SCL_LCD); espera(); subirScl(); espera();
  }
  bajar(PIN_SCL_LCD); espera();
  fin();
}

bool i2c_sw_escribir(uint8_t dir, const uint8_t *datos, uint8_t n) {
  inicio();
  bool ok = byteOut(dir << 1);        // bit R/W = 0: escribir
  for (uint8_t i = 0; ok && i < n; i++) ok = byteOut(datos[i]);
  fin();
  return ok;
}

bool i2c_sw_hay_equipo(uint8_t dir) { return i2c_sw_escribir(dir, nullptr, 0); }
