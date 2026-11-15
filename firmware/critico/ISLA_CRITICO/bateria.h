/*
 * =============================================================================
 *  bateria.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Voltaje de batería en A3 (DC_FB_A3)
 * -----------------------------------------------------------------------------
 *  HARDWARE (hardware/control §10, DC-007)
 *
 *    BAT+ ──[R1 68 kΩ]──[R2 68 kΩ]──┬──[R3 1 kΩ]──┬── A3
 *                                   │             └── C1 1 µF a GND
 *                                  [R4 10 kΩ]
 *                                   │
 *                                  GND
 *
 *    Ganancia = 10 / (68 + 68 + 10) = 10 / 146 = 0.0685
 *      48.0 V -> 3.29 V -> ~673 cuentas
 *      58.4 V -> 4.00 V -> ~818 cuentas
 *      73.0 V -> 5.00 V -> 1023 cuentas (tope del ADC)
 *    (R3 no cuenta en la ganancia: el ADC casi no toma corriente.)
 *
 *  CONVERSIÓN SIN FLOAT (como en todo el CRÍTICO)
 *    El tope del ADC (1023) equivale a 5 V × 146 / 10 = 73.0 V = 730 décimas.
 *      décimas de V = cuentas × 730 / 1023   (redondeado)
 *    Una cuenta = 0.071 V. Suficiente para el LCD y para el bajo voltaje.
 *
 *  EXACTITUD: la referencia del ADC es el 5 V de la tarjeta (AVCC). Si el
 *  7805 da 4.9 V, la lectura sale 2 % baja. Eso lo corregirá la calibración
 *  (constante K en EEPROM, pendiente); por ahora es la lectura nominal.
 *
 *  FILTRO: promedio de 8 lecturas cada 50 ms. C1 ya filtra el rizo de 120 Hz
 *  (τ ≈ 10 ms); el promedio quita el ruido de conmutación de la SPWM.
 *
 *  LÍMITES DE CORTE: perfiles_bateria.h (tecnologías y voltajes, con fuentes).
 *  La tecnología activa se guarda en la EEPROM (se escoge en el SETUP del
 *  panel); sin EEPROM válida se usa PERFIL_FABRICA (config_bateria.h).
 *  bateria_estado() dice si el inversor puede trabajar; el .ino detiene o
 *  rearranca la SPWM según eso.
 *
 *  FALLA SEGURA: si R4 se abre, A3 sube a ~5 V: la lectura sale ≥ 71.4 V,
 *  muy por arriba de cualquier batería de 48 V -> FUERA DE RANGO: se corta y
 *  NO se reconecta sola (hay que revisar la tarjeta y reiniciar).
 *
 *  RECURSOS: A3 con analogRead() (provisional, como P9: cuando exista el
 *  ADC disparado por la portadora, la lectura se moverá ahí).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "perfiles_bateria.h"

constexpr uint16_t BAT_DV_TOPE          = 730;   // 1023 cuentas = 73.0 V
constexpr uint16_t BAT_CUENTAS_R4_ABIERTA = 1000; // ≥ 71.4 V: R4 abierta o batería imposible
constexpr uint8_t  BAT_LECTURAS         = 8;
constexpr uint16_t BAT_PERIODO_MS       = 50;

// Cuentas del ADC -> décimas de volt de batería (función pura).
uint16_t bateria_cuentas_a_dv(uint16_t cuentas);

// Llamar seguido desde loop(); cada BAT_PERIODO_MS promedia BAT_LECTURAS.
void bateria_atender();

// Lee ya (sin esperar el periodo). Llamar en setup() antes de arrancar.
void bateria_leer_ahora();

class __FlashStringHelper;
enum EstadoBateria : uint8_t {
  BAT_NORMAL,         // trabajar
  BAT_AVISO,          // trabajar, avisar en el panel
  BAT_CORTE_BAJO,     // NO trabajar: espera la reconexión
  BAT_CORTE_ALTO,     // NO trabajar: espera bajar del alto
  BAT_FUERA_DE_RANGO  // NO trabajar: falla del divisor, sólo con reinicio
};
EstadoBateria bateria_estado();
bool bateria_permite_trabajar();   // NORMAL o AVISO
const __FlashStringHelper *bateria_texto(EstadoBateria e);

// Tecnología activa (PerfilBateria) y cambio pedido desde el panel.
uint8_t bateria_perfil();
const LimitesBateria &bateria_limites();
// Rechazos al pedir un cambio de tecnología.
enum RechazoPerfil : uint8_t { PERFIL_OK, PERFIL_INVALIDO, PERFIL_NO_CABE };
// Cambia la tecnología, la guarda en la EEPROM y reevalúa desde cero.
// Rechaza si el voltaje medido no cabe entre la emergencia y el alto de la
// tecnología nueva (batería equivocada). La SPWM la detiene quien llama.
RechazoPerfil bateria_cambiar_perfil(uint8_t perfil);
// Carga la tecnología de la EEPROM (llamar en setup() antes de leer).
void bateria_iniciar();

uint16_t bateria_dv();         // último voltaje en décimas de V (0 hasta la 1a lectura)
uint16_t bateria_cuentas();    // último promedio crudo
bool     bateria_fuera_de_rango();
