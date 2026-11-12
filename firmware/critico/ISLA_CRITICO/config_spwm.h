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
//     compuerta. Por eso la amplitud se limita (ver AMPLITUD_MAX_CUENTAS).
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
// La amplitud máxima deja SIEMPRE, con el tiempo muerto más largo (1 µs),
// un pulso del lado complementario de al menos PULSO_MIN. Ese pulso está
// centrado en TOP (o se arma con dos mitades iguales), así que basta con
// reservar la MITAD del pulso mínimo por lado de la rampa:
//   328P: 333 - 16 - 8/2  = 313  -> m máx = 0.939
//   4809: 416 - 20 - 10/2 = 391  -> m máx = 0.939
// Se usa el tiempo muerto legacy aunque el módulo sea moderno: así el límite
// no cambia según el gate driver instalado y el lazo de voltaje ve siempre
// el mismo techo.
constexpr uint16_t AMPLITUD_MAX_CUENTAS =
    TOP_CUENTAS - DT_LEGACY_CUENTAS - PULSO_MIN_CUENTAS / 2;

// Índice de modulación máximo en milésimas (sólo informativo, para el reporte).
constexpr uint16_t M_MAX_MILESIMAS =
    (uint16_t)((uint32_t)AMPLITUD_MAX_CUENTAS * 1000UL / TOP_CUENTAS);

static_assert(PULSO_MIN_CUENTAS >= 1, "El pulso mínimo debe ser de al menos 1 cuenta");
static_assert(AMPLITUD_MAX_CUENTAS <= COMPARADOR_ALTO_MAX,
  "La amplitud máxima no puede pasar del comparador máximo");
static_assert(M_MAX_MILESIMAS >= 900,
  "El índice de modulación máximo quedó abajo de 0.90: revise pulso mínimo y tiempo muerto");
