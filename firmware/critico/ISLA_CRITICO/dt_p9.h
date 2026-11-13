/*
 * =============================================================================
 *  dt_p9.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Lectura de P9: qué tiempo muerto pide cada gate driver
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Cada módulo gate driver declara su tiempo muerto con su pin P9:
 *      P9 a GND  -> 500 ns  (IRS2186, UCC21520DW)
 *      P9 abierto -> 1 µs   (TLP250H, IR2110 trinchera, o cable suelto)
 *    Las dos ramas se leen en UN solo pin analógico (DC-011), con resistencias
 *    distintas para saber cuál rama es cuál.
 *
 *  HARDWARE AL QUE CORRESPONDE (hardware/control, sección 13)
 *    A7 (solo analógico) <- R12 10 kΩ a 5 V      (pull-up)
 *                        <- R13 10 kΩ <- P9_A    (rama A, J1-5)
 *                        <- R14 20 kΩ <- P9_B    (rama B, J3-5)
 *                        -- C18 100 nF a GND     (filtro)
 *
 *      P9_A      P9_B      A7       ADC (10 bits, nominal)   tiempo muerto
 *      GND       GND       2.00 V   409                      500 ns
 *      GND       abierto   2.50 V   512                      1 µs (mixto)
 *      abierto   GND       3.33 V   682                      1 µs (mixto)
 *      abierto   abierto   5.00 V   1023                     1 µs
 *
 *    La lectura es RATIOMÉTRICA: R12 y la referencia del ADC salen del mismo
 *    5 V, así que si el 5 V varía, las cuentas no cambian.
 *
 *  REGLAS (docs/identificacion_dt_pin9.md)
 *    - 500 ns SOLO si las DOS ramas están a GND en TODAS las lecturas.
 *    - Cualquier otra cosa (mixto, abierto, lectura inestable) -> 1 µs.
 *    - A7 por debajo de ~1.5 V no es un estado válido (A7 en corto a GND,
 *      R12 abierta): FALLA, no se arranca.
 *    - En operación, si P9 cambia (módulo flojo) -> se apaga la SPWM.
 *
 *  RECURSOS: A7 con analogRead() (provisional: cuando exista el módulo de ADC
 *  disparado por la portadora, la vigilancia se moverá ahí).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// ======================== UMBRALES (cuentas del ADC de 10 bits) ==============
// Puestos a la mitad entre las bandas que dan resistencias de ±5 %:
//   ambos a GND      385–434        |  umbral 460  (2.25 V)
//   A a GND          486–537        |  umbral 598  (2.92 V)
//   B a GND          659–704        |  umbral 863  (4.22 V)
//   ambos abiertos   1023           |
//   debajo de 300 (1.47 V): fuera de rango = falla de hardware
// Verificado con todas las combinaciones de ±5 % en pruebas/prueba_p9.cpp.
constexpr uint16_t P9_UMBRAL_FALLA     = 300;
constexpr uint16_t P9_UMBRAL_AMBOS_GND = 460;
constexpr uint16_t P9_UMBRAL_A_GND     = 598;
constexpr uint16_t P9_UMBRAL_B_GND     = 863;

// Lecturas al arrancar (todas deben dar el mismo estado) y su separación.
constexpr uint8_t  P9_LECTURAS_ARRANQUE = 16;
constexpr uint16_t P9_ESPERA_INICIAL_MS = 10;   // C18 se carga en < 1 ms (τ ≤ 100 nF × 10 kΩ)
constexpr uint8_t  P9_SEPARACION_MS     = 1;

// Vigilancia en operación: lecturas seguidas distintas para declarar cambio
// (evita disparos falsos por ruido).
constexpr uint8_t  P9_LECTURAS_CAMBIO   = 3;
constexpr uint16_t P9_PERIODO_VIGILANCIA_MS = 50;

// ======================== ESTADOS ============================================
enum EstadoP9 : uint8_t {
  P9_AMBOS_GND,          // 500 ns
  P9_A_GND_B_ABIERTO,    // mixto -> 1 µs
  P9_A_ABIERTO_B_GND,    // mixto -> 1 µs
  P9_AMBOS_ABIERTOS,     // 1 µs
  P9_FUERA_DE_RANGO,     // falla de hardware: no arrancar
  P9_INESTABLE           // las lecturas de arranque no coincidieron -> 1 µs
};

// Clasifica una lectura del ADC (función pura, sin hardware).
EstadoP9 p9_clasificar(uint16_t adc);

// Tiempo muerto en cuentas que corresponde a un estado
// (DT_MODERNO_CUENTAS sólo para P9_AMBOS_GND; DT_LEGACY_CUENTAS para todo lo demás).
uint8_t p9_dt_cuentas(EstadoP9 e);

// true si con ese estado se puede arrancar (todo menos P9_FUERA_DE_RANGO).
bool p9_permite_arrancar(EstadoP9 e);

// Texto corto para el reporte (en flash, para Serial.print).
class __FlashStringHelper;
const __FlashStringHelper *p9_texto(EstadoP9 e);

// Al encender, con la SPWM detenida: espera, toma P9_LECTURAS_ARRANQUE lecturas
// y devuelve el estado (P9_INESTABLE si no todas coinciden). Guarda el estado
// para la vigilancia y entrega la última lectura en *adc.
EstadoP9 p9_detectar(uint16_t *adc);

// Llamar seguido desde loop(). Cada P9_PERIODO_VIGILANCIA_MS lee A7; devuelve
// true cuando P9_LECTURAS_CAMBIO lecturas seguidas dan un estado distinto al
// detectado al arrancar (módulo flojo o cambiado). Entrega lectura y estado.
bool p9_vigilar(uint16_t *adc, EstadoP9 *estado);
