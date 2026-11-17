/*
 * =============================================================================
 *  config_energia.h  -  Isla del Sol, AUX: perillas del medidor de energía
 * -----------------------------------------------------------------------------
 *  Todo lo que se elige aquí, se compila y se carga. Nada se configura desde
 *  el serie ni desde el menú.
 *
 *  El diseño y la razón de cada número están en docs/medicion_energia.md.
 *  Si cambia algo de aquí, cambie allá también.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

// ======================== EL EQUIPO =========================================
// Potencia nominal del inversor. Es la referencia del porcentaje de carga.
constexpr uint16_t POTENCIA_NOMINAL_W = 1000;

// ======================== LA MÁXIMA SOSTENIDA ===============================
// Para registrarse, la potencia debe sostenerse este tiempo. La razón no es
// técnica sino de utilidad: un refrigerador arranca en 1800 W y trabaja en
// 150; si el arranque contara, el número no diría nada de cómo se consume.
// Los picos ya los vigila el hardware (DC-016), así que la pantalla no tiene
// que hacer de detector de picos.
constexpr uint16_t MAX_SOSTENIDA_MS = 2000;

// La foto del CRÍTICO llega cada 100 ms -> 20 lecturas.
constexpr uint16_t MS_ENTRE_FOTOS = 100;
constexpr uint8_t  MUESTRAS_BLOQUE = MAX_SOSTENIDA_MS / MS_ENTRE_FOTOS;
static_assert(MUESTRAS_BLOQUE >= 2 && MUESTRAS_BLOQUE <= 100,
  "El bloque de la máxima sostenida quedó en un tamaño absurdo");

// ======================== LAS VENTANAS DE TIEMPO ============================
constexpr uint8_t  CUBETAS_24H   = 24;          // una por hora
constexpr uint32_t SEGUNDOS_HORA = 3600UL;
constexpr uint32_t SEGUNDOS_DIA  = 86400UL;

// Sin reloj de calendario no hay de otra: el mes son 30 días y el año 365.
// Queda dicho para que nadie saque cuentas contra el calendario.
constexpr uint16_t DIAS_MES = 30;
constexpr uint16_t DIAS_ANIO = 365;

// ======================== EL HISTORIAL DIARIO ===============================
// DOS historiales: lo que se consumió (modo inversor) y lo que se cargó (modo
// cargador). Un día a 1000 W continuos son 24 000 Wh, así que 2 bytes por día
// (tope 65 535) sobran.
//
// POR QUÉ 180 Y NO 365. Dos anillos de un año en Wh completo son 1460 B y la
// EEPROM tiene 1024. La alternativa era un año con 1 byte por día en pasos de
// 100 Wh, pero eso topa en 25.5 kWh/día y el equipo puede entregar 24: seis
// por ciento de margen antes de que el número se achate. Medio año menos de
// historial se paga solo con no tener que preocuparse por eso.
constexpr uint16_t DIAS_HISTORIAL = 180;

// Promedios que se muestran en la pantalla.
constexpr uint8_t PROM_CORTO = 7;
constexpr uint8_t PROM_LARGO = 30;
static_assert(PROM_LARGO <= DIAS_HISTORIAL, "El promedio largo no cabe en el historial");

// ======================== EL LADO DC: AMPERES-HORA ==========================
// El shunt + INA240 entregan I_OUT en A0 del AUX (0–3.33 V, centro 1.67 V,
// R de 1 kΩ en serie). El auxiliar lo lee DIRECTO: los Ah no necesitan
// voltaje, así que esta parte no pasa por el protocolo I2C.
//
// Por qué Ah y no Wh de este lado: la batería viene especificada en Ah, y los
// Ah son la cuenta culómbica: los amperes que entran son casi exactamente los
// que salen. Los Wh de AC dicen lo que se usó; los Ah de DC, cuánto le queda
// de vida al banco.
//
// ⚠️ CALIBRACIÓN. mA por cuenta del ADC depende de cuántos shunts estén
// montados. El README del sensor da esta tabla:
//      1 pieza  (1 mΩ)     20 mV/A
//      2 piezas (0.5 mΩ)   10 mV/A
//      3 piezas (0.333 mΩ)  6.67 mV/A   <- operación a 1000 W
// Con 4.888 mV por cuenta eso sale en 244 / 489 / 733 mA por cuenta, pero la
// tabla del README dice 0.37 / 0.73 / 1.1 A por cuenta: un factor de 1.5 que
// no se pudo reconciliar leyendo el documento. **Este número se mide en el
// banco contra una pinza de referencia antes de creerle nada al contador.**
constexpr uint16_t DC_MA_POR_CUENTA = 733;     // 3 shunts; verificar en banco
constexpr uint16_t DC_CUENTA_CENTRO = 341;     // 1.67 V de 5.00 V -> 341 cuentas
// Zona muerta alrededor del centro: por debajo de esto se toma como cero, para
// que el ruido del ADC no invente amperes-hora con el equipo apagado.
constexpr uint8_t  DC_ZONA_MUERTA_CUENTAS = 2;

// Un "mes" son 30 días de operación; el "año" es la suma de los 12 meses
// guardados, no un anillo aparte. Así no hay que definir un año distinto del
// de la pantalla DESPIERTO ni gastar EEPROM en algo que es una suma.
constexpr uint8_t MESES_HISTORIAL = 12;

// ======================== LA EEPROM =========================================
// El perfil de batería del CRÍTICO vive en la dirección 0 de SU EEPROM; ésta
// es la del AUX, pero se deja el hueco de todos modos por si alguna vez se
// comparte el mapa.
constexpr uint16_t ENER_EE_CABECERA = 16;       // 24 B de cabecera
constexpr uint16_t ENER_EE_CONSUMO  = 40;       // 180 x 2 B = 360 B
constexpr uint16_t ENER_EE_CARGA    = ENER_EE_CONSUMO + DIAS_HISTORIAL * 2;
constexpr uint16_t ENER_EE_MESES    = ENER_EE_CARGA + DIAS_HISTORIAL * 2;  // 12 x 4 B
constexpr uint8_t  ENER_EE_FIRMA    = 'E';

// Comprobación de que todo cabe en los 1024 B del ATmega328P.
static_assert(ENER_EE_MESES + MESES_HISTORIAL * 4 <= 1024,
  "Los historiales no caben en la EEPROM: baje DIAS_HISTORIAL o MESES_HISTORIAL");

// ======================== EL VOLCADO POR SERIE ==============================
// El "archivo de consumo": se pide por el menú y sale en CSV por el serie.
// 180 renglones son ~2.5 KB: 0.2 s a 115200 baudios.
#define ENER_CSV_ENCABEZADO "dia,Wh_consumidos,Wh_cargados"
