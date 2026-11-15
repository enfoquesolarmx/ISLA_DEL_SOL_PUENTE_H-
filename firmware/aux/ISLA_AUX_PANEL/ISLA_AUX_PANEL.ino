/*
 * =============================================================================
 *  ISLA_AUX_PANEL.ino  -  Isla del Sol, micro AUX (A2): PRUEBA DEL PANEL
 * -----------------------------------------------------------------------------
 *  ESTADO: prueba de pantalla 16x2 + botonera, SIN enlace con el CRÍTICO.
 *  Los números de las pantallas son de MUESTRA (se mueven solos para ver que
 *  la pantalla refresca). Cuando el panel quede validado, los datos saldrán
 *  de la foto de estado que manda el CRÍTICO por I2C.
 *
 *  Sirve igual en SimulIDE 1.1 (LCD_TIPO 1) y en el Nano real con el 16x2
 *  de mochila PCF8574 (LCD_TIPO 0). Se escoge en config_panel.h.
 *
 *  BOTONES (hardware/panel §3 y §5)
 *    B2 ARRIBA / B3 ABAJO : cambian de pantalla (1 -> 2 -> 3 -> 1)
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

constexpr uint8_t FRECUENCIA_MUESTRA_HZ = 60;   // de muestra; luego vendrá del CRÍTICO
constexpr uint16_t REFRESCO_MS = 250;

enum Vista : uint8_t { V_INVERSOR, V_BATERIA, V_TEMPERATURA, V_SETUP, V_FALLA };
static Vista vista = V_INVERSOR;
static uint8_t itemSetup = 0;
static bool luzEncendida = true;
constexpr uint8_t ITEMS_SETUP = 3;

static char fila[17];

static void dibujar() {
  // Datos de muestra que cambian poco a poco (sin float).
  const uint16_t s = (millis() / 1000) % 20;
  const uint16_t vInv = 1265 + (s % 5);           // décimas de V
  const uint16_t iAc  = 50 + s;                   // décimas de A
  const uint16_t pot  = (uint32_t)vInv * iAc / 100;
  const uint16_t vBat = 512 - (s % 3);            // décimas de V
  const uint8_t  soc  = 78;
  switch (vista) {
    case V_INVERSOR:
      snprintf(fila, sizeof fila, "INV %3uV  %2u.0Hz", vInv / 10, FRECUENCIA_MUESTRA_HZ); lcd_fila(0, fila);
      snprintf(fila, sizeof fila, "%u.%uA  %uW  CASA", iAc / 10, iAc % 10, pot);         lcd_fila(1, fila);
      break;
    case V_BATERIA:
      snprintf(fila, sizeof fila, "BAT %u.%uV SOC%u%%", vBat / 10, vBat % 10, soc); lcd_fila(0, fila);
      lcd_fila(1, "CARGA  +8.0A");
      break;
    case V_TEMPERATURA:
      snprintf(fila, sizeof fila, "T %uC  15V OK", 42 + s % 2); lcd_fila(0, fila);
      lcd_fila(1, "RED 125V  TN");
      break;
    case V_SETUP:
      snprintf(fila, sizeof fila, "SETUP %u/%u", itemSetup + 1, ITEMS_SETUP); lcd_fila(0, fila);
      if (itemSetup == 0) {
        snprintf(fila, sizeof fila, "Frec %uHz script", FRECUENCIA_MUESTRA_HZ);
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
      else if (vista <= V_TEMPERATURA) vista = (Vista)((vista + 2) % 3);
      break;
    case EV_ABAJO:  reporteEvento(F("B3 ABAJO"));
      if (vista == V_SETUP) itemSetup = (itemSetup + 1) % ITEMS_SETUP;
      else if (vista <= V_TEMPERATURA) vista = (Vista)((vista + 1) % 3);
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
