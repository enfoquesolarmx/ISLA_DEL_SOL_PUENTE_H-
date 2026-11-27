/*
 * =============================================================================
 *  prueba_rele.h  -  Isla del Sol, protocolo de banco
 *  Medir K4: demora de orden a contacto, su DISPERSION, y en CALIENTE
 * -----------------------------------------------------------------------------
 *  POR QUE. Al 27 de noviembre de 2026 el presupuesto de la transferencia es:
 *
 *      deteccion   7.53 ms   MEDIDO (analizador y serial, dos veces)
 *      alineacion  0 a 8.325 ms   (esperar el cruce por cero)
 *      rele        ? ? ?     <-- lo unico sin medir, y manda sobre todo
 *      ---------
 *      una fuente ATX de PC aguanta 16 a 20 ms
 *
 *  Con el rele en 5 ms la transferencia alineada cabe al limite; en 10 ms ya
 *  no cabe. **El numero decide el diseno**, no al reves.
 *
 *  CUAL RELE. K4, no K5. D4 = `INV_CMD` manda K4 + K2: es la orden de pasar
 *  la casa al inversor, o sea el camino que mantiene viva la PC. D13 =
 *  `TN_CMD` manda K5, la union N-PE, y ademas pasa por la compuerta Q2. Lo
 *  que hay que medir es D4 -> contacto de K4, que incluye el driver entero.
 *
 *  Y LA DISPERSION PESA MAS QUE EL PROMEDIO. Si el retardo medio es 6 ms pero
 *  varia +-1 ms, eso son **+-21.6 grados a 60 Hz** alrededor del blanco, y
 *  ninguna resta por lista que sea lo arregla: la ventana de aceptacion no
 *  puede ser mas angosta que la dispersion. Por eso son 30 operaciones y no
 *  una.
 *
 *  Y EN CALIENTE. El cobre sube 0.393 % de resistencia por grado. Una bobina
 *  que lleva rato energizada en una caja cerrada jala menos corriente y
 *  **pega mas lento**. Medirlo frio y darlo por bueno es enganarse. Por eso
 *  hay un remojo: se deja la bobina pegada 10 minutos y se repite la tanda.
 *
 *  LA BOBINA ES SU PROPIO TERMOMETRO. Con un multimetro, la resistencia de la
 *  bobina antes y despues da la temperatura sin necesidad de sensor:
 *
 *      T_cal = (R_cal / R_frio) x (234.5 + T_frio) - 234.5      [grados C]
 *
 *  (234.5 es la constante del cobre; es el metodo de resistencia de toda la
 *  vida para devanados.) Anotar las dos lecturas junto con los tiempos.
 *
 * -----------------------------------------------------------------------------
 *  COMO SE CORRE
 *
 *  ⚠️⚠️ ESTA PRUEBA PIDE **J15 CONECTADO**, al reves que el resto de la
 *  Parte C. El micro va a mover reles de verdad. Antes de empezar:
 *
 *      1. ETAPA DE POTENCIA FUERA. Sin 127 V en ningun lado.
 *      2. Nada conectado a los contactos de K4 ni de K2.
 *      3. Verificar a ojo que no hay red en la tarjeta de transferencia.
 *
 *  Mientras corre, la traza de cruces en D4 queda SUSPENDIDA: el pin lo toma
 *  la prueba.
 *
 *  INSTRUMENTACION. CH1 en D4 (la orden). CH2 en un contacto SECO de K4:
 *
 *      5 V --- 10 kOhm ---+--- contacto de K4 --- GND
 *                         |
 *                        CH2
 *
 *  Con el contacto abierto CH2 lee 1; al cerrar, 0. Solo logica, sin red.
 *  El contacto tiene que estar aislado de la acometida ANTES de cablear esto.
 *
 *  QUE SE SACA DE LA CAPTURA, por cada operacion:
 *
 *      a) PEGUE   : D4 sube        -> CH2 baja         (lo que importa)
 *      b) SUELTE  : D4 baja        -> CH2 sube
 *      c) REBOTE  : cuanto castanetea CH2 tras el primer toque
 *
 *  📌 El SUELTE suele sorprender: con un diodo de rueda libre solo, la
 *  corriente de la bobina se sostiene y el suelte se vuelve 2 o 3 veces mas
 *  lento. Si sale largo, la cura es un zener o una resistencia en serie con
 *  el diodo. Eso es cambio de hardware: se propone, no se toca.
 *
 *  TECLAS
 *      R  arranca o detiene la tanda  (30 operaciones, 1 por segundo)
 *      C  remojo caliente: deja la bobina pegada y cuenta el tiempo;
 *         al soltarlo con C otra vez, correr R de inmediato
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RELE_BIT          PD4        /* D4 = INV_CMD -> K4 + K2 */
#define RELE_OPERACIONES  30         /* la dispersion no sale de una medida */
#define RELE_PEGADO_MS   300u
#define RELE_SUELTO_MS   700u

void rele_preparar(void);

/*  Atiende la maquina en el lazo. No hace nada si la prueba esta apagada. */
void rele_atender(void);

/*  'R' = tanda, 'C' = remojo. Devuelve true si consumio la tecla. */
bool rele_tecla(char c);

bool rele_corriendo(void);
