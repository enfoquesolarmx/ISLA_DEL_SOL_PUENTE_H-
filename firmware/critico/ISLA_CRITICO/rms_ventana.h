/*
 * =============================================================================
 *  rms_ventana.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  RMS de medio ciclo, con las cuentas pesadas fuera de las interrupciones
 * -----------------------------------------------------------------------------
 *  POR QUÉ MEDIO CICLO, Y POR QUÉ EN PARES
 *
 *    Medio ciclo son 8.3 ms en vez de 16.7: el lazo responde al doble de
 *    rápido. Es DC-023.
 *
 *    Pero el RMS **no se puede calcular sobre una sola ventana de medio
 *    ciclo**, y eso costó una corrida de banco averiguarlo. `⟨x²⟩ − ⟨x⟩²`
 *    sobre medio seno no le quita el bias: le quita la media rectificada,
 *
 *        media del semiciclo        = bias + 0.6366·A      ← no es el bias
 *        RMS sobre su propia media  = 0.3078·A
 *        RMS verdadero de CA        = 0.7071·A    → **2.30 veces más**
 *
 *    El banco lo delató sin que nadie lo buscara: el `CD` reportado alternaba
 *    277 y 744 cuentas, simétrico alrededor de 510.5 — el bias exacto.
 *
 *    **Por eso se suman dos ventanas consecutivas = un ciclo completo**, donde
 *    el término rectificado se cancela y `⟨x⟩` sí es el bias. Y de paso
 *    arregla la dispersión, porque la red recibe 12.50 muestras por semiciclo
 *    —nunca entero— pero **12 + 13 = 25 = un ciclo exacto**. El par es
 *    deslizante, así que sigue habiendo un valor nuevo cada medio ciclo.
 *
 *  ⚠️ POR QUÉ AHORA HAY UN BÚFER CIRCULAR
 *
 *    La primera versión acumulaba Σx y Σx² **dentro** de la ISR del ADC. La
 *    multiplicación de 16×16 cuesta 24 ciclos y el resto de la acumulación
 *    otros 29: con eso la ISR del ADC llegó a 186 ciclos, y sumada al SPWM y
 *    al comparador el presupuesto quedó en 91 % del periodo de portadora. Al
 *    agregar la estadística del jitter se pasó de 100 % y **empezó a perder
 *    pasos de la onda**.
 *
 *    Ahora la ISR sólo **encola el evento** —unos 20 ciclos— y el lazo hace
 *    las cuentas.
 *
 *    Y esto NO es lo mismo que muestrear desde el lazo, que es lo que la
 *    etapa 3 demostró que no sirve. Ahí el lazo se quedaba ciego durante los
 *    2.7 ms de una transferencia I2C y **las muestras se perdían**. Aquí se
 *    encolan: en ese bloqueo entran ~12 muestras y el anillo tiene 64. El
 *    lazo las drena después, en orden, sin perder ninguna. Y si alguna vez se
 *    llenara, se cuenta — no se pierde en silencio.
 *
 *    El anillo lleva las muestras **y las fronteras de ventana mezcladas en
 *    el mismo orden en que ocurrieron**. Eso importa: la frontera de la red
 *    la pone la ISR del comparador y la de las muestras la del ADC, y si cada
 *    una llevara su propio camino el lazo no sabría cuál pasó primero.
 *
 *  LAS FRONTERAS
 *
 *    **La red (A6): la pone el comparador.** Medio ciclo va de un cruce al
 *    siguiente y el único que sabe cuándo ocurrió es él.
 *
 *    **El inversor (A0): la pone el conteo, y es exacta por construcción.**
 *
 *        24 024 desbordes/s ÷ 4            = 6 006 disparos de ADC/s
 *        2 de cada 4 disparos son A0       = 3 003 muestras/s del inversor
 *        línea de 60.06 Hz                 = 120.12 semiciclos/s
 *        3 003 ÷ 120.12                    = 25.000 muestras por semiciclo
 *
 *    Exactamente 25, sin redondeo, porque todo sale del mismo cristal.
 *
 *    ⚠️ Y es frágil igual que el 2.4985 del ADC: se cumple porque 400
 *    muestras/ciclo, 4 desbordes por disparo y 2 ranuras de A0 de cada 4 se
 *    dividen sin resto. Cambiar cualquiera de los tres rompe la alineación
 *    sin avisar.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include "adc_portadora.h"

// Muestras de A0 en medio ciclo. Exacto por construcción: ver arriba.
#define RMS_MUESTRAS_INV    25

#define RMS_INV   0            // A0, la salida del inversor
#define RMS_RED   1            // A6, la red

// Tope de seguridad de la ventana de la red, si el comparador no cierra.
#define RMS_TOPE_RED        60

/*  EL ANILLO. 64 entradas de 16 bits = 128 bytes de RAM.
 *
 *  ⚠️ DIMENSIONADO CONTRA EL BLOQUEO EQUIVOCADO, LA PRIMERA VEZ.
 *
 *  Lo puse en 64 contra los 2.7 ms de una transferencia I2C, donde entran ~12
 *  eventos. Y el banco perdió **79 eventos por segundo**, porque el bloqueo
 *  largo del lazo no es el I2C: **es imprimir el reporte**. Novecientos bytes
 *  a 115200 baudios son ~70 ms, y a 4 744 eventos por segundo eso son ~330.
 *
 *  Y no fue gratis: esos eventos faltantes corrompían las ventanas que
 *  atravesaban el reporte, y se veían como picos de σ del RMS —de 0.0 a
 *  10.6 cuentas— que sin el contador habríamos ido a buscar a la red.
 *
 *  En producción no existe el reporte. En el banco van dos cosas: 128
 *  entradas (27 ms de aguante) y drenar el anillo entre líneas del reporte,
 *  que es lo que lo deja en cero.
 *
 *  Formato de cada entrada:
 *      bit 15     1 = frontera de ventana, 0 = muestra
 *      bit 12     canal: 0 = inversor, 1 = red
 *      bits 9..0  la cuenta del ADC (sólo en las muestras)
 */
