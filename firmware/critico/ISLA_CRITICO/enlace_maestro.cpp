/*
 * =============================================================================
 *  enlace_maestro.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del enlace con el CRÍTICO de maestro. Ver enlace_maestro.h.
 * =============================================================================
 */

#include "enlace_maestro.h"
#include "enlace_protocolo.h"
#include "spwm.h"
#include "config_spwm.h"

#include <Arduino.h>
#include <Wire.h>

// ============================== ESTADO =======================================

static uint16_t ok, malos, sinResp, usMax;
static uint8_t  secuencia;        // va subiendo en cada foto
static uint8_t  eco;              // último seq que el AUX dice haber visto
static bool     auxPresente;
static bool     auxCambio;
static bool     hayPerfil;
static uint8_t  perfilPedido;
static uint32_t tUltimaBuena;
static uint32_t tEnvio;

// ============================== PÚBLICAS =====================================

void enlace_iniciar(void)
{
  Wire.begin();                   // sin dirección = MAESTRO
  Wire.setClock(100000);

  /*  Plazo del bus: si el esclavo estira el reloj más de 3 ms, se abandona la
   *  transferencia en vez de colgarse para siempre. Siendo maestro esto sí
   *  funciona, porque el reloj es nuestro.
   */
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);
#endif

  ok = malos = sinResp = usMax = 0;
  secuencia = eco = 0;
  auxPresente = false;
  auxCambio = hayPerfil = false;
  tUltimaBuena = tEnvio = millis();
}

void enlace_atender(uint8_t estadoP9, uint16_t adcP9, bool detenidoPorP9,
                    uint16_t bateriaDv, uint8_t estadoBateria,
                    uint8_t perfilBateria, bool perfilRechazado)
{
  if (millis() - tEnvio < ENLACE_MS_ENTRE_ENVIOS) {
    // Entre envíos sólo se vigila la presencia del AUX.
    if (auxPresente && millis() - tUltimaBuena > 500) {
      auxPresente = false;
      auxCambio = true;
    }
    return;
  }
  tEnvio = millis();

  /*  La foto se arma AQUÍ, en una variable local, y se manda en seguida.
   *
   *  Ésa es la diferencia que importa: siendo esclavo, esta foto vivía en un
   *  búfer que la interrupción podía leer en cualquier momento, así que había
   *  que copiarla con las interrupciones apagadas — 212 ciclos, el bloqueo
   *  más largo de todo el sistema. Aquí nadie más la lee: no hay carrera que
   *  proteger y no hace falta ningún cli().
   */
  uint8_t f[ENLACE_LARGO_ESTADO];
  const uint16_t ciclos = spwm_ciclos(), amax = spwm_amplitud_max();

  f[0] = ENLACE_VERSION;
  f[1] = ++secuencia;
  f[2] = eco;
  f[3] = (spwm_activo() ? 1 : 0) | (detenidoPorP9 ? 2 : 0)
       | (spwm_frecuencia_hz() == LINEA_50_HZ ? 4 : 0)
       | ((perfilBateria & 3) << 4) | (perfilRechazado ? 0x40 : 0);
  f[4] = estadoP9;
  f[5] = spwm_dt_cuentas();
  f[6] = ciclos & 0xFF;      f[7] = ciclos >> 8;
  f[8] = adcP9 & 0xFF;       f[9] = adcP9 >> 8;
  f[10] = amax & 0xFF;       f[11] = amax >> 8;
  f[12] = bateriaDv & 0xFF;  f[13] = bateriaDv >> 8;
  f[14] = estadoBateria;
  f[15] = enlace_crc8(f, 15);

  const uint32_t t0 = micros();

  // --- 1) la foto al AUX -----------------------------------------------
  Wire.beginTransmission(ENLACE_DIRECCION_AUX);
  Wire.write(f, ENLACE_LARGO_ESTADO);
  if (Wire.endTransmission() != 0) {        // sin ACK: el AUX no está
    sinResp++;
    return;
  }

  // --- 2) el comando de vuelta ------------------------------------------
  const uint8_t n = Wire.requestFrom((uint8_t)ENLACE_DIRECCION_AUX,
                                     (uint8_t)ENLACE_LARGO_COMANDO);
  if (n != ENLACE_LARGO_COMANDO) { sinResp++; return; }

  uint8_t c[ENLACE_LARGO_COMANDO];
  for (uint8_t i = 0; i < ENLACE_LARGO_COMANDO; i++) c[i] = (uint8_t)Wire.read();

  if (enlace_crc8(c, ENLACE_LARGO_COMANDO - 1) != c[ENLACE_LARGO_COMANDO - 1]) {
    malos++;
    return;
  }

  // Un perfil nuevo sólo se acepta si cambió el número de secuencia, para que
  // el reenvío del mismo comando no vuelva a escribir la EEPROM.
  if (c[0] == ENLACE_CMD_PERFIL && c[1] != eco) {
    perfilPedido = c[2];
    hayPerfil = true;
  }
  eco = c[1];

  ok++;
  tUltimaBuena = millis();
  if (!auxPresente) { auxPresente = true; auxCambio = true; }

  const uint16_t us = (uint16_t)(micros() - t0);
  if (us > usMax) usMax = us;
}

uint16_t enlace_envios_ok(void)     { return ok; }
uint16_t enlace_envios_malos(void)  { return malos; }
uint16_t enlace_sin_respuesta(void) { return sinResp; }
uint16_t enlace_us_max(void)        { return usMax; }
bool     enlace_aux_presente(void)  { return auxPresente; }
uint8_t  enlace_ultimo_seq(void)    { return eco; }

bool enlace_aux_cambio(void) { const bool c = auxCambio; auxCambio = false; return c; }

bool enlace_pedido_perfil(uint8_t *p)
{
  if (!hayPerfil) return false;
  hayPerfil = false;
  *p = perfilPedido;
  return true;
}

void enlace_reiniciar_contadores(void) { ok = malos = sinResp = usMax = 0; }
