/*
 * =============================================================================
 *  gancho_adc.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Lo que hay que hacer con cada muestra, dentro de la ISR del ADC
 * -----------------------------------------------------------------------------
 *  adc_portadora.cpp busca este archivo con __has_include. Si está, llama a
 *  ADC_CADA_MUESTRA(ranura, cuenta) justo después de leer el registro del ADC.
 *  Si no está —como en los sketches que sólo prueban el ADC— la macro queda
 *  vacía y el módulo del ADC no depende de nada.
 *
 *  Hoy aquí cuelga la acumulación del RMS, y va dentro de la ISR a propósito:
 *  el lazo se queda ciego mientras hay tráfico I2C (etapa 3), así que un RMS
 *  acumulado en el lazo dependería del tráfico del bus. Ver rms_ventana.h.
 *
 *  ⚠️ Lo que se ponga aquí corre 6 006 veces por segundo y se suma a los
 *  ~7.8 µs que ya cuesta la ISR del ADC. La acumulación son dos sumas y una
 *  multiplicación de 16×16: medirlo en la etapa 4 con la traza de D5, no
 *  suponerlo.
 *
 *  ⏳ 2026-11-25: se agrega `sct_meter`, la pinza de corriente en A2. Sale en
 *  ~4 ciclos para las 7 ranuras de 8 que no son la suya, y sólo acumula en la
 *  octava. **El costo no está medido** — toca medirlo antes de cerrar la
 *  etapa 5, porque el presupuesto de la ISR del ADC está en 127 ciclos y la
 *  suma de las cuatro interrupciones en 507 de 666.
 * =============================================================================
 */

#pragma once

#include "rms_ventana.h"
#include "sensor_corriente.h"

#define ADC_CADA_MUESTRA(ranura, cuenta)        \
  do {                                          \
    rms_meter((ranura), (cuenta));              \
    sct_meter((ranura), (cuenta));              \
  } while (0)
