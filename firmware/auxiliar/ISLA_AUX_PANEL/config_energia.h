/*
 * =============================================================================
 *  config_energia.h  -  Isla del Sol, AUX: perillas del medidor de energía
 * -----------------------------------------------------------------------------
 *  Todo lo que se elige aquí, se compila y se carga. Nada se configura desde
 *  el serie ni desde el menú.
 *
 *  El diseño y la razón de cada número están en docs/medicion_energia.md.
 *  Si cambia algo de aquí, cambie allá también.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// ======================== EL EQUIPO =========================================
// Potencia nominal del inversor. Es la referencia del porcentaje de carga.
constexpr uint16_t POTENCIA_NOMINAL_W = 1000;

// ======================== LA MÁXIMA SOSTENIDA ===============================
// Para registrarse, la potencia debe sostenerse este tiempo. La razón no es
// técnica sino de utilidad: un refrigerador arranca en 1800 W y trabaja en
// 150; si el arranque contara, el número no diría nada de cómo se consume.
// Los picos ya los vigila el hardware (DC-016), así que la pantalla no tiene
// que hacer de detector de picos.
constexpr uint16_t MAX_SOSTENIDA_MS = 2000;

// La foto del CRÍTICO llega cada 100 ms -> 20 lecturas.
constexpr uint16_t MS_ENTRE_FOTOS = 100;
constexpr uint8_t  MUESTRAS_BLOQUE = MAX_SOSTENIDA_MS / MS_ENTRE_FOTOS;
static_assert(MUESTRAS_BLOQUE >= 2 && MUESTRAS_BLOQUE <= 100,
  "El bloque de la máxima sostenida quedó en un tamaño absurdo");

// ======================== LAS VENTANAS DE TIEMPO ============================
constexpr uint8_t  CUBETAS_24H   = 24;          // una por hora
constexpr uint32_t SEGUNDOS_HORA = 3600UL;
constexpr uint32_t SEGUNDOS_DIA  = 86400UL;

// Sin reloj de calendario no hay de otra: el mes son 30 días y el año 365.
// Queda dicho para que nadie saque cuentas contra el calendario.
constexpr uint16_t DIAS_MES = 30;
constexpr uint16_t DIAS_ANIO = 365;

// ======================== EL HISTORIAL DIARIO ===============================
// DOS historiales: lo que se consumió (modo inversor) y lo que se cargó (modo
// cargador). Un día a 1000 W continuos son 24 000 Wh, así que 2 bytes por día
// (tope 65 535) sobran.
//
// POR QUÉ 180 Y NO 365. Dos anillos de un año en Wh completo son 1460 B y la
// EEPROM tiene 1024. La alternativa era un año con 1 byte por día en pasos de
// 100 Wh, pero eso topa en 25.5 kWh/día y el equipo puede entregar 24: seis
// por ciento de margen antes de que el número se achate. Medio año menos de
// historial se paga solo con no tener que preocuparse por eso.
constexpr uint16_t DIAS_HISTORIAL = 180;

// Promedios que se muestran en la pantalla.
constexpr uint8_t PROM_CORTO = 7;
constexpr uint8_t PROM_LARGO = 30;
static_assert(PROM_LARGO <= DIAS_HISTORIAL, "El promedio largo no cabe en el historial");

// ======================== LA EEPROM =========================================
// El perfil de batería del CRÍTICO vive en la dirección 0 de SU EEPROM; ésta
// es la del AUX, pero se deja el hueco de todos modos por si alguna vez se
// comparte el mapa.
constexpr uint16_t ENER_EE_CABECERA = 16;       // 18 B de cabecera
constexpr uint16_t ENER_EE_CONSUMO  = 40;       // 180 x 2 B = 360 B
constexpr uint16_t ENER_EE_CARGA    = ENER_EE_CONSUMO + DIAS_HISTORIAL * 2;
constexpr uint8_t  ENER_EE_FIRMA    = 'E';

// Comprobación de que todo cabe en los 1024 B del ATmega328P.
static_assert(ENER_EE_CARGA + DIAS_HISTORIAL * 2 <= 1024,
  "Los dos historiales no caben en la EEPROM: baje DIAS_HISTORIAL");

// ======================== EL VOLCADO POR SERIE ==============================
// El "archivo de consumo": se pide por el menú y sale en CSV por el serie.
// 180 renglones son ~2.5 KB: 0.2 s a 115200 baudios.
#define ENER_CSV_ENCABEZADO "dia,Wh_consumidos,Wh_cargados"
