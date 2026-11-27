// Registros del ATmega328P simulados en la PC, sólo para pruebas/prueba_spwm.cpp.
#pragma once
#include <stdint.h>
#define _BV(b) (1u << (b))
extern volatile uint8_t  PORTB, PORTD, DDRB, DDRD;
extern volatile uint8_t  TCCR1A, TCCR1B, TIMSK1, TIFR1;

/*  ⏳ 2026-11-27: el SPWM ahora incluye `reloj_4us.h` (el sello de fase) y
 *  `adc_portadora.h`, asi que la prueba de escritorio necesita tambien los
 *  registros del Timer0 y del ADC. No se simulan: solo existen para que el
 *  modulo enlace. La prueba del SPWM no los usa.
 */
extern volatile uint8_t  TCNT0, TIFR0, ADCSRA, ADMUX, ADCSRB, DIDR0;
extern volatile uint16_t ADC;
extern volatile unsigned long timer0_overflow_count;
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
#define TOV0 0
#define ADSC 6
#define ADEN 7
#define ADIE 3
#define ADIF 4
#define ADPS0 0
#define ADPS1 1
#define ADPS2 2
#define REFS0 6
#define REFS1 7
#define ADLAR 5
#define ACD 7
#define ACO 5
#define ACI 4
#define ACIE 3
#define ADC0D 0
#define ADC2D 2
#define ADC3D 3
#define TOIE1 0
