/*
 * =============================================================================
 *  spwm_4809.cpp  -  Isla del Sol, micro CRÍTICO (A1), ATmega4809 (Nano Every)
 *  SPWM híbrida con TCA0 (dual-slope, PER = 416)  -  PENDIENTE
 * -----------------------------------------------------------------------------
 *  Misma lógica que spwm_328p.cpp (ver su encabezado), con estas diferencias
 *  de hardware (DC-018):
 *    - TCA0 en modo dual-slope, PER = TOP_CUENTAS (416), interrupción en BOTTOM.
 *    - LA invertida con INVEN del pin (no hay modo de comparador invertido).
 *    - CMP0 = HA, CMP1 = LA, CMP1 = CMP0 + DT.
 *    - Tiempo por cuenta 50 ns: DT de 10 / 20 cuentas.
 *  Hasta que se escriba, compilar para el Nano Every da este error a propósito:
 *  así nadie carga un firmware sin SPWM pensando que funciona.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include "config_portadora.h"

#if ISLA_CHIP_4809
  #error "SPWM para ATmega4809 (TCA0) todavía no está escrita: use un Nano (ATmega328P)"
#endif
