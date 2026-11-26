/*
 * =============================================================================
 *  gancho_spwm.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Lo que hay que hacer en cada desborde de la portadora
 * -----------------------------------------------------------------------------
 *  spwm_328p.cpp busca este archivo con __has_include. Si está, lo incluye y
 *  llama a SPWM_CADA_PERIODO() al principio de su ISR, lo más cerca posible
 *  de BOTTOM. Si no está —como en los sketches que sólo prueban el SPWM— la
 *  macro queda vacía y el módulo del SPWM no depende de nada.
 *
 *  Hoy aquí sólo cuelga el disparo del ADC. Cuesta un contador y un ADSC.
 *
 *  ⚠️ Lo que se ponga aquí corre DENTRO de la ISR del SPWM, 24 024 veces por
 *  segundo, y se suma a los 12.75 µs que ya cuesta. Antes de agregar algo,
 *  medirlo: el presupuesto está en la Parte C del protocolo de banco.
 * =============================================================================
 */

#pragma once

#include "adc_portadora.h"

#define SPWM_CADA_PERIODO()   adc_disparar_si_toca()
