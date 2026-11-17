/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_ADC_AC  -  Isla del Sol, micro CRÍTICO (A1)
 *  Banco de pruebas del módulo ADC AC: corridas A6, A7 y A8
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Monta el ADC como lo define DC-020 y reporta por serie lo que las tres
 *    corridas del protocolo de banco necesitan medir:
 *
 *      A6  RMS de cada canal, para comparar contra el multímetro.
 *      A7  Instante del cruce por cero de cada canal, para medir el sesgo
 *          entre A0 y A6 y la simetría de los filtros.
 *      A8  Pin de traza que sube al disparar la conversión y baja al
 *          terminarla, para que el analizador mida fs y la duración.
 *
 *    NO corre el SPWM. Configura el Timer1 en modo 8 con el mismo TOP, para
 *    que la base de tiempo sea idéntica, pero deja los pines del puente como
 *    GPIO: en el banco no hay 48 V y no hace falta mover compuertas.
 *    Para saber si la ISR del SPWM le deja espacio al ADC está la perilla
 *    CARGA_FALSA_CICLOS, que quema en la interrupción los mismos ciclos que
 *    gasta la ISR real.
 *
 *  HARDWARE QUE ESPERA (protocolo de banco, parte A)
 *      A0  canal de red,     divisor AC + bias de 2.5 V + 10 nF al pin
 *      A6  canal de inversor, igual (sólo existe en el 328P TQFP, el del Nano)
 *      D8  traza del ADC para el analizador (perilla PIN_TRAZA_ADC)
 *    Para la corrida A7 los DOS pines se alimentan del MISMO divisor.
 *
 *  CÓMO SE USA
 *    1. Medir el riel de 5 V con el multímetro y poner el valor en VCC_MV.
 *    2. Armar el divisor elegido en A3 y poner su relación en DIVISOR_X100.
 *    3. Cargar, abrir el monitor serie a 115200 y leer el encabezado: trae
 *       todas las constantes derivadas, para que queden en la bitácora.
 *    4. Anotar en la hoja de registro del protocolo.
 *
 *  LO QUE NO HACE
 *    No es el módulo ac_fb_grid. No hay PLL, no hay seguimiento de frecuencia
 *    y no hay transferencia. Esto sólo mide para poder escribir ese módulo
 *    con números medidos en lugar de supuestos.
 *
 *  Referencias: docs/protocolo_banco_control.md (corridas A6, A7, A8)
 *               docs/decisiones_criticas.md     (DC-006, DC-020)
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#if !defined(__AVR_ATmega328P__)
  #error "Esta prueba es para el ATmega328P (Nano): A6 no existe en el DIP-28"
#endif
#if F_CPU != 16000000UL
  #error "Esta prueba espera 16 MHz: seleccione Arduino Nano en el IDE"
#endif

// ======================== PERILLAS (filosofía UNIX) ==========================
// Se eligen aquí, se compila, se carga. Nada se configura desde el serie.

// --- Corrida A7: orden de lectura de los canales -----------------------------
//   0 = A0 (inversor) primero, luego A6 (red)   -> orden normal
//   1 = A6 (red) primero, luego A0 (inversor)   -> invertido, diagnóstico de A7
#define ORDEN_INVERTIDO        0

// --- Corrida A8: pin de traza para el analizador -----------------------------
//   Sube al arrancar la conversión, baja al terminarla.
//   ⚠ Verifique que esté libre en su tarjeta. D7 está reservado por DC-017
//     para medir la ISR del SPWM, así que aquí se usa D8.
#define PIN_TRAZA_ADC          8

// --- Carga falsa en la ISR del Timer1 ----------------------------------------
//   Quema ciclos de CPU en la interrupción para emular la ISR del SPWM, que
//   según la simulación gasta 169–180 ciclos de los 666 del periodo.
//   0 = sin carga. 175 = como la ISR real. Suba el número hasta que el ADC
//   empiece a perder disparos: ese es el margen real que queda.
#define CARGA_FALSA_CICLOS     175

// --- Calibración (se llena en el banco) --------------------------------------
//   VCC_MV: el riel de 5 V medido con el multímetro, en milivolts.
#define VCC_MV                 5000
//   DIVISOR_X100: relación del divisor AC × 100, es decir (Rt + Rb) / Rb × 100.
//   Con Rt = 100 k y Rb = 10.2 k (trimmer al tope): 110.2 / 10.2 = 10.80 -> 1080
#define DIVISOR_X100           1080

