/*
 * =============================================================================
 *  ISLA_CRITICO.ino  -  Isla del Sol, micro CRÍTICO (A1) de la tarjeta de control
 * -----------------------------------------------------------------------------
 *  Archivo principal: sólo llama a los módulos. La lógica vive en cada
 *  archivo .h / .cpp de esta carpeta (ver README.md).
 *
 *  ESTADO: paso 3 + PRUEBA DEL ENLACE I2C (esclavo 0x20), BANCO SIN POTENCIA.
 *    Al encender:
 *      1) puente apagado: D9, D10, D11, D3 en bajo y D12 (EN_HW) = 0, con el
 *         tiempo muerto seguro (1 µs) mientras no se sepa otra cosa;
 *      2) lee P9 en A7 (16 lecturas) y elige el tiempo muerto: 500 ns SOLO si
 *         las dos ramas piden 500 ns; si no, 1 µs. Si A7 está fuera de rango
 *         (falla de hardware), NO arranca;
 *      3) reporte por el puerto serie: portadora, lectura de P9 y DT en uso;
 *      4) arranca la SPWM a FRECUENCIA_LINEA_HZ (60 o 50 Hz) y sube la amplitud
 *         en rampa hasta el máximo en ~1 s;
 *      5) vigila P9 cada 50 ms: si cambia (módulo flojo o cambiado), detiene la
 *         SPWM y no vuelve a arrancar hasta reiniciar el Nano;
 *      6) cada 10 s imprime los ciclos generados y la lectura de P9.
 *
 *    D12 SE QUEDA EN 0 TODO EL TIEMPO: las señales se ven en los pines del Nano
 *    pero la compuerta AND (74HC08) de la tarjeta de control no las deja pasar
 *    a los gate drivers. Esta versión NO puede mover el puente H.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include "config_portadora.h"
#include "config_spwm.h"
#include "spwm.h"
#include "dt_p9.h"
#include "enlace_i2c.h"

// Rampa de prueba de banco: cuentas que sube la amplitud en cada ciclo de línea.
// 313–316 cuentas / 5 por ciclo = 63 ciclos ≈ 1 s a 60 Hz (1.3 s a 50 Hz). NO es el arranque suave final
// (ése irá con el lazo de voltaje en ac_fb_inversor).
constexpr uint8_t RAMPA_BANCO_CUENTAS_POR_CICLO = 5;

// Imprime un valor en décimas, centésimas o milésimas sin usar float.
static void imprimirFijo(uint32_t v, uint16_t div) {
  Serial.print(v / div);
  Serial.print('.');
  uint32_t r = v % div;
  for (uint16_t d = div / 10; d > 0; d /= 10) { Serial.print((uint8_t)(r / d)); r %= d; }
}

static void reportePortadora() {
  Serial.println(F("=== Isla del Sol - CRITICO - SPWM hibrida (banco) ==="));
#if ISLA_CHIP_328P
  Serial.println(F("Chip: ATmega328P (Timer1 phase-correct)"));
#else
  Serial.println(F("Chip: ATmega4809 (TCA0 dual-slope)"));
#endif
  Serial.print(F("Reloj (Hz):              ")); Serial.println(RELOJ_HZ);
  Serial.print(F("TOP (cuentas):           ")); Serial.println(TOP_CUENTAS);
  Serial.print(F("Ciclos CPU por periodo:  ")); Serial.println(CICLOS_CPU_PERIODO);
  Serial.print(F("Portadora real (Hz):     ")); imprimirFijo(FSW_REAL_CHZ, 100); Serial.println();
  Serial.print(F("Frecuencia de linea:     ")); Serial.print(spwm_frecuencia_hz()); Serial.println(F(" Hz"));
  Serial.print(F("Muestras por ciclo:      ")); Serial.println(spwm_muestras_ciclo());
  Serial.print(F("Salida (Hz):             "));
  imprimirFijo(spwm_frecuencia_hz() == LINEA_50_HZ ? F_SALIDA_50_MHZ : F_SALIDA_60_MHZ, 1000);
  Serial.println();
  Serial.print(F("Error de frecuencia ppm: ")); Serial.println(ERROR_FRECUENCIA_PPM);
  Serial.print(F("ns por cuenta:           ")); imprimirFijo(NS_X10_POR_CUENTA, 10); Serial.println();
  Serial.print(F("DT 500 ns (cuentas):     ")); Serial.println(DT_MODERNO_CUENTAS);
  Serial.print(F("DT 1 us   (cuentas):     ")); Serial.println(DT_LEGACY_CUENTAS);
  Serial.print(F("Pulso minimo (cuentas):  ")); Serial.println(PULSO_MIN_CUENTAS);
  Serial.print(F("Techo de modulacion:     ")); Serial.print(M_TECHO_PORCIENTO); Serial.println(F(" %"));
  Serial.print(F("Amplitud max (cuentas):  ")); Serial.println(spwm_amplitud_max());
  Serial.print(F("Indice m maximo:         "));
  imprimirFijo((uint32_t)spwm_amplitud_max() * 1000UL / TOP_CUENTAS, 1000); Serial.println();
  Serial.print(F("DT en uso (cuentas):     ")); Serial.print(spwm_dt_cuentas());
  Serial.println(spwm_dt_cuentas() == DT_MODERNO_CUENTAS ? F("  (500 ns)") : F("  (1 us)"));
  Serial.println(F("D12 (EN_HW) = 0: puente bloqueado, prueba sin potencia"));
}

// Imprime una lectura de P9: cuentas, voltaje y lo que significa.
static void reporteP9(uint16_t adc, EstadoP9 e) {
  Serial.print(F("P9 en A7: ")); Serial.print(adc); Serial.print(F(" cuentas = "));
  imprimirFijo((uint32_t)adc * 5000UL / 1023UL, 1000); Serial.print(F(" V  -> "));
  Serial.println(p9_texto(e));
}

static bool detenidoPorP9 = false;
static EstadoP9 p9Actual = P9_AMBOS_ABIERTOS;   // lo último leído, para el enlace I2C
static uint16_t adcP9Actual = 0;

void setup() {
  // 1) Puente apagado y D12 = 0 antes que nada, con el tiempo muerto seguro.
  spwm_preparar(DT_LEGACY_CUENTAS, FRECUENCIA_LINEA_HZ);
  Serial.begin(115200);               // Monitor Serie a 115200

  // 2) P9: qué tiempo muerto piden los gate drivers.
  uint16_t adcP9 = 0;
  const EstadoP9 p9 = p9_detectar(&adcP9);
  p9Actual = p9; adcP9Actual = adcP9;
  enlace_iniciar();                   // esclavo I2C (0x20): el AUX ya puede preguntar

  // 3) Tiempo muerto según P9 (500 ns sólo si las dos ramas lo piden).
  const bool frecuenciaValida = spwm_preparar(p9_dt_cuentas(p9), FRECUENCIA_LINEA_HZ);
  reportePortadora();
  reporteP9(adcP9, p9);
  if (!frecuenciaValida) Serial.println(F("AVISO: frecuencia invalida, se usa 60 Hz"));

  if (!p9_permite_arrancar(p9)) {
    Serial.println(F("FALLA: P9 fuera de rango. La SPWM NO arranca. Revise A7, R12, R13, R14."));
    detenidoPorP9 = true;
    return;
  }
  spwm_arrancar();                    // 4) SPWM con amplitud 0
}

void loop() {
  // Vigilancia de P9: si cambia con la SPWM corriendo, se apaga y no vuelve.
  uint16_t adcP9 = 0;
  EstadoP9 p9 = p9Actual;
  const bool cambioP9 = !detenidoPorP9 && p9_vigilar(&adcP9, &p9);
  if (!detenidoPorP9 && adcP9 != 0) { p9Actual = p9; adcP9Actual = adcP9; }
  if (cambioP9) {
    spwm_detener();                   // D12 = 0 primero, salidas en bajo
    detenidoPorP9 = true;
    Serial.println(F("FALLA: P9 cambio con la SPWM corriendo (modulo flojo o cambiado)."));
    reporteP9(adcP9, p9);
    Serial.println(F("SPWM DETENIDA. Reinicie el Nano para volver a leer P9."));
  }
  enlace_atender(p9Actual, adcP9Actual, detenidoPorP9);   // I2C: comandos y foto nueva
  if (enlace_aux_cambio()) {
    Serial.print(F(">>> [")); Serial.print(millis()); Serial.print(F(" ms] "));
    Serial.println(enlace_aux_presente() ? F("AUX conectado: primer PING valido recibido")
                                         : F("AUX perdido: mas de 500 ms sin PING"));
  }
  if (detenidoPorP9) return;

  // Rampa de banco: sube la amplitud una vez por ciclo de línea.
  static uint16_t cicloAnterior = 0;
  static uint16_t amplitud = 0;
  const uint16_t c = spwm_ciclos();
  if (c != cicloAnterior) {
    cicloAnterior = c;
    if (amplitud < spwm_amplitud_max()) {
      amplitud += RAMPA_BANCO_CUENTAS_POR_CICLO;
      spwm_fijar_amplitud(amplitud);   // se recorta solo al máximo
    }
  }

  // Verificación de frecuencia: ciclos en ventanas de 10 s
  // (600 o 601 a 60 Hz; 500 o 501 a 50 Hz).
  static uint32_t t0 = millis();
  static uint16_t c0 = 0;
  if (millis() - t0 >= 10000UL) {
    t0 += 10000UL;
    Serial.print(F("Ciclos en 10 s: "));
    Serial.print((uint16_t)(c - c0));
    Serial.print(F("   |   "));
    const uint16_t a = analogRead(A7);
    reporteP9(a, p9_clasificar(a));
    Serial.print(F("I2C: PING validos ")); Serial.print(enlace_comandos_ok());
    Serial.print(F(", con CRC malo ")); Serial.print(enlace_comandos_malos());
    Serial.print(F(", ultimo seq ")); Serial.println(enlace_ultimo_seq());
    c0 = c;
  }
}
