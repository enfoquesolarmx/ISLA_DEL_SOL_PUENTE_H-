/*
 *  lcd.cpp  -  Isla del Sol, AUX: backends AIP31068 / PCF8574 (ver lcd.h)
 *  Licencia: MIT. Escrito desde cero (comandos de la hoja de datos HD44780).
 */
#include <Arduino.h>
#include "config_panel.h"
#include "i2c_sw.h"
#include "lcd.h"

// Comandos HD44780 (los mismos en AIP31068)
constexpr uint8_t CMD_BORRAR   = 0x01;
constexpr uint8_t CMD_ENTRADA  = 0x06;   // cursor avanza, sin desplazar pantalla
constexpr uint8_t CMD_DISPLAY  = 0x0C;   // pantalla on, sin cursor, sin parpadeo
constexpr uint8_t CMD_FUNCION  = 0x28;   // 4 bits, 2 líneas, 5x8
constexpr uint8_t CMD_DDRAM    = 0x80;   // + dirección: fila 0 = 0x00, fila 1 = 0x40

static bool okUltimo = false;

#if LCD_TIPO == 1
// ======================== AIP31068 ===========================================
static void enviar(uint8_t control, uint8_t b) {
  const uint8_t d[2] = { control, b };
  okUltimo = i2c_sw_escribir(LCD_DIR_AIP31068, d, 2);
}
static void comando(uint8_t c) { enviar(0x80, c); delayMicroseconds(50); }
static void dato(uint8_t c)    { enviar(0x40, c); delayMicroseconds(50); }

bool lcd_iniciar() {
  i2c_sw_iniciar();
  delay(50);                                  // > 40 ms tras encender
  for (uint8_t i = 0; i < 3; i++) { comando(CMD_FUNCION); delay(5); }
  comando(CMD_DISPLAY);
  comando(CMD_BORRAR); delay(2);
  comando(CMD_ENTRADA);
  return okUltimo;
}
void lcd_luz(bool) {}                         // el AIP31068 no maneja la luz
uint8_t lcd_direccion() { return LCD_DIR_AIP31068; }
const __FlashStringHelper *lcd_nombre() { return F("AIP31068"); }

#elif LCD_TIPO == 0
// ======================== PCF8574 + HD44780 (4 bits) =========================
constexpr uint8_t P_RS = 0x01, P_EN = 0x04, P_LUZ = 0x08;
static uint8_t luz = P_LUZ;

static void mitad(uint8_t nib, uint8_t rs) {  // nib en los 4 bits altos
  const uint8_t b = (nib & 0xF0) | rs | luz;
  const uint8_t d[2] = { (uint8_t)(b | P_EN), b };   // EN arriba y luego abajo
  okUltimo = i2c_sw_escribir(LCD_DIR_PCF8574, d, 2);
}
static void enviar(uint8_t v, uint8_t rs) { mitad(v, rs); mitad(v << 4, rs); delayMicroseconds(50); }
static void comando(uint8_t c) { enviar(c, 0); }
static void dato(uint8_t c)    { enviar(c, P_RS); }

bool lcd_iniciar() {
  i2c_sw_iniciar();
  delay(50);
  // Secuencia de "reinicio por instrucciones" de la hoja de datos HD44780:
  // no se sabe si quedó en 8 o 4 bits, así que se fuerza 8 bits tres veces
  // y luego se pasa a 4 bits.
  mitad(0x30, 0); delay(5);
  mitad(0x30, 0); delay(5);
  mitad(0x30, 0); delayMicroseconds(150);
  mitad(0x20, 0); delayMicroseconds(150);     // ya en 4 bits
  comando(CMD_FUNCION);
  comando(CMD_DISPLAY);
  comando(CMD_BORRAR); delay(2);
  comando(CMD_ENTRADA);
  return okUltimo;
}
void lcd_luz(bool e) { luz = e ? P_LUZ : 0; const uint8_t d = luz; i2c_sw_escribir(LCD_DIR_PCF8574, &d, 1); }
uint8_t lcd_direccion() { return LCD_DIR_PCF8574; }
const __FlashStringHelper *lcd_nombre() { return F("PCF8574 + HD44780"); }

#else
#error "LCD_TIPO debe ser 1 (AIP31068) o 0 (PCF8574)"
#endif

// ======================== COMÚN ==============================================
void lcd_borrar() { comando(CMD_BORRAR); delay(2); }
void lcd_cursor(uint8_t col, uint8_t fila) { comando(CMD_DDRAM | ((fila ? 0x40 : 0x00) + col)); }
void lcd_texto(const char *s) { while (*s) dato((uint8_t)*s++); }
void lcd_texto(const __FlashStringHelper *s) {
  const char *p = (const char *)s; char c;
  while ((c = pgm_read_byte(p++))) dato((uint8_t)c);
}
void lcd_fila(uint8_t fila, const char *s) {
  lcd_cursor(0, fila);
  uint8_t i = 0;
  for (; i < 16 && s[i]; i++) dato((uint8_t)s[i]);
  for (; i < 16; i++) dato(' ');
}
