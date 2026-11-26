/*
 * =============================================================================
 *  est_rms.h  -  Isla del Sol, protocolo de banco Parte C, etapa 4
 *  Estadística de las ventanas de RMS, por reporte
 * -----------------------------------------------------------------------------
 *  ⚠️ POR QUÉ ESTO ESTÁ EN UN ARCHIVO APARTE Y NO EN EL .ino
 *
 *  El IDE de Arduino, antes de compilar, **inserta los prototipos de todas
 *  las funciones del `.ino` arriba del archivo** — después de los `#include`
 *  y antes de cualquier otra cosa. Si una de esas funciones recibe un tipo
 *  declarado más abajo, el prototipo nombra un tipo que todavía no existe:
 *
 *      error: variable or field 'est_cero' declared void
 *      error: 'EstRms' was not declared in this scope
 *
 *  El IDE sólo genera prototipos para lo que está en archivos `.ino`. Con el
 *  tipo y sus funciones en un `.h`, el problema no puede ocurrir.
 *
 *  Compilar el `.cpp` a mano NO reproduce esto, así que la verificación de
 *  este proyecto ahora inserta los prototipos igual que el IDE antes de
 *  compilar. Es la segunda vez que una diferencia entre mi compilación y la
 *  del IDE cuesta una corrida de banco; la primera fue el
 *  `multiple definition of __vector_13`.
 * =============================================================================
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "rms_ventana.h"     // por rms_isqrt32()

/*  Estadística de las ventanas de RMS, por reporte. Se arma en el lazo: las
 *  ventanas cierran 120 veces por segundo y el lazo gira más de 10 000, así
 *  que no se pierde ninguna — y si se perdiera, `vent` lo delata contra las
 *  120 que deberían ser.
 */
struct EstRms {
  uint16_t vent;        // ventanas recogidas en el reporte
  uint32_t suma;        // Σ de los RMS en cuentas
  uint64_t sumaCuad;    // Σ de los cuadrados, para la σ
  uint16_t minimo, maximo;
  uint16_t cdUlt;       // el CD de la última ventana: debe rondar 511
  uint8_t  nUlt;        // muestras de la última ventana: 25 en el inversor
};
static EstRms est[2];

static void est_cero(EstRms &e)
{
  e.vent = 0; e.suma = 0; e.sumaCuad = 0;
  e.minimo = 0xFFFF; e.maximo = 0; e.cdUlt = 0; e.nUlt = 0;
}

static void est_mete(EstRms &e, uint16_t r, uint16_t cd, uint8_t n)
{
  e.vent++;
  e.suma     += r;
  e.sumaCuad += (uint32_t)r * r;
  if (r < e.minimo) e.minimo = r;
  if (r > e.maximo) e.maximo = r;
  e.cdUlt = cd;
  e.nUlt  = n;
}

// Promedio y desviación estándar, en cuentas ×100 para no perder el decimal.
static void est_resumen(const EstRms &e, uint32_t *promX100, uint32_t *sigmaX100)
{
  if (e.vent == 0) { *promX100 = 0; *sigmaX100 = 0; return; }
  *promX100 = (uint32_t)((e.suma * 100UL) / e.vent);

  /*  σ² = ⟨r²⟩ − ⟨r⟩², con la forma escalada por la misma razón que en
   *  rms_ventana.cpp: dividir primero trunca y el error se multiplica por 2⟨r⟩.
   *      σ² × n² = n·Σr² − (Σr)²
   */
  const uint64_t num = (uint64_t)e.vent * e.sumaCuad - (uint64_t)e.suma * e.suma;
  const uint32_t var = (uint32_t)(num / ((uint32_t)e.vent * e.vent));
  *sigmaX100 = (uint32_t)rms_isqrt32(var * 10000UL);   // ×100 dentro de la raíz
}

// Imprime un valor en cuentas y, si hay calibración, su equivalente en volts.
static void imprime_ac(uint32_t cuentasX100, uint32_t mvPorCuentaX100)
{
  Serial.print(cuentasX100 / 100);
  Serial.print('.');
  Serial.print((cuentasX100 % 100) / 10);
  Serial.print(F("c"));
  if (mvPorCuentaX100) {
    const uint32_t mv = (cuentasX100 * mvPorCuentaX100) / 10000UL;
    Serial.print(F(" = "));
    Serial.print(mv / 1000);
    Serial.print('.');
    const uint16_t d = (uint16_t)(mv % 1000);
    if (d < 100) Serial.print('0');
    if (d < 10)  Serial.print('0');
    Serial.print(d);
    Serial.print(F(" V"));
  }
}
