/*
 * =============================================================================
 *  ISLA_AUX_PRUEBA_I2C_ESCLAVO  -  Isla del Sol, micro AUX (A2)
 *  Esclavo DEL BUS en 0x21 - y de todas formas el que manda
 *  Protocolo de banco, Parte C — ETAPA 3-B: el I2C, pero al revés
 * -----------------------------------------------------------------------------
 *  EL AUX ESPERA EL BUS, PERO SIGUE MANDANDO
 *
 *    ⚠️ No confundir las dos cosas, porque las palabras del I2C enganan:
 *
 *        esclavo DEL BUS   = no abre la conversación, contesta cuando le hablan
 *        quien MANDA       = el que decide qué va a hacer el otro
 *
 *    El AUX es lo primero y sigue siendo lo segundo. Es el panel: pide los
 *    perfiles de batería, trae los permisos de carga y descarga del BMS por
 *    CAN, y tiene el AUX_KILL por hardware que apaga el puente sin pasar por
 *    este bus. El CRÍTICO abre la transferencia, pero lo único que hace en
 *    ella es entregar cuentas y recoger órdenes.
 *
 *    En una línea: **el CRÍTICO lleva el reloj, el AUX manda.**
 *
 *    Y esperar el bus, además, pone la parte frágil del I2C donde no duele:
 *
 *      · Un esclavo puede quedarse agarrando el bus si el maestro aborta a
 *        media transferencia. Al AUX eso no le cuesta nada — no tiene trabajo
 *        de tiempo real: si se traba, se reinicia y no pasa nada.
 *      · Al CRÍTICO sí le costaba. Era él quien perdía pasos de la onda.
 *
 *  LO QUE PASA EN CADA TRANSFERENCIA (la abre el CRÍTICO, cada 100 ms)
 *
 *      1) el CRÍTICO ESCRIBE 16 bytes: la foto de estado, con CRC-8
 *         → aquí llega por onReceive, se copia y ya. Nada más.
 *      2) el CRÍTICO LEE 4 bytes: el comando del AUX, con CRC-8
 *         → lo contesta onRequest con el comando que el lazo dejó listo
 *
 *    ⚠️ Las dos funciones de interrupción SÓLO COPIAN. El CRC se revisa en el
 *    lazo, nunca dentro de la interrupción: mientras el AUX está adentro de su
 *    ISR de TWI el bus está estirado y el CRÍTICO esperando.
 *
 *    El comando se arma por adelantado y se deja servido en `cmd[]`, para que
 *    onRequest no tenga que calcular ningún CRC con el bus detenido.
 *
 *  LO QUE HAY QUE MIRAR, Y ESTÁ EN EL OTRO NANO
 *
 *    SOBREPASOS=+0 en el reporte del CRÍTICO. Eso es toda la prueba.
 *    Aquí lo único que importa es que `fotos` siga subiendo y `CRCmalo` siga
 *    en cero: si aparecen errores, el problema es el bus (cables, pull-ups),
 *    no el reparto de tiempo del CRÍTICO.
 *
 *  PEDIR UN PERFIL DE BATERÍA (prueba del camino de vuelta)
 *
 *    Teclear 0, 1 ó 2 en el Monitor Serie. El AUX deja el comando servido y
 *    el CRÍTICO se lo lleva en la siguiente transferencia; se confirma cuando
 *    el eco de la foto (byte 2) trae el número de secuencia del comando.
 *
 *  CONEXIÓN entre los dos Nano (cables hembra-hembra, lo más cortos posible):
 *    A4 (SDA) <-> A4      A5 (SCL) <-> A5      GND <-> GND
 *    ⚠️ NO unir los 5 V: cada Nano se alimenta por su propio USB.
 *    Pull-up EXTERNAS de 8.2 kΩ de A4 y A5 a 5 V, una por línea.
 *
 *  Reglas del AUX (mapa de pines): D7 (AUX_KILL) en ALTO, D10 como salida.
 *  Protocolo: enlace_protocolo.h (idéntico en los dos sketches).
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#include <Wire.h>
#include <util/atomic.h>
#include "enlace_protocolo.h"
#include "watchdog_avr.h"

// La dirección del AUX como esclavo. No se reusa 0x20 a propósito: así una
// tarjeta con firmware viejo no contesta por error.
#define ENLACE_DIRECCION_AUX   0x21

// ============================== ESTADO =======================================

// Lo que llega. La ISR sólo copia aquí; el lazo valida.
static volatile uint8_t  rx[ENLACE_LARGO_ESTADO];
static volatile uint8_t  rxN;
static volatile bool     hayFoto;
static volatile uint32_t pedidos;        // veces que el CRÍTICO pidió el comando
static volatile uint32_t largoMalo;      // escrituras que no traían 16 bytes

// Lo que se contesta: servido de antemano, con su CRC ya calculado.
static volatile uint8_t  cmd[ENLACE_LARGO_COMANDO];

static uint8_t  f[ENLACE_LARGO_ESTADO];  // la última foto buena
static uint8_t  seqCmd;
static uint32_t fotos, crcMalo, versionMala;
static uint32_t tFotoBuena, tSeqCambio;
static uint8_t  seqAnterior;

enum Enlace : uint8_t { ENLACE_NUNCA, ENLACE_OK, ENLACE_PERDIDO, ENLACE_CONGELADO };
static Enlace enlace = ENLACE_NUNCA;

static uint32_t rescates;                // veces que se destrabó el bus a mano

constexpr uint16_t MS_PERDIDO   = 500;   // el CRÍTICO manda cada 100 ms
constexpr uint16_t MS_RESCATE   = 1000;  // cada cuánto se reintenta el rescate
constexpr uint16_t MS_CONGELADO = 600;   // responde, pero su secuencia no avanza

// ===================== LAS DOS INTERRUPCIONES, MÍNIMAS =======================

static void alRecibir(int n)
{
  uint8_t i = 0;
  while (Wire.available()) {
    const uint8_t b = Wire.read();
    if (i < ENLACE_LARGO_ESTADO) rx[i] = b;
    i++;
  }
  rxN = i;
  if (i == ENLACE_LARGO_ESTADO) hayFoto = true;
  else                          largoMalo++;
  (void)n;
}

static void alPedir()
{
  Wire.write((const uint8_t *)cmd, ENLACE_LARGO_COMANDO);
  pedidos++;
}

// =========================== EL COMANDO SERVIDO ==============================

/*  Deja listo el comando que el CRÍTICO se llevará en la próxima transferencia.
 *  Se llama desde el lazo, con las interrupciones apagadas apenas lo que dura
 *  la copia de cuatro bytes — no hay forma de que el CRÍTICO se lleve medio
 *  comando.
 */
