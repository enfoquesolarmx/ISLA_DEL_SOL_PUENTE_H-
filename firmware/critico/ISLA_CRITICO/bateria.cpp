/*
 *  bateria.cpp  -  Isla del Sol, CRÍTICO: voltaje de batería y límites
 *  (ver bateria.h y config_bateria.h)
 *  Licencia: MIT. Escrito desde cero.
 */
#include <Arduino.h>
#include "bateria.h"
#include "config_bateria.h"

static uint16_t cuentas = 0;
static EstadoBateria estado = BAT_NORMAL;
// Cronómetro de una condición: cuenta sólo mientras se cumple sin interrupción.
struct Cronometro { bool activo; uint32_t t0; };
static Cronometro cEmergencia, cBajo, cAlto, cVuelta;

static bool sostenida(Cronometro &c, bool seCumple, uint16_t ms) {
  if (!seCumple) { c.activo = false; return false; }
  if (!c.activo) { c.activo = true; c.t0 = millis(); }
  return millis() - c.t0 >= ms;
}

uint16_t bateria_cuentas_a_dv(uint16_t c) {
  return (uint16_t)(((uint32_t)c * BAT_DV_TOPE + 511) / 1023);
}

static void evaluar() {
  const uint16_t v = bateria_cuentas_a_dv(cuentas);
  if (cuentas >= BAT_CUENTAS_R4_ABIERTA) { estado = BAT_FUERA_DE_RANGO; return; }
  switch (estado) {
    case BAT_FUERA_DE_RANGO:
      return;                                       // sólo con reinicio
    case BAT_CORTE_BAJO:
      if (sostenida(cVuelta, v >= BAT_RECONEXION_DV, BAT_RETARDO_RECONEXION_MS)) estado = BAT_NORMAL;
      return;
    case BAT_CORTE_ALTO:
      if (sostenida(cVuelta, v <= BAT_ALTO_RECONEXION_DV, BAT_RETARDO_RECONEXION_MS)) estado = BAT_NORMAL;
      return;
    default: {                                      // NORMAL o AVISO: vigilar
      const bool emergencia = sostenida(cEmergencia, v < BAT_EMERGENCIA_DV, BAT_RETARDO_EMERGENCIA_MS);
      const bool bajo       = sostenida(cBajo,       v < BAT_CORTE_DV,      BAT_RETARDO_CORTE_MS);
      const bool alto       = sostenida(cAlto,       v > BAT_ALTO_CORTE_DV, BAT_RETARDO_ALTO_MS);
      cVuelta.activo = false;
      if (emergencia || bajo) estado = BAT_CORTE_BAJO;
      else if (alto)          estado = BAT_CORTE_ALTO;
      else                    estado = (v < BAT_AVISO_DV) ? BAT_AVISO : BAT_NORMAL;
      if (estado == BAT_CORTE_BAJO || estado == BAT_CORTE_ALTO)
        cEmergencia.activo = cBajo.activo = cAlto.activo = false;
    }
  }
}

static void leer() {
  uint16_t suma = 0;
  for (uint8_t i = 0; i < BAT_LECTURAS; i++) suma += analogRead(A3);
  cuentas = (suma + BAT_LECTURAS / 2) / BAT_LECTURAS;
}

void bateria_leer_ahora() {
  leer();
  // Al encender no se espera: si ya está debajo del corte o arriba del alto,
  // se arranca en corte (y el bajo necesitará la reconexión completa).
  const uint16_t v = bateria_cuentas_a_dv(cuentas);
  if (cuentas >= BAT_CUENTAS_R4_ABIERTA) estado = BAT_FUERA_DE_RANGO;
  else if (v < BAT_CORTE_DV)             estado = BAT_CORTE_BAJO;
  else if (v > BAT_ALTO_CORTE_DV)        estado = BAT_CORTE_ALTO;
  else                                   estado = (v < BAT_AVISO_DV) ? BAT_AVISO : BAT_NORMAL;
}

void bateria_atender() {
  static uint32_t t = 0;
  if (millis() - t < BAT_PERIODO_MS) return;
  t = millis();
  leer();
  evaluar();
}

EstadoBateria bateria_estado()   { return estado; }
bool bateria_permite_trabajar()  { return estado == BAT_NORMAL || estado == BAT_AVISO; }
uint16_t bateria_cuentas()       { return cuentas; }
uint16_t bateria_dv()            { return bateria_cuentas_a_dv(cuentas); }
bool     bateria_fuera_de_rango() { return estado == BAT_FUERA_DE_RANGO; }

const __FlashStringHelper *bateria_texto(EstadoBateria e) {
  switch (e) {
    case BAT_NORMAL:     return F("normal");
    case BAT_AVISO:      return F("AVISO: bateria baja");
    case BAT_CORTE_BAJO: return F("CORTE: bateria baja (espera reconexion)");
    case BAT_CORTE_ALTO: return F("CORTE: bateria alta");
    default:             return F("FUERA DE RANGO (R4 abierta?): reinicie tras revisar");
  }
}
