/*
 * =============================================================================
 *  energia.cpp  -  Isla del Sol, AUX: medidor de energía
 * -----------------------------------------------------------------------------
 *  Las cuatro trampas que este archivo esquiva, para que nadie las reintroduzca
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
 *  4. LO QUE SE PERSISTE SE EMPACA A MANO. La cabecera estaba como struct y el
 *     compilador le metía relleno entre el uint8_t de la firma y el uint32_t
 *     que sigue: 3 bytes en la PC y ninguno en el AVR, o sea dos formatos
 *     distintos y un CRC cubriendo basura. Lo cachó la prueba 9 del banco.
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
// Lo que se lleva por separado en cada sentido. El consumo y la carga usan la
// misma aritmética: la única diferencia es el signo de los watts.
struct Acum {
  uint32_t resto_wms;                 // lo que falta para el siguiente Wh
  uint32_t wh_historico;              // desde que se instaló
  uint32_t wh_dia;                    // el día en curso
  uint16_t cubeta_wh[CUBETAS_24H];    // consumo de cada hora
};
static Acum s_a[2];                   // [ENER_CONSUMO] y [ENER_CARGA]

static uint16_t s_watts;                    // magnitud de la última foto
static bool     s_cargando;                 // la energía entra a la batería
static uint16_t s_cubeta_max[CUBETAS_24H];  // máxima sostenida de cada hora (sólo consumo)
static uint8_t  s_cubeta;                   // cubeta en curso
static uint16_t s_bloque_min;               // mínima del bloque en curso
static uint8_t  s_bloque_n;                 // lecturas del bloque en curso
static uint32_t s_despierto_s;              // segundos desde el arranque
static uint32_t s_ms_previo;                // marca de millis() de la última vuelta
static uint32_t s_seg_hora;                 // segundos dentro de la hora
static uint32_t s_seg_dia;                  // segundos dentro del día
static uint16_t s_indice;                   // siguiente ranura de los anillos
static uint16_t s_dias;                     // días guardados (tope DIAS_HISTORIAL)
static bool     s_sucio;                    // hay algo sin guardar
static uint8_t  s_bat_previo;               // estado de batería de la vuelta anterior

// =========================== CABECERA EN EEPROM ============================
// [0] firma
// [1-4]   histórico de consumo    [5-8]   histórico de carga
// [9-10]  resto de consumo (W·s)  [11-12] resto de carga (W·s)
// [13-14] índice                  [15-16] días guardados
// [17] CRC-8 de 0..16
// Números de 16 y 32 bits: byte bajo primero, igual que el protocolo I2C.
// Ver la trampa 4 del encabezado: nada de structs para lo que se persiste.
constexpr uint8_t ENER_CAB_LARGO = 18;

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

static inline uint16_t base_anillo(Sentido s)
{
  return (s == ENER_CONSUMO) ? ENER_EE_CONSUMO : ENER_EE_CARGA;
}

static void anillo_escribir(Sentido s, uint16_t ranura, uint16_t wh)
{
  if (ranura >= DIAS_HISTORIAL) return;
  uint8_t b[2] = { (uint8_t)wh, (uint8_t)(wh >> 8) };
  ee_escribir(base_anillo(s) + ranura * 2, b, 2);
}

static uint16_t anillo_leer(Sentido s, uint16_t ranura)
{
  if (ranura >= DIAS_HISTORIAL) return 0;
  uint8_t b[2];
  ee_leer(base_anillo(s) + ranura * 2, b, 2);
  return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

static inline uint32_t lee32(const uint8_t *b)
{
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static inline void pon32(uint8_t *b, uint32_t v)
{
  b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}
static inline uint16_t lee16(const uint8_t *b) { return (uint16_t)(b[0] | ((uint16_t)b[1] << 8)); }
static inline void pon16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }

// =========================== ARRANQUE ======================================
void energia_iniciar()
{
  for (uint8_t k = 0; k < 2; k++) {
    s_a[k].resto_wms = 0; s_a[k].wh_historico = 0; s_a[k].wh_dia = 0;
    for (uint8_t i = 0; i < CUBETAS_24H; i++) s_a[k].cubeta_wh[i] = 0;
  }
  for (uint8_t i = 0; i < CUBETAS_24H; i++) s_cubeta_max[i] = 0;
  s_watts = 0; s_cargando = false; s_cubeta = 0;
  s_bloque_min = 0xFFFF; s_bloque_n = 0;
  s_despierto_s = 0; s_ms_previo = 0; s_seg_hora = 0; s_seg_dia = 0;
  s_sucio = false; s_bat_previo = 0;

  uint8_t b[ENER_CAB_LARGO];
  ee_leer(ENER_EE_CABECERA, b, ENER_CAB_LARGO);
  uint16_t indice = lee16(b + 13);
  uint16_t dias   = lee16(b + 15);
  if (b[0] == ENER_EE_FIRMA && b[17] == crc8(b, ENER_CAB_LARGO - 1)
      && indice < DIAS_HISTORIAL && dias <= DIAS_HISTORIAL) {
    s_a[ENER_CONSUMO].wh_historico = lee32(b + 1);
    s_a[ENER_CARGA].wh_historico   = lee32(b + 5);
    s_a[ENER_CONSUMO].resto_wms    = (uint32_t)lee16(b + 9)  * 1000UL;   // se guardó en W·s
    s_a[ENER_CARGA].resto_wms      = (uint32_t)lee16(b + 11) * 1000UL;
    s_indice = indice;
    s_dias   = dias;
  } else {
    // EEPROM virgen o dañada: se empieza de cero en vez de inventar un número.
    s_indice = 0; s_dias = 0;
  }
}

// =========================== INTEGRACIÓN ===================================
void energia_muestra(int16_t watts, uint16_t dt_ms)
{
  // El signo decide el sentido; de ahí en adelante la aritmética es la misma.
  s_cargando = (watts < 0);
  s_watts    = (uint16_t)(s_cargando ? -(int32_t)watts : watts);
  Acum &a = s_a[s_cargando ? ENER_CARGA : ENER_CONSUMO];

  // --- Trampa 1: en W·ms, nunca en Wh enteros ------------------------------
  a.resto_wms += (uint32_t)s_watts * dt_ms;
  while (a.resto_wms >= 3600000UL) {         // 3 600 000 W·ms = 1 Wh
    a.resto_wms -= 3600000UL;
    a.wh_historico++;
    a.wh_dia++;
    if (a.cubeta_wh[s_cubeta] < 0xFFFF) a.cubeta_wh[s_cubeta]++;
    s_sucio = true;
  }

  // --- Trampa 3: la máxima sale de la MÍNIMA del bloque --------------------
  // Sólo del consumo: la máxima del cargador la fija el cargador, no la casa,
  // así que no dice nada de cómo se usa la instalación.
  if (s_cargando) { s_bloque_min = 0xFFFF; s_bloque_n = 0; return; }
  if (s_watts < s_bloque_min) s_bloque_min = s_watts;
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
  for (uint8_t k = 0; k < 2; k++) {
    uint32_t v = s_a[k].wh_dia;
    anillo_escribir((Sentido)k, s_indice, (v > 0xFFFF) ? 0xFFFF : (uint16_t)v);
    s_a[k].wh_dia = 0;
  }
  s_indice = (uint16_t)((s_indice + 1) % DIAS_HISTORIAL);
  if (s_dias < DIAS_HISTORIAL) s_dias++;
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
      s_a[ENER_CONSUMO].cubeta_wh[s_cubeta] = 0;   // la que entra borra a la que sale
      s_a[ENER_CARGA].cubeta_wh[s_cubeta]   = 0;
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
  uint8_t b[ENER_CAB_LARGO];
  b[0] = ENER_EE_FIRMA;
  pon32(b + 1, s_a[ENER_CONSUMO].wh_historico);
  pon32(b + 5, s_a[ENER_CARGA].wh_historico);
  pon16(b + 9,  (uint16_t)(s_a[ENER_CONSUMO].resto_wms / 1000UL));   // se pierde <1 W·s
  pon16(b + 11, (uint16_t)(s_a[ENER_CARGA].resto_wms   / 1000UL));
  pon16(b + 13, s_indice);
  pon16(b + 15, s_dias);
  b[17] = crc8(b, ENER_CAB_LARGO - 1);
  ee_escribir(ENER_EE_CABECERA, b, ENER_CAB_LARGO);
  s_sucio = false;
}

void energia_borrar()
{
  for (uint8_t k = 0; k < 2; k++) {
    s_a[k].wh_historico = 0; s_a[k].resto_wms = 0; s_a[k].wh_dia = 0;
    for (uint16_t i = 0; i < DIAS_HISTORIAL; i++) anillo_escribir((Sentido)k, i, 0);
  }
  s_indice = 0; s_dias = 0;
  energia_guardar();
}

// =========================== LO QUE VE LA PANTALLA =========================
uint16_t energia_watts()          { return s_watts; }
bool     energia_cargando()       { return s_cargando; }
uint32_t energia_despierto_s()    { return s_despierto_s; }
uint16_t energia_dias_guardados() { return s_dias; }
uint32_t energia_historico_wh(Sentido s) { return s_a[s].wh_historico; }

uint16_t energia_por_ciento()
{
  return (uint16_t)(((uint32_t)s_watts * 100UL + POTENCIA_NOMINAL_W / 2) / POTENCIA_NOMINAL_W);
}

uint32_t energia_hoy_wh(Sentido s)
{
  uint32_t t = 0;
  for (uint8_t i = 0; i < CUBETAS_24H; i++) t += s_a[s].cubeta_wh[i];
  return t;
}

uint16_t energia_max24h_w()
{
  uint16_t m = 0;
  for (uint8_t i = 0; i < CUBETAS_24H; i++) if (s_cubeta_max[i] > m) m = s_cubeta_max[i];
  return m;
}

uint32_t energia_ayer_wh(Sentido s)
{
  if (s_dias == 0) return 0;
  return anillo_leer(s, (uint16_t)((s_indice + DIAS_HISTORIAL - 1) % DIAS_HISTORIAL));
}

uint32_t energia_promedio_wh(Sentido s, uint8_t dias)
{
  if (s_dias == 0 || dias == 0) return 0;
  uint16_t n = (s_dias < dias) ? s_dias : dias;   // promedia los que haya
  uint32_t suma = 0;                              // 30 x 65535 cabe de sobra
  for (uint16_t k = 1; k <= n; k++)
    suma += anillo_leer(s, (uint16_t)((s_indice + DIAS_HISTORIAL - k) % DIAS_HISTORIAL));
  return (suma + n / 2) / n;
}

uint16_t energia_rendimiento_pct()
{
  uint32_t carga = s_a[ENER_CARGA].wh_historico;
  if (carga == 0) return 0;
  uint32_t r = (s_a[ENER_CONSUMO].wh_historico * 100UL + carga / 2) / carga;
  return (r > 999) ? 999 : (uint16_t)r;
}

// =========================== EL ARCHIVO DE CONSUMO =========================
static uint8_t pon_num(char *d, uint16_t v)
{
  char t[6]; uint8_t n = 0, p = 0;
  do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (n) d[p++] = t[--n];
  return p;
}

void energia_volcar_csv(void (*escribe)(const char *))
{
  char linea[24];
  escribe(ENER_CSV_ENCABEZADO);
  // Del más viejo al más nuevo, numerados 1..n.
  for (uint16_t k = s_dias; k >= 1; k--) {
    uint16_t r = (uint16_t)((s_indice + DIAS_HISTORIAL - k) % DIAS_HISTORIAL);
    uint8_t p = 0;
    // Sin sprintf: en el AVR arrastra ~1.5 KB de flash.
    p += pon_num(linea + p, (uint16_t)(s_dias - k + 1));
    linea[p++] = ',';
    p += pon_num(linea + p, anillo_leer(ENER_CONSUMO, r));
    linea[p++] = ',';
    p += pon_num(linea + p, anillo_leer(ENER_CARGA, r));
    linea[p] = '\0';
    escribe(linea);
  }
}
