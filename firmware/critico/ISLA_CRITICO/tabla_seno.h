/*
 * =============================================================================
 *  tabla_seno.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  ARCHIVO GENERADO por herramientas/genera_tabla_seno.py: no editar a mano.
 * -----------------------------------------------------------------------------
 *  Un cuarto de onda de seno: 100 muestras, de 0° a 90°.
 *    tabla[k] = round( 65535 × sen( (k + 0.5) × 180° / 200 ) )
 *  Q16: 65535 = 1.0. Cada muestra tomada al CENTRO de su periodo de portadora,
 *  para que el espejo (muestra k = muestra 199 - k) sea exacto.
 *  Ocupa 200 B de flash y 0 B de RAM (PROGMEM).
 *  Por qué así: ver "MUESTRAS DE LA TABLA SENO" en config_portadora.h.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>
#include "config_portadora.h"

static_assert(MUESTRAS_CUARTO == 100,
  "tabla_seno.h no corresponde a MUESTRAS_CICLO: vuelva a correr genera_tabla_seno.py");

const uint16_t TABLA_SENO_Q16[100] PROGMEM = {
    515,  1544,  2573,  3601,  4629,  5655,  6680,  7703,  8724,  9743,   // k =  0.. 9  ( 0.45° ...)
  10760, 11774, 12785, 13793, 14798, 15799, 16796, 17789, 18777, 19761,   // k = 10..19  ( 9.45° ...)
  20740, 21714, 22683, 23646, 24603, 25554, 26499, 27437, 28368, 29293,   // k = 20..29  (18.45° ...)
  30210, 31120, 32022, 32916, 33802, 34680, 35549, 36409, 37261, 38103,   // k = 30..39  (27.45° ...)
  38936, 39759, 40572, 41376, 42169, 42952, 43724, 44485, 45236, 45975,   // k = 40..49  (36.45° ...)
  46703, 47419, 48124, 48817, 49497, 50166, 50822, 51466, 52097, 52715,   // k = 50..59  (45.45° ...)
  53320, 53912, 54490, 55056, 55607, 56145, 56669, 57179, 57675, 58157,   // k = 60..69  (54.45° ...)
  58624, 59077, 59515, 59939, 60348, 60742, 61120, 61484, 61833, 62167,   // k = 70..79  (63.45° ...)
  62485, 62787, 63075, 63346, 63602, 63842, 64067, 64276, 64469, 64646,   // k = 80..89  (72.45° ...)
  64807, 64952, 65081, 65194, 65291, 65371, 65436, 65484, 65517, 65533,   // k = 90..99  (81.45° ...)
};
