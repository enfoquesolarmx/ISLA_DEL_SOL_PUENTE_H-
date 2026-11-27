/*
 * =============================================================================
 *  prueba_colapso.cpp  -  Isla del Sol, prueba de escritorio
 *  El detector de colapso de la red, contra una senoidal simulada
 * -----------------------------------------------------------------------------
 *  QUE COMPRUEBA, y por que cada cosa:
 *
 *   1. Con la red SANA la marca nunca llega a COLAPSO_SUBE. Si llegara,
 *      estariamos cortando la red sin razon, y eso es una transferencia
 *      espuria: la casa parpadea sin que se haya ido la luz.
 *   2. Al colapsar, cuantas muestras tarda en declararlo. Es el numero que
 *      decide si una PC sobrevive: su fuente ATX aguanta 16 a 20 ms.
 *   3. Al volver la red, cuantas muestras tarda en limpiarse, y que NO
 *      castanetee en el camino.
 *   4. Que siga sirviendo con la red BAJA (105 V y 60 V), porque la banda es
 *      fija en cuentas y el pico se encoge.
 *
 *  Las muestras imitan al banco: 25 por ciclo de linea (la ranura de la red
 *  se muestrea 1501.5 veces por segundo, 1 de cada 4 del ADC, y la linea va
 *  a 60.06 Hz), bias 511 cuentas y pico de 318 cuentas, que es el RMS de 225
 *  del reporte por raiz de dos.
 *
 *  Compilar:
 *    g++ -O2 -I ../ISLA_CRITICO -o pc prueba_colapso.cpp \
 *        ../ISLA_CRITICO/colapso_red.cpp
 *    (colapso_red.cpp llama a cmp_red_presente() solo desde red_presente(),
 *     que esta prueba no usa; se le da un doble aqui abajo.)
 * =============================================================================
 */

#include <cstdio>
#include <cmath>
#include <cstdlib>

#include "colapso_red.h"

/*  Doble del comparador: esta prueba mide el criterio de AMPLITUD solo. */
bool cmp_red_presente(void) { return true; }

#define BIAS        511
#define PICO_SANO   318          /* 225 cuentas de RMS x raiz(2) */
#define POR_CICLO    25          /* 1501.5 muestras/s / 60.06 Hz */
#define MS_MUESTRA  (1000.0 / 1501.5)

static int fallas = 0;

static void exige(bool cond, const char *que)
{
  if (!cond) { printf("   FALLA: %s\n", que); fallas++; }
}

/*  Mete `ciclos` ciclos de senoidal de `pico` cuentas. Devuelve la marca
 *  mas alta que se vio, y cuenta cuantas veces se declaro colapso.
 */
static uint8_t senoidal(int ciclos, int pico, int *colapsos, int fase = 0)
{
  uint8_t maxMarca = 0;
  for (int k = 0; k < ciclos * POR_CICLO; k++) {
    const double a = 2.0 * M_PI * (double)(k + fase) / (double)POR_CICLO;
    const int    c = BIAS + (int)lround((double)pico * sin(a));
    colapso_meter((uint16_t)(c < 0 ? 0 : (c > 1023 ? 1023 : c)));
    if (colapso_marca() > maxMarca) maxMarca = colapso_marca();
    if (colapsos && colapso_hay()) (*colapsos)++;
  }
  return maxMarca;
}

/*  Red muerta: el nodo se queda en el bias. Devuelve en cuantas muestras
 *  se declaro el colapso, o -1 si no se declaro en `tope`.
 */
static int muerta(int tope)
{
  for (int k = 1; k <= tope; k++) {
    colapso_meter(BIAS);
    if (colapso_hay()) return k;
  }
  return -1;
}

