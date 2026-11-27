/*
 * =============================================================================
 *  colapso_red.h  -  Isla del Sol, micro CRITICO
 *  La red se cayo: detectado por AMPLITUD, no por cruces
 * -----------------------------------------------------------------------------
 *  POR QUE EXISTE. `cmp_red_presente()` cuenta cruces, y para declarar que no
 *  hay red tiene que esperar a que dejen de llegar: CMP_SIN_RED_MS = 50 ms,
 *  medidos en banco el 27 de noviembre de 2026 (50.0 y 50.2 ms, con el
 *  analizador y con el serial a la vez).
 *
 *      Una fuente ATX de PC aguanta 16 a 20 ms.
 *
 *  Con 50 ms de deteccion mas 5 a 15 del rele vamos al triple del
 *  presupuesto ANTES de que el rele empiece a moverse. La PC se apaga.
 *
 *  LA IDEA ES DEL USUARIO, y la saco de su propio reporte: cuando la red se
 *  va, la DISPERSION del RMS se dispara. Es cierto, y con un margen enorme:
 *
 *      sana    sigma 0.0 a 2.6 cuentas   min/max 221/227, 225/225
 *      corte   sigma 82 a 106 cuentas    min/max   0/225,   0/233
 *
 *  CALIBRACION -- de donde se toma. El `sigma` del reporte es una estadistica
 *  de UN SEGUNDO (dispersion de ~105 ventanas acumuladas por `est_rms`), asi
 *  que como esta impreso llega 20 veces mas lento que los 50 ms que queremos
 *  bajar. Y la ventana de RMS tampoco sirve: la cierra el cruce por cero, y
 *  sin cruces cae al tope de seguridad de 60 muestras -- 80 ms por par, PEOR
 *  que el criterio que queremos reemplazar.
 *
 *  Lo que hace grande al sigma si esta disponible rapido: **las muestras
 *  crudas**. Y ya pasan todas por el lazo, por el anillo de `rms_atender()`.
 *
 *  COMO. Una marca que sube cuando la muestra cae DENTRO de la banda muerta
 *  alrededor del bias, y baja cuando sale. Es la misma receta del voto de
 *  ACO en comparador_red.cpp: verdad rapida arriba, memoria corta abajo.
 *
 *      |muestra - bias| <= BANDA   ->  marca++   (techo TECHO)
 *      |muestra - bias| >  BANDA   ->  marca--   (piso 0)
 *      marca >= SUBE  ->  colapso
 *      marca == 0     ->  hay red
 *
 *  LOS NUMEROS, con los del banco. Red sana: RMS 225 cuentas, asi que el pico
 *  son 225 x raiz(2) = 318 cuentas sobre el bias (510-511). Con BANDA = 60
 *  cuentas (19 % del pico), una senoidal sana pasa por la banda solo
 *  2 x asin(60/318) = 21.8 grados de cada semiciclo: 1.0 ms, o sea **1 o 2
 *  muestras seguidas**. La ranura de la red se muestrea 1501.5 veces por
 *  segundo (1 de cada 4 del ADC), o sea una muestra cada 0.666 ms.
 *
 *      SUBE = 8 muestras  ->  5.33 ms para declarar el colapso
 *                             contra 1 o 2 que da una red sana: 4x de margen
 *      TECHO = 16         ->  aguanta 8 muestras de ruido sin soltarse,
 *                             y al volver la red tarda <= 16 muestras
 *                             (10.7 ms) en limpiarse: histeresis gratis
 *
 *  Sigue sirviendo con la red baja: a 60 V de pico la banda se cruza en
 *  3 muestras, todavia lejos de 8.
 *
 *  ⚠️ ESTO NO SUSTITUYE A DC-033. Los 105 V por ~2 s de aquella entrada son
 *  para una red que REGULA MAL, y ahi lo correcto es lento: primero se le
 *  baja al cargador, muchas veces se recupera sola. Esto es otra cosa: la
 *  red SE FUE. Son tres criterios, no dos:
 *
 *      hundimiento  105 V / 2 s     (DC-033)  lento A PROPOSITO
 *      colapso      < banda, 8 m.   (aqui)    5.33 ms
 *      sin cruces   50 ms           (cmp)     respaldo: hay voltaje
 *                                             pero la onda se descompuso
 *
 *  COSTO. CERO en las ISR. Corre en el lazo, dentro del drenado del anillo
 *  que `rms_atender()` ya hace. ~15 ciclos por muestra de red, 1501.5 por
 *  segundo: 0.14 % del CPU.
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*  Banda muerta alrededor del bias, en cuentas del ADC. Ver el calculo
 *  arriba: 60 de 318 de pico. No se toca sin rehacer ese calculo.
 */
#define COLAPSO_BANDA     60

#define COLAPSO_SUBE       8      /* marca que declara el colapso: 5.33 ms  */
#define COLAPSO_TECHO     16      /* inmunidad al ruido y histeresis        */

/*  Constante de tiempo del bias, en potencias de dos. 2^6 = 64 muestras =
 *  43 ms. La CA promedia cero en un ciclo, asi que esto converge al bias
 *  solo, sin pedirselo a nadie. El banco lo tiene clavado en 510-511.
 */
#define COLAPSO_BIAS_SHIFT 6

/*  Techo de `biasX64`: 1023 << 6 = 65 472, que cabe en uint16_t. El bias se
 *  satura ahi y en cero en vez de dar la vuelta. Ver la nota del .cpp sobre
 *  por que la cuenta va toda en SIN SIGNO.
 */
#define COLAPSO_BIAS_TOPE  ((uint16_t)(1023u << COLAPSO_BIAS_SHIFT))

/*  Una muestra cruda del canal de la red. La llama `rms_atender()` mientras
 *  drena el anillo, en el LAZO. Nunca desde una ISR.
 */
void colapso_meter(uint16_t cuenta);

void colapso_preparar(void);

bool    colapso_hay(void);      // true = la red se cayo por amplitud
uint8_t colapso_marca(void);    // 0..TECHO, para verlo en el reporte
uint16_t colapso_bias(void);    // el bias que esta siguiendo

/*  EL VEREDICTO COMPLETO: amplitud Y cruces.
 *
 *  Cada criterio tapa lo que el otro no ve. El colapso atrapa en 5.33 ms que
 *  la red se fue; los cruces atrapan que la onda se descompuso con el
 *  voltaje todavia puesto. Hace falta que los dos digan "hay red".
 */
bool red_presente(void);
