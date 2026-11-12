// Simulado para pruebas en la PC: la ISR se vuelve una función que llama la prueba.
#pragma once
#define ISR(v) extern "C" void v(void)
