#!/usr/bin/env python3
"""
genera_tabla_seno.py - Isla del Sol, firmware del CRÍTICO

Genera ISLA_CRITICO/tabla_seno.h: un cuarto de onda de seno (100 muestras)
en formato Q16 (0 .. 65535 = 0 .. 1.0), guardado en flash (PROGMEM).

Por qué un cuarto de onda y por qué muestreado al centro del periodo:
ver el bloque "MUESTRAS DE LA TABLA SENO" en config_portadora.h.

Uso (desde la carpeta firmware/critico):
    python3 herramientas/genera_tabla_seno.py
Sólo hace falta volver a correrlo si cambia MUESTRAS_CICLO (400).

Licencia: MIT.
"""
import math
import os

MUESTRAS_CICLO = 400                    # debe coincidir con config_portadora.h
MUESTRAS_SEMICICLO = MUESTRAS_CICLO // 2
MUESTRAS_CUARTO = MUESTRAS_CICLO // 4
ESCALA = 65535                          # Q16: 1.0 -> 65535

assert MUESTRAS_CICLO % 4 == 0

# Muestra al CENTRO de cada periodo de portadora: (k + 0.5)
tabla = [round(ESCALA * math.sin((k + 0.5) * math.pi / MUESTRAS_SEMICICLO))
         for k in range(MUESTRAS_CUARTO)]

# Verificación del espejo: la muestra k y la (199 - k) del semiciclo deben ser iguales.
semiciclo = [ESCALA * math.sin((k + 0.5) * math.pi / MUESTRAS_SEMICICLO)
             for k in range(MUESTRAS_SEMICICLO)]
for k in range(MUESTRAS_SEMICICLO):
    q = k if k < MUESTRAS_CUARTO else MUESTRAS_SEMICICLO - 1 - k
    assert abs(tabla[q] - semiciclo[k]) <= 0.5, (k, tabla[q], semiciclo[k])

aqui = os.path.dirname(os.path.abspath(__file__))
salida = os.path.join(aqui, '..', 'ISLA_CRITICO', 'tabla_seno.h')

lineas = []
for i in range(0, MUESTRAS_CUARTO, 10):
    fila = ', '.join(f'{v:5d}' for v in tabla[i:i + 10])
    grados = (i + 0.5) * 180 / MUESTRAS_SEMICICLO
    lineas.append(f'  {fila},   // k = {i:2d}..{i + 9:2d}  ({grados:5.2f}° ...)')

contenido = f"""/*
 * =============================================================================
 *  tabla_seno.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  ARCHIVO GENERADO por herramientas/genera_tabla_seno.py: no editar a mano.
 * -----------------------------------------------------------------------------
 *  Un cuarto de onda de seno: {MUESTRAS_CUARTO} muestras, de 0° a 90°.
 *    tabla[k] = round( 65535 × sen( (k + 0.5) × 180° / {MUESTRAS_SEMICICLO} ) )
 *  Q16: 65535 = 1.0. Cada muestra tomada al CENTRO de su periodo de portadora,
 *  para que el espejo (muestra k = muestra {MUESTRAS_SEMICICLO - 1} - k) sea exacto.
 *  Ocupa {MUESTRAS_CUARTO * 2} B de flash y 0 B de RAM (PROGMEM).
 *  Por qué así: ver "MUESTRAS DE LA TABLA SENO" en config_portadora.h.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>
#include "config_portadora.h"

static_assert(MUESTRAS_CUARTO == {MUESTRAS_CUARTO},
  "tabla_seno.h no corresponde a MUESTRAS_CICLO: vuelva a correr genera_tabla_seno.py");

const uint16_t TABLA_SENO_Q16[{MUESTRAS_CUARTO}] PROGMEM = {{
{chr(10).join(lineas)}
}};
"""
with open(salida, 'w', encoding='utf-8') as f:
    f.write(contenido)
print(f'Escrito {os.path.normpath(salida)}: {MUESTRAS_CUARTO} muestras, '
      f'primera {tabla[0]}, última {tabla[-1]}')
