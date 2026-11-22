/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_P9  -  Isla del Sol, banco de pruebas
 *  Corrida B1 del protocolo: lectura de P9 en A7 con resistencias reales
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Guía las cuatro combinaciones de P9 (y dos casos de falla), mide A7 con
 *    200 lecturas por estado, clasifica con los MISMOS umbrales del firmware
 *    de producción, y al final da un veredicto por estado con su margen.
 *
 *  POR QUÉ EXISTE
 *    DC-011 e identificacion_dt_pin9.md se validaron en SimulIDE, con túneles
 *    en vez de resistencias. Esta corrida los valida con cobre y con las
 *    resistencias que de verdad se consiguieron en la tienda.
 *
 *  HARDWARE QUE ESPERA (hardware/control sección 13, DC-011)
 *
 *      5 V ──[R12]──┬── A7 ── (al Nano)
 *                   ├──[R13]── P9_A   (puente a GND, o al aire)
 *                   ├──[R14]── P9_B   (puente a GND, o al aire)
 *                   └──[C18 100 nF]── GND
 *
 *    La lectura es RATIOMÉTRICA: R12 y la referencia del ADC salen del mismo
 *    5 V, así que si el riel varía, las cuentas no cambian. Por eso este
 *    sketch NO necesita saber cuánto vale el 5 V.
 *
 *  CÓMO SE USA
 *    1. Abrir el monitor serie a 115200.
 *    2. Poner los puentes como pida la pantalla y teclear el número.
 *    3. Al terminar los seis, teclear 'r' para el reporte.
 *    Las teclas se pueden repetir en cualquier orden y se puede rehacer un
 *    estado tecleándolo otra vez.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */

#include <Arduino.h>
#include <stdint.h>

// ======================== PERILLAS ===========================================
// Valores REALES de las tres resistencias, en ohms. Si consiguió otras, aquí se
// cambian y el sketch recalcula lo que debe esperar. Mídalas con el óhmetro y
// ponga el valor medido, no el marcado: con resistencias de 5 % la diferencia
// se ve en las cuentas.
constexpr uint32_t R12_OHM = 10000UL;   // pull-up a 5 V
constexpr uint32_t R13_OHM = 10000UL;   // a P9_A  (rama A, J1-5)
constexpr uint32_t R14_OHM = 22000UL;   // a P9_B  (rama B, J3-5) — el diseño pide 20000

constexpr uint8_t  PIN_P9      = A7;
constexpr uint16_t N_LECTURAS  = 200;   // por estado, ~1 s en total
constexpr uint8_t  MS_ENTRE    = 5;

// ============ UMBRALES — COPIADOS TAL CUAL DE firmware/critico/ISLA_CRITICO/dt_p9.h
// Si allá cambian, aquí tienen que cambiar. El sketch los imprime al arrancar
// justamente para poder compararlos a ojo contra el archivo.
constexpr uint16_t P9_UMBRAL_FALLA     = 300;
constexpr uint16_t P9_UMBRAL_AMBOS_GND = 460;
constexpr uint16_t P9_UMBRAL_A_GND     = 598;
constexpr uint16_t P9_UMBRAL_B_GND     = 863;
// Y los tiempos del arranque, también de dt_p9.h:
constexpr uint8_t  P9_LECTURAS_ARRANQUE = 16;
constexpr uint16_t P9_ESPERA_INICIAL_MS = 10;
constexpr uint8_t  P9_SEPARACION_MS     = 1;

// ======================== CLASIFICADOR =======================================
// Copiado tal cual de dt_p9.cpp. No se "mejora" ni se simplifica: si esta
// prueba clasificara distinto que el firmware, no estaría probando nada.
enum EstadoP9 : uint8_t {
  P9_AMBOS_GND, P9_A_GND_B_ABIERTO, P9_A_ABIERTO_B_GND,
  P9_AMBOS_ABIERTOS, P9_FUERA_DE_RANGO, P9_INESTABLE
};

EstadoP9 p9_clasificar(uint16_t adc) {
  if (adc < P9_UMBRAL_FALLA)     return P9_FUERA_DE_RANGO;
  if (adc < P9_UMBRAL_AMBOS_GND) return P9_AMBOS_GND;
  if (adc < P9_UMBRAL_A_GND)     return P9_A_GND_B_ABIERTO;
  if (adc < P9_UMBRAL_B_GND)     return P9_A_ABIERTO_B_GND;
  return P9_AMBOS_ABIERTOS;
}

