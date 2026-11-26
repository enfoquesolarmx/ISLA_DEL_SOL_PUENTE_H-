/*
 * =============================================================================
 *  dt_p9.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Lectura de P9 en A7 (ver dt_p9.h para el hardware, la tabla y las reglas)
 * -----------------------------------------------------------------------------
 *  RECURSOS QUE SON DE ESTE ARCHIVO: A7 (solo analógico, sin pull-up interno).
 *  Funciona igual en ATmega328P y ATmega4809 (sólo usa analogRead/millis).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include <Arduino.h>
#include "config_portadora.h"
#include "dt_p9.h"

static EstadoP9 estadoArranque = P9_AMBOS_ABIERTOS;

EstadoP9 p9_clasificar(uint16_t adc) {
  if (adc < P9_UMBRAL_FALLA)     return P9_FUERA_DE_RANGO;
  if (adc < P9_UMBRAL_AMBOS_GND) return P9_AMBOS_GND;
  if (adc < P9_UMBRAL_A_GND)     return P9_A_GND_B_ABIERTO;
  if (adc < P9_UMBRAL_B_GND)     return P9_A_ABIERTO_B_GND;
  return P9_AMBOS_ABIERTOS;
}

uint8_t p9_dt_cuentas(EstadoP9 e) {
  // Regla de seguridad: 500 ns sólo si las DOS ramas lo piden.
  return (e == P9_AMBOS_GND) ? DT_MODERNO_CUENTAS : DT_LEGACY_CUENTAS;
}

bool p9_permite_arrancar(EstadoP9 e) {
  return e != P9_FUERA_DE_RANGO;
}

const __FlashStringHelper *p9_texto(EstadoP9 e) {
  switch (e) {
    case P9_AMBOS_GND:       return F("P9_A GND, P9_B GND -> 500 ns");
    case P9_A_GND_B_ABIERTO: return F("P9_A GND, P9_B abierto -> MIXTO, se usa 1 us");
    case P9_A_ABIERTO_B_GND: return F("P9_A abierto, P9_B GND -> MIXTO, se usa 1 us");
    case P9_AMBOS_ABIERTOS:  return F("P9_A abierto, P9_B abierto -> 1 us");
    case P9_FUERA_DE_RANGO:  return F("FUERA DE RANGO (A7 < 1.5 V): falla de hardware");
    default:                 return F("INESTABLE (lecturas distintas) -> se usa 1 us");
  }
}

EstadoP9 p9_detectar(uint16_t *adc) {
  delay(P9_ESPERA_INICIAL_MS);                  // que se cargue C18
  uint16_t lectura = analogRead(A7);
  const EstadoP9 primero = p9_clasificar(lectura);
  EstadoP9 resultado = primero;
  for (uint8_t i = 1; i < P9_LECTURAS_ARRANQUE; i++) {
    delay(P9_SEPARACION_MS);
    lectura = analogRead(A7);
    if (p9_clasificar(lectura) != primero) resultado = P9_INESTABLE;
  }
  if (adc) *adc = lectura;
  // Para vigilar se guarda lo que realmente hay en A7 (no "INESTABLE").
  estadoArranque = (resultado == P9_INESTABLE) ? p9_clasificar(lectura) : resultado;
  return resultado;
}

bool p9_vigilar(uint16_t *adc, EstadoP9 *estado) {
  static uint32_t ultima = 0;
  static uint8_t  distintas = 0;
  if (millis() - ultima < P9_PERIODO_VIGILANCIA_MS) return false;
  ultima = millis();
  const uint16_t lectura = analogRead(A7);
  const EstadoP9 e = p9_clasificar(lectura);
  if (adc) *adc = lectura;
  if (estado) *estado = e;
  if (e == estadoArranque) { distintas = 0; return false; }
  if (++distintas >= P9_LECTURAS_CAMBIO) { distintas = 0; return true; }
  return false;
}
