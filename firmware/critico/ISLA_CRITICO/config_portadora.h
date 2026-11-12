/*
 * =============================================================================
 *  config_portadora.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  Variables principales de la portadora del SPWM
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Define la frecuencia de conmutación (portadora), las muestras de la tabla
 *    seno, el TOP del timer y el tiempo muerto en cuentas. Todo lo derivado se
 *    calcula al compilar a partir de F_CPU; no ocupa RAM ni tiempo de CPU.
 *    Este archivo NO toca registros: sólo números. Los timers se configuran
 *    en spwm_328p.cpp / spwm_4809.cpp (capa por chip, DC-018).
 *
 *  HARDWARE AL QUE CORRESPONDE
 *    Tarjeta de control (hardware/control), micro CRÍTICO en zócalo de Nano:
 *      D9  -> HA  (lado alto, pierna A, 24 kHz)   J5/J9/J10 pin 1 del módulo
 *      D10 -> LA  (lado bajo, pierna A, 24 kHz)   J5/J9/J10 pin 5 del módulo
 *      D11 -> HB  (lado alto, pierna B, 60 Hz)
 *      D3  -> LB  (lado bajo, pierna B, 60 Hz)
 *    Puente H (hardware/puente_h): pierna A = Q1/Q2, pierna B = Q3/Q4.
 *    Tiempo muerto según P9 del módulo gate driver (docs/identificacion_dt_pin9.md):
 *      P9 a GND  -> 500 ns  (IRS2186, UCC21520DW)
 *      P9 abierto -> 1 µs   (TLP250H, IR2110 trinchera, o falla: valor seguro)
 *
 *  CHIPS SOPORTADOS (zócalo multimodelo, DC-018)
 *    ATmega328P a 16 MHz (Nano):        Timer1 phase-correct, TOP = 333
 *    ATmega4809 a 20 MHz (Nano Every):  TCA0 dual-slope,      PER = 416
 *
 *  RECURSOS: ninguno (sólo constantes).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// ============================ CHIP Y RELOJ ===================================
#if defined(__AVR_ATmega328P__)
  #define ISLA_CHIP_328P 1
  constexpr uint32_t RELOJ_ESPERADO_HZ = 16000000UL;
#elif defined(__AVR_ATmega4809__)
  #define ISLA_CHIP_4809 1
  constexpr uint32_t RELOJ_ESPERADO_HZ = 20000000UL;   // MegaCoreX a 20 MHz
#else
  #error "Chip no soportado: el CRÍTICO es ATmega328P (Nano) o ATmega4809 (Nano Every)"
#endif

#ifndef F_CPU
  #error "F_CPU no definido: seleccione la placa correcta en el IDE"
#endif

constexpr uint32_t RELOJ_HZ = F_CPU;
static_assert(RELOJ_HZ == RELOJ_ESPERADO_HZ,
  "Reloj distinto del previsto: 328P a 16 MHz / 4809 a 20 MHz (MegaCoreX). "
  "Revise la placa seleccionada en el IDE");

// ======================== PARÁMETROS DE DISEÑO ===============================
// Únicos valores que se eligen a mano. Todo lo demás se deriva.

// ---------------- FRECUENCIA DE LÍNEA: 60 Hz o 50 Hz -------------------------
//   60 Hz: México, Centroamérica, Colombia, Venezuela, Ecuador, Perú,
//          Brasil (la mayor parte), Estados Unidos, Canadá...
//   50 Hz: casi toda África, Europa, Medio Oriente, Argentina, Chile,
//          Uruguay, Paraguay, Bolivia, gran parte de Asia...
//   (Confirme siempre con la placa de la red o del generador del lugar.)
//
// El mismo firmware trae las DOS frecuencias. Aquí se elige la que usa el
// equipo al encender; el valor se pasa a spwm_preparar(). Más adelante se
// podrá elegir desde el menú SETUP del panel (AUX, guardado en EEPROM) y, con
// red presente, se tomará la de la red medida (ac_fb_grid).
//
// ⚠ CAMBIAR A 50 Hz NO BASTA CON EL FIRMWARE. Antes, revisar el hardware:
//   - TRANSFORMADOR: a 50 Hz el núcleo trabaja con 20 % MÁS FLUJO que a 60 Hz
//     (flujo ∝ V / f). Un transformador de 60 Hz puede saturarse y calentarse:
//     debe estar especificado para 50 Hz (o "50/60 Hz").
//   - VOLTAJE: casi todas las redes de 50 Hz son de 220–240 V, no de 127 V.
//     El transformador de 48 V -> 127 V NO sirve: hace falta otra relación de
//     vueltas (y relés, capacitores y fusibles de salida para 250 V).
//   Ver README.md, sección "Frecuencia de línea: 50 o 60 Hz".
constexpr uint8_t LINEA_60_HZ = 60;
constexpr uint8_t LINEA_50_HZ = 50;
constexpr uint8_t FRECUENCIA_LINEA_HZ = LINEA_60_HZ;   // <-- 60 o 50

static_assert(FRECUENCIA_LINEA_HZ == LINEA_60_HZ || FRECUENCIA_LINEA_HZ == LINEA_50_HZ,
  "FRECUENCIA_LINEA_HZ sólo puede ser 60 o 50");

// ---------------- PORTADORA: 24 kHz FIJOS EN LAS DOS FRECUENCIAS -------------
// Sincronizada con la línea: múltiplo exacto de 60 y de 50 Hz
//   60 Hz: 24 000 / 60 = 400 periodos por ciclo
//   50 Hz: 24 000 / 50 = 480 periodos por ciclo
// (docs/modulacion_spwm.md, sección 4.3). Cada ciclo tiene un número exacto de
// periodos y el cruce por cero cae siempre en el mismo punto: sin subarmónicos.
//
// POR QUÉ NO SE BAJA LA PORTADORA A 20 kHz EN 50 Hz (400 muestras, 50.000 Hz
// exactos): con 24 kHz fijos NO cambian el TOP, el tiempo muerto en cuentas,
// el pulso mínimo, la amplitud máxima, el rizo del filtro de salida, las
// pérdidas de conmutación ni el ruido. Sólo cambian el número de muestras y
// la tabla. A 20 kHz el rizo del filtro sube 20 % y el zumbido queda en el
// borde de lo audible. El precio: la salida queda en 50.05 Hz (+0.1 %), igual
// que la de 60 Hz queda en 60.06 Hz.
constexpr uint32_t FSW_OBJETIVO_HZ = 24000UL;

// Tiempos muertos que puede pedir el módulo gate driver por P9.
// No cambiar sin actualizar docs/identificacion_dt_pin9.md.
constexpr uint16_t DT_MODERNO_NS = 500;    // P9 = GND
constexpr uint16_t DT_LEGACY_NS  = 1000;   // P9 abierto o falla (valor seguro)

// ======================== MUESTRAS DE LA TABLA SENO ==========================
// Una muestra por periodo de portadora:
//                        60 Hz      50 Hz
//   por ciclo             400        480
//   por semiciclo         200        240
//   por cuarto de onda    100        120   <- lo que se guarda en flash
//
// POR QUÉ SÓLO SE GUARDA UN CUARTO DE ONDA
//   La senoidal es simétrica. Con el tramo de 0° a 90° se arma el ciclo entero
//   (S = muestras por semiciclo, 200 a 60 Hz o 240 a 50 Hz):
//
//     muestra k del semiciclo (0..S-1)     se lee de la tabla
//     ---------------------------------    ------------------------------
//     0 .. S/2-1   (0° a 90°,   subida)    tabla[k]          tal cual
//     S/2 .. S-1   (90° a 180°, bajada)    tabla[S-1 - k]    en espejo
//     segundo semiciclo (180° a 360°)      los mismos valores; la polaridad
//                                          la pone la PIERNA B, no la tabla
//
//   En la modulación híbrida la pierna A sólo necesita la MAGNITUD |sen θ|:
//   el signo lo da la pierna B al cambiar de lado en el cruce por cero.
//   Por eso nunca se guardan valores negativos.
//
//   Lo que cuesta cada opción a 60 Hz (2 bytes por muestra):
//     tabla completa  400 muestras = 800 B
//     semiciclo       200 muestras = 400 B
//     cuarto de onda  100 muestras = 200 B   <- la elegida
//   Las dos tablas (60 y 50 Hz) juntas ocupan 440 B de flash.
//   El espejo cuesta 1 comparación + 1 resta en la ISR: unos 3-4 ciclos de
//   los 666 que hay por periodo (~0.5 %). Se ahorra flash casi gratis.
//
//   Las tablas van en FLASH (PROGMEM), no en RAM. El 328P tiene sólo 2 KB de
//   RAM: una tabla completa calculada en RAM al arrancar se comería el 40 %.
//
// DETALLE QUE NO SE DEBE CAMBIAR: MUESTRA EN EL CENTRO DEL PERIODO
//   Cada muestra se toma a la mitad de su periodo de portadora:
//       tabla[k] = sen( (k + 0.5) × 180° / S )      <- correcto
//   y NO en el borde:
//       tabla[k] = sen(  k        × 180° / S )      <- mal
//   Tomada al centro, la muestra k y la S-1 - k valen exactamente lo mismo y el
//   espejo sale perfecto. En el borde, el pico de 90° queda en una sola muestra
//   sin pareja: el espejo se corre una muestra, la onda sale un poco asimétrica
//   y aparece componente par (distorsión que se puede evitar sin costo).
//
//   Por eso las muestras por ciclo deben ser múltiplo de 4 (ver static_assert
//   abajo): 400 y 480 se parten en cuatro cuartos exactos (100 y 120).
constexpr uint16_t MUESTRAS_CICLO_60     = FSW_OBJETIVO_HZ / LINEA_60_HZ;   // 400
constexpr uint16_t MUESTRAS_SEMICICLO_60 = MUESTRAS_CICLO_60 / 2;           // 200
constexpr uint16_t MUESTRAS_CUARTO_60    = MUESTRAS_CICLO_60 / 4;           // 100
constexpr uint16_t MUESTRAS_CICLO_50     = FSW_OBJETIVO_HZ / LINEA_50_HZ;   // 480
constexpr uint16_t MUESTRAS_SEMICICLO_50 = MUESTRAS_CICLO_50 / 2;           // 240
constexpr uint16_t MUESTRAS_CUARTO_50    = MUESTRAS_CICLO_50 / 4;           // 120

static_assert(FSW_OBJETIVO_HZ % LINEA_60_HZ == 0 && FSW_OBJETIVO_HZ % LINEA_50_HZ == 0,
  "La portadora debe ser múltiplo entero de 60 Hz y de 50 Hz");
static_assert(MUESTRAS_CICLO_60 % 4 == 0 && MUESTRAS_CICLO_50 % 4 == 0,
  "Las muestras por ciclo deben ser múltiplo de 4 (tabla de un cuarto de onda)");
static_assert(MUESTRAS_SEMICICLO_50 <= 255 && MUESTRAS_SEMICICLO_60 <= 255,
  "El índice de muestra de la ISR es de 8 bits");

// ======================== TOP DEL TIMER ======================================
// Modo de doble pendiente (phase-correct en el 328P, dual-slope en el 4809):
// el contador sube 0 -> TOP y baja TOP -> 0, una cuenta por ciclo de reloj.
//   f_portadora = RELOJ / (2 × TOP)   ->   TOP = RELOJ / (2 × f_portadora)
// Se trunca (división entera): la portadora queda un poco ARRIBA del objetivo.
//   328P: 16 MHz / 48 000 = 333.33 -> 333  -> 24 024.0 Hz
//   4809: 20 MHz / 48 000 = 416.67 -> 416  -> 24 038.5 Hz
// Doble pendiente: pulsos centrados en el periodo, simétricos; el tiempo
// muerto se obtiene desplazando el comparador del lado bajo (ver abajo).
constexpr uint16_t TOP_CUENTAS = (uint16_t)(RELOJ_HZ / (2UL * FSW_OBJETIVO_HZ));

// Ciclos de CPU disponibles en cada periodo de portadora (presupuesto de la ISR).
// 328P: 666 ciclos; 4809: 832 ciclos. La ISR del SPWM debe usar una fracción
// pequeña (DC-017 estima 80–120 ciclos). Medir en banco con un pin de prueba.
constexpr uint16_t CICLOS_CPU_PERIODO = (uint16_t)(2UL * TOP_CUENTAS);

// ======================== FRECUENCIAS REALES =================================
// En centésimas de Hz para no usar float (se imprimen en el reporte de arranque).
//   FSW_REAL_CHZ: 328P = 2 402 402 (24 024.02 Hz); 4809 = 2 403 846 (24 038.46 Hz)
constexpr uint32_t FSW_REAL_CHZ = (uint32_t)((RELOJ_HZ * 100ULL) / (2UL * TOP_CUENTAS));

// Salida en milésimas de Hz (portadora real / muestras por ciclo):
//            60 Hz (400)              50 Hz (480)
//   328P:    60 060 (60.060 Hz)       50 050 (50.050 Hz)     +0.10 %
//   4809:    60 096 (60.096 Hz)       50 080 (50.080 Hz)     +0.16 %
// En modo isla esto basta: el error queda dentro de lo que toleran cargas,
// relojes y motores. En modo red, el PLL ajusta el TOP para seguir a la red
// (ac_fb_grid, pendiente).
constexpr uint32_t F_SALIDA_60_MHZ = (uint32_t)((RELOJ_HZ * 1000ULL) /
                                                (2ULL * TOP_CUENTAS * MUESTRAS_CICLO_60));
constexpr uint32_t F_SALIDA_50_MHZ = (uint32_t)((RELOJ_HZ * 1000ULL) /
                                                (2ULL * TOP_CUENTAS * MUESTRAS_CICLO_50));

// Error de frecuencia en partes por millón (positivo = más rápida que la nominal).
// Es el mismo en 60 y 50 Hz: sólo depende de la portadora real.
constexpr int32_t ERROR_FRECUENCIA_PPM =
    (int32_t)(((int64_t)F_SALIDA_60_MHZ - (int64_t)LINEA_60_HZ * 1000) * 1000000LL /
              ((int64_t)LINEA_60_HZ * 1000));
constexpr int32_t ERROR_FRECUENCIA_50_PPM =
    (int32_t)(((int64_t)F_SALIDA_50_MHZ - (int64_t)LINEA_50_HZ * 1000) * 1000000LL /
              ((int64_t)LINEA_50_HZ * 1000));

static_assert(FSW_REAL_CHZ >= 2000000UL && FSW_REAL_CHZ <= 2500000UL,
  "Portadora fuera de 20–25 kHz: revise RELOJ_HZ y FSW_OBJETIVO_HZ");
static_assert(ERROR_FRECUENCIA_PPM >= -5000 && ERROR_FRECUENCIA_PPM <= 5000 &&
              ERROR_FRECUENCIA_50_PPM >= -5000 && ERROR_FRECUENCIA_50_PPM <= 5000,
  "La frecuencia de salida se aleja más de 0.5 % de la nominal");

// ======================== RESOLUCIÓN Y TIEMPO MUERTO =========================
// Cada cuenta del comparador desplaza cada flanco un ciclo de reloj:
//   328P: 62.5 ns;  4809: 50 ns.  Se guarda en décimas de ns.
constexpr uint16_t NS_X10_POR_CUENTA = (uint16_t)(10000000000ULL / RELOJ_HZ);

// Conversión de ns a cuentas, redondeando HACIA ARRIBA (nunca menos tiempo
// muerto del pedido).  500 ns -> 8 / 10 cuentas;  1 µs -> 16 / 20 cuentas.
#define ISLA_NS_A_CUENTAS(ns) \
  ((uint8_t)((((uint64_t)(ns)) * RELOJ_HZ + 999999999ULL) / 1000000000ULL))

constexpr uint8_t DT_MODERNO_CUENTAS = ISLA_NS_A_CUENTAS(DT_MODERNO_NS);
constexpr uint8_t DT_LEGACY_CUENTAS  = ISLA_NS_A_CUENTAS(DT_LEGACY_NS);

// Lado alto (OCR1A / CMP0) normal; lado bajo (OCR1B / CMP1) invertido con
//   comparador_bajo = comparador_alto + DT_cuentas
// En doble pendiente eso abre un hueco de DT_cuentas en CADA flanco.
// Para que el comparador del lado bajo nunca pase de TOP, el del lado alto
// se limita a TOP - DT del peor caso (legacy):
constexpr uint16_t COMPARADOR_ALTO_MAX = TOP_CUENTAS - DT_LEGACY_CUENTAS;   // 317 / 396

// Fracción del periodo que se lleva el tiempo muerto (en milésimas):
//   500 ns -> 12 (1.2 %);  1 µs -> 24 (2.4 %).  Coincide con modulacion_spwm.md.
constexpr uint16_t DT_MODERNO_MILESIMAS =
    (uint16_t)((uint32_t)DT_MODERNO_CUENTAS * 1000UL / CICLOS_CPU_PERIODO);
constexpr uint16_t DT_LEGACY_MILESIMAS  =
    (uint16_t)((uint32_t)DT_LEGACY_CUENTAS * 1000UL / CICLOS_CPU_PERIODO);

static_assert(DT_LEGACY_CUENTAS > DT_MODERNO_CUENTAS,
  "El tiempo muerto legacy debe ser mayor que el moderno (regla de P9)");
static_assert((uint32_t)DT_MODERNO_CUENTAS * NS_X10_POR_CUENTA >= (uint32_t)DT_MODERNO_NS * 10,
  "El tiempo muerto moderno en cuentas quedó por debajo de 500 ns");
static_assert((uint32_t)DT_LEGACY_CUENTAS * NS_X10_POR_CUENTA >= (uint32_t)DT_LEGACY_NS * 10,
  "El tiempo muerto legacy en cuentas quedó por debajo de 1 µs");
static_assert(DT_LEGACY_CUENTAS * 10 < TOP_CUENTAS,
  "El tiempo muerto ocupa más del 10 % del semiperiodo: revise la portadora");