// ======================== BASE DE TIEMPO =====================================
// ⚠ Estos valores DEBEN coincidir con config_portadora.h del firmware real.
//   Se repiten aquí para que el sketch sea autosuficiente en el IDE.
constexpr uint16_t TOP_CUENTAS = 333;              // modo 8, 24 024.02 Hz
constexpr uint8_t  PERIODOS_POR_DISPARO = 3;       // DC-020: 3 × 41.625 > 104 µs

// Periodo de portadora en nanosegundos: 2 × TOP / 16 MHz
constexpr uint32_t PERIODO_PORT_NS = (uint32_t)(2UL * TOP_CUENTAS * 1000UL / 16UL);
// Entre disparos (alternando canales) y entre muestras del MISMO canal
constexpr uint32_t ENTRE_DISPAROS_NS = PERIODO_PORT_NS * PERIODOS_POR_DISPARO;
constexpr uint32_t ENTRE_MUESTRAS_NS = ENTRE_DISPAROS_NS * 2UL;

// Ventana de medición: 200 muestras por canal = 3 ciclos de salida EXACTOS.
//   1 ciclo = 400 periodos de portadora; cada canal se muestrea cada 6
//   periodos; 1200 / 6 = 200. Al caer en número entero de ciclos, el RMS no
//   tiene rizo de ventana.
constexpr uint16_t MUESTRAS_VENTANA = 200;
constexpr uint8_t  VENTANAS_POR_REPORTE = 20;      // ~1 s (20 × 50 ms)

static_assert(ENTRE_DISPAROS_NS > 104000UL,
  "El disparo cae antes de que termine la conversión de 104 µs");
static_assert(PIN_TRAZA_ADC >= 8 && PIN_TRAZA_ADC <= 13,
  "El pin de traza se escribe por PORTB: tiene que ser D8..D13");

// ======================== CANALES ============================================
// OJO: A0 es el INVERSOR y A6 es la RED, no al reves. El README de la tarjeta
// de control (seccion 8) lo dice sin ambiguedad: "el del inversor (J2 -> A0)
// cierra el lazo de amplitud; el de la red (J17 -> A6) da la fase para
// sincronizarse antes de transferir". Estaban intercambiados en la primera
// version de este sketch y en DC-020.
constexpr uint8_t CANAL_INVERSOR = 0;   // A0, J2, lazo de amplitud
constexpr uint8_t CANAL_RED      = 6;   // A6, J17, fase para sincronizar
// El índice 0/1 es el ORDEN de lectura, no el número de canal: con el orden
// invertido el índice 0 es A6. Los nombres siguen al orden para no mentir.
#if ORDEN_INVERTIDO
constexpr uint8_t ORDEN[2] = { CANAL_RED, CANAL_INVERSOR };
static const char* const NOMBRE[2] = { "A6 red     ", "A0 inversor" };
#else
constexpr uint8_t ORDEN[2] = { CANAL_INVERSOR, CANAL_RED };
static const char* const NOMBRE[2] = { "A0 inversor", "A6 red     " };
#endif

// ======================== ESTADO =============================================
struct Acumulador {
  uint32_t suma;          // Σx     (para la media = el bias)
  uint32_t sumaCuad;      // Σx²    (para el RMS por varianza, en una pasada)
  uint16_t n;
  int16_t  dPrev;         // muestra anterior menos el bias de referencia
  uint32_t perCruce;      // cuenta de portadora del disparo previo al cruce
  uint8_t  fracCruce;     // posición del cruce dentro del intervalo, en 1/256
  bool     cruceListo;
  bool     dPrevValida;
};

static volatile Acumulador acc[2];
static volatile Acumulador snap[2];
static volatile bool     hayVentana = false;
static volatile uint32_t periodos = 0;        // base de tiempo común, en periodos
static volatile uint32_t perDisparo = 0;      // cuenta al disparar la conversión
static volatile uint8_t  cuentaDisparo = 0;
static volatile uint8_t  indiceCanal = 0;
static volatile uint16_t perdidos = 0;        // disparos que cayeron ocupados

// Bias de referencia de la ventana anterior, usado para detectar el cruce.
static volatile int16_t mediaRef[2] = { 512, 512 };

// ======================== INTERRUPCIONES =====================================

