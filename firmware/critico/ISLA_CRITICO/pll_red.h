/* =============================================================================
 *  ISLA DEL SOL — enganche de fase del inversor a la red (PLL)
 *
 *  PARA QUE SIRVE, Y PARA QUE NO
 *
 *    La Isla del Sol **no inyecta a la red** (DC-019): transfiere con relevadores,
 *    corta antes de cerrar. Asi que esto NO es para paralelar.
 *
 *    Sirve para una sola cosa: **que al cerrar el relevador las dos ondas esten
 *    alineadas**, y no haya el golpe de corriente que da transferir a un motor
 *    o a un transformador en mal angulo.
 *
 *  EL VOLANTE ES EL TOP DE LA PORTADORA, NO LA TABLA
 *
 *        f_linea = 16 MHz / (2 x TOP x 400)
 *
 *    Mover el TOP mueve la portadora y la linea juntas. Por eso **las 25
 *    muestras por semiciclo de DC-023 se conservan exactas** —el disparo del
 *    ADC tambien cuelga de la portadora— y **la tabla de seno no se toca**: no
 *    se salta ni se repite una sola muestra, la onda queda intacta.
 *
 *    La alternativa habria sido un acumulador de fase sobre la tabla, y esa si
 *    rompia la alineacion y ensuciaba la onda con saltos de 0.9 grados.
 *
 *  LOS TRES LIMITES QUE PIDIO EL ARQUITECTO
 *
 *    1. **Rango +-2 Hz.** Fuera de eso se suelta y vuelve a libre. Un generador
 *       viejo se hunde a 57 Hz con carga, y **arrastrar la casa a 57 Hz es peor
 *       que no transferir**: los motores y el refrigerador trabajan mal. Mejor
 *       quedarse en los 60.060 del cristal, que es mas estable que cualquier
 *       red.
 *
 *    2. **Rampa de 2 segundos.** La correccion no salta: se limita la velocidad
 *       de cambio para que recorrer el rango completo tome 2 s. Son 0.185
 *       cuentas de TOP por actualizacion a 60/s.
 *
 *    3. **Avisa cuando engancho y cuanto tardo.** El enganche no es una
 *       opinion: es el error de fase quieto dentro de una banda durante medio
 *       segundo seguido.
 *
 *  !! EL ACOPLAMIENTO DE AMPLITUD: CONOCIDO, MEDIDO, Y A PROPOSITO SIN COMPENSAR
 *
 *    Al subir el TOP, el mismo `d` de la tabla es un ciclo de trabajo relativo
 *    menor: **la amplitud baja**. En el extremo del rango (TOP 344, o sea
 *    -2 Hz) son **-3.3 %**.
 *
 *    Este modulo NO lo corrige, y no es un pendiente:
 *
 *      · Corregirlo aqui seria leer `spwm_amplitud()`, escalarla y volverla a
 *        escribir -- y la siguiente vuelta leeria la ya escalada y la
 *        escalaria otra vez. Es una realimentacion sobre si misma.
 *
 *      · **El lazo de voltaje de salida lo va a corregir solo.** Ese lazo mide
 *        el RMS real en A0 y ajusta la amplitud; cualquier error de ganancia,
 *        venga de donde venga, le queda adentro. Corregirlo dos veces es
 *        pelearse con el.
 *
 *    En enganche normal son 0.1 %: la red a 60.00 contra nuestros 60.06 son
 *    0.33 cuentas de TOP. El 3.3 % solo aparece con un generador dos hertz
 *    abajo, y ahi hay cosas peores de que preocuparse.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>

/* ------------------------------------------------------------- parametros */

#define PLL_RANGO_MHZ       2000L   /* +-2 Hz: fuera de aqui, suelta         */
#define PLL_RAMPA_MS        2000U   /* recorrer el rango completo toma 2 s   */
#define PLL_BANDA_GRADOS_X10   50   /* +-5.0 grados: la banda de "enganchado" */
#define PLL_SOSTENIDO_CICLOS   30   /* medio segundo adentro para declararlo  */

/* ---------------------------------------------------------------- estados */

enum PllEstado : uint8_t {
  PLL_LIBRE = 0,      /* corriendo del cristal, 60.060 Hz                    */
  PLL_ENGANCHANDO,    /* corrigiendo, todavia fuera de banda                 */
  PLL_ENGANCHADO,     /* error de fase quieto dentro de banda                */
  PLL_FUERA_RANGO,    /* la fuente se salio de +-2 Hz: se solto a proposito  */
  PLL_SIN_RED         /* no hay cruces que seguir: se solto y se dice         */
};

/*  !! EL ENGANCHE NO SE LATCHEA, SE REVALIDA CADA CICLO.
 *
 *  Lo encontro el banco: al cortar la red el PLL dejaba de corregir pero
 *  seguia diciendo ENGANCHANDO para siempre, como si la estuviera buscando.
 *  Un estado que miente es peor que no tenerlo, porque quien decida cerrar el
 *  relevador va a preguntarle a `pll_estado()`.
 *
 *  Ahora hay PLL_SIN_RED, y lo que lo dispara NO es solo `cmp_red_presente()`
 *  sino que **el sello calificado deje de avanzar**. La diferencia importa:
 *  con la red cortada la entrada queda al aire y el comparador puede
 *  castanetear con el ruido; `cmp_red_presente()` diria que si hay red y el
 *  lazo se engancharia AL RUIDO. Eso es justo el limbo que no queremos.
 */

void        pll_preparar(void);

/*  Se llama desde el lazo, una vez por ciclo del inversor. Ella sola detecta
 *  si hay ciclo nuevo; llamarla mas seguido no hace dano.
 */
void        pll_atender(void);

/*  Pide enganche. Mientras no se pida, el inversor corre libre del cristal:
 *  el enganche se pide cuando una transferencia es inminente, no siempre.
 */
void        pll_pedir_enganche(bool si);

PllEstado   pll_estado(void);
int16_t     pll_error_grados_x10(void);   /* error de fase, decimas de grado */
uint16_t    pll_ms_para_enganchar(void);  /* cuanto tardo la ultima vez      */
int16_t     pll_correccion_mhz(void);     /* cuanto estamos corrigiendo      */
int32_t     pll_red_mhz(void);            /* deriva de la red vs 60.060 Hz   */
uint16_t    pll_descartados(void);        /* muestras de fase imposibles     */

/*  Los ultimos 8 errores de fase CRUDOS, uno por ciclo, sin filtrar. i=0 es la
 *  mas vieja. Es el instrumento para ver que pasa entre reporte y reporte.
 */
int16_t     pll_huella(uint8_t i);
