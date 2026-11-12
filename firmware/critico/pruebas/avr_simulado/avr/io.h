// Registros del ATmega328P simulados en la PC, sólo para pruebas/prueba_spwm.cpp.
#pragma once
#include <stdint.h>
#define _BV(b) (1u << (b))
extern volatile uint8_t  PORTB, PORTD, DDRB, DDRD;
extern volatile uint8_t  TCCR1A, TCCR1B, TIMSK1, TIFR1;
extern volatile uint16_t TCNT1, ICR1, OCR1A, OCR1B;
#define PB1 1
#define PB2 2
#define PB3 3
#define PB4 4
#define PD3 3
#define COM1A1 7
#define COM1A0 6
#define COM1B1 5
#define COM1B0 4
#define WGM13 4
#define CS10 0
#define TOV1 0
#define TOIE1 0