const __FlashStringHelper *nombre(EstadoP9 e) {
  switch (e) {
    case P9_AMBOS_GND:       return F("AMBOS_GND -> 500 ns");
    case P9_A_GND_B_ABIERTO: return F("A_GND_B_ABIERTO -> 1 us");
    case P9_A_ABIERTO_B_GND: return F("A_ABIERTO_B_GND -> 1 us");
    case P9_AMBOS_ABIERTOS:  return F("AMBOS_ABIERTOS -> 1 us");
    case P9_FUERA_DE_RANGO:  return F("FUERA_DE_RANGO -> no arranca");
    default:                 return F("INESTABLE -> 1 us");
  }
}

// ======================== LO QUE SE DEBE ESPERAR =============================
// Cuentas = 1024 * Rparalelo / (R12 + Rparalelo).  Ratiométrico: el 5 V se va.
static uint16_t cuentas_de(uint32_t rpar) {
  if (rpar == 0xFFFFFFFFUL) return 1023;              // nada a GND
  return (uint16_t)((1024UL * rpar) / (R12_OHM + rpar));
}
static uint32_t paralelo(uint32_t a, uint32_t b) { return (a * b) / (a + b); }

struct Caso {
  const __FlashStringHelper *instruccion;
  EstadoP9 esperado;
  uint16_t nominal;        // cuentas que debe dar, calculadas de las perillas
  bool     tiene_nominal;  // los casos de falla no tienen valor calculable
};

Caso casos[6];

static void armar_casos() {
  casos[0] = { F("P9_A a GND     y  P9_B a GND"),      P9_AMBOS_GND,
               cuentas_de(paralelo(R13_OHM, R14_OHM)), true };
  casos[1] = { F("P9_A a GND     y  P9_B AL AIRE"),    P9_A_GND_B_ABIERTO,
               cuentas_de(R13_OHM), true };
  casos[2] = { F("P9_A AL AIRE   y  P9_B a GND"),      P9_A_ABIERTO_B_GND,
               cuentas_de(R14_OHM), true };
  casos[3] = { F("P9_A AL AIRE   y  P9_B AL AIRE"),    P9_AMBOS_ABIERTOS,
               1023, true };
  casos[4] = { F("A7 DESCONECTADO de la red (pin al aire)"), P9_INESTABLE, 0, false };
  casos[5] = { F("A7 EN CORTO a GND"),                 P9_FUERA_DE_RANGO, 0, false };
}

// ======================== CAPTURA ============================================
struct Medida { bool hecha; uint16_t mn, mx; uint32_t suma; uint16_t n;
                uint8_t distintas; EstadoP9 primera; };
Medida med[6];

static void capturar(uint8_t i) {
  Medida &m = med[i];
  m.mn = 1023; m.mx = 0; m.suma = 0; m.n = 0; m.distintas = 0;

  delay(P9_ESPERA_INICIAL_MS);          // que se cargue C18, igual que el firmware
  analogRead(PIN_P9);                   // la primera lectura tras cambiar de canal se tira

  for (uint16_t k = 0; k < N_LECTURAS; k++) {
    uint16_t v = analogRead(PIN_P9);
    if (v < m.mn) m.mn = v;
    if (v > m.mx) m.mx = v;
    m.suma += v; m.n++;
    EstadoP9 e = p9_clasificar(v);
    if (k == 0) m.primera = e;
    else if (e != m.primera && m.distintas < 255) m.distintas++;
    delay(MS_ENTRE);
  }
  m.hecha = true;

  const uint16_t prom = m.suma / m.n;
  Serial.println();
  Serial.print(F("  minimo "));  Serial.print(m.mn);
  Serial.print(F("   promedio ")); Serial.print(prom);
  Serial.print(F("   maximo ")); Serial.print(m.mx);
  Serial.print(F("   (")); Serial.print(prom * 5000UL / 1024UL); Serial.println(F(" mV con riel de 5.00 V)"));
  Serial.print(F("  clasificado: ")); Serial.println(nombre(p9_clasificar(prom)));
  if (m.distintas) {
    Serial.print(F("  OJO: ")); Serial.print(m.distintas);
    Serial.print(F(" de ")); Serial.print(m.n);
    Serial.println(F(" lecturas cayeron en otro estado. Eso es inestabilidad."));
  }
  if (casos[i].tiene_nominal) {
    const int16_t d = (int16_t)prom - (int16_t)casos[i].nominal;
    Serial.print(F("  esperado ")); Serial.print(casos[i].nominal);
    Serial.print(F(", diferencia ")); Serial.print(d > 0 ? "+" : ""); Serial.println(d);
  }
  Serial.println();
}

