/*
 * =============================================================================
 *  energia.h  -  Isla del Sol, AUX (A2): medidor de energía
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Toma la potencia que el CRÍTICO reporta en cada foto y de ahí saca lo que
 *    el usuario ve, en los DOS modos de trabajo:
 *
 *      INVERSOR  (la energía sale de la batería)  -> "consumo"
 *      CARGADOR  (la energía entra a la batería)  -> "carga"
 *
 *    De cada uno: los watts de ahora, lo de hoy, lo de ayer, los promedios de
 *    7 y 30 días y el histórico acumulado. Más la máxima sostenida del
 *    consumo y cuánto lleva despierto el equipo.
 *
 *  EL SIGNO ES LA ÚNICA DIFERENCIA
 *    El INA240 mide la corriente de batería en los dos sentidos, así que no
 *    hacen falta dos sensores ni dos protocolos: basta el signo.
 *
 *        watts > 0  ->  sale de la batería  ->  CONSUMO (inversor)
 *        watts < 0  ->  entra a la batería  ->  CARGA   (cargador)
 *
 *  QUÉ NO HACE
 *    No mide. La potencia se la dan ya calculada; este módulo integra, guarda
 *    y promedia. De dónde salen los watts y por qué se miden en DC está en
 *    docs/medicion_energia.md.
 *
 *  DÓNDE VIVE EL DATO
 *    En RAM: las ventanas de 24 h y los contadores del día en curso.
 *    En EEPROM: los dos históricos y 180 días de cada sentido, que se escriben
 *    una vez cada 24 h (365 escrituras al año: 273 años de vida) y además
 *    cuando la batería avisa que se va a apagar.
 *
 *  CÓMO SE USA
 *      energia_iniciar();                           // una vez, en setup()
 *      ...
 *      if (hay_foto_fresca)  energia_muestra(watts_con_signo, dt_ms);
 *      else                  energia_sin_enlace();
 *      energia_atender(millis(), estado_bateria);   // cada vuelta de loop()
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "config_energia.h"

// Los dos sentidos. Se usa como índice, así que los valores importan.
enum Sentido : uint8_t { ENER_CONSUMO = 0, ENER_CARGA = 1 };

// ---------------------------------------------------------------------------
void energia_iniciar();

// Una foto fresca. `watts` lleva signo: positivo sale de la batería (inversor),
// negativo entra (cargador). `dt_ms` es lo que pasó desde la anterior.
void energia_muestra(int16_t watts, uint16_t dt_ms);

// Se perdió el enlace. Tira el bloque de la máxima sostenida a medias: juntar
// lecturas de antes y después del hueco y llamarlas "sostenidas" sería mentira.
void energia_sin_enlace();

// Cada vuelta de loop(). `ms` es millis(); `estadoBateria` viene de la foto
// (0 normal, 1 aviso, 2 corte bajo, ...). Con aviso o corte guarda enseguida.
void energia_atender(uint32_t ms, uint8_t estadoBateria);

// ---------------------------------------------------------------------------
// Lo que la pantalla necesita. Todo lo que lleva `s` es por sentido.
uint16_t energia_watts();                   // magnitud de la potencia de ahora
bool     energia_cargando();                // true si la energía entra a la batería
uint16_t energia_por_ciento();              // % de POTENCIA_NOMINAL_W (puede pasar de 100)
uint16_t energia_max24h_w();                // máxima SOSTENIDA del consumo

uint32_t energia_hoy_wh(Sentido s);         // últimas 24 h
uint32_t energia_ayer_wh(Sentido s);        // el último día cerrado; 0 si no hay
uint32_t energia_historico_wh(Sentido s);   // desde que se instaló
uint32_t energia_promedio_wh(Sentido s, uint8_t dias);   // 0 si no hay días

uint32_t energia_despierto_s();             // segundos desde el último arranque
uint16_t energia_dias_guardados();          // cuántos días hay en el historial

// Rendimiento de ida y vuelta, en porcentaje: de la energía que entró a la
// batería, cuánta volvió a salir. Un número que baja mes con mes es una
// batería envejeciendo. Devuelve 0 si todavía no hay carga registrada.
uint16_t energia_rendimiento_pct();

// ---------------------------------------------------------------------------
void energia_guardar();      // lo llama energia_atender() solo; queda para el menú
void energia_borrar();       // borra todo; sólo desde el menú, con confirmación

// El "archivo de consumo": vuelca los dos historiales en CSV, un renglón por
// día con las dos columnas. Llama a `escribe` una vez por renglón, sin \n.
void energia_volcar_csv(void (*escribe)(const char *));
