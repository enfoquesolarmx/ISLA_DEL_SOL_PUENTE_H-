/*
 * =============================================================================
 *  traza_adc.h  -  Isla del Sol, protocolo de banco Parte C, etapa 2
 * -----------------------------------------------------------------------------
 *  adc_portadora.cpp busca este archivo con __has_include. En la carpeta de
 *  producción NO EXISTE, así que las macros quedan vacías.
 *
 *  POR QUÉ LA ISR DEL ADC SE MIDE CON EL ANALIZADOR Y NO CON TCNT1
 *
 *    Para la ISR del SPWM sí usamos TCNT1: arranca siempre en BOTTOM, así que
 *    el contador leído adentro dice cuántos ciclos van desde el disparo.
 *
 *    La del ADC NO sirve así. Llega 104 µs después del disparo, o sea en
 *    cualquier punto del periodo — y en modo 8 el contador **sube y baja**.
 *    Una resta entre dos lecturas es ambigua: no se sabe si el contador iba
 *    de ida o de regreso, ni si cruzó el TOP en medio.
 *
 *    Por eso aquí sólo movemos un pin, y el ancho lo mide el analizador.
 *
 *  Pin: PD5 = D5, que es CARG_CMD con J15 puesto.
 *  ⚠️ J15 DESCONECTADO durante toda la prueba — ver Parte C, sección C0.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>

#define TRZ_ADC_BIT   PD5

#define ADC_TRAZA_ENTRA()   (PORTD |=  _BV(TRZ_ADC_BIT))
#define ADC_TRAZA_SALE()    (PORTD &= ~_BV(TRZ_ADC_BIT))
