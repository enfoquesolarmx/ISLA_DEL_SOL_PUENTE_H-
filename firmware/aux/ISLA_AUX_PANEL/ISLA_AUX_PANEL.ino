/*
 * =============================================================================
 *  ISLA_AUX_PANEL.ino  -  Isla del Sol, micro AUX (A2): PRUEBA DEL PANEL
 * -----------------------------------------------------------------------------
 *  ESTADO: prueba de pantalla 16x2 + botonera + ENLACE I2C con el CRÍTICO.
 *  - La pantalla 4 (CRITICO) y la frecuencia de la pantalla 1 son DATOS
 *    REALES de la foto que manda el CRÍTICO (sketch ISLA_CRITICO, 0x20).
 *  - La batería (pantalla 2) es REAL: la mide el CRÍTICO en A3 y llega en
 *    la foto (protocolo v2). Con SIMULIDE = 1 (config_panel.h) se lee de una
 *    copia del divisor en A3 del AUX, sólo para el simulador.
 *  - Voltajes AC, corrientes y temperatura siguen siendo de MUESTRA:
 *    el CRÍTICO todavía no los mide (módulos ADC pendientes).
 *  - Sin enlace, la frecuencia sale "--" y la pantalla 4 dice SIN ENLACE.
 *
 *  Sirve igual en SimulIDE 1.1 (LCD_TIPO 1) y en el Nano real con el 16x2
 *  de mochila PCF8574 (LCD_TIPO 0). Se escoge en config_panel.h.
 *
 *  BOTONES (hardware/panel §3 y §5)
 *    B2 ARRIBA / B3 ABAJO : cambian de pantalla (1 -> 2 -> 3 -> 4 -> 1)
 *    B4 OK sostenido 3 s  : entra a SETUP
 *    En SETUP: B2/B3 mueven, B4 cambia el valor, B1 ATRÁS sale.
 *    La frecuencia 50/60 Hz se MUESTRA pero NO se cambia aquí: sólo en
 *    config_portadora.h del CRÍTICO (decisión de diseño).
 *
 *  Monitor Serie a 115200: dice qué pantalla encontró y cada evento de botón
 *  con la lectura cruda de A7 (para calibrar la escalera).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#include "config_panel.h"
#include "i2c_sw.h"
#include "lcd.h"
#include "botonera.h"
#include "enlace_aux.h"

constexpr uint16_t REFRESCO_MS = 250;

enum Vista : uint8_t { V_INVERSOR, V_BATERIA, V_TEMPERATURA, V_CRITICO, V_SETUP, V_FALLA };
constexpr uint8_t VISTAS_NORMALES = 4;
static Vista vista = V_INVERSOR;
static uint8_t itemSetup = 0;
static bool luzEncendida = true;
constexpr uint8_t ITEMS_SETUP = 3;

static char fila[17];

#if SIMULIDE
// Misma conversión que bateria.cpp del CRÍTICO (divisor 68k + 68k + 10k):
// 1023 cuentas = 73.0 V = 730 décimas. Promedio de 8 lecturas.
static uint16_t bateriaSimulide() {
  uint16_t suma = 0;
  for (uint8_t i = 0; i < 8; i++) suma += analogRead(PIN_BAT_SIMULIDE);
  const uint16_t c = (suma + 4) / 8;
  return (uint16_t)(((uint32_t)c * 730 + 511) / 1023);
}
#endif

static void dibujar() {
  // Datos de muestra que cambian poco a poco (sin float).
  const uint16_t s = (millis() / 1000) % 20;
  const uint16_t vInv = 1265 + (s % 5);           // décimas de V
  const uint16_t iAc  = 50 + s;                   // décimas de A
  const uint16_t pot  = (uint32_t)vInv * iAc / 100;
  FotoCritico foto;
  const bool hayFoto = enlace_aux_foto(&foto);
  char frec[4] = "--";
  if (hayFoto) snprintf(frec, sizeof frec, "%u", foto.frecuenciaHz);
  // Batería: del CRÍTICO por el enlace (o de A3 del AUX en SimulIDE).
  char bat[8] = "--.-";
#if SIMULIDE
  const uint16_t vBat = bateriaSimulide();
  snprintf(bat, sizeof bat, "%u.%u", vBat / 10, vBat % 10);
#else
  if (hayFoto) snprintf(bat, sizeof bat, "%u.%u", foto.bateriaDv / 10, foto.bateriaDv % 10);
#endif
  switch (vista) {
    case V_INVERSOR:
      snprintf(fila, sizeof fila, "INV %3uV  %s.0Hz", vInv / 10, frec); lcd_fila(0, fila);
      snprintf(fila, sizeof fila, "%u.%uA  %uW  CASA", iAc / 10, iAc % 10, pot);         lcd_fila(1, fila);
      break;
    case V_BATERIA:
      snprintf(fila, sizeof fila, "BAT %sV SOC--%%", bat); lcd_fila(0, fila);   // SOC: del BMS (pendiente)
      lcd_fila(1, "CARGA  +8.0A");
      break;
    case V_TEMPERATURA:
      snprintf(fila, sizeof fila, "T %uC  15V OK", 42 + s % 2); lcd_fila(0, fila);
      lcd_fila(1, "RED 125V  TN");
      break;
    case V_CRITICO:
      if (!hayFoto) {
        lcd_fila(0, "CRITICO 0x20");
        const EstadoEnlace e = enlace_aux_estado();
        lcd_fila(1, e == ENLACE_NUNCA ? "esperando..." : e == ENLACE_CONGELADO ? "CONGELADO" : "SIN ENLACE");
      } else {
        snprintf(fila, sizeof fila, "SPWM %s %sHz", foto.detenidoPorP9 ? "STOP P9" : foto.spwmActiva ? "ON" : "OFF", frec);
        lcd_fila(0, fila);
        snprintf(fila, sizeof fila, "DT %s P9 %4u", foto.dtCuentas == 8 ? "500n" : "1us ", foto.adcP9);
        lcd_fila(1, fila);
      }
      break;
    case V_SETUP:
      snprintf(fila, sizeof fila, "SETUP %u/%u", itemSetup + 1, ITEMS_SETUP); lcd_fila(0, fila);
      if (itemSetup == 0) {
        snprintf(fila, sizeof fila, "Frec %sHz script", frec);
      } else if (itemSetup == 1) {
        snprintf(fila, sizeof fila, "Luz fondo: %s", luzEncendida ? "SI" : "NO");
      } else {
        snprintf(fila, sizeof fila, "A7 = %4u cuent", botonera_adc());
      }
      lcd_fila(1, fila);
      break;
    case V_FALLA:
      lcd_fila(0, "FALLA BOTONERA");
      snprintf(fila, sizeof fila, "A7=%u corto GND", botonera_adc()); lcd_fila(1, fila);
      break;
  }
}

static void reporteEvento(const __FlashStringHelper *nombre) {
  Serial.print(F("[")); Serial.print(millis()); Serial.print(F(" ms] "));
  Serial.print(nombre); Serial.print(F("   A7 = ")); Serial.println(botonera_adc());
}

static void atender(EventoBoton e) {
  switch (e) {
    case EV_ATRAS:  reporteEvento(F("B1 ATRAS"));
      if (vista == V_SETUP || vista == V_FALLA) vista = V_INVERSOR;
      break;
    case EV_ARRIBA: reporteEvento(F("B2 ARRIBA"));
      if (vista == V_SETUP) itemSetup = (itemSetup + ITEMS_SETUP - 1) % ITEMS_SETUP;
      else if (vista < VISTAS_NORMALES) vista = (Vista)((vista + VISTAS_NORMALES - 1) % VISTAS_NORMALES);
      break;
    case EV_ABAJO:  reporteEvento(F("B3 ABAJO"));
      if (vista == V_SETUP) itemSetup = (itemSetup + 1) % ITEMS_SETUP;
      else if (vista < VISTAS_NORMALES) vista = (Vista)((vista + 1) % VISTAS_NORMALES);
      break;
    case EV_OK:     reporteEvento(F("B4 OK"));
      if (vista == V_SETUP && itemSetup == 1) { luzEncendida = !luzEncendida; lcd_luz(luzEncendida); }
      break;
    case EV_SETUP:  reporteEvento(F("B4 sostenido 3 s -> SETUP"));
      vista = V_SETUP; itemSetup = 0;
      break;
    case EV_FALLA:  reporteEvento(F("FALLA: A7 en corto a GND"));
      vista = V_FALLA;
      break;
    default: return;
  }
  dibujar();                          // respuesta inmediata al botón
}

void setup() {
  pinMode(PIN_AUX_KILL, OUTPUT); digitalWrite(PIN_AUX_KILL, HIGH);   // no matar al CRÍTICO
  pinMode(PIN_CS_CAN, OUTPUT);   digitalWrite(PIN_CS_CAN, HIGH);     // CAN deseleccionado
  Serial.begin(115200);
  Serial.println(F("=== Isla del Sol - AUX - prueba de panel 16x2 + botonera ==="));
  Serial.print(F("LCD_TIPO = ")); Serial.print(LCD_TIPO); Serial.print(F(" -> "));
  Serial.print(lcd_nombre()); Serial.print(F(" en 0x")); Serial.println(lcd_direccion(), HEX);
  Serial.println(F("Bus: SDA_LCD = D4, SCL_LCD = D5 (I2C por software)"));
  Serial.println(F("Enlace: A4/A5 (Wire, maestro) -> CRITICO 0x20, 100 kHz"));
  enlace_aux_iniciar();

  const bool ok = lcd_iniciar();
  Serial.println(ok ? F("Pantalla contesta (ACK). OK.")
                    : F("SIN ACK: revisa LCD_TIPO, direccion (0x27/0x3F/0x3E), D4/D5 y pull-ups."));
  lcd_fila(0, "ISLA DEL SOL");
  snprintf(fila, sizeof fila, "AUX panel  T=%u", LCD_TIPO); lcd_fila(1, fila);
  delay(1500);
  dibujar();
}

void loop() {
  atender(botonera_evento());
  if (enlace_aux_atender()) {
    Serial.print(F(">>> [")); Serial.print(millis()); Serial.print(F(" ms] "));
    Serial.println(enlace_aux_texto(enlace_aux_estado()));
  }
  // Resumen del enlace cada 5 s.
  static uint32_t tResumen = 0;
  if (millis() - tResumen >= 5000) {
    tResumen = millis();
    FotoCritico p;
    Serial.print(F("Enlace: tramas ")); Serial.print(enlace_aux_tramas_ok());
    Serial.print('/'); Serial.print(enlace_aux_tramas_total());
    if (enlace_aux_foto(&p)) {
      Serial.print(F(" | SPWM ")); Serial.print(p.spwmActiva ? F("activa") : F("detenida"));
      if (p.detenidoPorP9) Serial.print(F(" POR P9"));
      Serial.print(F(" | ")); Serial.print(p.frecuenciaHz); Serial.print(F(" Hz | DT "));
      Serial.print(p.dtCuentas == 8 ? F("500 ns") : F("1 us"));
      Serial.print(F(" | P9 ")); Serial.print(p.adcP9);
      Serial.print(F(" | ciclos ")); Serial.print(p.ciclos);
      Serial.print(F(" | BAT ")); Serial.print(p.bateriaDv / 10); Serial.print('.');
      Serial.print(p.bateriaDv % 10); Serial.print(F(" V | seq ")); Serial.println(p.seq);
    } else Serial.println(F(" | sin foto valida"));
#if SIMULIDE
    const uint16_t v = bateriaSimulide();
    Serial.print(F("SIMULIDE: BAT en A3 del AUX = ")); Serial.print(v / 10); Serial.print('.');
    Serial.print(v % 10); Serial.println(F(" V (copia del divisor, no viene del CRITICO)"));
#endif
  }
  static uint32_t t = 0;
  if (millis() - t >= REFRESCO_MS) { t = millis(); dibujar(); }
  // Si la pantalla se desconecta y vuelve, se reinicia sola (cada 2 s pregunta).
  static uint32_t tRevisa = 0;
  static bool presente = true;
  if (millis() - tRevisa >= 2000) {
    tRevisa = millis();
    const bool ahora = i2c_sw_hay_equipo(lcd_direccion());
    if (ahora && !presente) { Serial.println(F("Pantalla de vuelta: reiniciando")); lcd_iniciar(); lcd_luz(luzEncendida); }
    if (!ahora && presente) { Serial.println(F("Pantalla perdida (sin ACK)")); i2c_sw_recuperar(); }
    presente = ahora;
  }
}
