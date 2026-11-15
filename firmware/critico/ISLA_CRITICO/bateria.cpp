/*
 *  bateria.cpp  -  Isla del Sol, CRÍTICO: voltaje de batería (ver bateria.h)
 *  Licencia: MIT. Escrito desde cero.
 */
#include <Arduino.h>
#include "bateria.h"

static uint16_t cuentas = 0;

uint16_t bateria_cuentas_a_dv(uint16_t c) {
  return (uint16_t)(((uint32_t)c * BAT_DV_TOPE + 511) / 1023);
}

void bateria_atender() {
  static uint32_t t = 0;
  if (millis() - t < BAT_PERIODO_MS) return;
  t = millis();
  uint16_t suma = 0;
  for (uint8_t i = 0; i < BAT_LECTURAS; i++) suma += analogRead(A3);
  cuentas = (suma + BAT_LECTURAS / 2) / BAT_LECTURAS;
}

uint16_t bateria_cuentas()       { return cuentas; }
uint16_t bateria_dv()            { return bateria_cuentas_a_dv(cuentas); }
bool     bateria_fuera_de_rango() { return cuentas >= BAT_CUENTAS_R4_ABIERTA; }
