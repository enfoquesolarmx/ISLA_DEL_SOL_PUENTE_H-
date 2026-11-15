/*
 * =============================================================================
 *  enlace_protocolo.h  -  Isla del Sol: protocolo I2C CRÍTICO <-> AUX
 *  ESTE ARCHIVO DEBE SER IDÉNTICO EN LOS DOS SKETCHES (CRÍTICO y AUX).
 * -----------------------------------------------------------------------------
 *  DC-017: el CRÍTICO es ESCLAVO (dirección 0x20); el AUX es el MAESTRO.
 *  El CRÍTICO nunca espera al bus: sólo responde con una "foto" ya armada.
 *
 *  COMANDO del AUX al CRÍTICO (4 bytes):
 *    [0] tipo   [1] secuencia   [2] dato   [3] CRC-8 de 0..2
 *  ESTADO del CRÍTICO al AUX (16 bytes):
 *    [0]  versión del protocolo        [1]  secuencia de la foto
 *    [2]  eco: secuencia del último comando válido
 *    [3]  banderas: b0 SPWM activa, b1 detenida por P9, b2 línea de 50 Hz
 *    [4]  estado de P9 (EstadoP9)       [5]  tiempo muerto en cuentas
 *    [6-7]   ciclos de línea            [8-9]   lectura de A7 (P9)
 *    [10-11] amplitud máxima            [12-13] comandos con CRC malo
 *    [14] reservado (0)                 [15] CRC-8 de 0..14
 *  Números de 16 bits: byte bajo primero.
 *  CRC-8 SMBus: polinomio 0x07, valor inicial 0x00.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

constexpr uint8_t ENLACE_DIRECCION_CRITICO = 0x20;
constexpr uint8_t ENLACE_VERSION           = 1;
constexpr uint8_t ENLACE_LARGO_COMANDO     = 4;
constexpr uint8_t ENLACE_LARGO_ESTADO      = 16;
constexpr uint8_t ENLACE_CMD_PING          = 0x01;

inline uint8_t enlace_crc8(const uint8_t *d, uint8_t n) {
  uint8_t crc = 0;
  while (n--) {
    crc ^= *d++;
    for (uint8_t i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}
