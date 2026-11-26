/*
 * =============================================================================
 *  spwm.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  SPWM híbrida (unipolar con una pierna a frecuencia de línea): interfaz
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Genera la senoidal de 60 Hz en el puente H con modulación HÍBRIDA
 *    (docs/modulacion_spwm.md):
 *      - Pierna A (Q1/Q2, D9 = HA, D10 = LA): par complementario a 24 kHz con
 *        tiempo muerto, ancho de pulso = amplitud × |sen θ|.
 *      - Pierna B (Q3/Q4, D11 = HB, D3 = LB): un interruptor fijo por semiciclo
 *        (a la frecuencia de línea, 60 o 50 Hz);
 *        define la polaridad.
 *
 *          semiciclo +  :  LB encendido,  HA modula,  LA complementario
 *          semiciclo -  :  HB encendido,  LA modula,  HA complementario
 *
 *    Frecuencia de línea de 60 Hz o 50 Hz, elegida al preparar (portadora
 *    fija de 24 kHz; 400 o 480 periodos por ciclo).
 *
 *  QUIÉN ES DUEÑO DE QUÉ
 *    Implementación por chip (DC-018):
 *      spwm_328p.cpp : Timer1 (completo) e interrupción TIMER1_OVF.
 *      spwm_4809.cpp : TCA0 (pendiente).
 *    Pines: D9, D10, D11, D3 y D12 (EN_HW, permiso del puente).
 *    NINGÚN otro módulo debe tocar estos timers ni pines.
 *
 *  PERMISO DEL PUENTE (D12 = EN_HW, docs/corte_por_hardware.md)
 *    Este módulo NUNCA pone D12 en 1. Con D12 = 0, la compuerta AND (74HC08)
 *    bloquea las 4 señales: se pueden ver en los pines del Nano pero no llegan
 *    a los gate drivers. Subir D12 es decisión del arranque (con batería,
 *    P9, AUX y sensor verificados). spwm_detener() SÍ lo baja, y lo baja
 *    PRIMERO, antes de tocar el timer (corta en nanosegundos).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include "config_spwm.h"

// Prepara el timer y deja el puente apagado (pierna A y B en bajo, D12 = 0).
//   dt_cuentas:   tiempo muerto pedido por P9 (DT_MODERNO_CUENTAS o
//                 DT_LEGACY_CUENTAS). Cualquier otro valor -> LEGACY (seguro).
//   frecuencia_hz: LINEA_60_HZ o LINEA_50_HZ. La portadora sigue en 24 kHz;
//                 sólo cambian las muestras (400 / 480) y la tabla.
//                 Cualquier otro valor -> 60 Hz y spwm_preparar devuelve false.
// La frecuencia sólo se puede cambiar con la SPWM detenida (aquí se detiene).
bool spwm_preparar(uint8_t dt_cuentas, uint8_t frecuencia_hz);

// Arranca la modulación con amplitud 0. El primer periodo útil es el inicio
// de un semiciclo positivo. La amplitud se sube después con spwm_fijar_amplitud().
void spwm_arrancar();

// Apagado inmediato: D12 = 0, salidas desconectadas del timer y en bajo,
// timer detenido. Se puede llamar desde una interrupción.
void spwm_detener();

// Amplitud en cuentas (0 .. spwm_amplitud_max(); se recorta si es mayor).
// Se aplica al inicio del siguiente ciclo de 60 Hz, nunca a la mitad:
// así los dos semiciclos de un ciclo tienen siempre la misma amplitud y
// no aparece componente de CD en el transformador.
void spwm_fijar_amplitud(uint16_t cuentas);

// Ciclos completos de línea (60 o 50 Hz) generados desde el arranque (cuenta circular).
uint16_t spwm_ciclos();

// true mientras la modulación está corriendo.
bool spwm_activo();

// Tiempo muerto en uso, en cuentas.
uint8_t spwm_dt_cuentas();

// Amplitud máxima en cuentas con el tiempo muerto en uso: la menor entre el
// techo absoluto del 95 % y el límite físico (ver config_spwm.h).
//   328P: 316 con DT de 500 ns, 313 con DT de 1 µs.
uint16_t spwm_amplitud_max();

// Frecuencia de línea en uso (60 o 50) y muestras por ciclo (400 o 480).
uint8_t  spwm_frecuencia_hz();
uint16_t spwm_muestras_ciclo();

/* -------------------------------------------------- detector de fase -------
 *  Sello de 4 us del ultimo cruce por cero del inversor, y un contador que
 *  cambia en cada cruce. Restando contra el sello del cruce de la red sale la
 *  diferencia de fase con 0.086 grados de resolucion — siete veces mas fina
 *  que el jitter medido de 0.59.
 */
/*  LA FASE, MEDIDA CONTRA TIMER1 (ver el .cpp para el porque).
 *
 *  `spwm_fase_cuentas()` dice EN QUE PUNTO DE NUESTRO CICLO estamos ahora, de
 *  0 a `spwm_fase_vuelta()`. Llamada desde la ISR del comparador da
 *  directamente la fase del cruce de la red: nuestro cruce es la posicion 0
 *  por construccion, asi que no hay resta ni segundo reloj.
 *
 *  Resolucion: 62.5 ns = 0.0014 grados. Y no puede perder cuentas, porque es
 *  el mismo timer que genera la onda.
 */
uint32_t spwm_fase_cuentas(void);
uint32_t spwm_fase_vuelta(void);

uint16_t spwm_t_cruce(void);
uint8_t  spwm_n_cruce(void);

/* ------------------------------------- volante de frecuencia (PLL) --------
 *  Fija el TOP de la portadora con dither de 8 bits. Mueve la frecuencia de
 *  linea sin tocar la tabla: 0.18 Hz por cuenta entera, 0.70 mHz por paso de
 *  fraccion. Las 25 muestras por semiciclo NO se mueven, porque el ADC
 *  tambien cuelga de la portadora.
 *
 *  !! Al subir el TOP baja el ciclo de trabajo relativo, o sea la amplitud:
 *  -3.3 % en el extremo del rango, 0.1 % en enganche normal. NO se compensa
 *  desde el PLL a proposito (ver pll_red.h): lo absorbe el lazo de voltaje de
 *  salida, que mide el RMS real. Compensarlo dos veces es pelearse con el.
 */
void     spwm_top_fijar(uint16_t entero, uint8_t frac);
uint16_t spwm_top_actual(void);
