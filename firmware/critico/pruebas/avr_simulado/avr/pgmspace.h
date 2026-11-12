// Simulado para pruebas en la PC: la flash es memoria normal.
#pragma once
#define PROGMEM
#define pgm_read_word(p) (*(p))
