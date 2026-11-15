/*
 *  enlace_aux.cpp  -  Isla del Sol, AUX: maestro I2C (ver enlace_aux.h)
 *  Licencia: MIT. Escrito desde cero (misma lógica que ISLA_AUX_PRUEBA_I2C).
 */
#include <Arduino.h>
#include <Wire.h>
#include "enlace_protocolo.h"
#include "enlace_aux.h"

constexpr uint16_t PERIODO_MS = 100;
constexpr uint8_t  FALLAS_PARA_PERDIDO = 3;
constexpr uint16_t MS_CONGELADO = 300, MS_FOTO_VIEJA = 300;

static uint8_t  seq = 0, seqCritico = 0, fallas = 0;
static uint8_t  f[ENLACE_LARGO_ESTADO];
static uint32_t tFoto = 0, tSeqCambio = 0, ok = 0, total = 0;
static EstadoEnlace estado = ENLACE_NUNCA;
static bool    hayPerfil = false;
static uint8_t perfilAEnviar = 0;

static void iniciarWire() {
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(3000, true);   // nunca colgarse si el bus se traba
}

// "Bus clear" (NXP UM10204): hasta 9 pulsos de SCL para que un esclavo
// suelte SDA, luego STOP y se reinicia el TWI.
static void recuperarBus() {
  Wire.end();
  pinMode(A4, INPUT_PULLUP); pinMode(A5, INPUT_PULLUP);
  delayMicroseconds(10);
  for (uint8_t i = 0; i < 9 && digitalRead(A4) == LOW; i++) {
    pinMode(A5, OUTPUT); digitalWrite(A5, LOW); delayMicroseconds(5);
    pinMode(A5, INPUT_PULLUP); delayMicroseconds(5);
  }
  pinMode(A4, OUTPUT); digitalWrite(A4, LOW); delayMicroseconds(5);
  pinMode(A4, INPUT_PULLUP); delayMicroseconds(5);
  iniciarWire();
}

void enlace_aux_iniciar() { iniciarWire(); }

bool enlace_aux_atender() {
  static uint32_t t = 0;
  if (millis() - t < PERIODO_MS) return false;
  t = millis();
  total++;
  uint8_t c[ENLACE_LARGO_COMANDO] = { ENLACE_CMD_PING, ++seq, 0, 0 };
  if (hayPerfil) { c[0] = ENLACE_CMD_PERFIL; c[2] = perfilAEnviar; hayPerfil = false; }
  c[3] = enlace_crc8(c, 3);
  Wire.beginTransmission(ENLACE_DIRECCION_CRITICO);
  Wire.write(c, ENLACE_LARGO_COMANDO);
  const uint8_t r = Wire.endTransmission();
  uint8_t g[ENLACE_LARGO_ESTADO];
  const uint8_t n = Wire.requestFrom(ENLACE_DIRECCION_CRITICO, ENLACE_LARGO_ESTADO);
  for (uint8_t i = 0; i < n && i < ENLACE_LARGO_ESTADO; i++) g[i] = Wire.read();
  if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();

  const bool buena = r == 0 && n == ENLACE_LARGO_ESTADO &&
                     enlace_crc8(g, 15) == g[15] && g[0] == ENLACE_VERSION;
  const EstadoEnlace antes = estado;
  if (buena) {
    for (uint8_t i = 0; i < ENLACE_LARGO_ESTADO; i++) f[i] = g[i];
    ok++; tFoto = millis(); fallas = 0;
    if (f[1] != seqCritico) { seqCritico = f[1]; tSeqCambio = millis(); }
    estado = (millis() - tSeqCambio > MS_CONGELADO) ? ENLACE_CONGELADO : ENLACE_OK;
  } else if (++fallas >= FALLAS_PARA_PERDIDO) {
    // Al perderse y luego cada ~1 s mientras siga sin responder.
    if (fallas == FALLAS_PARA_PERDIDO || fallas % 10 == 0) recuperarBus();
    if (estado != ENLACE_NUNCA) estado = ENLACE_PERDIDO;   // nunca visto: sigue "esperando"
    if (fallas == 250) fallas = FALLAS_PARA_PERDIDO + 1;  // sin desbordar
  }
  return estado != antes;
}

EstadoEnlace enlace_aux_estado() { return estado; }

uint8_t enlace_aux_pedir_perfil(uint8_t perfil) {
  perfilAEnviar = perfil; hayPerfil = true;
  return (uint8_t)(seq + 1);              // sale en el siguiente turno
}

bool enlace_aux_foto(FotoCritico *p) {
  if (estado == ENLACE_NUNCA || millis() - tFoto > MS_FOTO_VIEJA) return false;
  p->spwmActiva    = f[3] & 1;
  p->detenidoPorP9 = f[3] & 2;
  p->frecuenciaHz  = (f[3] & 4) ? 50 : 60;
  p->estadoP9      = f[4];
  p->dtCuentas     = f[5];
  p->ciclos        = f[6] | (f[7] << 8);
  p->adcP9         = f[8] | (f[9] << 8);
  p->amplitudMax   = f[10] | (f[11] << 8);
  p->bateriaDv     = f[12] | (f[13] << 8);
  p->estadoBateria = f[14];
  p->seq           = f[1];
  p->eco           = f[2];
  p->perfilBateria = (f[3] >> 4) & 3;
  p->perfilRechazado = f[3] & 0x40;
  return true;
}

const __FlashStringHelper *enlace_aux_texto(EstadoEnlace e) {
  switch (e) {
    case ENLACE_OK:        return F("ENLACE OK con el CRITICO (0x20)");
    case ENLACE_PERDIDO:   return F("ENLACE PERDIDO: el CRITICO no responde");
    case ENLACE_CONGELADO: return F("CRITICO CONGELADO: responde pero su foto no avanza");
    default:               return F("Esperando al CRITICO...");
  }
}
uint32_t enlace_aux_tramas_ok()    { return ok; }
uint32_t enlace_aux_tramas_total() { return total; }
