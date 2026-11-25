/* =============================================================================
 *  ISLA DEL SOL — sensor de corriente AC (pinza SCT-013) en A2 del CRÍTICO
 *
 *  La pinza abraza el cable del devanado de 127 V del transformador, entre el
 *  transformador y los relés de transferencia (DC-013). Por ahí pasa la
 *  corriente de los dos modos: la que el inversor entrega y la que el cargador
 *  toma.
 *
 *  LO QUE MIDE, Y LO QUE NO
 *
 *    Mide la corriente del transformador. **NO ve la carga de la casa**, que
 *    cuelga del mismo generador por otro camino. Por eso el límite de carga de
 *    DC-034 puede ser fijo pero no repartido automáticamente.
 *
 *  EL MODELO QUE SE CONSIGUIÓ ES DE 30 A, Y SE COMPENSA CON VUELTAS
 *
 *    Una pinza mide **ampere-vuelta**. Si el cable pasa dos veces por la
 *    ventana, 10 A de cable se ven como 20 A, y un SCT-013-030 se comporta
 *    exactamente como el -015 que se quería:
 *
 *        corriente          -030, 1 vuelta     -030, 2 vueltas
 *        inversor 1 kW           54 cuentas        107 cuentas
 *        cargador a 105 V        71                142
 *
 *    ⚠️ El precio son dos cosas que hay que tener presentes:
 *
 *      · con 2 vueltas el **núcleo satura a 15 A de cable** (30 A-vuelta), así
 *        que el arranque de un motor va a leer bajo. No importa para proteger:
 *        el corte rápido cuelga del comparador de CD (DC-014), no de aquí.
 *      · si una vuelta se zafa, **la lectura se va a la mitad sin avisar**.
 *        El cable se amarra.
 *
 *  EL PISO DE RUIDO: EL RMS DE CERO NO ES CERO
 *
 *    El RMS se calcula como √(⟨x²⟩ − ⟨x⟩²), y el bias se cancela exacto porque
 *    se resta la media. Lo que **no** se cancela es el ruido:
 *
 *        RMS medido = √(señal² + ruido²)
 *
 *    Promediar más ventanas reduce la dispersión de la estimación, **no la
 *    estimación**: con corriente cero el canal converge al RMS de su propio
 *    ruido. Con 1 cuenta de ruido y 2 vueltas, el piso queda en ~0.07 A (9 W a
 *    127 V), útil desde ~0.22 A y exacto desde ~0.5 A.
 *
 *    ⏳ **Ese "1 cuenta" es estimación, no medición.** Se mide directo: pinza
 *    cerrada en el aire o sobre un cable sin corriente, y lo que lea el RMS
 *    **es** el piso. Mientras no se mida, `SCT_PISO_CUENTAS` es un supuesto.
 *
 *  DETECCIÓN DE PINZA DESCONECTADA — SALE GRATIS
 *
 *    En el esquemático el bias le llega al pin **a través de la bobina de la
 *    pinza**:  BIAS_I_AC → manga del jack → bobina → punta → R15 → A2.
 *
 *    Entonces el nivel de CD del canal dice si la pinza está puesta:
 *
 *        pinza conectada      CD ≈ 511   (el bias)
 *        pinza desconectada   CD ≈ 0     (C20 se descarga, no hay camino)
 *
 *    Importa porque sin esto **una pinza floja se ve igual que "no hay
 *    carga"**, y de este canal cuelga la alarma de relé pegado de DC-019.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>

/* ---------------------------------------------------------------- la pinza */

/*  Fondo de escala del modelo, en amperes. El SCT-013 de salida de voltaje da
 *  **1 V RMS a fondo de escala** y trae la resistencia de carga adentro, así
 *  que el circuito no lleva resistencia externa.
 *
 *  ⚠️ Tiene que ser un modelo de SALIDA DE VOLTAJE (-005 … -060). El -000 es
 *  de salida de corriente (100 A : 50 mA) y SIN resistencia interna: con ése
 *  este circuito no mide, y además la pinza queda en circuito abierto.
 */
#define SCT_FONDO_A         30

/*  Cuántas veces pasa el cable por la ventana de la pinza. Con 2 vueltas un
 *  -030 se comporta como un -015. Si algún día se consigue el -015, aquí van
 *  30 → 15 y 2 → 1, y no se toca nada más.
 */
#define SCT_VUELTAS          2

/*  Piso de ruido supuesto, en cuentas de RMS. ⏳ PENDIENTE DE MEDIR: pinza
 *  cerrada en el aire y leer el RMS. Abajo de este valor el canal reporta
 *  cero en vez de inventar una corriente que es su propio ruido.
 */
#define SCT_PISO_CUENTAS     1

/* ------------------------------------------------- detección de desconexión */

/*  El CD esperado con la pinza puesta es el bias, AVCC/2 ≈ 511 cuentas, y es
 *  **inmune a que AVCC se mueva** porque el divisor del bias cuelga del mismo
 *  riel que la referencia del ADC (DC-029).
 */
#define SCT_CD_ESPERADO    511
#define SCT_CD_TOLERANCIA   80   // ±80 cuentas: ~0.4 V, de sobra para el ruido

void      sct_preparar(void);

/*  Se llama desde el lazo. Cierra la cuenta de la ventana que la ISR dejo
 *  lista: la raiz y las divisiones NO van en la ISR.
 */
void      sct_atender(void);

/*  Corriente RMS en miliamperes **de cable** (ya dividida entre las vueltas).
 *  Devuelve 0 si el RMS está por debajo del piso de ruido.
 */
uint16_t  sct_corriente_ma(void);

/*  false = la pinza no está conectada. El CD se fue del bias porque el camino
 *  de CD pasa por la bobina. Mientras sea false, `sct_corriente_ma()` no es
 *  una medición de corriente: es ruido.
 */
bool      sct_pinza_presente(void);

uint16_t  sct_cd_cuentas(void);    // para diagnóstico y para el reporte

/* ------------------------------------------------- lo que corre en la ISR ---
 *
 *  La decisión de ranura va EN LINEA, igual que en `rms_meter`. Si se deja
 *  dentro de la función, el compilador empuja cuatro registros ANTES de mirar
 *  la ranura y las 7 muestras de 8 que no son la pinza pagan ~20 ciclos por
 *  nada. Así pagan una comparación y un salto.
 */
#include "adc_portadora.h"

void sct_acumular(uint16_t cuenta);

static inline void sct_meter(uint8_t ranura, uint16_t cuenta)
{
  if (ranura == ADC_RANURA_COR) sct_acumular(cuenta);
}