static void servirComando(uint8_t tipo, uint8_t dato)
{
  uint8_t c[ENLACE_LARGO_COMANDO] = { tipo, ++seqCmd, dato, 0 };
  c[3] = enlace_crc8(c, ENLACE_LARGO_COMANDO - 1);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for (uint8_t i = 0; i < ENLACE_LARGO_COMANDO; i++) cmd[i] = c[i];
  }
}

/*  RESCATE DEL BUS — y esto NO lo hace el watchdog.
 *
 *  Si el TWI se quedó agarrado a SDA, el lazo sigue corriendo igual (la
 *  máquina de estados del esclavo vive en la interrupción), así que el perro
 *  nunca vence. La salida directa es bajar TWEN: `Wire.end()` llama a
 *  `twi_disable()`, que apaga el periférico y deja los dos pines en alta
 *  impedancia. Con las pull-up de 8.2 kΩ las líneas suben y el bus queda
 *  libre, sin reiniciar nada y sin perder el estado.
 *
 *  ⚠️ Si las dos líneas siguen en bajo después de esto, el AUX no era el que
 *  las tenía: lo más probable es que el otro Nano esté sin alimentar — un AVR
 *  apagado jala SDA y SCL por sus diodos de protección, sin estar trabado.
 */
static void rescatar_bus(void)
{
  Wire.end();
  delayMicroseconds(10);

  const bool sda = digitalRead(A4), scl = digitalRead(A5);

  Wire.begin(ENLACE_DIRECCION_AUX);
  Wire.onReceive(alRecibir);
  Wire.onRequest(alPedir);
  rescates++;

  Serial.print(F(">>> bus rescatado ("));
  Serial.print(rescates);
  Serial.print(F("): SDA="));
  Serial.print(sda);
  Serial.print(F(" SCL="));
  Serial.print(scl);
  Serial.println((!sda && !scl) ? F("  <<< las dos en bajo: revisar si el CRITICO esta alimentado")
                                : F("  (lineas libres)"));
}

