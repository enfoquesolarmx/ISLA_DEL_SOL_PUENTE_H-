// Arduino.h falso, sólo para prueba_bateria.cpp: millis() y analogRead() controlados por la prueba.
#pragma once
#include <stdint.h>
#define A3 17
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper*)(s))
extern uint32_t g_ms; extern uint16_t g_adc;
inline uint32_t millis(){return g_ms;}
inline uint16_t analogRead(uint8_t){return g_adc;}
