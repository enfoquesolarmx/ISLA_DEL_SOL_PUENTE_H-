/*
 * =============================================================================
 *  config_bateria.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Tecnología de batería DE FÁBRICA y retardos
 * -----------------------------------------------------------------------------
 *  La tecnología se escoge en el SETUP del panel y queda guardada en la EEPROM
 *  del CRÍTICO. Este valor se usa sólo cuando la EEPROM está vacía o dañada
 *  (Nano nuevo). Los límites de cada tecnología están en perfiles_bateria.h.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "perfiles_bateria.h"

// ======================== DE FÁBRICA =========================================
constexpr uint8_t PERFIL_FABRICA = PERFIL_PLOMO;   // PERFIL_PLOMO, PERFIL_LFP15 o PERFIL_LFP16

// Retardos (iguales para todas las tecnologías)
constexpr uint16_t BAT_RETARDO_CORTE_MS      = 10000;  // 10 s abajo del corte
constexpr uint16_t BAT_RETARDO_EMERGENCIA_MS = 500;    // 0.5 s abajo de emergencia
constexpr uint16_t BAT_RETARDO_RECONEXION_MS = 30000;  // 30 s para volver
constexpr uint16_t BAT_RETARDO_ALTO_MS       = 2000;   // 2 s arriba del alto

// Dirección en la EEPROM: [firma 'B'][perfil][CRC-8]
constexpr uint16_t BAT_EEPROM_DIR = 0;