static void avisar(const __FlashStringHelper *msg)
{
  Serial.print(F(">>> [")); Serial.print(millis()); Serial.print(F(" ms] "));
  Serial.println(msg);
}

static const char *textoP9(uint8_t e)
{
  switch (e) {
    case 0: return "ambas GND (500 ns)";
    case 1: return "A GND, B abierto (mixto)";
    case 2: return "A abierto, B GND (mixto)";
    case 3: return "ambas abiertas (1 us)";
    case 4: return "FUERA DE RANGO";
    default: return "inestable";
  }
}

// ================================= SETUP =====================================

void setup()
{
  digitalWrite(7, HIGH); pinMode(7, OUTPUT);   // AUX_KILL = 1: no apaga el puente
  pinMode(10, OUTPUT);                          // SS como salida (mapa de pines)

  Serial.begin(115200);

  servirComando(ENLACE_CMD_PING, 0);            // algo válido servido desde el arranque

  Wire.begin(ENLACE_DIRECCION_AUX);             // ESCLAVO en 0x21
  Wire.onReceive(alRecibir);
  Wire.onRequest(alPedir);

  Serial.println();
  Serial.println(F("=== Isla del Sol - AUX - enlace I2C (esclavo del bus, 0x21) ==="));
  Serial.println(F("El CRITICO lleva el reloj del bus; el que manda es el AUX."));
  Serial.println(F("El CRITICO abre, reporta su estado y recoge ordenes."));
  Serial.print(F("Arranque: "));
  Serial.println(wdt_texto_arranque());
  if (wdt_fue_watchdog()) {
    Serial.println(F(">>> OJO: el arranque anterior lo corto el WATCHDOG."));
    Serial.println(F("    Si esto se repite solo, hay un cuelgue que hay que buscar."));
  }
  Serial.println(F("Teclear 0, 1 o 2 para pedir un perfil de bateria."));
  Serial.println(F("Teclear h para colgarlo a proposito y probar el watchdog."));

  /*  El perro se prende AL FINAL, con todo ya inicializado. Si el Nano trae
   *  el cargador de arranque viejo, el salvavidas de `.init3` es lo que
   *  evita que un reinicio nos deje en bucle. Ver watchdog_avr.h.
   */
  wdt_arrancar();
}

// ================================== LAZO =====================================

