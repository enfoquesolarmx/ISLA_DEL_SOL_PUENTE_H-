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
 *    QUIMICA_BATERIA = BAT_LIFEPO4      ->  16 o 17 celdas de 3.2 V (CELDAS_LIFEPO4):
 *                                          51.2 V o 54.4 V nominal
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
// ---------- LiFePO4, 16 o 17 celdas en serie ----------
#define CELDAS_LIFEPO4 16      // 16 (51.2 V nominal) o 17 (54.4 V nominal)
static_assert(CELDAS_LIFEPO4 == 16 || CELDAS_LIFEPO4 == 17, "LiFePO4: 16 o 17 celdas");
// Fuentes (por celda):
//  - Victron, Lithium Battery Smart 12.8 V (hoja de datos) y smallBMS:
//    pre-alarma < 3.1 V, desconexión de carga < 2.8 V, paro de carga > 3.75 V.
//  - Hojas de celda EVE (LF280K / LF105): carga hasta 3.65 V, descarga hasta 2.5 V.
// El inversor ve el PROMEDIO del paquete; el BMS ve cada celda. Una celda
// desbalanceada llega a 2.8 V antes que el promedio, por eso el corte del
// inversor va 0.1 V arriba del corte del BMS: el inversor corta PRIMERO.
// Antes de usarlo en campo, revisar contra la hoja de TU batería y TU BMS.
constexpr uint16_t BAT_MV_AVISO      = 3100;   // pre-alarma de Victron
constexpr uint16_t BAT_MV_CORTE      = 2900;   // 0.1 V arriba del corte del BMS
constexpr uint16_t BAT_MV_EMERGENCIA = 2800;   // corte de carga del BMS (Victron)
constexpr uint16_t BAT_MV_RECONEXION = 3200;   // nominal: ya no es rebote
constexpr uint16_t BAT_MV_ALTO       = 3750;   // paro de carga de Victron (> 3.65 de EVE)
constexpr uint16_t BAT_MV_ALTO_VUELTA = 3600;
// mV por celda × celdas / 100 = décimas de V del paquete
constexpr uint16_t BAT_AVISO_DV        = (uint32_t)BAT_MV_AVISO      * CELDAS_LIFEPO4 / 100;
constexpr uint16_t BAT_CORTE_DV        = (uint32_t)BAT_MV_CORTE      * CELDAS_LIFEPO4 / 100;
constexpr uint16_t BAT_EMERGENCIA_DV   = (uint32_t)BAT_MV_EMERGENCIA * CELDAS_LIFEPO4 / 100;
constexpr uint16_t BAT_RECONEXION_DV   = (uint32_t)BAT_MV_RECONEXION * CELDAS_LIFEPO4 / 100;
constexpr uint16_t BAT_ALTO_CORTE_DV   = (uint32_t)BAT_MV_ALTO       * CELDAS_LIFEPO4 / 100;
constexpr uint16_t BAT_ALTO_RECONEXION_DV = (uint32_t)BAT_MV_ALTO_VUELTA * CELDAS_LIFEPO4 / 100;
//   16S: aviso 49.6 | corte 46.4 | emergencia 44.8 | reconexión 51.2 | alto 60.0 | vuelve 57.6
//   17S: aviso 52.7 | corte 49.3 | emergencia 47.6 | reconexión 54.4 | alto 63.7 | vuelve 61.2
// ⚠ HARDWARE con 17S: cargada llega a 62.05 V (17 × 3.65). El TVS D1 SMBJ58A
//   (58 V de trabajo) conduciría: con 17S va el SMBJ64A, igual que con plomo
//   en ecualización (hardware/control §2, DC-003).
#if CELDAS_LIFEPO4 == 16
#define BAT_NOMBRE "LiFePO4 16S"
#else
#define BAT_NOMBRE "LiFePO4 17S (D1 = SMBJ64A)"
#endif

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