int main(void)
{
  printf("\n  DETECTOR DE COLAPSO DE LA RED - prueba de escritorio\n");
  printf("  banda=%d cuentas  sube=%d  techo=%d   muestra = %.3f ms\n\n",
         COLAPSO_BANDA, COLAPSO_SUBE, COLAPSO_TECHO, MS_MUESTRA);

  /* ---- 1. Red sana: la marca no debe acercarse al disparo --------------- */
  printf("  1. RED SANA (pico %d cuentas = 119 V)\n", PICO_SANO);
  {
    int colapsos = 0;
    colapso_preparar();
    const uint8_t m = senoidal(200, PICO_SANO, &colapsos);
    printf("     marca maxima = %u de %d        colapsos = %d\n",
           m, COLAPSO_SUBE, colapsos);
    exige(colapsos == 0, "declaro colapso con la red sana");
    exige(m < COLAPSO_SUBE, "la marca llego al disparo con la red sana");
    printf("     margen: %d veces\n", m ? COLAPSO_SUBE / m : 99);
  }

  /* ---- 2. Red baja: la banda pesa mas, pero debe seguir sirviendo ------- */
  printf("\n  2. RED BAJA\n");
  {
    const int picos[] = { 280, 160, 120 };   /* ~105 V, ~60 V, ~45 V */
    const char *eti[] = { "105 V", " 60 V", " 45 V" };
    for (int i = 0; i < 3; i++) {
      int colapsos = 0;
      colapso_preparar();
      const uint8_t m = senoidal(100, picos[i], &colapsos);
      printf("     pico %3d (%s): marca maxima = %u   colapsos = %d\n",
             picos[i], eti[i], m, colapsos);
      if (i < 2) exige(colapsos == 0, "corto una red baja pero viva");
    }
  }

  /* ---- 3. El colapso: cuanto tarda, desde varias fases ------------------ */
  printf("\n  3. COLAPSO (la red se va)\n");
  {
    int peor = 0, mejor = 999;
    for (int fase = 0; fase < POR_CICLO; fase++) {
      colapso_preparar();
      senoidal(40, PICO_SANO, nullptr, fase);
      const int n = muerta(100);
      exige(n > 0, "no declaro el colapso");
      if (n > peor)  peor  = n;
      if (n < mejor) mejor = n;
    }
    printf("     sobre las %d fases del ciclo:\n", POR_CICLO);
    printf("       mejor caso %d muestras = %.2f ms\n", mejor, mejor * MS_MUESTRA);
    printf("       PEOR  caso %d muestras = %.2f ms\n", peor,  peor  * MS_MUESTRA);
    printf("       (una fuente ATX aguanta 16 a 20 ms)\n");
    exige(peor * MS_MUESTRA < 16.0, "el peor caso no cabe en el aguante de una ATX");
  }

  /* ---- 4. El regreso: que se limpie y que no castanetee ----------------- */
  printf("\n  4. REGRESO DE LA RED\n");
  {
    colapso_preparar();
    senoidal(40, PICO_SANO, nullptr);
    muerta(100);
    exige(colapso_hay(), "deberia estar en colapso antes del regreso");

    int k = 0, cambios = 0; bool prev = true;
    for (; k < 200; k++) {
      const double a = 2.0 * M_PI * (double)k / (double)POR_CICLO;
      const int c = BIAS + (int)lround((double)PICO_SANO * sin(a));
      colapso_meter((uint16_t)c);
      if (colapso_hay() != prev) { cambios++; prev = colapso_hay(); }
      if (!colapso_hay()) break;
    }
    printf("     se limpio en %d muestras = %.2f ms   cambios = %d\n",
           k + 1, (k + 1) * MS_MUESTRA, cambios);
    exige(!colapso_hay(), "no se limpio al volver la red");
    exige(cambios == 1, "castaneteo al volver la red");

    /*  Y que se quede limpio. */
    int colapsos = 0;
    senoidal(100, PICO_SANO, &colapsos);
    exige(colapsos == 0, "volvio a declarar colapso con la red ya de vuelta");
  }

  /* ---- 5. Ruido sobre la red muerta: no debe soltarse ------------------- */
  printf("\n  5. RUIDO SOBRE LA RED MUERTA\n");
  {
    colapso_preparar();
    senoidal(40, PICO_SANO, nullptr);
    muerta(100);
    int sueltas = 0;
    for (int k = 0; k < 500; k++) {
      /*  Una espiga aislada cada 20 muestras, bien afuera de la banda. */
      const int c = (k % 20 == 0) ? BIAS + 200 : BIAS + (k % 5) - 2;
      colapso_meter((uint16_t)c);
      if (!colapso_hay()) sueltas++;
    }
    printf("     espigas aisladas: se solto %d veces de 500\n", sueltas);
    exige(sueltas == 0, "una espiga de ruido solto el colapso");
  }

  printf("\n  RESULTADO: %s\n\n", fallas ? "HAY FALLAS" : "todo correcto");
  return fallas ? 1 : 0;
}
