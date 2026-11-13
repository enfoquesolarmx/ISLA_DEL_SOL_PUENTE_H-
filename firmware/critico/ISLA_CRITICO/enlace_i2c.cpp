/*
 * =============================================================================
 *  enlace_i2c.cpp  -  Isla del Sol, micro CRÍTICO (A1): esclavo I2C
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <util/atomic.h>
#include "enlace_protocolo.h"
#include "enlace_i2c.h"
#include "config_portadora.h"
#include "spwm.h"

static volatile uint8_t tx[ENLACE_LARGO_ESTADO];      // foto que se envía
static volatile uint8_t rx[ENLACE_LARGO_COMANDO];     // último comando recibido
static volatile bool    hayComando = false;
static uint8_t  secuencia = 0, eco = 0;
static uint16_t ok = 0, malos = 0;
static uint32_t tUltimoPing = 0;
static bool     auxPresente = false, auxCambio = false;

// ---- Interrupciones de I2C: sólo copian ----
static void alPedir() { Wire.write((const uint8_t *)tx, ENLACE_LARGO_ESTADO); }
static void alRecibir(int n) {
  uint8_t i = 0;
  while (Wire.available()) {
    const uint8_t b = Wire.read();
    if (i < ENLACE_LARGO_COMANDO) rx[i] = b;
    i++;
  }
  if (i == ENLACE_LARGO_COMANDO) hayComando = true;
  (void)n;
}

void enlace_iniciar() {
  Wire.begin(ENLACE_DIRECCION_CRITICO);   // esclavo
  Wire.onRequest(alPedir);
  Wire.onReceive(alRecibir);
}

void enlace_atender(uint8_t estadoP9, uint16_t adcP9, bool detenidoPorP9) {
  // 1) Comando pendiente: se valida aquí, no en la interrupción.
  if (hayComando) {
    uint8_t c[ENLACE_LARGO_COMANDO];
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      for (uint8_t i = 0; i < ENLACE_LARGO_COMANDO; i++) c[i] = rx[i];
      hayComando = false;
    }
    if (enlace_crc8(c, 3) == c[3] && c[0] == ENLACE_CMD_PING) {
      eco = c[1]; ok++; tUltimoPing = millis();
      if (!auxPresente) { auxPresente = true; auxCambio = true; }
    }
    else malos++;
  }
  // El AUX manda PING cada 100 ms: 500 ms sin PING = AUX perdido.
  if (auxPresente && millis() - tUltimoPing > 500) { auxPresente = false; auxCambio = true; }
  // Sin PING: se reinicia el TWI esclavo cada 1 s, por si quedó trabado a la
  // mitad de un byte sosteniendo SDA (desconexión en plena transferencia).
  static uint32_t tReinicio = 0;
  if (!auxPresente && millis() - tReinicio > 1000) {
    tReinicio = millis();
    Wire.end();
    enlace_iniciar();
  }
  // 2) Foto nueva cada 50 ms.
  static uint32_t t = 0;
  if (millis() - t < 50) return;
  t = millis();
  uint8_t f[ENLACE_LARGO_ESTADO];
  const uint16_t ciclos = spwm_ciclos(), amax = spwm_amplitud_max();
  f[0] = ENLACE_VERSION;  f[1] = ++secuencia;  f[2] = eco;
  f[3] = (spwm_activo() ? 1 : 0) | (detenidoPorP9 ? 2 : 0) | (spwm_frecuencia_hz() == LINEA_50_HZ ? 4 : 0);
  f[4] = estadoP9;  f[5] = spwm_dt_cuentas();
  f[6] = ciclos & 0xFF;  f[7] = ciclos >> 8;
  f[8] = adcP9 & 0xFF;   f[9] = adcP9 >> 8;
  f[10] = amax & 0xFF;   f[11] = amax >> 8;
  f[12] = malos & 0xFF;  f[13] = malos >> 8;
  f[14] = 0;
  f[15] = enlace_crc8(f, 15);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {          // la interrupción nunca ve media foto
    for (uint8_t i = 0; i < ENLACE_LARGO_ESTADO; i++) tx[i] = f[i];
  }
}

uint16_t enlace_comandos_ok()    { return ok; }
uint16_t enlace_comandos_malos() { return malos; }
uint8_t  enlace_ultimo_seq()     { return eco; }
bool enlace_aux_presente() { return auxPresente; }
bool enlace_aux_cambio() { const bool c = auxCambio; auxCambio = false; return c; }
