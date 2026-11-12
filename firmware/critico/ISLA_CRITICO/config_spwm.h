/*
 * =============================================================================
 *  config_spwm.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Límites del ciclo de trabajo de la SPWM híbrida
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Fija el pulso más angosto que se permite y la amplitud máxima de la
 *    senoidal, en cuentas del timer. Sólo constantes.
 *
 *  ESTADO: valores INICIALES, POR VALIDAR EN BANCO con el osciloscopio
 *          (ver README.md, prueba de banco del paso 2).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include "config_portadora.h"

// ======================== PULSO MÍNIMO =======================================
// Un pulso de compuerta más angosto que esto no se genera:
//   - Cerca del cruce por cero el seno pide pulsos de pocas cuentas (la muestra
//     0 pide 0.8 % del periodo). El gate driver y el MOSFET no alcanzan a
//     encender completo en ~100 ns: el MOSFET pasa por la zona lineal y se
//     calienta sin entregar energía útil. Se cambia por "sin pulso".
//   - Del otro lado, cerca del pico, el pulso del lado bajo se hace angosto.
//     Ese pulso es el que recarga el capacitor de bootstrap del lado alto en
//     cada periodo: si desaparece, el lado alto se queda sin voltaje de
//     compuerta. Por eso la amplitud se limita (ver AMPLITUD MÁXIMA).
// Valor inicial: 500 ns (igual al tiempo muerto moderno). Por validar en banco.
//
// OJO, doble pendiente: el pulso que está centrado en BOTTOM se arma con la
// bajada de un periodo (comparador c1) y la subida del siguiente (c2): dura
// c1 + c2 cuentas. Si el periodo vecino no lleva pulso (c2 = 0), el pulso
// queda sólo con su mitad: c1 cuentas. Por eso el umbral del comparador es el
// ancho COMPLETO del pulso mínimo, no la mitad:
//   500 ns  ->  328P: 8 cuentas;  4809: 10 cuentas
// (Comprobado con pruebas/prueba_spwm.cpp: con la mitad salían pulsos de 250 ns.)
constexpr uint16_t PULSO_MIN_NS      = 500;
constexpr uint8_t  PULSO_MIN_CUENTAS = ISLA_NS_A_CUENTAS(PULSO_MIN_NS);   // 8 / 10

// ======================== AMPLITUD MÁXIMA ====================================
// La amplitud se maneja en cuentas: es el ancho (por lado de la rampa) del
// pulso de potencia en el pico de la senoidal. Índice de modulación
//   m = amplitud / TOP
// La amplitud máxima es la MENOR de dos:
//
// 1) TECHO ABSOLUTO: 95 % del TOP. Decisión del proyecto: la modulación nunca
//    pasa de m = 0.95, con ningún módulo. Deja margen para que el lazo de
//    voltaje no trabaje pegado al límite y para el PLL (que puede bajar el TOP).
//      328P: 333 × 95 % = 316        4809: 416 × 95 % = 395
//
// 2) LÍMITE FÍSICO según el tiempo muerto EN USO (P9): siempre debe quedar un
//    pulso del lado complementario de al menos PULSO_MIN, porque ese pulso
//    recarga el capacitor de bootstrap del lado alto en cada periodo. Está
//    centrado en TOP (o se arma con dos mitades iguales): basta con reservar
//    la MITAD del pulso mínimo por lado de la rampa.
//                         328P                     4809
//      DT 500 ns:   333 - 8  - 4 = 321       416 - 10 - 5 = 401
//      DT 1 µs:     333 - 16 - 4 = 313       416 - 20 - 5 = 391
//
// Resultado (lo que usa spwm_fijar_amplitud según el módulo instalado):
//                         328P                     4809
//      DT 500 ns:   316  (m = 0.949, techo)  395  (m = 0.950, techo)
//      DT 1 µs:     313  (m = 0.939, físico) 391  (m = 0.940, físico)
constexpr uint8_t  M_TECHO_PORCIENTO = 95;
constexpr uint16_t AMPLITUD_TECHO_CUENTAS =
    (uint16_t)((uint32_t)TOP_CUENTAS * M_TECHO_PORCIENTO / 100);

constexpr uint16_t AMPLITUD_FISICA_MODERNO = TOP_CUENTAS - DT_MODERNO_CUENTAS - PULSO_MIN_CUENTAS / 2;
constexpr uint16_t AMPLITUD_FISICA_LEGACY  = TOP_CUENTAS - DT_LEGACY_CUENTAS  - PULSO_MIN_CUENTAS / 2;

constexpr uint16_t AMPLITUD_MAX_MODERNO =
    AMPLITUD_TECHO_CUENTAS < AMPLITUD_FISICA_MODERNO ? AMPLITUD_TECHO_CUENTAS : AMPLITUD_FISICA_MODERNO;
constexpr uint16_t AMPLITUD_MAX_LEGACY =
    AMPLITUD_TECHO_CUENTAS < AMPLITUD_FISICA_LEGACY ? AMPLITUD_TECHO_CUENTAS : AMPLITUD_FISICA_LEGACY;

static_assert(PULSO_MIN_CUENTAS >= 1, "El pulso mínimo debe ser de al menos 1 cuenta");
static_assert(M_TECHO_PORCIENTO <= 95, "El techo absoluto de modulación es 95 %");
static_assert(AMPLITUD_MAX_LEGACY <= COMPARADOR_ALTO_MAX && AMPLITUD_MAX_MODERNO <= TOP_CUENTAS - DT_MODERNO_CUENTAS,
  "La amplitud máxima no puede pasar del comparador máximo");
static_assert((uint32_t)AMPLITUD_MAX_LEGACY * 1000UL / TOP_CUENTAS >= 900,
  "El índice de modulación máximo quedó abajo de 0.90: revise pulso mínimo y tiempo muerto");
