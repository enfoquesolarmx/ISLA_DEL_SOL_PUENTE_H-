/*
 * =============================================================================
 *  rms_ventana.cpp  -  Isla del Sol, micro CRÍTICO (A1)
 *  Implementación del RMS de medio ciclo. Ver rms_ventana.h.
 * =============================================================================
 */

#include "rms_ventana.h"

#include <util/atomic.h>

volatile RmsAcum  rms_banco[4];
volatile uint8_t  rms_vivo[2];
volatile uint8_t  rms_lista[2];
volatile uint16_t rms_desbordes;

void rms_cerrar_red(void)
{
  // Se llama desde la ISR del comparador: las interrupciones ya están
  // apagadas, así que no lleva protección.
  rms_cerrar(RMS_RED);
}

void rms_preparar(void)
{
  rms_reiniciar();
}

void rms_reiniciar(void)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for (uint8_t b = 0; b < 4; b++) {
      rms_banco[b].sumaCuad = 0;
      rms_banco[b].suma     = 0;
      rms_banco[b].n        = 0;
    }
    rms_vivo[0] = rms_vivo[1] = 0;
    rms_lista[0] = rms_lista[1] = 0;
    rms_desbordes = 0;
  }
}

uint16_t rms_desbordes_red(void)
{
  uint16_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { v = rms_desbordes; }
  return v;
}

/*  Raíz cuadrada entera por Newton, con una semilla por bits.
 *
 *  Converge en 5 o 6 vueltas para los valores que nos tocan (hasta ~1e6), y
 *  cada vuelta es una división de 32 bits: ~600 ciclos en total. Corre 120
 *  veces por segundo en el lazo = 0.45 % del CPU, contra ~1.4 kB de flash y
 *  más ciclos si arrastráramos sqrt() de punto flotante.
 */
uint16_t rms_isqrt32(uint32_t v)
{
  if (v == 0) return 0;

  // Semilla: 2^(ceil(bits/2)) deja el error inicial abajo del 50 %.
  uint32_t x = v, semilla = 1;
  while (x > 3) { x >>= 2; semilla <<= 1; }

  uint32_t r = semilla;
  for (uint8_t k = 0; k < 8; k++) {
    const uint32_t sig = (r + v / r) >> 1;
    if (sig == r || sig == r + 1) { r = sig; break; }
    r = sig;
  }
  while (r * r > v) r--;          // Newton puede quedar uno arriba
  return (uint16_t)r;
}

bool rms_recoger(uint8_t i, uint16_t *rmsCuentas, uint16_t *cd, uint8_t *n)
{
  if (i > RMS_RED) return false;

  uint32_t sc;
  uint16_t s;
  uint8_t  cuantas;

  /*  El banco que NO está vivo es el de la ventana cerrada, y la ISR no lo
   *  toca. Aun así la lectura va protegida: si una ventana cerrara justo en
   *  medio, el índice cambiaría debajo de nosotros y leeríamos mitad de una y
   *  mitad de la otra. Son ~20 ciclos con las interrupciones apagadas, en el
   *  lazo y 240 veces por segundo: 0.03 % del CPU (DC-026).
   */
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (!rms_lista[i]) { cuantas = 0; sc = 0; s = 0; }
    else {
      const uint8_t muerto = (uint8_t)((i << 1) | (rms_vivo[i] ^ 1));
      sc      = rms_banco[muerto].sumaCuad;
      s       = rms_banco[muerto].suma;
      cuantas = rms_banco[muerto].n;
      rms_lista[i] = 0;
    }
  }

  if (cuantas < 4) return false;   // ventana vacía o demasiado corta

  /*  RMS² = ⟨x²⟩ − ⟨x⟩², y de paso ⟨x⟩ es el CD de la ventana.
   *
   *  ⚠️ NO se calcula así de directo, y la razón es numérica. Dividiendo
   *  primero, `⟨x⟩ = Σx / n` se trunca hasta en una cuenta, y al elevarla al
   *  cuadrado ese error se multiplica por 2⟨x⟩ ≈ 1 022. Contra una varianza de
   *  ~122 500 (un RMS de 350 cuentas) son 0.83 % en la varianza y **0.4 % en
   *  el RMS**: medio volt a 127 V. El criterio de la etapa 4 es ±0.3 V, así
   *  que ese atajo no pasa la prueba.
   *
   *  La forma escalada no divide hasta el final y es exacta:
   *
   *      varianza = (n·Σx² − (Σx)²) / n²
   *
   *  El numerador se hace en 64 bits: con 60 muestras, n·Σx² llega a 3.8e9 y
   *  en 32 bits quedaría a un pelo del tope (4.29e9). Corre 120 veces por
   *  segundo en el lazo, no en una ISR, así que el costo da igual.
   */
  const uint64_t numerador = (uint64_t)cuantas * sc - (uint64_t)s * s;
  const uint32_t n2        = (uint32_t)cuantas * cuantas;
  const uint32_t varianza  = (uint32_t)(numerador / n2);

  const uint16_t media = (uint16_t)(s / cuantas);    // ⟨x⟩ = el CD, diagnóstico

  if (rmsCuentas) *rmsCuentas = rms_isqrt32(varianza);
  if (cd)         *cd = media;
  if (n)          *n  = cuantas;
  return true;
}
