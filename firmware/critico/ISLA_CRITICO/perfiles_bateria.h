/*
 * =============================================================================
 *  perfiles_bateria.h  -  Isla del Sol: tecnologías de batería y sus límites
 *  ESTE ARCHIVO DEBE SER IDÉNTICO EN LOS DOS SKETCHES (CRÍTICO y AUX).
 * -----------------------------------------------------------------------------
 *  La tecnología se escoge en el SETUP del panel (AUX). El AUX sólo la PROPONE
 *  por el enlace I2C; el CRÍTICO la revisa, la guarda en su EEPROM y es el que
 *  protege con ella (aunque el enlace se caiga). El AUX usa esta tabla sólo
 *  para mostrar los voltajes antes de confirmar.
 *
 *  Todos los límites en DÉCIMAS DE VOLT del paquete, medidos CON CARGA.
 *    aviso      : avisar en el panel; el inversor sigue
 *    corte      : abajo 10 s seguidos -> apagar (deja pasar el arranque de un motor)
 *    emergencia : abajo 0.5 s -> apagar (batería agotada o cable flojo)
 *    reconexion : después de un corte bajo, volver sólo arriba de esto 30 s.
 *                 Muy arriba del corte A PROPÓSITO: el rebote de la batería al
 *                 quitarle la carga no debe alcanzarlo (si no, prende y apaga
 *                 sin parar)
 *    alto       : arriba 2 s -> apagar (cargador descompuesto, batería equivocada)
 *    altoVuelta : después de un corte alto, volver abajo de esto 30 s
 *
 *  FUENTES
 *   Plomo-ácido (24 celdas, por celda a 25 °C):
 *    - Trojan, Flooded Battery Quick Start Guide: reposo 2.122 V = 100 %,
 *      2.083 V = 80 %; "no descargar más de 80 %"; ecualización 2.70 V.
 *    - Victron, inversores VE.Direct: corte 1.55 V/celda, alarma 1.82 V/celda.
 *    - Isla del Sol, decisiones_criticas.md: plomo 42–64 V; XL7015 80 V.
 *   LiFePO4 (15 o 16 celdas, por celda):
 *    - Victron, Lithium Battery Smart y smallBMS: pre-alarma 3.1 V,
 *      desconexión 2.8 V.
 *    - EVE (LF280K, LF105): carga máxima 3.65 V; reposo 3.40 V = 100 %,
 *      3.20 V = 20 %, 3.00 V = 10 %.
 *    - El inversor ve el PROMEDIO; el BMS ve cada celda: el corte del inversor
 *      va 0.1 V/celda arriba del BMS para cortar PRIMERO.
 *    - Revisado con una batería 16S real que se llena a 56.0 V (3.50 V/celda).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

enum PerfilBateria : uint8_t { PERFIL_PLOMO = 0, PERFIL_LFP15 = 1, PERFIL_LFP16 = 2, PERFILES = 3 };

struct LimitesBateria {
  uint16_t aviso, corte, emergencia, reconexion, alto, altoVuelta;   // décimas de V
};

// mV por celda × celdas / 100 = décimas de V del paquete
constexpr uint16_t bat_dv(uint16_t mvCelda, uint8_t celdas) { return (uint32_t)mvCelda * celdas / 100; }

constexpr LimitesBateria LIMITES_BATERIA[PERFILES] = {
  // Plomo-ácido 24 celdas: 1.85 / 1.75 / 1.55 / 2.083 / 2.75 / 2.67 V por celda
  { bat_dv(1850, 24), bat_dv(1750, 24), bat_dv(1550, 24), bat_dv(2083, 24), bat_dv(2750, 24), bat_dv(2667, 24) },
  // LiFePO4 15S: 3.10 / 2.90 / 2.80 / 3.20 / 3.65 / 3.45 V por celda
  { bat_dv(3100, 15), bat_dv(2900, 15), bat_dv(2800, 15), bat_dv(3200, 15), bat_dv(3650, 15), bat_dv(3450, 15) },
  // LiFePO4 16S: igual por celda
  { bat_dv(3100, 16), bat_dv(2900, 16), bat_dv(2800, 16), bat_dv(3200, 16), bat_dv(3650, 16), bat_dv(3450, 16) },
};
//  Plomo : aviso 44.4 | corte 42.0 | emergencia 37.2 | reconexión 49.9 | alto 66.0 | vuelve 64.0
//  LFP15 : aviso 46.5 | corte 43.5 | emergencia 42.0 | reconexión 48.0 | alto 54.7 | vuelve 51.7
//  LFP16 : aviso 49.6 | corte 46.4 | emergencia 44.8 | reconexión 51.2 | alto 58.4 | vuelve 55.2

// Nombre corto (5 letras, cabe en el LCD).
inline const char *perfil_nombre(uint8_t p) {
  return p == PERFIL_PLOMO ? "PLOMO" : p == PERFIL_LFP15 ? "LFP15" : p == PERFIL_LFP16 ? "LFP16" : "?????";
}

// Revisiones al compilar: el orden de los límites no se puede romper, y el
// alto debe quedar abajo del FUERA DE RANGO del divisor (71.4 V).
constexpr bool limites_en_orden(const LimitesBateria &l) {
  return l.emergencia < l.corte && l.corte < l.aviso && l.aviso < l.reconexion &&
         l.reconexion < l.altoVuelta && l.altoVuelta < l.alto && l.alto < 714;
}
static_assert(limites_en_orden(LIMITES_BATERIA[0]) && limites_en_orden(LIMITES_BATERIA[1]) &&
              limites_en_orden(LIMITES_BATERIA[2]), "limites de bateria fuera de orden");
