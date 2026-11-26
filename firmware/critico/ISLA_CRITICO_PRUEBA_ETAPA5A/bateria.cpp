/*
 *  bateria.cpp  -  Isla del Sol, CRÍTICO: voltaje de batería y límites
 *  (ver bateria.h y config_bateria.h)
 *  Licencia: MIT. Escrito desde cero.
 */
#include <Arduino.h>
#include "bateria.h"
#include "config_bateria.h"
#include "enlace_protocolo.h"     // enlace_crc8()
#include <avr/eeprom.h>

static uint8_t perfil = PERFIL_FABRICA;
static const LimitesBateria *lim = &LIMITES_BATERIA[PERFIL_FABRICA];

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
      if (sostenida(cVuelta, v >= lim->reconexion, BAT_RETARDO_RECONEXION_MS)) estado = BAT_NORMAL;
      return;
    case BAT_CORTE_ALTO:
      if (sostenida(cVuelta, v <= lim->altoVuelta, BAT_RETARDO_RECONEXION_MS)) estado = BAT_NORMAL;
      return;
    default: {                                      // NORMAL o AVISO: vigilar
      const bool emergencia = sostenida(cEmergencia, v < lim->emergencia, BAT_RETARDO_EMERGENCIA_MS);
      const bool bajo       = sostenida(cBajo,       v < lim->corte,      BAT_RETARDO_CORTE_MS);
      const bool alto       = sostenida(cAlto,       v > lim->alto, BAT_RETARDO_ALTO_MS);
      cVuelta.activo = false;
      if (emergencia || bajo) estado = BAT_CORTE_BAJO;
      else if (alto)          estado = BAT_CORTE_ALTO;
      else                    estado = (v < lim->aviso) ? BAT_AVISO : BAT_NORMAL;
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
  else if (v < lim->corte)             estado = BAT_CORTE_BAJO;
  else if (v > lim->alto)        estado = BAT_CORTE_ALTO;
  else                                   estado = (v < lim->aviso) ? BAT_AVISO : BAT_NORMAL;
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
void bateria_iniciar() {
  uint8_t e[3];
  eeprom_read_block(e, (const void *)BAT_EEPROM_DIR, 3);
  if (e[0] == 'B' && e[1] < PERFILES && enlace_crc8(e, 2) == e[2]) perfil = e[1];
  else perfil = PERFIL_FABRICA;            // EEPROM vacía o dañada
  lim = &LIMITES_BATERIA[perfil];
}

uint8_t bateria_perfil() { return perfil; }
const LimitesBateria &bateria_limites() { return *lim; }

RechazoPerfil bateria_cambiar_perfil(uint8_t p) {
  if (p >= PERFILES) return PERFIL_INVALIDO;
  leer();
  const uint16_t v = bateria_cuentas_a_dv(cuentas);
  const LimitesBateria &n = LIMITES_BATERIA[p];
  if (v < n.emergencia || v > n.alto) return PERFIL_NO_CABE;
  perfil = p;
  lim = &LIMITES_BATERIA[perfil];
  uint8_t e[3] = { 'B', perfil, 0 };
  e[2] = enlace_crc8(e, 2);
  eeprom_update_block(e, (void *)BAT_EEPROM_DIR, 3);   // sólo escribe lo que cambia
  cEmergencia.activo = cBajo.activo = cAlto.activo = cVuelta.activo = false;
  const bool fuera = (estado == BAT_FUERA_DE_RANGO);
  bateria_leer_ahora();                    // estado nuevo desde cero
  if (fuera) estado = BAT_FUERA_DE_RANGO;  // la falla del divisor sólo se borra con reinicio
  return PERFIL_OK;
}

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
