/*
 * =============================================================================
 *  rms_ventana.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  RMS de medio ciclo, acumulado dentro de la interrupción del ADC
 * -----------------------------------------------------------------------------
 *  POR QUÉ MEDIO CICLO Y NO UN CICLO
 *
 *    Porque el lazo tiene que responder rápido, y medio ciclo son 8.3 ms en
 *    vez de 16.7 ms. Es DC-023.
 *
 *    El precio de medir medio ciclo es que **una componente de CD ya no se
 *    cancela sola**: en un ciclo completo los dos semiciclos se compensan, en
 *    medio ciclo no. Por eso aquí el CD se quita a mano, con la identidad
 *
 *        RMS² = ⟨x²⟩ − ⟨x⟩²
 *
 *    Se acumulan las dos sumas —Σx y Σx²— y el promedio de la ventana **es**
 *    el CD de esa ventana. Sale gratis y además es un diagnóstico: debe
 *    rondar las 511 cuentas, que es el bias de 2.5 V. Si se corre, algo pasó
 *    en el divisor.
 *
 *  POR QUÉ SE ACUMULA EN LA INTERRUPCIÓN Y NO EN EL LAZO
 *
 *    Ésta es la lección de la etapa 3, y costó una retractación: **el lazo se
 *    queda ciego justo cuando hay tráfico.** Durante los 2.63 ms de una
 *    transferencia I2C el lazo está detenido dentro de `endTransmission()`, y
 *    las ~63 interrupciones de ese rato pasan sin que nadie las recoja.
 *
 *    Un RMS al que le faltan muestras cuando hay I2C no es un RMS: es un
 *    número que depende del tráfico del bus. Así que la suma se hace **dentro
 *    de la ISR del ADC**, donde no hay ventana ciega, y el lazo sólo recoge la
 *    ventana ya cerrada.
 *
 *  LAS VENTANAS SE CIERRAN DISTINTO EN CADA CANAL
 *
 *    **La red (A6): la cierra el comparador.** Medio ciclo va de un cruce al
 *    siguiente, y el único que sabe cuándo ocurrió el cruce es la ISR del
 *    comparador. Por eso llama a `rms_cerrar_red()`.
 *
 *    **El inversor (A0): la cierra el conteo, y es exacto por construcción.**
 *    El inversor no necesita comparador porque su cruce lo sabe el firmware
 *    (DC-023). Y la cuenta cierra sin residuo:
 *
 *        24 024 desbordes/s ÷ 4            = 6 006 disparos de ADC/s
 *        de cada 4 disparos, 2 son A0      = 3 003 muestras/s del inversor
 *        línea de 60.06 Hz                 = 120.12 semiciclos/s
 *        3 003 ÷ 120.12                    = 25.000 muestras por semiciclo
 *
 *    **Exactamente 25, sin redondeo**, porque todo sale del mismo cristal.
 *    Son las 25 muestras por ventana que DC-023 daba por supuestas.
 *
 *    ⚠️ Y es frágil, igual que el 2.4985 del ADC: esto se cumple porque
 *    400 muestras/ciclo, 4 desbordes por disparo y 2 ranuras de A0 de cada 4
 *    se dividen sin resto. Cambiar cualquiera de los tres rompe la alineación
 *    sin avisar. Si se pierde un disparo (`adc_perdidas()` distinto de 0) la
 *    ventana se corre y hay que resincronizar: por eso el sketch de banco
 *    compara la frontera contra el semiciclo real del SPWM y lo reporta.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include "adc_portadora.h"

// Muestras de A0 en medio ciclo. Exacto por construcción: ver arriba.
#define RMS_MUESTRAS_INV    25

// ============================ ACUMULADORES ===================================
//
// Públicos porque la función de abajo va en línea dentro de la ISR del ADC, y
// una llamada a otra unidad de compilación costaría el salto y el marco. No se
// tocan desde fuera del módulo.

struct RmsAcum {
  uint32_t sumaCuad;     // Σx²  — máx 25 × 1023² = 26.2e6, cabe
  uint16_t suma;         // Σx   — máx 25 × 1023  = 25 575, cabe
  uint8_t  n;
};

/*  DOBLE BANCO, PARA NO COPIAR NADA DENTRO DE LA ISR
 *
 *  La primera versión de esto copiaba los 7 bytes del acumulador a una zona
 *  de recogida al cerrar la ventana: **83 ciclos contados en el binario**,
 *  dentro de la ISR del ADC, y se pagan justo en el peor caso — cuando la
 *  ventana cierra encima de otra interrupción.
 *
 *  Con dos bancos no se copia: se voltea un índice y se limpia el banco que
 *  queda libre. El lazo lee el otro. Son ~25 ciclos en vez de 83, y la cuenta
 *  importa: 58 ciclos son 3.6 µs de los 41.625 del periodo.
 */
