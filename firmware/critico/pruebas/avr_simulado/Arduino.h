// Arduino simulado en la PC, sólo para pruebas/prueba_p9.cpp.
#pragma once
#include <stdint.h>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define A7 21
uint16_t analogRead(uint8_t pin);   // la define la prueba
uint32_t millis();                  // la define la prueba
void delay(uint32_t ms);            // la define la prueba
