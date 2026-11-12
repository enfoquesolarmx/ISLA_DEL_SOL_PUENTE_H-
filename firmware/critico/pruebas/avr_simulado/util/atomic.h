// Simulado para pruebas en la PC: sin interrupciones reales, el bloque se ejecuta una vez.
#pragma once
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(x) for (int _una_vez = 1; _una_vez; _una_vez = 0)
