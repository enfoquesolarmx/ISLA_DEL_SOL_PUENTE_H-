/*
 * =============================================================================
 *  watchdog_avr.h  -  Isla del Sol, micro AUX (A2)
 *  El perro guardián interno del ATmega328P
 * -----------------------------------------------------------------------------
 *  NO LLEVA UN SOLO COMPONENTE
 *
 *    El watchdog está adentro del ATmega328P, con su propio oscilador de
 *    128 kHz independiente del cristal. Se prende escribiendo dos registros.
 *    **No ocupa un pin, no ocupa una pista y no cambia el PCB.**
 *
 *    Y lo que hace falta para que su reinicio sea seguro ya está puesto en la
 *    tarjeta desde el principio: `R24`, el pull-down de 10 kΩ de EN_HW. Con
 *    cualquier micro en reset, EN_HW cae a 0 y el 74HC08 bloquea las cuatro
 *    salidas del puente (ver `corte_por_hardware.md`). No se puso para esto,
 *    pero lo cubre.
 *
 *  ⚠️ QUÉ NO ARREGLA — Y ESTO ES LO QUE MÁS IMPORTA ENTENDER
 *
 *    **El watchdog no destraba el bus I2C.** Si el TWI del AUX se queda
 *    agarrado a SDA, el lazo principal sigue corriendo —la máquina de estados
 *    del esclavo vive en la interrupción— así que el lazo sigue pateando al
 *    perro y el perro nunca vence.
 *
 *    Para eso está el rescate del bus: `Wire.end()` baja TWEN y deja los dos
 *    pines en alta impedancia, lo que suelta el bus sin reiniciar nada. Son
 *    dos seguros para dos fallas distintas:
 *
 *        TWI agarrado al bus, programa vivo   →  Wire.end() + Wire.begin()
 *        programa colgado, ya nadie corre     →  este watchdog
 *
 *  ⚠️ LA TRAMPA DEL CARGADOR DE ARRANQUE VIEJO
 *
 *    Tras un reinicio por watchdog el AVR **deja el watchdog prendido**:
 *    `WDRF` mantiene puesto `WDE`, y además el plazo queda en el más corto.
 *    Optiboot lo apaga al arrancar; `ATmegaBOOT` —el que traen muchos Nano
 *    clon, el que el IDE llama «Old Bootloader»— **no**. Resultado: se
 *    reinicia, el plazo vence antes de llegar a `setup()`, y la tarjeta queda
 *    en bucle de reinicio, inservible hasta reprogramarla con un programador
 *    externo.
 *
 *    La defensa va **antes de `main()`**, en la sección `.init3`, que corre
 *    después de que `.init2` arma la pila y limpia r1, y antes de que `.init4`
 *    copie las variables. Por eso el MCUSR guardado vive en `.noinit`: si
 *    estuviera en `.bss`, `.init4` lo borraría antes de que nadie lo lea.
 *
 *    `MCUSR` se limpia PRIMERO: mientras `WDRF` esté puesto, `WDE` no se
 *    puede bajar. Ese orden no es estilo, es la hoja de datos.
 *
 *  EL PLAZO
 *
 *    Un segundo. Tiene que ser más largo que el bloqueo legítimo más largo
 *    del lazo, y en el AUX ése no es el I2C: es **la escritura de EEPROM,
 *    ~3.4 ms por byte**. Guardar un perfil de varios bytes puede pasar de
 *    30 ms. Con 1 s sobra; con 15 ms te reinicias guardando un perfil.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>

/*  Prende el watchdog. Se llama al final de setup(), cuando ya está todo
 *  inicializado, y de ahí en adelante hay que patearlo antes de cada segundo.
 */
void wdt_arrancar(void);

/*  Patear al perro. Va al principio de loop(), una sola vez: ponerlo en
 *  varios lugares esconde justamente el cuelgue que se quiere detectar.
 */
void wdt_patear(void);

/*  Por qué arrancó el micro la última vez. Es el MCUSR de antes de limpiarlo,
 *  guardado en .init3.
 */
uint8_t  wdt_mcusr_arranque(void);
bool     wdt_fue_watchdog(void);        // ⚠️ se reinició solo: hay que decirlo
bool     wdt_fue_brownout(void);        // cayó la alimentación
const char *wdt_texto_arranque(void);   // para imprimir tal cual
