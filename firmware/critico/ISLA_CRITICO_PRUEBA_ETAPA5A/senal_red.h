/*
 * =============================================================================
 *  senal_red.h  -  Isla del Sol, protocolo de banco Parte C
 *  "Hay red" sacado a un pin, para validarlo con el analizador logico
 * -----------------------------------------------------------------------------
 *  QUE ES. El veredicto de `cmp_red_presente()` manejado a D13 como NIVEL:
 *
 *      D13 = 1  ->  el firmware cree que hay red
 *      D13 = 0  ->  el firmware cree que no hay red
 *
 *  PARA QUE. Todo lo que hace la transferencia descansa en ese veredicto, y
 *  hasta hoy solo se podia creer lo que decia el serial. Con el pin afuera,
 *  el analizador lo contesta sin preguntarle al firmware: una prueba de la
 *  prueba. Ademas mide lo que DC-035 dejo pendiente -- cuanto tarda el
 *  firmware en darse cuenta -- que es el numero que decide si una PC
 *  sobrevive la transferencia (una ATX aguanta 16 a 20 ms).
 *
 *  ⚠️ SOLO CON J15 DESCONECTADO. D13 es `TN_CMD`, la orden de union N-PE
 *  (rele K5). Esta senal NO es TN_CMD y su polaridad es la CONTRARIA a la de
 *  TN: aqui 1 = hay red, y TN en 1 significa isla. Es un instrumento, no una
 *  orden. Con J15 puesto, Q2 lo bloquearia (compuerta Y con AISLAR), asi que
 *  no moveria K5 -- pero entonces el analizador mostraria una senal que no
 *  hace nada, que es la peor clase de prueba. J15 fuera, como toda la Parte C.
 *
 *  LO QUE DEBE SALIR, y esto se escribe ANTES de medir a proposito:
 *
 *  `cmp_red_presente()` es un puro vencimiento: hay red si el ultimo cruce
 *  llego hace menos de CMP_SIN_RED_MS. De ahi se predice, leyendo el codigo:
 *
 *      D13 CAE   ->  ~50 ms despues del ultimo cruce real  (CMP_SIN_RED_MS)
 *      D13 SUBE  ->  < 1 ms despues del primer cruce que vuelve
 *
 *  Si el analizador da eso, el firmware hace lo que dice el codigo. Si da
 *  otra cosa, el problema esta entre el veredicto y el pin, y hay que
 *  buscarlo. **La asimetria es el hallazgo**: lento para caer y rapido para
 *  subir es justo al reves de lo que conviene a la transferencia.
 *
 *  COSTO. Cero en las ISR. Ni una linea corre en interrupcion: el nivel se
 *  refresca en el lazo, al lado de `cmp_atender()`, con un `sbi`/`cbi` de
 *  una instruccion (62.5 ns). No se usa `digitalWrite()`, que son ~50 ciclos
 *  de busqueda en tablas de pines.
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*  1 = D13 sigue el veredicto de la red.  0 = D13 queda quieto en 0.
 *  Se sube en 1 porque es la prueba en curso; cuando se cierre, a 0.
 */
#define SENAL_RED_ACTIVA   1

/*  Prepara el pin. Lo deja en 0 (sin red) hasta la primera vuelta del lazo. */
void senal_red_preparar(void);

/*  Refresca el nivel y, si cambio, anota la latencia. Va en el lazo, pegado
 *  a `cmp_atender()`, y hay que llamarla tambien desde los puntos donde el
 *  reporte serial bloquea el lazo -- si no, el pin se atrasa justo cuando se
 *  esta midiendo.
 */
void senal_red_atender(void);

/*  Lo que vio, para cotejar el serial contra el analizador. */
bool     senal_red_nivel(void);      // lo que tiene el pin ahora
uint16_t senal_red_cambios(void);    // transiciones totales: si crece sola,
                                     // hay falsos cortes y eso es una
                                     // transferencia que dispara sin razon
bool     senal_red_ultima(uint8_t *subio, uint16_t *latenciaUs);
                                     // la ultima transicion: hacia donde fue
                                     // y a cuantos us del cruce mas reciente
