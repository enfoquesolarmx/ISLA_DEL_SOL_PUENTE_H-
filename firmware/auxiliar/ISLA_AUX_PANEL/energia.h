/*
 * =============================================================================
 *  energia.h  -  Isla del Sol, AUX (A2): medidor de energía
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Toma la potencia que el CRÍTICO reporta en cada foto y de ahí saca todo
 *    lo que el usuario ve: watts de ahora, la máxima sostenida de las últimas
 *    24 horas, el consumo de hoy y de ayer, los promedios de 7 y 30 días, el
 *    histórico acumulado y cuánto lleva despierto el equipo.
 *
 *  QUÉ NO HACE
 *    No mide. La potencia se la dan ya calculada; este módulo integra, guarda
 *    y promedia. De dónde salen los watts y por qué se miden en DC está en
 *    docs/medicion_energia.md.
 *
 *  DÓNDE VIVE EL DATO
 *    En RAM: la ventana de 24 h y los contadores del día en curso.
 *    En EEPROM: el histórico y un año de consumo diario, que se escriben una
 *    vez cada 24 h (365 escrituras al año: 273 años de vida) y además cuando
 *    la batería avisa que se va a apagar.
 *
 *  CÓMO SE USA
 *      energia_iniciar();                       // una vez, en setup()
 *      ...
 *      if (hay_foto_fresca)  energia_muestra(watts, ms_desde_la_anterior);
 *      else                  energia_sin_enlace();
 *      energia_atender(millis(), estado_bateria);   // cada vuelta de loop()
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "config_energia.h"

// ---------------------------------------------------------------------------
// Arranque. Lee la EEPROM; si está vacía o dañada, empieza de cero.
void energia_iniciar();

// Una foto fresca del CRÍTICO. `dt_ms` es lo que pasó desde la anterior.
// Sólo se llama con fotos válidas: ver energia_sin_enlace().
void energia_muestra(uint16_t watts, uint16_t dt_ms);

// Se perdió el enlace. Tira el bloque de la máxima sostenida a medias: juntar
// lecturas de antes y después del hueco y llamarlas "sostenidas" sería mentira.
void energia_sin_enlace();

// Cada vuelta de loop(). `ms` es millis(); `estadoBateria` viene de la foto
// (0 normal, 1 aviso, 2 corte bajo, ...). Con aviso o corte guarda enseguida.
void energia_atender(uint32_t ms, uint8_t estadoBateria);

// ---------------------------------------------------------------------------
// Lo que la pantalla necesita.
uint16_t energia_watts();            // potencia de este momento
uint16_t energia_por_ciento();       // % de POTENCIA_NOMINAL_W (puede pasar de 100)
uint16_t energia_max24h_w();         // máxima SOSTENIDA de las últimas 24 h
uint32_t energia_hoy_wh();           // consumo de las últimas 24 h
uint32_t energia_ayer_wh();          // el último día cerrado; 0 si no hay
uint32_t energia_historico_wh();     // desde que se instaló
uint32_t energia_despierto_s();      // segundos desde el último arranque
uint16_t energia_dias_guardados();   // cuántos días hay en el historial

// Promedio de consumo diario sobre los últimos `dias` días CERRADOS.
// Si hay menos días guardados, promedia los que haya. Devuelve 0 si no hay
// ninguno, y la pantalla debe mostrar "---" en ese caso.
uint32_t energia_promedio_wh(uint8_t dias);

// ---------------------------------------------------------------------------
// Fuerza el guardado a EEPROM. Lo llama energia_atender() solo; queda
// expuesto para el menú ("guardar ahora") y para las pruebas.
void energia_guardar();

// Borra el histórico y el historial diario. Sólo desde el menú, con
// confirmación: no hay vuelta atrás.
void energia_borrar();

// ---------------------------------------------------------------------------
// El "archivo de consumo": vuelca el historial en CSV. Llama a `escribe` una
// vez por renglón, sin \n (lo pone quien recibe). Se le pasa Serial.println
// envuelto, o cualquier cosa en las pruebas.
void energia_volcar_csv(void (*escribe)(const char *));
