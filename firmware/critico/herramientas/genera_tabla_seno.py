#!/usr/bin/env python3
"""
genera_tabla_seno.py - Isla del Sol, firmware del CRÍTICO

Genera ISLA_CRITICO/tabla_seno.h con DOS tablas de un cuarto de onda de seno,
en formato Q16 (0 .. 65535 = 0 .. 1.0), guardadas en flash (PROGMEM):
    TABLA_SENO_60_Q16[100]   para 60 Hz (400 muestras por ciclo a 24 kHz)
    TABLA_SENO_50_Q16[120]   para 50 Hz (480 muestras por ciclo a 24 kHz)

Por qué un cuarto de onda y por qué muestreado al centro del periodo:
ver el bloque "MUESTRAS DE LA TABLA SENO" en config_portadora.h.

Uso (desde la carpeta firmware/critico):
    python3 herramientas/genera_tabla_seno.py
Sólo hace falta volver a correrlo si cambia la portadora (24 kHz).

Licencia: MIT.
"""
import math
import os

FSW_HZ = 24000                          # debe coincidir con FSW_OBJETIVO_HZ
ESCALA = 65535                          # Q16: 1.0 -> 65535


def cuarto_de_onda(hz):
    ciclo = FSW_HZ // hz
    assert FSW_HZ % hz == 0 and ciclo % 4 == 0
    semi, cuarto = ciclo // 2, ciclo // 4
    # Muestra al CENTRO de cada periodo de portadora: (k + 0.5)
    tabla = [round(ESCALA * math.sin((k + 0.5) * math.pi / semi)) for k in range(cuarto)]
    # Verificación del espejo: la muestra k y la (semi - 1 - k) deben ser iguales.
    for k in range(semi):
        q = k if k < cuarto else semi - 1 - k
        exacto = ESCALA * math.sin((k + 0.5) * math.pi / semi)
        assert abs(tabla[q] - exacto) <= 0.5, (hz, k, tabla[q], exacto)
    return ciclo, semi, cuarto, tabla


def bloque(hz):
    ciclo, semi, cuarto, tabla = cuarto_de_onda(hz)
    lineas = []
    for i in range(0, cuarto, 10):
        fila = ', '.join(f'{v:5d}' for v in tabla[i:i + 10])
        grados = (i + 0.5) * 180 / semi
        lineas.append(f'  {fila},   // k = {i:3d}..{i + 9:3d}  ({grados:5.2f}° ...)')
    return f"""// {hz} Hz: {ciclo} muestras por ciclo, {semi} por semiciclo, {cuarto} guardadas.
//   tabla[k] = round( 65535 × sen( (k + 0.5) × 180° / {semi} ) ),  k = 0 .. {cuarto - 1}
static_assert(MUESTRAS_CUARTO_{hz} == {cuarto},
  "tabla_seno.h no corresponde a la portadora: vuelva a correr genera_tabla_seno.py");
const uint16_t TABLA_SENO_{hz}_Q16[{cuarto}] PROGMEM = {{
{chr(10).join(lineas)}
}};
""", cuarto


b60, c60 = bloque(60)
b50, c50 = bloque(50)

contenido = f"""/*
 * =============================================================================
 *  tabla_seno.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  ARCHIVO GENERADO por herramientas/genera_tabla_seno.py: no editar a mano.
 * -----------------------------------------------------------------------------
 *  Dos tablas de un cuarto de onda de seno (0° a 90°), una por frecuencia de
 *  línea, con la portadora fija de {FSW_HZ} Hz.
 *  Q16: 65535 = 1.0. Cada muestra tomada al CENTRO de su periodo de portadora,
 *  para que el espejo (muestra k = muestra S-1 - k) sea exacto.
 *  Ocupan {(c60 + c50) * 2} B de flash y 0 B de RAM (PROGMEM).
 *  Por qué así: ver "MUESTRAS DE LA TABLA SENO" en config_portadora.h.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>
#include "config_portadora.h"

{b60}
{b50}"""

aqui = os.path.dirname(os.path.abspath(__file__))
salida = os.path.join(aqui, '..', 'ISLA_CRITICO', 'tabla_seno.h')
with open(salida, 'w', encoding='utf-8') as f:
    f.write(contenido)
print(f'Escrito {os.path.normpath(salida)}: 60 Hz {c60} muestras, 50 Hz {c50} muestras')
