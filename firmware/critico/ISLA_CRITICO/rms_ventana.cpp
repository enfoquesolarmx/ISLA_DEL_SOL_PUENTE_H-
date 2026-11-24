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

// La ventana anterior de cada canal, para formar el par de un ciclo completo.
static RmsAcum rms_prev[2];

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
    for (uint8_t i = 0; i < 2; i++) {
      rms_prev[i].sumaCuad = 0; rms_prev[i].suma = 0; rms_prev[i].n = 0;
    }
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

  /*  ⚠️ AQUÍ ESTABA EL ERROR QUE EL BANCO DESTAPÓ EL 24 DE NOVIEMBRE
   *
   *  La versión anterior calculaba `RMS² = ⟨x²⟩ − ⟨x⟩²` sobre **una** ventana
   *  de medio ciclo. Eso no le quita el bias: le quita **la media rectificada
   *  del semiciclo**, que no es lo mismo. Para medio seno de amplitud A:
   *
   *      media del semiciclo        = bias + 0.6366·A      ← no es el bias
   *      RMS sobre su propia media  = 0.3078·A
   *      RMS verdadero de CA        = 0.7071·A      → **2.30 veces más**
   *
   *  Y el banco lo confirmó sin que nadie lo buscara: el `CD` reportado
   *  alternaba 277 y 744 cuentas, simétrico alrededor de 510.5 — el bias
   *  exacto. De ahí sale A = 366.8 cuentas, el código reportaba 109.7 y la
   *  predicción de la fórmula mala era 112.9. Cuadraba con el error, no con
   *  la realidad.
   *
   *  EL ARREGLO: SUMAR DOS VENTANAS CONSECUTIVAS = UN CICLO COMPLETO.
   *
   *  Sobre un ciclo entero el término rectificado se cancela, así que ⟨x⟩ SÍ
   *  es el bias y la fórmula vuelve a ser correcta. Y de paso arregla la
   *  dispersión: el canal de la red recibe 12.50 muestras por semiciclo
   *  —nunca entero, alterna 12 y 13— pero **12 + 13 = 25 = un ciclo exacto**.
   *  En el inversor, 25 + 25 = 50.
   *
   *  La latencia no se pierde: el par es deslizante, así que sigue habiendo
   *  un valor nuevo cada medio ciclo (8.3 ms), que es lo que DC-023 pedía.
   */
  const uint8_t  nPrev = rms_prev[i].n;      // ⚠️ leer ANTES de sobreescribir
  const uint32_t scTot = sc + rms_prev[i].sumaCuad;
  const uint32_t sTot  = (uint32_t)s + rms_prev[i].suma;
  const uint16_t nTot  = (uint16_t)cuantas + nPrev;

  rms_prev[i].sumaCuad = sc;        // esta ventana es la "anterior" de la próxima
  rms_prev[i].suma     = s;
  rms_prev[i].n        = cuantas;

  // En la primerísima ventana no hay par todavía: se guarda y se espera.
  if (nPrev == 0) return false;

  /*  La forma escalada, por la razón numérica de siempre: dividir primero
   *  trunca ⟨x⟩ hasta en una cuenta y el error se multiplica por 2⟨x⟩ ≈ 1 022,
   *  lo que son 0.4 % en el RMS = medio volt a 127 V, con un criterio de
   *  ±0.3 V.
   *
   *      varianza = (N·Σx² − (Σx)²) / N²
   *
   *  En 64 bits: con 50 muestras, N·Σx² llega a 2.6e9 y (Σx)² a 2.6e9. Las
   *  dos cabrían en 32 bits por los pelos; en 64 no hay que pensarlo. Corre
   *  120 veces por segundo en el lazo, no en una ISR.
   */
  const uint64_t numerador = (uint64_t)nTot * scTot - (uint64_t)sTot * sTot;
  const uint32_t n2        = (uint32_t)nTot * nTot;
  const uint32_t varianza  = (uint32_t)(numerador / n2);

  /*  Ahora sí: sobre un ciclo completo, ⟨x⟩ es el bias. Debe rondar 511 y
   *  —esto es lo nuevo— **debe quedarse quieto**. Si se mueve reporte a
   *  reporte, el par no está cubriendo un ciclo entero y hay que ir a ver por
   *  qué.
   */
  const uint16_t media = (uint16_t)(sTot / nTot);

  if (rmsCuentas) *rmsCuentas = rms_isqrt32(varianza);
  if (cd)         *cd = media;
  if (n)          *n  = (uint8_t)nTot;
  return true;
}
