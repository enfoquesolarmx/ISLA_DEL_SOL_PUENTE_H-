/*
 * =============================================================================
 *  traza_isr.h  -  Isla del Sol, protocolo de banco Parte C, etapa 1
 *  v3 — separa costo propio de latencia de entrada, y detecta sobrepasos
 * -----------------------------------------------------------------------------
 *  spwm_328p.cpp busca este archivo con __has_include y, si lo encuentra, lo
 *  incluye. En la carpeta de producción (ISLA_CRITICO/) NO EXISTE, así que las
 *  macros quedan vacías y el código compilado es byte por byte el mismo.
 *  Verificado: el .o da el mismo md5 con y sin este archivo presente.
 *
 *  QUÉ MIDE CADA COSA
 *
 *    isr_t0 = TCNT1 al entrar.
 *        TOV1 se levanta en BOTTOM con TCNT1 = 0, así que esto es el tiempo
 *        desde que la interrupción se disparó hasta la primera instrucción
 *        útil. Son dos cosas sumadas:
 *            el piso fijo    = vector (7) + prólogo (32) + traza (~4) = ~43
 *            la ESPERA       = lo que tardó otra ISR en soltar el CPU
 *        El mínimo de isr_t0 en una corrida larga ES el piso. Todo lo que
 *        pase de ahí es tiempo robado por otro.
 *
 *    isr_t1 - isr_t0 = el CUERPO. Esto sí es costo propio del SPWM y no
 *        depende de quién más esté corriendo.
 *
 *    costo propio = piso + cuerpo + epílogo.  Es el número de B5.
 *    ocupación real = isr_t1 + epílogo.       Es lo que hay que caber.
 *
 *    isr_sobrepasos: el AVR limpia TOV1 al entrar a la ISR. Si al SALIR la
 *        bandera está puesta otra vez, hubo un segundo desborde mientras
 *        estábamos adentro — o sea, no terminamos a tiempo y se perdió una
 *        actualización de OCR. Detección exacta, sin umbral y sin falsos
 *        positivos.
 *
 *        ⚠️ Es LIGERAMENTE PESIMISTA: la propia instrumentación que corre
 *        después de leer isr_t1 (el contador y esta misma comprobación,
 *        ~20 ciclos) alarga la ISR. En producción esos 20 ciclos no existen.
 *
 *    isr_n: cuenta TODAS las ISR. El lazo sólo alcanza a recoger ~99.4 % de
 *        las muestras mientras imprime, así que contar en el lazo daba de
 *        menos. Esto cuenta de verdad.
 *
 *  ORDEN DE LAS OPERACIONES, que importa:
 *    - al entrar: primero sube el pin, luego lee TCNT1
 *    - al salir : primero lee TCNT1, LUEGO todo lo demás
 *  Así el cuerpo reportado no incluye la instrumentación de salida, y el pin
 *  nunca mide menos que el software.
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

extern volatile uint16_t isr_t0;          // TCNT1 al entrar  (piso + espera)
extern volatile uint16_t isr_t1;          // TCNT1 al salir, antes del epílogo
extern volatile uint8_t  isr_lista;       // bandera: hay una muestra nueva
extern volatile uint32_t isr_n;           // TODAS las ISR, no sólo las recogidas
extern volatile uint16_t isr_sobrepasos;  // no terminó antes del siguiente TOV

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
    isr_n++;                                    \
    if (TIFR1 & _BV(TOV1)) isr_sobrepasos++;    \
    isr_lista = 1;                              \
    PORTD &= ~_BV(TRZ_BIT);                     \
  } while (0)