// Desborde del Timer1: en modo 8 ocurre en BOTTOM. Es la base de tiempo y el
// disparador del ADC (DC-020: cada 3 periodos).
ISR(TIMER1_OVF_vect)
{
  periodos++;

  if (++cuentaDisparo >= PERIODOS_POR_DISPARO) {
    cuentaDisparo = 0;
    if (ADCSRA & (1 << ADSC)) {
      perdidos++;                       // la conversión anterior no terminó
    } else {
      perDisparo = periodos;
      PORTB |= (1 << (PIN_TRAZA_ADC - 8));
      ADCSRA |= (1 << ADSC);
    }
  }

#if CARGA_FALSA_CICLOS > 0
  // Emula la ISR del SPWM quemando ciclos exactos. El built-in necesita una
  // constante de compilación, que es justo lo que la perilla es.
  __builtin_avr_delay_cycles(CARGA_FALSA_CICLOS);
#endif
}

// Fin de conversión: guarda la muestra, detecta el cruce y cambia de canal.
ISR(ADC_vect)
{
  const uint16_t x = ADC;
  PORTB &= ~(1 << (PIN_TRAZA_ADC - 8));

  const uint8_t c = indiceCanal;
  Acumulador &a = *(Acumulador *)&acc[c];

  a.suma     += x;
  a.sumaCuad += (uint32_t)x * x;
  a.n++;

  // Cruce por cero ascendente, interpolando entre las dos muestras que lo
  // encierran. El bias de referencia es la media de la ventana anterior.
  const int16_t d = (int16_t)x - mediaRef[c];
  if (a.dPrevValida && !a.cruceListo && a.dPrev < 0 && d >= 0) {
    const int16_t salto = d - a.dPrev;                 // > 0 por construcción
    a.fracCruce  = (uint8_t)(((int32_t)(-a.dPrev) * 256) / salto);
    a.perCruce   = perDisparo - PERIODOS_POR_DISPARO * 2;   // disparo anterior
    a.cruceListo = true;
  }
  a.dPrev       = d;
  a.dPrevValida = true;

  // Fin de ventana: se cierra con el primer canal del orden.
  if (c == 0 && a.n >= MUESTRAS_VENTANA && !hayVentana) {
    for (uint8_t k = 0; k < 2; k++) {
      *(Acumulador *)&snap[k] = *(Acumulador *)&acc[k];
      acc[k].suma = 0; acc[k].sumaCuad = 0; acc[k].n = 0;
      acc[k].cruceListo = false; acc[k].dPrevValida = false;
    }
    hayVentana = true;
  }

  indiceCanal = (uint8_t)(c ^ 1);
  ADMUX = (1 << REFS0) | ORDEN[indiceCanal];
}

// ======================== AUXILIARES =========================================

// Raíz cuadrada entera (Newton sobre enteros). Sin float: regla del proyecto.
static uint16_t raiz(uint32_t v)
{
  if (v == 0) return 0;
  uint32_t r = v, ant = 0;
  for (uint8_t i = 0; i < 20 && r != ant; i++) { ant = r; r = (r + v / r) / 2; }
  return (uint16_t)r;
}

// Imprime un entero escalado como decimal, sin float.
//   imprimeDec(2271, 100, 2)  ->  "22.71"
static void imprimeDec(int32_t valor, int32_t escala, uint8_t decimales)
{
  if (valor < 0) { Serial.print('-'); valor = -valor; }
  Serial.print(valor / escala);
  if (decimales == 0) return;
  Serial.print('.');
  const int32_t resto = valor % escala;
  int32_t div = escala / 10;
  for (uint8_t i = 0; i < decimales && div > 0; i++, div /= 10) {
    Serial.print((char)('0' + (resto / div) % 10));
  }
}

