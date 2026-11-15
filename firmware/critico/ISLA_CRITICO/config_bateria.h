/*
 * =============================================================================
 *  config_bateria.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  QUÍMICA DE LA BATERÍA Y LÍMITES DE CORTE
 * -----------------------------------------------------------------------------
 *  AQUÍ SE ESCOGE LA BATERÍA (una perilla en el script, como la frecuencia en
 *  config_portadora.h; NO se cambia desde el SETUP: una química mal escogida
 *  desde el panel puede matar una batería).
 *
 *    QUIMICA_BATERIA = BAT_PLOMO_ACIDO  ->  24 celdas de 2 V (48 V nominal)
 *    QUIMICA_BATERIA = BAT_LIFEPO4      ->  16 celdas de 3.2 V (51.2 V nominal)
 *
 *  CÓMO SE USAN LOS LÍMITES (bateria.cpp)
 *    AVISO       : por debajo, aviso en el panel. El inversor sigue.
 *    CORTE BAJO  : por debajo durante BAT_RETARDO_CORTE_MS seguidos -> se apaga
 *                  la SPWM (D12 = 0 primero). El retardo deja pasar el bajón
 *                  de unos segundos al arrancar un motor.
 *    EMERGENCIA  : por debajo durante BAT_RETARDO_EMERGENCIA_MS -> se apaga
 *                  casi de inmediato (batería agotada o cable flojo).
 *    RECONEXIÓN  : después de un corte bajo, la SPWM sólo vuelve cuando la
 *                  batería pasa de este valor durante BAT_RETARDO_RECONEXION_MS.
 *                  Está muy arriba del corte A PROPÓSITO: al quitar la carga,
 *                  la batería "rebota" varios volts sin haberse cargado; si la
 *                  reconexión estuviera cerca del corte, el inversor prendería y
 *                  apagaría sin parar (y la batería se acabaría de matar).
 *    CORTE ALTO  : por arriba durante BAT_RETARDO_ALTO_MS -> se apaga. Protege
 *                  al XL7015 (80 V), al TVS D1 y a los capacitores del bus.
 *                  Vuelve al bajar de BAT_ALTO_RECONEXION.
 *    Todo en DÉCIMAS DE VOLT (la misma unidad que bateria_dv()).
 *
 *  LOS VOLTAJES SON CON CARGA: con el inversor trabajando, la batería marca
 *  menos que en reposo. Por eso el corte va más abajo que el "0 %" de las
 *  tablas de reposo.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

#define BAT_PLOMO_ACIDO 0
#define BAT_LIFEPO4     1

// ======================== LA PERILLA =========================================
#define QUIMICA_BATERIA BAT_PLOMO_ACIDO

#if QUIMICA_BATERIA == BAT_PLOMO_ACIDO
// ---------- Plomo-ácido, 24 celdas (inundada, AGM o gel) ----------
// Fuentes (por celda, 25 °C):
//  - Trojan, Flooded Battery Quick Start Guide: reposo 2.122 V = 100 %,
//    2.083 V = 80 %, 2.017 V = 50 %; "no descargar más de 80 %";
//    absorción 2.35–2.45 V, ecualización 2.70 V, flotación 2.25 V.
//  - Victron, inversores VE.Direct: corte por batería baja 1.55 V/celda
//    (9.3 V en 12 V; 37.2 V en 48 V); alarma/reinicio 1.82 V/celda (10.9 V en 12 V).
//  - Diseño de Isla del Sol (decisiones_criticas.md, DC-001/DC-003): rango
//    de plomo 42–64 V; TVS SMBJ64A; XL7015 aguanta 80 V.
constexpr uint16_t BAT_AVISO_DV        = 444;  // 1.85 V/celda: avisar
constexpr uint16_t BAT_CORTE_DV        = 420;  // 1.75 V/celda con carga: cortar
constexpr uint16_t BAT_EMERGENCIA_DV   = 372;  // 1.55 V/celda (piso de Victron)
constexpr uint16_t BAT_RECONEXION_DV   = 500;  // 2.083 V/celda = 80 % en reposo (Trojan)
constexpr uint16_t BAT_ALTO_CORTE_DV   = 660;  // 2.75 V/celda: arriba de la ecualización (2.70 = 64.8 V)
constexpr uint16_t BAT_ALTO_RECONEXION_DV = 640; // 2.67 V/celda
#define BAT_NOMBRE "Plomo-acido 24 celdas"

#elif QUIMICA_BATERIA == BAT_LIFEPO4
// ---------- LiFePO4, 16 celdas ----------
// PROVISIONAL: sale del rango de diseño del proyecto (40–58.4 V = 2.5–3.65
// V/celda, DC-001). Antes de usarlo en campo se revisa contra la hoja del
// fabricante de la batería y los cortes de SU BMS: el inversor debe cortar
// ANTES que el BMS, nunca después.
constexpr uint16_t BAT_AVISO_DV        = 480;  // 3.00 V/celda
constexpr uint16_t BAT_CORTE_DV        = 448;  // 2.80 V/celda
constexpr uint16_t BAT_EMERGENCIA_DV   = 400;  // 2.50 V/celda (mínimo absoluto)
constexpr uint16_t BAT_RECONEXION_DV   = 512;  // 3.20 V/celda
constexpr uint16_t BAT_ALTO_CORTE_DV   = 592;  // 3.70 V/celda (arriba de 58.4 V de carga completa)
constexpr uint16_t BAT_ALTO_RECONEXION_DV = 576; // 3.60 V/celda
#define BAT_NOMBRE "LiFePO4 16 celdas (PROVISIONAL)"

#else
#error "QUIMICA_BATERIA debe ser BAT_PLOMO_ACIDO o BAT_LIFEPO4"
#endif

// Retardos (iguales para las dos químicas)
constexpr uint16_t BAT_RETARDO_CORTE_MS      = 10000;  // 10 s abajo del corte
constexpr uint16_t BAT_RETARDO_EMERGENCIA_MS = 500;    // 0.5 s abajo de emergencia
constexpr uint16_t BAT_RETARDO_RECONEXION_MS = 30000;  // 30 s arriba de reconexión
constexpr uint16_t BAT_RETARDO_ALTO_MS       = 2000;   // 2 s arriba del corte alto

// Revisiones al compilar: el orden de los límites no se puede romper.
static_assert(BAT_EMERGENCIA_DV < BAT_CORTE_DV && BAT_CORTE_DV < BAT_AVISO_DV &&
              BAT_AVISO_DV < BAT_RECONEXION_DV && BAT_RECONEXION_DV < BAT_ALTO_RECONEXION_DV &&
              BAT_ALTO_RECONEXION_DV < BAT_ALTO_CORTE_DV, "limites de bateria fuera de orden");
static_assert(BAT_ALTO_CORTE_DV < 714, "el corte alto debe quedar abajo del FUERA DE RANGO (71.4 V)");
