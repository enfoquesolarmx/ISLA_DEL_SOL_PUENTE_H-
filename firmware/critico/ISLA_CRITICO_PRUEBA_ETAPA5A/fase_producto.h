/* =============================================================================
 *  ISLA DEL SOL — detector de fase por PRODUCTO (correlación)
 *
 *  POR QUE EXISTE: EL CRUCE POR CERO ES UN INSTANTE, Y UN INSTANTE SE ENSUCIA
 *
 *    Todo el 26 de noviembre se fue en pelear con el detector de cruce. El
 *    analizador dio el veredicto: los dos pulsos de D13 separados EXACTAMENTE
 *    un periodo de portadora (41.7 us contra los 41.6 que mide D4 en la misma
 *    pantalla) son **nuestro propio SPWM acoplandose a la entrada del
 *    comparador**. Y como el detector se queda con el primer flanco de la
 *    rafaga, y el acoplamiento solo puede ADELANTARLA, el error de fase salia
 *    con un sesgo de un solo lado: los seis saltos de la corrida fueron todos
 *    negativos, de -79 a -86.5 grados, ni uno positivo.
 *
 *    Ningun filtro arregla eso de raiz, porque el problema no es el ruido:
 *    es que **toda la informacion de fase depende de un solo instante**, y
 *    ese instante es justo donde la senal es mas lenta y el ruido manda.
 *
 *  LA IDEA, DE UN PLL BIEN HECHO (ernane-aac.github.io/Arduino-PLL)
 *
 *    Ese diseno no detecta cruces por cero. Multiplica la senal de entrada
 *    por el coseno de su propia referencia y promedia: el detector de fase es
 *    un PRODUCTO, no un evento. La fase sale de LA ONDA COMPLETA.
 *
 *    Su implementacion cuesta 195 us de ISR en un periodo de 250 -- 78 % del
 *    CPU -- y eso para nosotros es imposible: el SPWM solo ya va en 40 %.
 *    Pero el costo es suyo, no de la idea. El puede permitirse 4000 senos por
 *    segundo porque su salida es un PWM filtrado con dos RC; nosotros no.
 *
 *  LO QUE SI NOS CABE, Y POR QUE
 *
 *    Las dos piezas caras ya las tenemos pagadas:
 *
 *      · **A6 ya se muestrea a 1501.5/s**, amarrado a la portadora (DC-020),
 *        o sea 25 muestras por ciclo en posiciones que conocemos exacto.
 *      · **Sabemos nuestro propio angulo en cada muestra**: es `muestra`. Y el
 *        coseno sale de la misma TABLA_SENO leida a un cuarto de vuelta, que
 *        es el mismo espejo de cuarto de onda que ya usa el SPWM.
 *
 *    Con  v = A sen(theta - theta0)  y theta = 2*pi*pos/400:
 *
 *        Sc = suma( v cos(theta) ) = -(A N / 2) sen(theta0)
 *        Ss = suma( v sen(theta) ) =  (A N / 2) cos(theta0)
 *
 *        theta0 = atan2( -Sc , Ss )
 *
 *    Y fijate en lo que eso da gratis:
 *
 *      · **La amplitud se cancela.** El cociente no depende de A, asi que la
 *        ganancia del detector no cambia con el voltaje de la fuente.
 *      · **El offset de CD se cancela** al sumar sobre un ciclo entero.
 *      · **Una muestra corrompida mueve el resultado 1/25**, no lo tumba. El
 *        acoplamiento de nuestra portadora deja de importar.
 *      · No hay comparador, no hay repique, no hay rafaga, no hay primer
 *        flanco, no hay blanqueo, no hay sentido que adivinar.
 *
 *  EL PRESUPUESTO, ANTES DE ESCRIBIR (la leccion del 26)
 *
 *    La multiplicacion con signo cuesta ~150 ciclos por muestra. Metida en la
 *    ISR del ADC serian 9.4 us DENTRO del periodo de portadora, con OCUPA ya
 *    llegando a 31 us de 41.6. Eso es exactamente lo que reventó SOBREPASOS.
 *
 *    Asi que va con el patron de siempre: **la ISR guarda, el lazo calcula.**
 *    La ISR lee la posicion y empuja dos enteros al anillo -- ~25 ciclos,
 *    1.5 us, 0.23 % del CPU. Las tablas y los productos corren en el lazo,
 *    que tiene 7680 vueltas por segundo de holgura y no es critico.
 *
 *  !! POR AHORA SOLO MIDE. El PLL sigue usando el comparador. Los dos numeros
 *  se imprimen lado a lado para que el banco diga cual es mejor antes de
 *  cambiar nada.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "adc_portadora.h"

/*  Cuantas muestras pide para dar un resultado. 25 por ciclo a 60 Hz, asi que
 *  20 es un ciclo con holgura para las que se pierdan mientras imprime.
 */
#define FASE_MIN_MUESTRAS   20

void fase_preparar(void);

/*  Desde la ISR del ADC. Guarda y ya: ni tablas ni multiplicaciones aqui. */
void fase_acumular(uint16_t cuenta);

/*  Filtro de ranura EN LINEA, por la misma razon que en sct_meter: si el `if`
 *  vive dentro de la funcion, el compilador empuja registros ANTES de saber si
 *  hacen falta, y 7 de cada 8 muestras pagan el prologo completo.
 */
/*  !! APAGADO POR OMISION, Y ESO ES A PROPOSITO.
 *
 *  Medido antes de escribirlo, que es la leccion del 26:
 *
 *      ISR del ADC          104 -> 114 instrucciones, push 16 -> 18
 *      fase_acumular         42 instrucciones, 5 push  (~90 ciclos)
 *
 *  El presupuesto de las cuatro interrupciones esta en 507 de 666 ciclos por
 *  periodo de portadora. Noventa ciclos mas en la ranura de la red lo dejan en
 *  608: 91 % de ocupacion. Ayer un 91 % tiro SOBREPASOS de 0 a 42.
 *
 *  Asi que esto es un EXPERIMENTO QUE SE PRENDE Y SE APAGA desde el monitor,
 *  sin reflashear. Apagado cuesta una lectura de byte y un salto: ~4 ciclos.
 *  Si SOBREPASOS se mueve, se apaga y se sigue trabajando.
 */
extern volatile bool fase_activa;
void fase_habilitar(bool si);

static inline void fase_meter(uint8_t ranura, uint16_t cuenta)
{
  /*  La ranura PRIMERO: es un valor que ya esta en registro. Al reves, el
   *  compilador empuja para leer el volatile antes de saber si hace falta --
   *  el mismo error del filtro de ranura de la pinza, otra vez.
   */
  if (ranura == ADC_RANURA_RED && fase_activa) fase_acumular(cuenta);
}

/*  En el lazo: drena el anillo y acumula los productos. */
void fase_atender(void);

/*  Fase de la red respecto a NUESTRO ciclo, en decimas de grado (-1800..1800),
 *  y cuantas muestras la respaldan. Devuelve false si no hay suficientes.
 *  Consume: deja los acumuladores en cero para la siguiente ventana.
 */
bool fase_leer(int16_t *gradosX10, uint8_t *muestras);

/*  Muestras que se perdieron porque el lazo no drenó a tiempo (el reporte lo
 *  bloquea decenas de ms). Si esto crece, el anillo es chico.
 */
uint16_t fase_perdidas(void);