// ======================== ARRANQUE ===========================================

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_TRAZA_ADC, OUTPUT);
  digitalWrite(PIN_TRAZA_ADC, LOW);

  // --- Timer1 en modo 8 (phase and frequency correct), TOP = ICR1 -----------
  // Misma base de tiempo que el SPWM real, pero SIN sacar nada por los pines:
  // los bits COM quedan en 0, así que D9 y D10 siguen siendo GPIO.
  TCCR1A = 0;
  TCCR1B = (1 << WGM13);
  ICR1   = TOP_CUENTAS;
  TCNT1  = 0;
  TIMSK1 = (1 << TOIE1);
  TCCR1B |= (1 << CS10);                 // preescalador 1

  // --- ADC: /128 (125 kHz), referencia AVCC, interrupción al terminar -------
  // DC-020: /128 es el único valor dentro de la ventana de 50–200 kHz que la
  // hoja de datos exige para 10 bits plenos.
  ADMUX  = (1 << REFS0) | ORDEN[0];
  ADCSRB = 0;                            // sin auto-disparo
  DIDR0  = (1 << ADC0D);                 // apaga la entrada digital de A0
  ADCSRA = (1 << ADEN) | (1 << ADIE) |
           (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);   // /128

  // La primera conversión toma 25 ciclos de ADC (200 µs) en lugar de 13:
  // se dispara y se tira aquí, antes de empezar a acumular.
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC)) { }
  (void)ADC;

  for (uint8_t k = 0; k < 2; k++) {
    acc[k].suma = 0; acc[k].sumaCuad = 0; acc[k].n = 0;
    acc[k].cruceListo = false; acc[k].dPrevValida = false;
  }

  sei();

  // --- Encabezado: todo lo derivado, para que quede en la bitácora ----------
  Serial.println();
  Serial.println(F("=== ISLA DEL SOL - prueba del ADC AC (corridas A6, A7, A8) ==="));
  Serial.print(F("TOP del Timer1      : ")); Serial.println(TOP_CUENTAS);
  Serial.print(F("Periodo portadora   : "));
  imprimeDec(PERIODO_PORT_NS, 1000, 3); Serial.println(F(" us"));
  Serial.print(F("Reloj del ADC       : 125 kHz (/128), conversion 104.0 us"));
  Serial.println();
  Serial.print(F("Entre disparos      : "));
  imprimeDec(ENTRE_DISPAROS_NS, 1000, 3);
  Serial.print(F(" us  (")); Serial.print(PERIODOS_POR_DISPARO);
  Serial.println(F(" periodos)"));
  Serial.print(F("Entre muestras/canal: "));
  imprimeDec(ENTRE_MUESTRAS_NS, 1000, 3); Serial.println(F(" us"));
  Serial.print(F("Ventana             : ")); Serial.print(MUESTRAS_VENTANA);
  Serial.println(F(" muestras/canal = 3 ciclos de salida exactos"));
  Serial.print(F("Orden de lectura    : "));
  Serial.print(ORDEN_INVERTIDO ? F("INVERTIDO (A6 primero)") : F("normal (A0 primero)"));
  Serial.println();
  Serial.print(F("Carga falsa en ISR  : ")); Serial.print(CARGA_FALSA_CICLOS);
  Serial.println(F(" ciclos"));
  Serial.print(F("VCC / divisor       : ")); Serial.print(VCC_MV);
  Serial.print(F(" mV  /  x"));
  imprimeDec(DIVISOR_X100, 100, 2); Serial.println();
  Serial.print(F("Traza del ADC       : D")); Serial.println(PIN_TRAZA_ADC);
  Serial.println(F("--------------------------------------------------------------"));
  Serial.println(F("A7: el sesgo de DC-020 ya esta descontado, asi que la"));
  Serial.println(F("    diferencia de cruce debe salir CERO. Lo que sobre es"));
  Serial.println(F("    asimetria de los filtros. Sin descontarlo saldria"));
  Serial.print  (F("    "));
  imprimeDec(ENTRE_DISPAROS_NS, 1000, 3); Serial.println(F(" us."));
  Serial.println(F("    Validado en la PC: con 1 s de promedio el sesgo da"));
  Serial.println(F("    0.39 us; si sale mas, es el hardware, no la cuenta."));
  Serial.println(F("    'desc' son pares de cruces descartados por el candado"));
  Serial.println(F("    de +-3 periodos: unos pocos son normales."));
  Serial.println(F("--------------------------------------------------------------"));
}

// ======================== LAZO PRINCIPAL =====================================

