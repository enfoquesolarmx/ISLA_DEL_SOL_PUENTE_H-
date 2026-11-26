/*
 * =============================================================================
 *  adc_portadora.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  El ADC amarrado a la portadora del SPWM
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *
 *    Lee los tres canales analógicos que el lazo de control necesita, pero no
 *    cuando le da la gana: **siempre en el mismo punto del periodo de la
 *    portadora**. Eso es lo que pide DC-020, y la razón no es la velocidad.
 *
 *    > Si el ADC muestrea libre, tarde o temprano cae encima de un flanco del
 *    > puente y esa muestra sale sucia sin que nadie lo sepa. Amarrado, cae
 *    > siempre en el mismo punto, y las pocas que coincidan con el flanco se
 *    > identifican y se tiran.
 *
 *    Sustituye al `analogRead()` bloqueante que `bateria.cpp` usa hoy, que su
 *    propio encabezado marca como provisional «cuando exista el ADC disparado
 *    por la portadora». Éste es.
 *
 *  CÓMO SE DISPARA, Y POR QUÉ ASÍ
 *
 *    Por software, desde la ISR de desborde del Timer1 **que ya existe** para
 *    actualizar el SPWM. No con el auto-disparo del ADC, porque de las fuentes
 *    de `ADTS` las dos útiles están ocupadas en este diseño: `OCR1B` es el
 *    tiempo muerto e `ICR1` es el TOP del modo 8. Queda el desborde del
 *    Timer1, que llega cada 41.6 µs — demasiado seguido para una conversión
 *    de 104 µs. Por eso se cuenta.
 *
 *    El costo dentro de la ISR del SPWM es **un contador y un `ADSC`**.
 *
 *  UNA CONVERSIÓN CADA CUATRO DESBORDES
 *
 *        4 × 41.625 µs = 166.5 µs de ventana
 *        conversión a 125 kHz = 104 µs
 *        holgura = 62.5 µs
 *
 *    ⚠️ **Eran 3 y el banco dijo 4.** Con 3 la cuenta de la hoja de datos
 *    cerraba (124.875 > 104) pero se perdían 434 muestras por segundo. El
 *    error era comparar la conversión contra la ventana y no contra la
 *    holgura: lo que importa es que la ISR del ADC alcance a correr en lo que
 *    sobra, y ahí puede caer una ISR del SPWM.
 *
 *        cada 3:  20.9 µs de holgura — 1 ISR de SPWM (12.75) + ADC (~13) = 25.8   NO CABE
 *        cada 4:  62.5 µs de holgura — 2 ISR de SPWM (25.5) + ADC (~13) = 38.5    CABE
 *
 *    Los 12.75 µs del SPWM están medidos en banco (Parte C del protocolo).
 *
 *  EL REPARTO ENTRE CANALES (DC-025)
 *
 *        rotación:   A0 → A6 → A0 → A3 → A0 → A6 → A0 → A2 → (repite)
 *
 *        A0  AC_FB_A0        salida del inversor   3003 muestras/s   50/ciclo
 *        A6  AC_FB_GRID_A6   la red                1501.5            25/ciclo
 *        A3  DC_FB_A3        batería                750.75            12.5/ciclo
 *        A2  I_AC            pinza de corriente     750.75            12.5/ciclo
 *
 *    El inversor va doble porque es el único de los tres que cierra un lazo
 *    dentro del periodo. La red sólo necesita fase y la batería se mueve
 *    despacio.
 *
 *  EL INSTANTE DE LA MEDICIÓN
 *
 *    El valor NO se captura al final de la conversión. El *sample-and-hold*
 *    ocurre **1.5 ciclos de ADC después del disparo = 12 µs**, que son 192
 *    cuentas del Timer1. Ése, y no el final, es el instante de la medida — y
 *    es determinista. De ahí sale la corrección de fase del PLL.
 *
 *    ⏳ Pendiente de verificar en banco (etapa 2 de la Parte C): se compara la
 *    traza de la ISR del SPWM contra la del ADC en el analizador lógico.
 *
 *  RECURSOS QUE TOMA
 *
 *    ADC completo (ADMUX, ADCSRA, ADCSRB, DIDR0) y el vector ADC_vect.
 *    ⚠️ Incompatible con `analogRead()`: quien llame a `analogRead()` le
 *    cambia el multiplexor por debajo. Hay que sacar esa llamada de
 *    `bateria.cpp` al integrar.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>
#include <avr/io.h>

// ------------------------------ Configuración -------------------------------

// Cada cuántos desbordes del Timer1 se dispara una conversión. Ver arriba:
// 3 no alcanza, 4 sí. No bajarlo sin volver a medir `adc_perdidas()`.
#define ADC_DESBORDES_POR_DISPARO   4

// Las tres ranuras donde quedan las lecturas.
#define ADC_RANURA_INV   0      // A0 — salida del inversor
#define ADC_RANURA_RED   1      // A6 — la red
#define ADC_RANURA_BAT   2      // A3 — batería
#define ADC_RANURA_COR   3      // A2 — pinza de corriente SCT-013 (DC-013)
#define ADC_RANURAS      4

// ------------------------------- Interfaz -----------------------------------

// Configura el ADC y deja listo el primer canal. Llamar una vez, antes de
// arrancar el SPWM.
void adc_preparar(void);

// Última cuenta cruda de una ranura (0..1023). Lectura atómica.
uint16_t adc_cuentas(uint8_t ranura);

// Cuántas conversiones lleva esa ranura. Sirve para comprobar el reparto:
// deben salir 2:1:1 entre INV, RED y BAT.
uint32_t adc_muestras(uint8_t ranura);

// Disparos que cayeron con el ADC todavía ocupado. **Tiene que ser 0.**
// Cualquier otra cosa significa que la holgura no alcanza, que es
// exactamente lo que pasaba con 3 desbordes por disparo.
uint16_t adc_perdidas(void);

void adc_reiniciar_contadores(void);

// ------------------- El disparo, llamado desde la ISR del SPWM --------------
//
// Va en el encabezado y en línea a propósito: corre dentro de la ISR del
// Timer1 y no debe costar una llamada. Son un contador y un ADSC.
//
// El multiplexor NO se toca aquí: lo deja listo la ISR del ADC al terminar la
// conversión anterior, así que lleva más de 150 µs asentado cuando llega el
// disparo. Eso evita el error clásico de cambiar el canal junto con el
// disparo y medir el canal anterior.

extern volatile uint8_t  adc_cuenta_desbordes;
extern volatile uint16_t adc_perdidas_n;

static inline void adc_disparar_si_toca(void)
{
  if (++adc_cuenta_desbordes < ADC_DESBORDES_POR_DISPARO) return;
  adc_cuenta_desbordes = 0;

  if (ADCSRA & _BV(ADSC)) {      // seguía convirtiendo: se pierde el disparo
    adc_perdidas_n++;            // y queremos enterarnos, no adivinarlo
    return;
  }
  ADCSRA |= _BV(ADSC);
}
