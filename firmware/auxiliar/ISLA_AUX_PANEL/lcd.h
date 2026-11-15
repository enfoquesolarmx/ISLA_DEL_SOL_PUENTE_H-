/*
 * =============================================================================
 *  lcd.h  -  Isla del Sol, AUX: pantalla 16x2 por I2C, UNA sola interfaz
 * -----------------------------------------------------------------------------
 *  El resto del firmware sólo conoce estas funciones. Si es AIP31068 o
 *  PCF8574 lo decide LCD_TIPO en config_panel.h y vive en lcd.cpp:
 *
 *    AIP31068: el controlador ya habla I2C. Cada envío es
 *              [dirección][control][byte]; control 0x80 = comando,
 *              0x40 = dato (letra). Bus de 8 bits por dentro: un envío por byte.
 *    PCF8574 : es un expansor de 8 salidas cableado al HD44780 en modo
 *              4 bits:  P0=RS  P1=RW  P2=EN  P3=luz  P4..P7=D4..D7.
 *              Cada byte se manda en dos mitades y cada mitad con un pulso
 *              en EN (el HD44780 lee en el flanco de bajada de EN).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// true si la pantalla contestó ACK al iniciarse.
bool lcd_iniciar();
void lcd_borrar();
void lcd_cursor(uint8_t col, uint8_t fila);     // fila 0 o 1
void lcd_texto(const char *s);                  // texto en RAM
class __FlashStringHelper;
void lcd_texto(const __FlashStringHelper *s);   // texto en flash: F("...")
// Escribe una fila completa (16 caracteres, rellena con espacios): sin parpadeo.
void lcd_fila(uint8_t fila, const char *s);
void lcd_luz(bool encendida);                   // sólo PCF8574 (AIP: sin efecto)
uint8_t lcd_direccion();
const __FlashStringHelper *lcd_nombre();