void loop()
{
  static uint8_t  ventanas = 0;
  static int32_t  sumaSesgoNs = 0;
  static uint8_t  cruces = 0;
  static uint16_t rechazados = 0;
  static uint32_t rmsAcum[2] = { 0, 0 };
  static uint32_t biasAcum[2] = { 0, 0 };
  static uint32_t segundos = 0;

  if (!hayVentana) return;

  Acumulador v[2];
  noInterrupts();
  v[0] = *(Acumulador *)&snap[0];
  v[1] = *(Acumulador *)&snap[1];
  hayVentana = false;
  interrupts();

  // --- RMS en una pasada: var = Σx²/n − (Σx/n)² -----------------------------
  // La media sale del mismo acumulador, así que el bias se quita solo: no hay
  // que suponer que vale 512.
  for (uint8_t k = 0; k < 2; k++) {
    if (v[k].n == 0) continue;
    const uint32_t n = v[k].n;
    const uint32_t media = (v[k].suma + n / 2) / n;
    const uint32_t cuadMedia = (uint32_t)(((uint64_t)v[k].suma * v[k].suma) / n);
    const uint32_t var = (v[k].sumaCuad > cuadMedia)
                       ? (v[k].sumaCuad - cuadMedia) / n : 0;
    rmsAcum[k]  += raiz(var);
    biasAcum[k] += media;
    // Escritura de 16 bits: no es atómica en AVR y la ISR la lee.
    noInterrupts();
    mediaRef[k] = (int16_t)media;         // referencia para la ventana siguiente
    interrupts();
  }

  // --- Sesgo entre canales, en la base de tiempo común ----------------------
  // Cada cruce se ubica con la cuenta de portadora de su propio disparo, así
  // que el desfase del multiplexado ya está descontado por construcción.
  if (v[0].cruceListo && v[1].cruceListo) {
    const int32_t dPer = (int32_t)v[1].perCruce - (int32_t)v[0].perCruce;

    // CANDADO: los dos canales tienen que haber agarrado el MISMO cruce.
    // Las dos rejillas de muestreo están corridas 3 periodos, así que para un
    // mismo cruce la diferencia de cuentas sólo puede ser +3 o -3, según de
    // qué lado del intervalo caiga. Cualquier otro valor significa que un
    // canal atrapó el cruce y el otro agarró el del ciclo siguiente, y
    // entonces la diferencia salta un periodo entero de línea (16.7 ms).
    // Sin este candado el promedio de 1 s salía con 1.7 ms de error.
    if (dPer == PERIODOS_POR_DISPARO || dPer == -(int32_t)PERIODOS_POR_DISPARO) {
      const int32_t dFrac = (int32_t)v[1].fracCruce - (int32_t)v[0].fracCruce;
      int32_t ns = dPer * (int32_t)PERIODO_PORT_NS
                 + (dFrac * (int32_t)ENTRE_MUESTRAS_NS) / 256;
#if ORDEN_INVERTIDO
      ns = -ns;    // con el orden invertido, los papeles de los canales se cambian
#endif
      sumaSesgoNs += ns;
      cruces++;
    } else {
      rechazados++;
    }
  }

  if (++ventanas < VENTANAS_POR_REPORTE) return;
  segundos++;

  // --- Reporte --------------------------------------------------------------
  Serial.print(F("t=")); Serial.print(segundos); Serial.print(F("s  "));

  for (uint8_t k = 0; k < 2; k++) {
    const uint32_t rms  = rmsAcum[k] / ventanas;
    const uint32_t bias = biasAcum[k] / ventanas;
    // cuentas -> mV en el pin -> VAC del secundario (centivolts)
    const uint32_t mv  = (rms * VCC_MV) / 1023UL;
    const uint32_t cv  = (mv * DIVISOR_X100) / 10000UL;
    Serial.print(NOMBRE[k]);
    Serial.print(F(": bias ")); Serial.print(bias);
    Serial.print(F("  rms ")); Serial.print(rms);
    Serial.print(F(" cnt = ")); Serial.print(mv);
    Serial.print(F(" mV = ")); imprimeDec(cv, 100, 2);
    Serial.print(F(" VAC | "));
  }

  if (cruces) {
    const int32_t ns = sumaSesgoNs / cruces;
    Serial.print(F("sesgo "));
    imprimeDec(ns, 1000, 2); Serial.print(F(" us ("));
    // grados a 60 Hz: 1 us = 0.0216 deg; en centésimas de grado = ns x 216/100000
    imprimeDec((ns * 216) / 100000, 100, 2); Serial.print(F(" deg) "));
    Serial.print(cruces); Serial.print(F(" cruces, "));
    Serial.print(rechazados); Serial.print(F(" desc | "));
  } else {
    Serial.print(F("sin cruces (senal ausente?) | "));
  }

  Serial.print(F("perdidos ")); Serial.println(perdidos);

  ventanas = 0; cruces = 0; rechazados = 0; sumaSesgoNs = 0;
  rmsAcum[0] = rmsAcum[1] = 0;
  biasAcum[0] = biasAcum[1] = 0;
}
