/*
 * =============================================================================
 *  watchdog_avr.cpp  -  Isla del Sol, micro AUX (A2)
 *  Implementación del watchdog interno. Ver watchdog_avr.h.
 * =============================================================================
 */

#include "watchdog_avr.h"

#include <avr/io.h>
#include <avr/wdt.h>

/*  El MCUSR del arranque, guardado antes de limpiarlo.
 *
 *  Vive en `.noinit` a propósito: se escribe en `.init3` y `.init4` —que corre
 *  después— borra `.bss` y copia `.data`. Una variable normal se perdería
 *  entre que se guarda y que alguien la lee.
 */
static uint8_t mcusr_arranque __attribute__((section(".noinit")));

/*  Esto corre ANTES de main(), en `.init3`.
 *
 *  `naked` es obligatorio: a las secciones `.initN` no se las llama, se cae
 *  dentro de ellas una tras otra. Si el compilador le pusiera un `ret`, el
 *  procesador saltaría a una dirección sacada de una pila que nadie llenó.
 *  Sin prólogo ni epílogo, el código simplemente sigue hacia `.init4`.
 *
 *  Para cuando llega aquí, `.init2` ya armó el puntero de pila y limpió r1,
 *  que es todo lo que necesita este pedazo.
 */
void wdt_salvavidas(void) __attribute__((naked, used, section(".init3")));
void wdt_salvavidas(void)
{
  mcusr_arranque = MCUSR;   // la causa, antes de borrarla
  MCUSR = 0;                // PRIMERO: con WDRF puesto, WDE no se puede bajar
  wdt_disable();            // y así el cargador viejo no nos deja en bucle
}

// ============================ FUNCIONES PÚBLICAS =============================

void wdt_arrancar(void)
{
  wdt_reset();              // se patea antes de prenderlo, no después
  wdt_enable(WDTO_1S);
}

void wdt_patear(void)
{
  wdt_reset();
}

uint8_t wdt_mcusr_arranque(void) { return mcusr_arranque; }
bool    wdt_fue_watchdog(void)   { return (mcusr_arranque & _BV(WDRF)) != 0; }
bool    wdt_fue_brownout(void)   { return (mcusr_arranque & _BV(BORF)) != 0; }

const char *wdt_texto_arranque(void)
{
  /*  El orden importa: un reinicio por watchdog suele traer también el bit de
   *  reset externo puesto en algunas tarjetas, así que el watchdog se revisa
   *  primero para no esconderlo detrás de una causa más inocente.
   */
  if (mcusr_arranque & _BV(WDRF)) return "POR WATCHDOG (se reinicio solo)";
  if (mcusr_arranque & _BV(BORF)) return "por caida de alimentacion";
  if (mcusr_arranque & _BV(EXTRF)) return "por reset externo o carga de codigo";
  if (mcusr_arranque & _BV(PORF)) return "normal (se acaba de energizar)";
  return "desconocida";
}
