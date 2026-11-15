/*
 *  botonera.cpp  -  Isla del Sol, AUX: botonera en A7 (ver botonera.h)
 *  Licencia: MIT. Escrito desde cero.
 */
#include <Arduino.h>
#include "config_panel.h"
#include "botonera.h"

static uint16_t adc = 1023;

Boton botonera_clasificar(uint16_t a) {
  if (a < BOT_UMBRAL_FALLA) return BOT_FALLA;
  if (a < BOT_UMBRAL_B1)    return BOT_ATRAS;
  if (a < BOT_UMBRAL_B2)    return BOT_ARRIBA;
  if (a < BOT_UMBRAL_B3)    return BOT_ABAJO;
  if (a < BOT_UMBRAL_B4)    return BOT_OK;
  return BOT_NINGUNO;
}

EventoBoton botonera_evento() {
  static uint32_t tLectura = 0, tOk = 0;
  static Boton candidato = BOT_NINGUNO, estable = BOT_NINGUNO;
  static uint8_t iguales = 0;
  static bool setupDado = false;

  if (millis() - tLectura < BOT_PERIODO_MS) {
    // Entre lecturas sólo se vigila el sostenido de OK.
    if (estable == BOT_OK && !setupDado && millis() - tOk >= BOT_SETUP_MS) { setupDado = true; return EV_SETUP; }
    return EV_NADA;
  }
  tLectura = millis();
  adc = analogRead(PIN_BOTONES);
  const Boton b = botonera_clasificar(adc);
  if (b != candidato) { candidato = b; iguales = 1; return EV_NADA; }
  if (iguales < BOT_LECTURAS_IGUALES) iguales++;
  if (iguales < BOT_LECTURAS_IGUALES || b == estable) return EV_NADA;

  // Cambio confirmado de estable -> b
  const Boton antes = estable;
  estable = b;
  if (antes == BOT_OK && !setupDado) return EV_OK;   // OK corto: al soltar
  switch (b) {
    case BOT_ATRAS:  return EV_ATRAS;
    case BOT_ARRIBA: return EV_ARRIBA;
    case BOT_ABAJO:  return EV_ABAJO;
    case BOT_OK:     tOk = millis(); setupDado = false; return EV_NADA;
    case BOT_FALLA:  return EV_FALLA;
    default:         return EV_NADA;
  }
}

uint16_t botonera_adc() { return adc; }
