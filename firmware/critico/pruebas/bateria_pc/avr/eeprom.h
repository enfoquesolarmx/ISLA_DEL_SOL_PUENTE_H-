// avr/eeprom.h falso para la prueba en PC: 1 KB en RAM.
#pragma once
#include <string.h>
#include <stdint.h>
extern uint8_t g_eeprom[1024];
inline void eeprom_read_block(void *d, const void *a, size_t n) { memcpy(d, g_eeprom + (uintptr_t)a, n); }
inline void eeprom_update_block(const void *s, void *a, size_t n) { memcpy(g_eeprom + (uintptr_t)a, s, n); }