#define RMS_ANILLO     128
#define RMS_ANILLO_MSK (RMS_ANILLO - 1)
#define RMS_EV_FRONTERA 0x8000u
#define RMS_EV_CANAL    0x1000u

extern volatile uint16_t rms_anillo[RMS_ANILLO];
extern volatile uint8_t  rms_w;                 // escritura: la mueven las ISR
extern volatile uint8_t  rms_r;                 // lectura: la mueve el lazo
extern volatile uint16_t rms_anillo_lleno;      // eventos que no cupieron
extern volatile uint8_t  rms_cuenta_inv;        // muestras de A0 en la ventana

/*  Encola un evento. Se llama desde dentro de una ISR, así que no lleva
 *  protección: las interrupciones ya están apagadas.
 *
 *  La comprobación de lleno cuesta ocho ciclos y vale cada uno: un anillo que
 *  se desborda en silencio da un RMS que parece bueno y no lo es.
 */
static inline void rms_encola(uint16_t ev)
{
  if ((uint8_t)(rms_w - rms_r) >= RMS_ANILLO) { rms_anillo_lleno++; return; }
  rms_anillo[rms_w & RMS_ANILLO_MSK] = ev;
  rms_w++;
}

/*  Una muestra nueva. Va EN LÍNEA dentro de la ISR del ADC, vía gancho_adc.h.
 *
 *  La batería (A3) no entra: es una medición de CD y promediarla al cuadrado
 *  no significa nada.
 */
static inline void rms_meter(uint8_t ranura, uint16_t cuenta)
{
  if (ranura == ADC_RANURA_INV) {
    rms_encola(cuenta);
    if (++rms_cuenta_inv >= RMS_MUESTRAS_INV) {
      rms_cuenta_inv = 0;
      rms_encola(RMS_EV_FRONTERA);             // medio ciclo del inversor
    }
  }
  else if (ranura == ADC_RANURA_RED) {
    rms_encola(RMS_EV_CANAL | cuenta);
  }
}

/*  La frontera de la red. La llama la ISR del comparador en cada cruce
 *  aceptado — reemplaza al cierre de ventana que antes se hacía ahí mismo.
 */
static inline void rms_marca_red(void)
{
  rms_encola(RMS_EV_FRONTERA | RMS_EV_CANAL);
}

// ========================== LO QUE USA EL LAZO ===============================

void rms_preparar(void);

/*  Drena el anillo y hace las cuentas. Se llama en cada vuelta del lazo; si no
 *  hay nada encolado regresa en seguida.
 */
void rms_atender(void);

/*  El último RMS de un ciclo completo. false si no hay uno nuevo.
 *
 *    rmsCuentas — RMS verdadero de CA, en cuentas del ADC
 *    cd         — el bias: debe rondar 511 **y quedarse quieto**
 *    n          — muestras del par: 25 en la red, 50 en el inversor
 */
bool rms_recoger(uint8_t i, uint16_t *rmsCuentas, uint16_t *cd, uint8_t *n);

uint16_t rms_desbordes_red(void);     // ventanas de red cerradas por el tope
uint16_t rms_anillo_perdidos(void);   // eventos que no cupieron en el anillo
void     rms_reiniciar(void);

/*  Raíz cuadrada entera, por Newton. Evita arrastrar la de punto flotante,
 *  que para este trabajo cuesta ~1.4 kB de flash y no aporta un dígito útil.
 */
uint16_t rms_isqrt32(uint32_t v);
