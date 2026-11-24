/*
 * =============================================================================
 *  traza_isr.h  -  Isla del Sol, protocolo de banco Parte C, etapa 1
 *  v4 — histograma de latencia de entrada, para identificar a los ladrones
 *       por el ANCHO de su bloqueo y no por descarte
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
 *            el piso fijo = vector (7) + prólogo (32) + traza (~4) = 43
 *            la ESPERA    = lo que tardó otro en soltar las interrupciones
 *        Medido en banco: el piso es 43, clavado en todos los reportes.
 *
 *    isr_t1 - isr_t0 = el CUERPO. Costo propio del SPWM, no depende de quién
 *        más esté corriendo. Medido: 49/116/126 ciclos, idéntico siempre.
 *
 *    isr_sobrepasos: el AVR limpia TOV1 al entrar a la ISR. Si al SALIR está
 *        puesto otra vez, hubo un segundo desborde mientras estábamos adentro
 *        y se perdió una actualización de OCR. Exacto, sin umbral.
 *
 *    isr_n: cuenta TODAS las ISR; el lazo sólo alcanza a muestrear ~99 %.
 *
 *  ⚠️ La instrumentación que corre DESPUÉS de leer isr_t1 (el contador y la
 *  comprobación de TOV1, ~23 ciclos) alarga la ISR real pero NO entra en el
 *  número reportado. Por eso el detector de sobrepasos es algo pesimista.
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