void loop()
{
  /*  Patear al perro, una sola vez y aquí. Ponerlo en varios lugares del lazo
   *  esconde justo el cuelgue que se quiere detectar.
   */
  wdt_patear();

  // ---- 1) una foto nueva? se valida aquí, nunca en la interrupción --------
  if (hayFoto) {
    uint8_t g[ENLACE_LARGO_ESTADO];
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      for (uint8_t i = 0; i < ENLACE_LARGO_ESTADO; i++) g[i] = rx[i];
      hayFoto = false;
    }

    if (enlace_crc8(g, ENLACE_LARGO_ESTADO - 1) != g[ENLACE_LARGO_ESTADO - 1]) {
      crcMalo++;
    } else if (g[0] != ENLACE_VERSION) {
      versionMala++;
    } else {
      for (uint8_t i = 0; i < ENLACE_LARGO_ESTADO; i++) f[i] = g[i];
      fotos++;
      tFotoBuena = millis();
      if (f[1] != seqAnterior) { seqAnterior = f[1]; tSeqCambio = millis(); }
    }
  }

  // ---- 2) un perfil pedido por el Monitor Serie ---------------------------
  while (Serial.available()) {
    const int k = Serial.read();
    if (k >= '0' && k <= '2') {
      servirComando(ENLACE_CMD_PERFIL, (uint8_t)(k - '0'));
      Serial.print(F(">>> comando servido: PERFIL "));
      Serial.print((char)k);
      Serial.print(F(" seq "));
      Serial.println(seqCmd);
    }
    /*  PRUEBA DEL WATCHDOG — teclear 'h' (de "hang", colgarse).
     *
     *  Se cuelga a propósito, sin patear al perro. Si el watchdog está bien
     *  puesto, la tarjeta se reinicia en ~1 s y el banner del arranque dice
     *  POR WATCHDOG. Es la única forma de comprobarlo: un watchdog que nunca
     *  se probó es una suposición.
     *
     *  ⚠️ Aquí también se descubre si este Nano trae el cargador de arranque
     *  viejo. Si lo trae, el salvavidas de `.init3` es lo que evita que este
     *  cuelgue deje la tarjeta en bucle de reinicio. Si después de teclear
     *  'h' el Nano NO vuelve a imprimir el banner, hay que reprogramarlo y el
     *  salvavidas no funcionó — reportarlo, porque entonces el CRÍTICO no
     *  lleva watchdog hasta entenderlo.
     */
    else if (k == 'h' || k == 'H') {
      Serial.println(F(">>> colgandome a proposito. El watchdog debe reiniciarme en ~1 s..."));
      Serial.flush();
      for (;;) { }               // ni un wdt_patear() aquí
    }
  }

  // ---- 3) estado del enlace: avisos sólo cuando cambia --------------------
  const uint32_t edad = millis() - tFotoBuena;
  if (fotos == 0) {
    /* todavía nada: no se avisa hasta la primera foto */
  } else if (edad > MS_PERDIDO) {
    if (enlace != ENLACE_PERDIDO) {
      enlace = ENLACE_PERDIDO;
      avisar(F("ENLACE PERDIDO: el CRITICO dejo de mandar la foto"));
      rescatar_bus();
    }
    // Mientras siga perdido se reintenta cada segundo: puede ser que el
    // CRÍTICO esté reiniciándose, o que el bus se haya trabado otra vez.
    static uint32_t tRescate = 0;
    if (millis() - tRescate > MS_RESCATE) { tRescate = millis(); rescatar_bus(); }
  } else if ((millis() - tSeqCambio) > MS_CONGELADO) {
    if (enlace != ENLACE_CONGELADO) {
      enlace = ENLACE_CONGELADO;
      avisar(F("CRITICO CONGELADO: manda la foto pero su secuencia no avanza"));
    }
  } else if (enlace != ENLACE_OK) {
    avisar(enlace == ENLACE_NUNCA ? F("ENLACE ESTABLECIDO: el CRITICO (maestro) ya manda")
                                  : F("ENLACE RECUPERADO"));
    enlace = ENLACE_OK;
  }

  // ---- 4) reporte cada segundo -------------------------------------------
  static uint32_t tr = 0;
  if (millis() - tr < 1000) return;
  tr = millis();

  uint32_t ped, lm;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { ped = pedidos; lm = largoMalo; }

  Serial.print(F("Fotos ")); Serial.print(fotos);
  Serial.print(F("  | pedidos de comando ")); Serial.print(ped);
  Serial.print(F(" | CRCmalo ")); Serial.print(crcMalo);
  Serial.print(F(" ver ")); Serial.print(versionMala);
  Serial.print(F(" largo ")); Serial.print(lm);
  Serial.print(F(" | rescates ")); Serial.print(rescates);
  Serial.print(F(" | cmd servido seq ")); Serial.println(seqCmd);

  if (fotos == 0) {
    Serial.println(F("   Esperando al CRITICO (maestro)..."));
    return;
  }
  if (edad > MS_PERDIDO) {
    Serial.print(F("   SIN DATOS DEL CRITICO: ultima foto valida hace "));
    Serial.print(edad); Serial.println(F(" ms"));
    return;
  }

  const uint16_t ciclos = f[6] | (f[7] << 8);
  const uint16_t adc    = f[8] | (f[9] << 8);
  const uint16_t amax   = f[10] | (f[11] << 8);

  Serial.print(F("   CRITICO: SPWM "));
  Serial.print((f[3] & 1) ? F("activa") : F("detenida"));
  if (f[3] & 2) Serial.print(F(" POR P9"));
  Serial.print((f[3] & 4) ? F(" | 50 Hz") : F(" | 60 Hz"));
  Serial.print(F(" | DT ")); Serial.print(f[5]);
  Serial.print(f[5] == 8 ? F(" (500 ns)") : F(" (1 us)"));
  Serial.print(F(" | P9 ")); Serial.print(adc);
  Serial.print(F(" = ")); Serial.print(textoP9(f[4]));
  Serial.print(F(" | ciclos ")); Serial.print(ciclos);
  Serial.print(F(" | amp max ")); Serial.print(amax);
  Serial.print(F(" | seq ")); Serial.print(f[1]);
  Serial.print(F(" eco ")); Serial.println(f[2]);
}
