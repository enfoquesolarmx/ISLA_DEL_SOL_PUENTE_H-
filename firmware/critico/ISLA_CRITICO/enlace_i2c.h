/*
 * =============================================================================
 *  enlace_i2c.h  -  Isla del Sol, micro CRÍTICO (A1): esclavo I2C hacia el AUX
 * -----------------------------------------------------------------------------
 *  RECURSOS: A4 (SDA) y A5 (SCL), periférico TWI y su interrupción.
 *  Las interrupciones de I2C sólo copian bytes; el trabajo se hace en loop().
 *  Protocolo: enlace_protocolo.h.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

void enlace_iniciar();
// Llamar en cada vuelta de loop(): procesa comandos y renueva la foto cada 50 ms.
void enlace_atender(uint8_t estadoP9, uint16_t adcP9, bool detenidoPorP9);
// Para el reporte por Serial (sólo banco).
uint16_t enlace_comandos_ok();
uint16_t enlace_comandos_malos();
uint8_t  enlace_ultimo_seq();
// Presencia del AUX (PING válido en los últimos 500 ms) y aviso de cambio.
bool enlace_aux_presente();
bool enlace_aux_cambio();   // true una vez cada que cambia la presencia
