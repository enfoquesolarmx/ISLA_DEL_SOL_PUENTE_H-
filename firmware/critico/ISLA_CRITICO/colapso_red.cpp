/*
 * =============================================================================
 *  colapso_red.cpp  -  la red se cayo, detectado por amplitud
 *  Ver colapso_red.h: el porque, la calibracion y de donde salen los numeros.
 * =============================================================================
 */

#include "colapso_red.h"
#include "comparador_red.h"

/*  Nada de esto lo toca una ISR: vive en el lazo, dentro del drenado del
 *  anillo. Por eso no lleva `volatile` ni ATOMIC_BLOCK (DC-026).
 */
static uint16_t biasX64 = 511u << COLAPSO_BIAS_SHIFT;
static uint8_t  marca   = 0;
static bool     colapso = false;
static bool     sembrado = false;

void colapso_preparar(void)
{
  biasX64  = 511u << COLAPSO_BIAS_SHIFT;
  marca    = 0;
  colapso  = false;
  sembrado = false;
}

void colapso_meter(uint16_t cuenta)
{
  /*  La primera muestra siembra el bias, para no arrancar con 64 muestras de
   *  transitorio declarando lo que no es.
   */
  if (!sembrado) {
    biasX64  = (uint16_t)(cuenta << COLAPSO_BIAS_SHIFT);
    sembrado = true;
    return;
  }

  const int16_t bias = (int16_t)(biasX64 >> COLAPSO_BIAS_SHIFT);
  int16_t       dif  = (int16_t)cuenta - bias;
  if (dif < 0) dif = (int16_t)-dif;

  if (dif > COLAPSO_BANDA) {
    /*  Hay CA aqui. La marca baja, y el bias sigue a la senal: el promedio
     *  de una senoidal completa ES el bias.
     */
    if (marca) marca--;
  } else {
    if (marca < COLAPSO_TECHO) marca++;
  }

  /*  El bias se actualiza SIEMPRE, con o sin red. Es un promedio corrido:
   *  biasX64 += (cuenta - bias)  equivale a  bias += (cuenta - bias)/64.
   *
   *  ⚠️ TODO EN SIN SIGNO, Y ES A PROPOSITO. La version anterior era
   *
   *      biasX64 = (uint16_t)((int16_t)biasX64 + ((int16_t)cuenta - bias));
   *
   *  y en el AVR `int` son 16 BITS: 32704 + 318 = 33022 se sale del rango con
   *  signo, y el desbordamiento con signo es comportamiento indefinido. En el
   *  escritorio `int` son 32 bits, asi que la prueba NO lo veia -- la misma
   *  clase de trampa que el vuelco de `reloj_4us()` y que el doble buffer del
   *  dither. `biasX64` llega como mucho a 1023 << 6 = 65 472, que SI cabe en
   *  uint16_t, y en sin signo no hay comportamiento indefinido.
   */
  const int16_t paso = (int16_t)cuenta - bias;        /* -1023 .. +1023 */
  if (paso >= 0) {
    const uint16_t s = (uint16_t)paso;
    biasX64 = (biasX64 > (uint16_t)(COLAPSO_BIAS_TOPE - s))
                ? COLAPSO_BIAS_TOPE : (uint16_t)(biasX64 + s);
  } else {
    const uint16_t s = (uint16_t)(-paso);
    biasX64 = (biasX64 < s) ? 0u : (uint16_t)(biasX64 - s);
  }

  /*  Histeresis por construccion: sube en SUBE, baja solo al llegar a cero.
   *  Entre 1 y 7 la marca no cambia el veredicto, en ningun sentido.
   */
  if      (marca >= COLAPSO_SUBE) colapso = true;
  else if (marca == 0)            colapso = false;
}

bool     colapso_hay(void)   { return colapso; }
uint8_t  colapso_marca(void) { return marca; }
uint16_t colapso_bias(void)  { return (uint16_t)(biasX64 >> COLAPSO_BIAS_SHIFT); }

bool red_presente(void)
{
  return !colapso && cmp_red_presente();
}
