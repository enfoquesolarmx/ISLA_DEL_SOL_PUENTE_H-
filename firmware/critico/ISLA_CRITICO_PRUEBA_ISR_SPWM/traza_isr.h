/*
 * =============================================================================
 *  traza_isr.h  -  Isla del Sol, protocolo de banco Parte C, etapa 1
 * -----------------------------------------------------------------------------
 *  Define los enganches de medición de la ISR del SPWM.
 *
 *  spwm_328p.cpp busca este archivo con __has_include y, si lo encuentra, lo
 *  incluye. En la carpeta de producción (ISLA_CRITICO/) NO EXISTE, así que las
 *  macros quedan vacías y el código compilado es byte por byte el mismo.
 *  Verificado: el .o da el mismo md5 con y sin este archivo presente.
 *
 *  Gracias a esto el módulo se compila como un .cpp NORMAL del sketch. No hay
 *  que incluir un .cpp desde el .ino — eso provocaba símbolos duplicados
 *  (__vector_13 definido dos veces) porque el IDE de Arduino compila por su
 *  cuenta todo .cpp que encuentra en la carpeta del sketch.
 *
 *  ORDEN DE LAS OPERACIONES, que importa:
 *    - al entrar: primero sube el pin, luego lee TCNT1
 *    - al salir : primero lee TCNT1, luego baja el pin
 *  Así el pin nunca mide MENOS que el software, y el número del software
 *  queda del lado pesimista.
 * =============================================================================
 */

#pragma once

#include <avr/io.h>

// Pin de traza para el analizador lógico: PD4 = D4 (INV_CMD con J15 puesto).
// ⚠️ J15 DESCONECTADO durante toda la prueba — ver Parte C, sección C0.
#define TRZ_BIT    PD4

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint16_t isr_t0;     // TCNT1 al entrar: vector + prólogo ya idos
extern volatile uint16_t isr_t1;     // TCNT1 al salir, antes del epílogo
extern volatile uint8_t  isr_lista;  // bandera: hay una muestra nueva

#ifdef __cplusplus
}
#endif

#define SPWM_TRAZA_ENTRA()                      \
  do {                                          \
    PORTD |= _BV(TRZ_BIT);                      \
    isr_t0 = TCNT1;                             \
  } while (0)

#define SPWM_TRAZA_SALE()                       \
  do {                                          \
    isr_t1 = TCNT1;                             \
    isr_lista = 1;                              \
    PORTD &= ~_BV(TRZ_BIT);                     \
  } while (0)