extern volatile RmsAcum  rms_banco[4];       // [canal*2 + banco], plano a propósito
extern volatile uint8_t  rms_vivo[2];        // en qué banco se está acumulando
extern volatile uint8_t  rms_lista[2];       // hay una ventana sin recoger
extern volatile uint16_t rms_desbordes;      // ventanas de red sin cerrar a tiempo

/*  ⚠️ EL ARREGLO ES PLANO, Y NO ES CAPRICHO
 *
 *  La primera versión usaba `rms_banco[canal][banco]`. En el binario eso
 *  resultó **peor que la copia que vino a quitar**: con `volatile`, el
 *  compilador vuelve a leer el índice y a recalcular la dirección en CADA
 *  acceso al campo, y la ISR del ADC creció 100 instrucciones.
 *
 *  La forma correcta es calcular la dirección UNA vez en un puntero local y
 *  usar ése. El `volatile` protege contra que el compilador cachee lo que la
 *  otra ISR puede cambiar; adentro de la ISR, con las interrupciones
 *  apagadas y el banco vivo que nadie más toca, esa protección no hace falta
 *  y sí cuesta.
 */
#define RMS_INV   0            // A0, la salida del inversor
#define RMS_RED   1            // A6, la red

#define RMS_TOPE_RED   60      // tope de seguridad si el comparador no cierra

/*  Cierra una ventana: voltea el banco y limpia el que queda libre. Se llama
 *  desde dentro de una ISR, así que no lleva protección.
 */
static inline void rms_cerrar(uint8_t i)
{
  const uint8_t nuevo = (uint8_t)(rms_vivo[i] ^ 1);
  RmsAcum *b = (RmsAcum *)&rms_banco[(uint8_t)((i << 1) | nuevo)];

  b->sumaCuad = 0;
  b->suma     = 0;
  b->n        = 0;

  rms_vivo[i]  = nuevo;        // de aquí en adelante se acumula en el nuevo
  rms_lista[i] = 1;            // y el lazo puede leer el otro con calma
}

/*  Una muestra nueva. Va EN LÍNEA dentro de la ISR del ADC, vía gancho_adc.h.
 *
 *  Sólo entran los dos canales que llevan RMS. La batería (A3) no: es una
 *  medición de CD y promediarla al cuadrado no significa nada.
 */
static inline void rms_meter(uint8_t ranura, uint16_t cuenta)
{
  uint8_t i;
  if      (ranura == ADC_RANURA_INV) i = RMS_INV;
  else if (ranura == ADC_RANURA_RED) i = RMS_RED;
  else return;                                   // la batería no lleva RMS

  // La dirección, una sola vez. Ver el aviso de arriba.
  RmsAcum *a = (RmsAcum *)&rms_banco[(uint8_t)((i << 1) | rms_vivo[i])];

  a->suma     += cuenta;
  a->sumaCuad += (uint32_t)cuenta * cuenta;

  const uint8_t n = (uint8_t)(a->n + 1);
  a->n = n;

  if (i == RMS_INV) {
    if (n >= RMS_MUESTRAS_INV) rms_cerrar(RMS_INV);   // medio ciclo, exacto
  }
  else if (n >= RMS_TOPE_RED) {
    /*  La red la cierra el comparador. Si no está corriendo —o no hay red—
     *  el acumulador crecería sin freno: el tope es el seguro, y queda
     *  contado para que el reporte lo delate.
     */
    rms_desbordes++;
    rms_cerrar(RMS_RED);
  }
}

/*  La llama la ISR del comparador en cada cruce aceptado. Está en el .cpp para
 *  que comparador_red.cpp no tenga que conocer las entrañas.
 */
void rms_cerrar_red(void);

// ========================== LO QUE USA EL LAZO ===============================

void rms_preparar(void);

/*  Recoge una ventana cerrada. Devuelve false si no hay nada nuevo.
 *
 *    rmsCuentas — RMS ya sin el CD, en cuentas del ADC
 *    cd         — el promedio de la ventana: debe rondar 511
 *    n          — cuántas muestras tenía (25 en el inversor; ~25 en la red)
 */
bool rms_recoger(uint8_t i, uint16_t *rmsCuentas, uint16_t *cd, uint8_t *n);

uint16_t rms_desbordes_red(void);
void     rms_reiniciar(void);

/*  Raíz cuadrada entera, por Newton. Se usa aquí y en el sketch de banco; se
 *  deja a la vista porque evita arrastrar la de punto flotante, que para este
 *  trabajo cuesta ~1.4 kB de flash y no aporta un dígito útil.
 */
uint16_t rms_isqrt32(uint32_t v);
