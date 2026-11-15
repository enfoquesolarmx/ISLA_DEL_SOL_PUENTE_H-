/*
 * =============================================================================
 *  i2c_sw.h  -  Isla del Sol, AUX: maestro I2C por software (bit-bang)
 * -----------------------------------------------------------------------------
 *  RECURSOS: PIN_SDA_LCD y PIN_SCL_LCD (config_panel.h). No usa Wire ni el TWI:
 *  el TWI (A4/A5) es del enlace con el CRÍTICO.
 *
 *  DRENADOR ABIERTO EMULADO: un "1" nunca se maneja a 5 V; se suelta la línea
 *  (INPUT_PULLUP) y la sube la resistencia. Un "0" es OUTPUT en bajo. Así dos
 *  equipos nunca pelean (uno a 5 V y otro a GND = corto).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#pragma once
#include <stdint.h>

void i2c_sw_iniciar();
// Escribe n bytes a la dirección (7 bits). true si TODOS los bytes tuvieron ACK.
bool i2c_sw_escribir(uint8_t dir, const uint8_t *datos, uint8_t n);
// true si alguien contesta ACK en esa dirección.
bool i2c_sw_hay_equipo(uint8_t dir);
// Libera un bus trabado: 9 pulsos de SCL + STOP.
void i2c_sw_recuperar();
