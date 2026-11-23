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
 * =============================================================================
 */

#pragma once

#include "rms_ventana.h"

#define ADC_CADA_MUESTRA(ranura, cuenta)   rms_meter((ranura), (cuenta))
