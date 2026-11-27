/*
 * =============================================================================
 *  prueba_rele.cpp  -  medir K4: orden a contacto, dispersion y en caliente
 *  Ver prueba_rele.h: por que, como se cablea y que se saca de la captura.
 * =============================================================================
 */

#include <Arduino.h>
#include <avr/io.h>

#include "prueba_rele.h"
#include "traza_cruce.h"

/*  Mientras la prueba manda D4, la traza de cruces lo suelta. Una variable y
 *  no un #define, porque esto se prende y se apaga desde el teclado.
 */
extern "C" { volatile unsigned char cruceTrazaOn = 1; }

enum { APAGADO = 0, TANDA, REMOJO };

static uint8_t  modo      = APAGADO;
static uint8_t  op        = 0;       /* operaciones hechas en la tanda   */
static bool     pegado    = false;
static uint32_t tCambio   = 0;       /* millis del ultimo cambio de D4   */
static uint32_t tRemojo   = 0;
static uint32_t tAviso    = 0;

static inline void pin_pega(void)   { PORTD |=  _BV(RELE_BIT); }
static inline void pin_suelta(void) { PORTD &= ~_BV(RELE_BIT); }

void rele_preparar(void)
{
  pin_suelta();
  DDRD |= _BV(RELE_BIT);
  modo = APAGADO; op = 0; pegado = false;
  cruceTrazaOn = 1;
}

static void soltar_el_pin(void)
{
  pin_suelta();
  modo = APAGADO;
  cruceTrazaOn = 1;            /* D4 vuelve a llevar los cruces de la red */
  Serial.println(F("\n>>> RELE: prueba terminada. D4 vuelve a los cruces."));
}

bool rele_tecla(char c)
{
  if (c == 'r' || c == 'R') {
    if (modo != APAGADO) { soltar_el_pin(); return true; }

    cruceTrazaOn = 0;          /* la traza suelta D4 ANTES de tomarlo */
    modo    = TANDA;
    op      = 0;
    pegado  = false;
    pin_suelta();
    tCambio = millis();
    Serial.println(F("\n================================================"));
    Serial.println(F(">>> RELE K4: tanda de 30 operaciones"));
    Serial.println(F("    !! J15 CONECTADO y ETAPA DE POTENCIA FUERA."));
    Serial.println(F("    CH1 = D4 (orden)   CH2 = contacto seco de K4"));
    Serial.println(F("    pegado 300 ms / suelto 700 ms"));
    Serial.println(F("================================================"));
    return true;
  }

  if (c == 'c' || c == 'C') {
    if (modo == REMOJO) {
      const uint32_t s = (millis() - tRemojo) / 1000u;
      pin_suelta();
      modo = APAGADO;
      cruceTrazaOn = 1;
      Serial.print(F("\n>>> RELE: remojo terminado, "));
      Serial.print(s); Serial.println(F(" s con la bobina pegada."));
      Serial.println(F("    CORRER 'R' DE INMEDIATO, antes de que enfrie."));
      Serial.println(F("    Y medir la resistencia de la bobina ahora:"));
      Serial.println(F("    T_cal = (R_cal/R_frio) x (234.5 + T_frio) - 234.5"));
      return true;
    }
    if (modo != APAGADO) return false;

    cruceTrazaOn = 0;
    modo    = REMOJO;
    tRemojo = millis();
    tAviso  = tRemojo;
    pin_pega();
    Serial.println(F("\n>>> RELE: remojo caliente. Bobina pegada."));
    Serial.println(F("    Dejarla 10 min. 'C' otra vez para soltar."));
    return true;
  }

  return false;
}

void rele_atender(void)
{
  if (modo == APAGADO) return;

  const uint32_t ahora = millis();

  if (modo == REMOJO) {
    if (ahora - tAviso >= 30000u) {
      tAviso = ahora;
      Serial.print(F("    remojo: "));
      Serial.print((ahora - tRemojo) / 1000u);
      Serial.println(F(" s"));
    }
    return;
  }

  /*  TANDA. El pin se mueve PRIMERO y se imprime despues: lo que se esta
   *  midiendo es cuando cambia el pin, no cuando llega el texto.
   */
  const uint32_t espera = pegado ? RELE_PEGADO_MS : RELE_SUELTO_MS;
  if (ahora - tCambio < espera) return;

  tCambio = ahora;

  if (!pegado) {
    pin_pega();
    pegado = true;
    op++;
    Serial.print(F("    op ")); Serial.print(op);
    Serial.print('/'); Serial.print(RELE_OPERACIONES);
    Serial.println(F("  PEGA"));
  } else {
    pin_suelta();
    pegado = false;
    if (op >= RELE_OPERACIONES) {
      Serial.println(F("\n>>> RELE: 30 operaciones hechas."));
      Serial.println(F("    De la captura, por cada una:"));
      Serial.println(F("      PEGUE  = D4 sube  -> CH2 baja   <- el que importa"));
      Serial.println(F("      SUELTE = D4 baja  -> CH2 sube"));
      Serial.println(F("      REBOTE = castaneteo de CH2 tras el primer toque"));
      Serial.println(F("    Anotar minimo, maximo y promedio de los 30."));
      soltar_el_pin();
    }
  }
}

bool rele_corriendo(void) { return modo != APAGADO; }
