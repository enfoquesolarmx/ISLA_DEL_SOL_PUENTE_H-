/*
 * =============================================================================
 *  energia.cpp  -  Isla del Sol, AUX: medidor de energía
 * -----------------------------------------------------------------------------
 *  Las tres trampas que este archivo esquiva, para que nadie las reintroduzca
 *  "simplificando":
 *
 *  1. EL ACUMULADO NO PUEDE SER Wh ENTEROS. A 780 W cada foto de 100 ms aporta
 *     0.0217 Wh. Sumando eso a un entero en Wh, cada suma redondea a CERO y el
 *     medidor marca cero PARA SIEMPRE, sin fallar ni avisar. Por eso se
 *     acumula en W·ms y se normaliza a Wh (3 600 000 W·ms = 1 Wh).
 *
 *  2. millis() SE DESBORDA A LOS 49.7 DÍAS. Calculando el tiempo despierto
 *     directo de ahí, el contador se regresa a cero justo cuando empieza a ser
 *     presumible. La resta sin signo `ahora - ultimo` sí es correcta cuando da
 *     la vuelta, y de ahí se acumulan segundos en uint32 (136 años).
 *
 *  3. LA MÁXIMA SE TOMA DE LA MÍNIMA DEL BLOQUE, no del promedio. Si la mínima
 *     de 2 segundos fue 800 W, entonces estuvo arriba de 800 W TODO el tiempo:
 *     eso es "se sostuvo". Y el bloque se tira si se cae el enlace.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include "energia.h"

// --- Acceso a la EEPROM ----------------------------------------------------
// En el AVR va a la EEPROM de verdad; en la PC, a un arreglo, para poder
// probar toda la aritmética sin fierro (pruebas/prueba_energia.cpp).
#ifdef __AVR__
  #include <avr/eeprom.h>
  static inline void ee_leer(uint16_t d, void *b, uint16_t n)  { eeprom_read_block(b, (const void *)d, n); }
  static inline void ee_escribir(uint16_t d, const void *b, uint16_t n) { eeprom_update_block(b, (void *)d, n); }
#else
  extern void ee_leer(uint16_t d, void *b, uint16_t n);
  extern void ee_escribir(uint16_t d, const void *b, uint16_t n);
#endif

// =========================== ESTADO EN RAM =================================
static uint16_t s_watts;                    // potencia de la última foto
static uint32_t s_resto_wms;                // lo que falta para el siguiente Wh
static uint32_t s_wh_historico;             // desde que se instaló
static uint16_t s_cubeta_wh[CUBETAS_24H];   // consumo de cada hora
static uint16_t s_cubeta_max[CUBETAS_24H];  // máxima sostenida de cada hora
static uint8_t  s_cubeta;                   // cubeta en curso
static uint16_t s_bloque_min;               // mínima del bloque en curso
static uint8_t  s_bloque_n;                 // lecturas del bloque en curso
static uint32_t s_despierto_s;              // segundos desde el arranque
static uint32_t s_ms_previo;                // marca de millis() de la última vuelta
static uint32_t s_seg_hora;                 // segundos dentro de la hora
static uint32_t s_seg_dia;                  // segundos dentro del día
static uint16_t s_indice;                   // siguiente ranura del anillo
static uint16_t s_dias;                     // días guardados (tope DIAS_HISTORIAL)
static uint32_t s_wh_dia;                   // consumo del día en curso
static bool     s_sucio;                    // hay algo sin guardar
static uint8_t  s_bat_previo;               // estado de batería de la vuelta anterior

// =========================== CABECERA EN EEPROM ============================
// [0] firma  [1-4] histórico  [5-6] resto en W·s  [7-8] índice  [9-10] días  [11] CRC
// Números de 16 y 32 bits: byte bajo primero, igual que el protocolo I2C.
//
// NO SE USA UN struct. El compilador mete relleno entre un uint8_t y el
// uint32_t que le sigue —en la PC son 3 bytes, en el AVR ninguno—, así que el
// mismo código produciría formatos distintos y el CRC cubriría basura sin
// inicializar. Lo cachó la prueba 9 del banco de PC: guardaba bien y al releer
// el CRC no cuadraba. Un formato que se persiste se empaca a mano, byte por
// byte, siempre.
constexpr uint8_t ENER_CAB_LARGO = 12;

static uint8_t crc8(const uint8_t *d, uint8_t n)
{
  uint8_t c = 0xFF;
  while (n--) {
    c ^= *d++;
    for (uint8_t i = 0; i < 8; i++)
      c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  }
  return c;
}

// El anillo también se empaca a mano, por la misma razón que la cabecera:
// el formato en EEPROM no debe depender de cómo acomode el compilador.
static void anillo_escribir(uint16_t ranura, uint16_t wh)
{
  if (ranura >= DIAS_HISTORIAL) return;
  uint8_t b[2] = { (uint8_t)wh, (uint8_t)(wh >> 8) };
  ee_escribir(ENER_EE_ANILLO + ranura * 2, b, 2);
}

static uint16_t anillo_leer(uint16_t ranura)
{
  if (ranura >= DIAS_HISTORIAL) return 0;
  uint8_t b[2];
  ee_leer(ENER_EE_ANILLO + ranura * 2, b, 2);
  return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

// =========================== ARRANQUE ======================================
void energia_iniciar()
{
  for (uint8_t i = 0; i < CUBETAS_24H; i++) { s_cubeta_wh[i] = 0; s_cubeta_max[i] = 0; }
  s_watts = 0; s_resto_wms = 0; s_cubeta = 0;
  s_bloque_min = 0xFFFF; s_bloque_n = 0;
  s_despierto_s = 0; s_ms_previo = 0; s_seg_hora = 0; s_seg_dia = 0;
  s_wh_dia = 0; s_sucio = false; s_bat_previo = 0;

  uint8_t b[ENER_CAB_LARGO];
  ee_leer(ENER_EE_CABECERA, b, ENER_CAB_LARGO);
  uint16_t indice = (uint16_t)(b[7] | ((uint16_t)b[8] << 8));
  uint16_t dias   = (uint16_t)(b[9] | ((uint16_t)b[10] << 8));
  if (b[0] == ENER_EE_FIRMA && b[11] == crc8(b, ENER_CAB_LARGO - 1)
      && indice < DIAS_HISTORIAL && dias <= DIAS_HISTORIAL) {
    s_wh_historico = (uint32_t)b[1] | ((uint32_t)b[2] << 8)
                   | ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 24);
    uint16_t resto_ws = (uint16_t)(b[5] | ((uint16_t)b[6] << 8));
    s_resto_wms    = (uint32_t)resto_ws * 1000UL;     // se guardó en W·s
    s_indice       = indice;
    s_dias         = dias;
  } else {
    // EEPROM virgen o dañada: se empieza de cero en vez de inventar un número.
    s_wh_historico = 0; s_resto_wms = 0; s_indice = 0; s_dias = 0;
  }
}

// =========================== INTEGRACIÓN ===================================
void energia_muestra(uint16_t watts, uint16_t dt_ms)
{
  s_watts = watts;

  // --- Trampa 1: en W·ms, nunca en Wh enteros ------------------------------
  s_resto_wms += (uint32_t)watts * dt_ms;
  while (s_resto_wms >= 3600000UL) {         // 3 600 000 W·ms = 1 Wh
    s_resto_wms -= 3600000UL;
    s_wh_historico++;
    s_wh_dia++;
    if (s_cubeta_wh[s_cubeta] < 0xFFFF) s_cubeta_wh[s_cubeta]++;
    s_sucio = true;
  }

  // --- Trampa 3: la máxima sale de la MÍNIMA del bloque --------------------
  if (watts < s_bloque_min) s_bloque_min = watts;
  if (++s_bloque_n >= MUESTRAS_BLOQUE) {
    // Estuvo arriba de s_bloque_min durante todo el bloque: eso se sostuvo.
    if (s_bloque_min > s_cubeta_max[s_cubeta]) s_cubeta_max[s_cubeta] = s_bloque_min;
    s_bloque_min = 0xFFFF;
    s_bloque_n = 0;
  }
}

void energia_sin_enlace()
{
  // El bloque a medias no vale: se tira entero.
  s_bloque_min = 0xFFFF;
  s_bloque_n = 0;
  s_watts = 0;
}

// =========================== EL RELOJ Y EL GUARDADO ========================
static void cerrar_dia()
{
  uint16_t wh = (s_wh_dia > 0xFFFF) ? 0xFFFF : (uint16_t)s_wh_dia;
  anillo_escribir(s_indice, wh);
  s_indice = (uint16_t)((s_indice + 1) % DIAS_HISTORIAL);
  if (s_dias < DIAS_HISTORIAL) s_dias++;
  s_wh_dia = 0;
  energia_guardar();
}

void energia_atender(uint32_t ms, uint8_t estadoBateria)
{
  // --- Trampa 2: esta resta es correcta aun cuando millis() da la vuelta ---
  uint32_t transcurrido = ms - s_ms_previo;
  if (transcurrido >= 1000UL) {
    uint32_t seg = transcurrido / 1000UL;
    s_ms_previo += seg * 1000UL;
    s_despierto_s += seg;
    s_seg_hora    += seg;
    s_seg_dia     += seg;

    while (s_seg_hora >= SEGUNDOS_HORA) {
      s_seg_hora -= SEGUNDOS_HORA;
      s_cubeta = (uint8_t)((s_cubeta + 1) % CUBETAS_24H);
      s_cubeta_wh[s_cubeta] = 0;          // la cubeta que entra borra a la que sale
      s_cubeta_max[s_cubeta] = 0;
    }
    if (s_seg_dia >= SEGUNDOS_DIA) {
      s_seg_dia -= SEGUNDOS_DIA;
      cerrar_dia();
    }
  }

  // Guardado extra cuando la batería avisa que esto se va a apagar. Son pocos
  // eventos al mes, así que el conteo de escrituras casi no se mueve.
  if (estadoBateria >= 1 && s_bat_previo == 0 && s_sucio) energia_guardar();
  s_bat_previo = estadoBateria;
}

void energia_guardar()
{
  uint16_t resto_ws = (uint16_t)(s_resto_wms / 1000UL);   // se pierde <1 W·s
  uint8_t b[ENER_CAB_LARGO];
  b[0]  = ENER_EE_FIRMA;
  b[1]  = (uint8_t)(s_wh_historico);
  b[2]  = (uint8_t)(s_wh_historico >> 8);
  b[3]  = (uint8_t)(s_wh_historico >> 16);
  b[4]  = (uint8_t)(s_wh_historico >> 24);
  b[5]  = (uint8_t)(resto_ws);
  b[6]  = (uint8_t)(resto_ws >> 8);
  b[7]  = (uint8_t)(s_indice);
  b[8]  = (uint8_t)(s_indice >> 8);
  b[9]  = (uint8_t)(s_dias);
  b[10] = (uint8_t)(s_dias >> 8);
  b[11] = crc8(b, ENER_CAB_LARGO - 1);
  ee_escribir(ENER_EE_CABECERA, b, ENER_CAB_LARGO);
  s_sucio = false;
}

void energia_borrar()
{
  s_wh_historico = 0; s_resto_wms = 0; s_indice = 0; s_dias = 0; s_wh_dia = 0;
  for (uint16_t i = 0; i < DIAS_HISTORIAL; i++) anillo_escribir(i, 0);
  energia_guardar();
}

// =========================== LO QUE VE LA PANTALLA =========================
uint16_t energia_watts()       { return s_watts; }
uint32_t energia_historico_wh(){ return s_wh_historico; }
uint32_t energia_despierto_s() { return s_despierto_s; }
uint16_t energia_dias_guardados() { return s_dias; }

uint16_t energia_por_ciento()
{
  return (uint16_t)(((uint32_t)s_watts * 100UL + POTENCIA_NOMINAL_W / 2) / POTENCIA_NOMINAL_W);
}

uint32_t energia_hoy_wh()
{
  uint32_t t = 0;
  for (uint8_t i = 0; i < CUBETAS_24H; i++) t += s_cubeta_wh[i];
  return t;
}

uint16_t energia_max24h_w()
{
  uint16_t m = 0;
  for (uint8_t i = 0; i < CUBETAS_24H; i++) if (s_cubeta_max[i] > m) m = s_cubeta_max[i];
  return m;
}

uint32_t energia_ayer_wh()
{
  if (s_dias == 0) return 0;
  uint16_t r = (uint16_t)((s_indice + DIAS_HISTORIAL - 1) % DIAS_HISTORIAL);
  return anillo_leer(r);
}

uint32_t energia_promedio_wh(uint8_t dias)
{
  if (s_dias == 0 || dias == 0) return 0;
  uint16_t n = (s_dias < dias) ? s_dias : dias;   // promedia los que haya
  uint32_t suma = 0;                              // 30 x 65535 cabe de sobra
  for (uint16_t k = 1; k <= n; k++)
    suma += anillo_leer((uint16_t)((s_indice + DIAS_HISTORIAL - k) % DIAS_HISTORIAL));
  return (suma + n / 2) / n;
}

// =========================== EL ARCHIVO DE CONSUMO =========================
void energia_volcar_csv(void (*escribe)(const char *))
{
  char linea[20];
  escribe(ENER_CSV_ENCABEZADO);
  // Del más viejo al más nuevo, numerados 1..n.
  for (uint16_t k = s_dias; k >= 1; k--) {
    uint16_t r = (uint16_t)((s_indice + DIAS_HISTORIAL - k) % DIAS_HISTORIAL);
    uint16_t wh = anillo_leer(r);
    uint16_t dia = (uint16_t)(s_dias - k + 1);
    uint8_t p = 0;
    // Sin sprintf: en el AVR arrastra ~1.5 KB de flash.
    char tmp[6]; uint8_t n = 0;
    uint16_t v = dia; do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) linea[p++] = tmp[--n];
    linea[p++] = ',';
    v = wh; n = 0; do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) linea[p++] = tmp[--n];
    linea[p] = '\0';
    escribe(linea);
  }
}
