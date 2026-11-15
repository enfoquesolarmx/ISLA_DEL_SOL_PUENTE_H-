/*
 * =============================================================================
 *  config_panel.h  -  Isla del Sol, micro AUX (A2): panel LCD 16x2 + botonera
 * -----------------------------------------------------------------------------
 *  AQUÍ SE ESCOGE LA PANTALLA (filosofía UNIX: una sola perilla, en el script).
 *
 *    LCD_TIPO = 1  ->  AIP31068  (LCD I2C "nativo", dirección 0x3E).
 *                      Es el que trae SimulIDE 1.1 y el de los Grove LCD.
 *    LCD_TIPO = 0  ->  PCF8574 + HD44780 (el 16x2 "normal" con mochila I2C
 *                      azul que venden en todas las tiendas, 0x27 o 0x3F).
 *
 *  Cambia el número, compila y sube. Nada más cambia.
 *
 *  BUS DE LA PANTALLA (hardware/control, mapa de pines del AUX)
 *    D4 = SDA_LCD, D5 = SCL_LCD  ->  I2C por software, SEPARADO del enlace
 *    con el CRÍTICO (A4/A5). Así una pantalla trabada o un cable largo del
 *    panel nunca tumba el enlace de control.
 *    Pull-ups: el Nano pone las internas (~35 kΩ), pero con cable de panel o
 *    en SimulIDE pon 4.7 kΩ a 5 V en D4 y D5.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// ======================== LA PERILLA =========================================
#define LCD_TIPO 1          // 1 = AIP31068 (SimulIDE / Grove), 0 = PCF8574 16x2

// ======================== PERILLA DE SIMULADOR ================================
// SIMULIDE = 1 SÓLO para SimulIDE 1.1: ahí el enlace I2C no funciona (el
// CRÍTICO simulado no suelta SDA al transmitir), así que el AUX lee la batería
// de una COPIA del divisor de DC-007 dibujada en su A3 (libre en el AUX).
// SIMULIDE = 0 en el firmware real: la batería llega del CRÍTICO por el enlace
// y A3 del AUX no se usa (en la tarjeta está sin conectar).
#define SIMULIDE 0
constexpr uint8_t PIN_BAT_SIMULIDE = A3;

// ======================== DIRECCIONES (7 bits) ===============================
constexpr uint8_t LCD_DIR_AIP31068 = 0x3E;
constexpr uint8_t LCD_DIR_PCF8574  = 0x27;  // si tu mochila es PCF8574A: 0x3F

// ======================== PINES ==============================================
constexpr uint8_t PIN_SDA_LCD  = 4;
constexpr uint8_t PIN_SCL_LCD  = 5;
constexpr uint8_t PIN_BOTONES  = A7;        // escalera de resistencias (panel §3)
constexpr uint8_t PIN_AUX_KILL = 7;         // AUX_KILL: HIGH = no matar (mapa de pines)
constexpr uint8_t PIN_CS_CAN   = 10;        // D10 debe ser OUTPUT (SPI maestro)

// ======================== I2C POR SOFTWARE ===================================
// Medio periodo en µs. 5 µs + lo que tarda digitalWrite ≈ 50–70 kHz.
constexpr uint8_t I2C_SW_MEDIO_PERIODO_US = 5;
// Tiempo máximo esperando que el esclavo suelte SCL (clock stretching).
constexpr uint16_t I2C_SW_ESPERA_SCL_US = 1000;