// margen en cuentas hasta el umbral que cambiaria la clasificacion
static int16_t margen(uint16_t prom, EstadoP9 e) {
  switch (e) {
    case P9_FUERA_DE_RANGO:  return (int16_t)(P9_UMBRAL_FALLA - prom);
    case P9_AMBOS_GND:       return (int16_t)min(prom - P9_UMBRAL_FALLA,
                                                 P9_UMBRAL_AMBOS_GND - prom);
    case P9_A_GND_B_ABIERTO: return (int16_t)min(prom - P9_UMBRAL_AMBOS_GND,
                                                 P9_UMBRAL_A_GND - prom);
    case P9_A_ABIERTO_B_GND: return (int16_t)min(prom - P9_UMBRAL_A_GND,
                                                 P9_UMBRAL_B_GND - prom);
    default:                 return (int16_t)(prom - P9_UMBRAL_B_GND);
  }
}

static void reporte() {
  Serial.println();
  Serial.println(F("================ REPORTE B1 ================"));
  Serial.print(F("R12 ")); Serial.print(R12_OHM);
  Serial.print(F("  R13 ")); Serial.print(R13_OHM);
  Serial.print(F("  R14 ")); Serial.print(R14_OHM);
  if (R14_OHM != 20000UL) Serial.print(F("  (el diseno pide 20000)"));
  Serial.println();
  Serial.println();
  Serial.println(F("  #  estado medido                   prom  margen  veredicto"));

  uint8_t faltan = 0, malos = 0, apretados = 0;
  for (uint8_t i = 0; i < 6; i++) {
    Serial.print(F("  ")); Serial.print(i + 1); Serial.print(F("  "));
    if (!med[i].hecha) { Serial.println(F("--- sin medir ---")); faltan++; continue; }
    const uint16_t prom = med[i].suma / med[i].n;
    const EstadoP9 e = p9_clasificar(prom);
    const int16_t  mg = margen(prom, e);

    // nombre recortado para que la columna cuadre
    char buf[32];
    strncpy_P(buf, (PGM_P)nombre(e), sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char *p = strstr(buf, " ->"); if (p) *p = 0;
    Serial.print(buf);
    for (uint8_t k = strlen(buf); k < 26; k++) Serial.print(' ');
    Serial.print(prom); Serial.print(F("   "));
    Serial.print(mg); Serial.print(F("   "));

    bool ok;
    if (i == 4)       ok = true;                       // A7 al aire: solo se observa
    else if (i == 5)  ok = (e == P9_FUERA_DE_RANGO);
    else              ok = (e == casos[i].esperado) && (med[i].distintas == 0);

    if (!ok) { Serial.println(F("FALLA")); malos++; }
    else if (i < 4 && mg < 20) { Serial.println(F("PASA, pero APRETADO")); apretados++; }
    else if (i == 4) Serial.println(F("solo informativo"));
    else Serial.println(F("PASA"));
  }

  Serial.println();
  if (faltan) { Serial.print(F("  Faltan ")); Serial.print(faltan); Serial.println(F(" estados por medir.")); }
  if (malos)  Serial.println(F("  HAY FALLAS: el circuito no identifica bien las ramas."));
  else if (!faltan) Serial.println(F("  B1 PASA: las cuatro combinaciones se identifican."));
  if (apretados)
    Serial.println(F("  Algun estado quedo a menos de 20 cuentas de su umbral. Anotelo:\r\n"
                     "  no es falla, pero con ruido podria cruzarse. Si se cruza, el\r\n"
                     "  firmware escoge 1 us en vez de 500 ns, o sea hacia el lado seguro."));
  Serial.println(F("  Pase estos numeros a la hoja de registro del protocolo."));
  Serial.println(F("============================================"));
  Serial.println();
}

static void menu() {
  Serial.println();
  Serial.println(F("--- PRUEBA P9 (corrida B1) -------------------------------"));
  Serial.println(F("  Ponga los puentes y teclee el numero:"));
  for (uint8_t i = 0; i < 6; i++) {
    Serial.print(F("   ")); Serial.print(i + 1); Serial.print(F(") "));
    Serial.print(casos[i].instruccion);
    if (casos[i].tiene_nominal) { Serial.print(F("   [esperado ")); Serial.print(casos[i].nominal); Serial.print(F("]")); }
    if (med[i].hecha) Serial.print(F("   (ya medido)"));
    Serial.println();
  }
  Serial.println(F("   m) monitor continuo (cualquier tecla lo corta)"));
  Serial.println(F("   d) correr la deteccion real de arranque (16 lecturas)"));
  Serial.println(F("   r) reporte"));
  Serial.println(F("----------------------------------------------------------"));
}

static void monitor() {
  Serial.println(F("Monitor. Mueva los puentes y vea. Cualquier tecla para salir."));
  while (!Serial.available()) {
    const uint16_t v = analogRead(PIN_P9);
    Serial.print(v); Serial.print(F("  "));
    Serial.print(v * 5000UL / 1024UL); Serial.print(F(" mV  "));
    Serial.println(nombre(p9_clasificar(v)));
    delay(250);
  }
  while (Serial.available()) Serial.read();
}

// La deteccion de arranque tal como la hace el firmware: 16 lecturas, y si una
// sola cae en otro estado, el resultado es INESTABLE y se usa 1 us.
static void deteccion_real() {
  Serial.println(F("Corriendo p9_detectar() como en el firmware..."));
  delay(P9_ESPERA_INICIAL_MS);
  uint16_t v = analogRead(PIN_P9);
  const EstadoP9 primero = p9_clasificar(v);
  EstadoP9 resultado = primero;
  for (uint8_t i = 1; i < P9_LECTURAS_ARRANQUE; i++) {
    delay(P9_SEPARACION_MS);
    v = analogRead(PIN_P9);
    Serial.print(v); Serial.print(' ');
    if (p9_clasificar(v) != primero) resultado = P9_INESTABLE;
  }
  Serial.println();
  Serial.print(F("  resultado: ")); Serial.println(nombre(resultado));
  if (resultado == P9_INESTABLE)
    Serial.println(F("  INESTABLE significa que las 16 no coincidieron. El firmware\r\n"
                     "  se va a 1 us por precaucion. Revise contactos y el C18."));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  armar_casos();
  for (uint8_t i = 0; i < 6; i++) med[i].hecha = false;

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" Isla del Sol - prueba de P9 en A7 (corrida B1)"));
  Serial.println(F("=============================================="));
  Serial.print(F(" Umbrales de dt_p9.h: FALLA<")); Serial.print(P9_UMBRAL_FALLA);
  Serial.print(F("  AMBOS_GND<"));                 Serial.print(P9_UMBRAL_AMBOS_GND);
  Serial.print(F("  A_GND<"));                     Serial.print(P9_UMBRAL_A_GND);
  Serial.print(F("  B_GND<"));                     Serial.println(P9_UMBRAL_B_GND);
  Serial.println(F(" Compare esa linea contra dt_p9.h antes de confiar en el reporte."));
  menu();
}

void loop() {
  if (!Serial.available()) return;
  const char c = Serial.read();
  if (c >= '1' && c <= '6') {
    const uint8_t i = c - '1';
    Serial.print(F("\r\nMidiendo: ")); Serial.println(casos[i].instruccion);
    Serial.print(F("  (")); Serial.print(N_LECTURAS); Serial.println(F(" lecturas, no mueva nada)"));
    capturar(i);
    menu();
  } else if (c == 'm' || c == 'M') { monitor(); menu(); }
  else if (c == 'd' || c == 'D')   { deteccion_real(); menu(); }
  else if (c == 'r' || c == 'R')   { reporte(); menu(); }
}
