/*
 * =============================================================================
 *  ISLA_AUX_PRUEBA_I2C.ino  -  Isla del Sol, micro AUX (A2): MAESTRO I2C
 *  Prueba de banco del enlace CRÍTICO <-> AUX (sin potencia).
 * -----------------------------------------------------------------------------
 *  Cada 100 ms:
 *    1) manda un PING (4 bytes) con número de secuencia y CRC-8;
 *    2) pide la foto de estado del CRÍTICO (16 bytes) y revisa longitud,
 *       CRC-8, versión y que el eco sea el PING recién mandado o el anterior.
 *  Cada 1 s imprime un resumen por el Monitor Serie (115200).
 *  Avisos al momento (líneas que empiezan con >>>): ENLACE ESTABLECIDO,
 *  ENLACE PERDIDO (3 fallas seguidas), ENLACE RECUPERADO y CRITICO CONGELADO
 *  (responde pero su foto no avanza). Nunca muestra una foto de más de 300 ms.
 *
 *  CONEXIÓN entre los dos Nanos (cables hembra-hembra, lo más cortos posible):
 *    A4 (SDA) <-> A4      A5 (SCL) <-> A5      GND <-> GND
 *    NO unir los 5 V: cada Nano se alimenta por su propio USB.
 *  Sin la tarjeta de control no hay R10/R11 (4.7 kΩ): se usan las pull-up
 *  internas del ATmega (~30-50 kΩ). Alcanzan a 100 kHz con cables cortos.
 *
 *  Reglas del AUX (mapa de pines): D7 (AUX_KILL) en ALTO, D10 como salida.
 *  Protocolo: enlace_protocolo.h (idéntico en los dos sketches).
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include <Wire.h>
#include "enlace_protocolo.h"

static uint8_t  seq = 0;
static uint32_t total = 0, ok = 0, sinRespuesta = 0, largoMalo = 0, crcMalo = 0, versionMala = 0, ecoMalo = 0;
static uint32_t usMax = 0, usSuma = 0;
static uint8_t  f[ENLACE_LARGO_ESTADO];
static uint32_t tFotoBuena = 0;        // millis() de la última foto válida
static uint32_t tSeqCambio = 0;        // millis() en que la secuencia del CRÍTICO avanzó
static uint8_t  seqAnterior = 0;
static uint8_t  fallasSeguidas = 0;
enum Enlace : uint8_t { ENLACE_NUNCA, ENLACE_OK, ENLACE_PERDIDO, ENLACE_CONGELADO };
static Enlace   enlace = ENLACE_NUNCA;
constexpr uint8_t  FALLAS_PARA_PERDIDO = 3;      // 3 × 100 ms
constexpr uint16_t MS_CONGELADO        = 300;    // la foto se renueva cada 50 ms
constexpr uint16_t MS_FOTO_VIEJA       = 300;

static uint32_t recuperaciones = 0, timeouts = 0;

static void iniciarWire() {
  Wire.begin();                                   // maestro
  Wire.setClock(100000);
  Wire.setWireTimeout(3000, true);                // nunca colgarse si el bus se traba
}

// Liberación del bus (especificación I2C de NXP, UM10204, "bus clear"):
// si un esclavo quedó a la mitad de un byte sosteniendo SDA en bajo, se le
// dan hasta 9 pulsos de reloj para que lo suelte, luego un STOP, y se
// reinicia el periférico TWI.
static void recuperarBus() {
  Wire.end();
  pinMode(A4, INPUT_PULLUP); pinMode(A5, INPUT_PULLUP);
  delayMicroseconds(10);
  Serial.print(F("    Bus antes: SDA=")); Serial.print(digitalRead(A4));
  Serial.print(F(" SCL=")); Serial.print(digitalRead(A5));
  uint8_t pulsos = 0;
  while (digitalRead(A4) == LOW && pulsos < 9) {  // SDA trabada en bajo
    pinMode(A5, OUTPUT); digitalWrite(A5, LOW); delayMicroseconds(5);
    pinMode(A5, INPUT_PULLUP);                  delayMicroseconds(5);
    pulsos++;
  }
  pinMode(A4, OUTPUT); digitalWrite(A4, LOW); delayMicroseconds(5);   // STOP: SDA sube con SCL alta
  pinMode(A4, INPUT_PULLUP);                  delayMicroseconds(5);
  Serial.print(F(" | pulsos SCL ")); Serial.print(pulsos);
  Serial.print(F(" | despues: SDA=")); Serial.print(digitalRead(A4));
  Serial.print(F(" SCL=")); Serial.println(digitalRead(A5));
  iniciarWire();
  recuperaciones++;
}

static void avisar(const __FlashStringHelper *msg) {
  Serial.print(F(">>> [")); Serial.print(millis()); Serial.print(F(" ms] ")); Serial.println(msg);
}

static const char *textoP9(uint8_t e) {
  switch (e) {
    case 0: return "ambas GND (500 ns)";
    case 1: return "A GND, B abierto (mixto)";
    case 2: return "A abierto, B GND (mixto)";
    case 3: return "ambas abiertas (1 us)";
    case 4: return "FUERA DE RANGO";
    default: return "inestable";
  }
}

void setup() {
  digitalWrite(7, HIGH); pinMode(7, OUTPUT);     // AUX_KILL = 1: no apaga el puente
  pinMode(10, OUTPUT);                            // SS como salida (regla del mapa de pines)
  Serial.begin(115200);
  iniciarWire();
  Serial.println(F("=== Isla del Sol - AUX - prueba del enlace I2C (maestro) ==="));
  Serial.println(F("CRITICO esperado en 0x20, 100 kHz, PING + foto cada 100 ms"));
}

void loop() {
  static uint32_t t = 0, tr = 0;
  if (millis() - t >= 100) {
    t = millis();
    total++;
    // 1) PING
    uint8_t c[ENLACE_LARGO_COMANDO] = {ENLACE_CMD_PING, ++seq, 0, 0};
    c[3] = enlace_crc8(c, 3);
    const uint32_t t0 = micros();
    Wire.beginTransmission(ENLACE_DIRECCION_CRITICO);
    Wire.write(c, ENLACE_LARGO_COMANDO);
    const uint8_t r = Wire.endTransmission();
    // 2) Foto de estado
    const uint8_t n = Wire.requestFrom(ENLACE_DIRECCION_CRITICO, ENLACE_LARGO_ESTADO);
    for (uint8_t i = 0; i < n && i < ENLACE_LARGO_ESTADO; i++) f[i] = Wire.read();
    const uint32_t dt = micros() - t0;
    if (Wire.getWireTimeoutFlag()) { timeouts++; Wire.clearWireTimeoutFlag(); }
    bool buena = false;
    if (r != 0 || n == 0)                         sinRespuesta++;
    else if (n != ENLACE_LARGO_ESTADO)            largoMalo++;
    else if (enlace_crc8(f, 15) != f[15])         crcMalo++;
    else if (f[0] != ENLACE_VERSION)              versionMala++;
    else {
      // El CRÍTICO renueva la foto cada 50 ms: el eco puede ser este PING o el anterior.
      if (f[2] != seq && f[2] != (uint8_t)(seq - 1)) ecoMalo++;
      ok++; buena = true; tFotoBuena = millis();
      usSuma += dt; if (dt > usMax) usMax = dt;
      if (f[1] != seqAnterior) { seqAnterior = f[1]; tSeqCambio = millis(); }
    }
    // ---- Estado del enlace: avisos sólo cuando cambia ----
    if (buena) {
      fallasSeguidas = 0;
      const bool congelado = (millis() - tSeqCambio) > MS_CONGELADO;
      if (congelado && enlace != ENLACE_CONGELADO) {
        enlace = ENLACE_CONGELADO;
        avisar(F("CRITICO CONGELADO: responde por I2C pero su foto no avanza (programa trabado)"));
      } else if (!congelado && enlace != ENLACE_OK) {
        avisar(enlace == ENLACE_NUNCA ? F("ENLACE ESTABLECIDO con el CRITICO (0x20)")
                                      : F("ENLACE RECUPERADO con el CRITICO"));
        enlace = ENLACE_OK;
      }
    } else if (++fallasSeguidas >= FALLAS_PARA_PERDIDO && enlace != ENLACE_PERDIDO) {
      enlace = ENLACE_PERDIDO;
      avisar(F("ENLACE PERDIDO: el CRITICO no responde (3 intentos seguidos)"));
      recuperarBus();
    } else if (enlace == ENLACE_PERDIDO && fallasSeguidas % 10 == 0) {
      recuperarBus();                             // mientras siga perdido: cada ~1 s
    }
  }
  if (millis() - tr >= 1000) {
    tr = millis();
    Serial.print(F("Tramas ")); Serial.print(ok); Serial.print('/'); Serial.print(total);
    Serial.print(F("  | sin resp ")); Serial.print(sinRespuesta);
    Serial.print(F(" largo ")); Serial.print(largoMalo);
    Serial.print(F(" CRC ")); Serial.print(crcMalo);
    Serial.print(F(" ver ")); Serial.print(versionMala);
    Serial.print(F(" eco ")); Serial.print(ecoMalo);
    Serial.print(F(" timeout ")); Serial.print(timeouts);
    Serial.print(F(" recup ")); Serial.print(recuperaciones);
    if (ok) { Serial.print(F("  | us prom ")); Serial.print(usSuma / ok); Serial.print(F(" max ")); Serial.print(usMax); }
    Serial.println();
    const uint32_t edad = millis() - tFotoBuena;
    if (enlace == ENLACE_NUNCA) {
      Serial.println(F("   Esperando al CRITICO..."));
    } else if (edad > MS_FOTO_VIEJA) {
      Serial.print(F("   SIN DATOS DEL CRITICO: ultima foto valida hace ")); Serial.print(edad); Serial.println(F(" ms"));
    } else {
      const uint16_t ciclos = f[6] | (f[7] << 8), adc = f[8] | (f[9] << 8), amax = f[10] | (f[11] << 8);
      Serial.print(F("   CRITICO: SPWM ")); Serial.print((f[3] & 1) ? F("activa") : F("detenida"));
      if (f[3] & 2) Serial.print(F(" POR P9"));
      Serial.print((f[3] & 4) ? F(" | 50 Hz") : F(" | 60 Hz"));
      Serial.print(F(" | DT ")); Serial.print(f[5]); Serial.print(f[5] == 8 ? F(" (500 ns)") : F(" (1 us)"));
      Serial.print(F(" | P9 ")); Serial.print(adc); Serial.print(F(" = ")); Serial.print(textoP9(f[4]));
      Serial.print(F(" | ciclos ")); Serial.print(ciclos);
      Serial.print(F(" | amp max ")); Serial.print(amax);
      Serial.print(F(" | seq ")); Serial.print(f[1]); Serial.print(F(" eco ")); Serial.println(f[2]);
    }
  }
}
