/*
 * =============================================================================
 *  botonera.h  -  Isla del Sol, AUX: 4 botones en UN pin analógico (A7)
 * -----------------------------------------------------------------------------
 *  HARDWARE (hardware/panel, sección 3): escalera de resistencias, 10 kΩ a 5 V.
 *
 *    botón  resistencia  voltaje  cuentas  función
 *    B1     2.4 kΩ       0.97 V   ~198     ATRÁS / ESC
 *    B2     6.8 kΩ       2.02 V   ~414     ARRIBA / −
 *    B3     15 kΩ        3.00 V   ~614     ABAJO / +
 *    B4     39 kΩ        3.98 V   ~815     OK (sostenido 3 s: SETUP)
 *    ninguno             5.00 V   1023     (también si el cable está suelto)
 *
 *  Umbrales a la mitad entre bandas: 102 / 307 / 512 / 716 / 921 cuentas.
 *  Debajo de 102 (A7 en corto a GND) = FALLA, no es un botón.
 *  Antirrebote: 3 lecturas iguales seguidas, cada 10 ms (~30 ms).
 *  Dos botones a la vez: gana el de número menor (resistencia menor).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

constexpr uint16_t BOT_UMBRAL_FALLA = 102;
constexpr uint16_t BOT_UMBRAL_B1    = 307;
constexpr uint16_t BOT_UMBRAL_B2    = 512;
constexpr uint16_t BOT_UMBRAL_B3    = 716;
constexpr uint16_t BOT_UMBRAL_B4    = 921;
constexpr uint8_t  BOT_LECTURAS_IGUALES = 3;
constexpr uint8_t  BOT_PERIODO_MS       = 10;
constexpr uint16_t BOT_SETUP_MS         = 3000;

enum Boton : uint8_t { BOT_NINGUNO, BOT_ATRAS, BOT_ARRIBA, BOT_ABAJO, BOT_OK, BOT_FALLA };

// Eventos que entrega botonera_evento() (uno por llamada).
enum EventoBoton : uint8_t {
  EV_NADA, EV_ATRAS, EV_ARRIBA, EV_ABAJO,
  EV_OK,      // B4 soltado antes de 3 s
  EV_SETUP,   // B4 sostenido 3 s (se da una sola vez, sin esperar a soltar)
  EV_FALLA    // A7 < 102: corto a GND en el cable del panel
};

Boton botonera_clasificar(uint16_t adc);   // función pura
EventoBoton botonera_evento();            // llamar seguido desde loop()
uint16_t botonera_adc();                  // última lectura cruda de A7
