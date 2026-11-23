/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_ISR_SPWM  -  Isla del Sol, micro CRÍTICO (A1)
 *  Protocolo de banco, Parte C — ETAPA 1: el SPWM solo, y cuánto dura su ISR
 *
 *  v4 — histograma de la latencia de entrada: identifica a cada ladrón por
 *       el ANCHO de su bloqueo, y cuenta las vueltas del lazo para saber
 *       cuánto robo mete la propia instrumentación
 * -----------------------------------------------------------------------------
 *  LO QUE YA ESTÁ MEDIDO EN BANCO (22-nov-2026, dos Nano distintos, v3)
 *
 *      cuerpo   = 49/116/126 ciclos   idéntico en todos los reportes
 *      entrada  = 43/51/~165          piso 43 clavado
 *      PROPIO   = 204 c = 12.75 us = 30.6 %   (196 c netos de instrumentación)
 *      SOBREPASOS = 0                 no se pierde ningún paso de la tabla
 *
 *    simavr había predicho 193 c. Difieren 1.6 %. B5 CERRADO.
 *
 *  LO QUE FALTA: QUIÉN SE ROBA LA ENTRADA
 *
 *    El 15.1 % de las ISR entra tarde, y esa fracción NO cambió al pasar el
 *    reporte de 1 s a 2 s (15.18 % contra 15.17 %), así que el UART no es.
 *
 *    Tampoco cuadra mi sección crítica: medida en el binario son 18 ciclos,
 *    y con piso 43 eso daría entradas de 61 como mucho. Pero la media de 51
 *    con 15 % tardías implica que las tardías promedian ~85 ciclos, o sea
 *    ~42 de retraso. Un bloqueo de 18 ciclos no produce eso.
 *
 *    El máximo sí está explicado: robo llega a 128 y la ISR del Timer0
 *    (millis) mide justo eso — pero su frecuencia sólo da para el 0.73 %.
 *
 *    Queda un segundo ladrón de unos 40-90 ciclos disparando ~28 000 veces
 *    por segundo. Esta versión lo saca por el ancho.
 *
 *  CÓMO LO SACA
 *
 *    1. HISTOGRAMA de la espera (entrada - 43) en ocho cajones. Cada fuente
 *       de bloqueo deja su firma: un bloqueo de ancho W reparte las esperas
 *       uniformemente entre 1 y W, así que el histograma se corta justo en W.
 *       Leer dónde se corta = medir el ancho del ladrón.
 *
 *    2. VUELTAS DEL LAZO por segundo. Con eso el sketch calcula solo cuánto
 *       robo le toca a su propia sección crítica:
 *             robo propio = 18 ciclos x vueltas / 16 000 000
 *       Si ese número se parece al 15 %, el culpable soy yo. Si no, hay un
 *       tercero y el histograma dice de qué tamaño.
 *
 *    3. TRAZA EN D12 durante la sección crítica (opcional). Con PulseView se
 *       ve directo si las entradas tardías caen encima de D12 en alto.
 *
 *  CÓMO CAZAR AL CULPABLE — tres corridas de un minuto
 *
 *      corrida 1:  SIN_MILLIS 0, SEGUNDOS_POR_REPORTE 1    ← referencia
 *      corrida 2:  SIN_MILLIS 1, SEGUNDOS_POR_REPORTE 1    ← se va millis()
 *      corrida 3:  SIN_MILLIS 0, SEGUNDOS_POR_REPORTE 10   ← se calla el UART
 *
 *    Lo que hay que mirar ahora es el HISTOGRAMA, no el máximo:
 *      - si en la corrida 2 se vacía el último cajón y los demás no se
 *        mueven, millis() era sólo el máximo y no el problema
 *      - el cajón donde se concentre el 15 % dice el ancho del verdadero
 *        ladrón, y ese ancho lo identifica
 *
 *  LO QUE ESTE SKETCH NO PUEDE VER, Y SÍ NECESITA PULSEVIEW
 *
 *      - tiempo muerto: que HA/LA y HB/LB nunca estén altos a la vez.
 *        Son 16 cuentas = 1 us; a 24 MS/s son 24 muestras, de sobra.
 *      - simetría de la pierna B: diferencia entre semiciclos <= 20 us.
 *
 *    Puntas:  D9 = HA   D10 = LA   D11 = HB   D3 = LB   (D4 = traza ISR,
 *             D12 = traza de la sección crítica del lazo)
 *
 *    NO buscar "anchos repetidos" en la pierna A: cerca del pico la senoidal
 *    es plana y los anchos se repiten SOLOS, sin que haya falla. Ese criterio
 *    estaba mal planteado en la Parte C. El detector bueno es isr_sobrepasos.
 *
 *  PINES  (⚠️ J15 DESCONECTADO — ver Parte C, sección C0)
 *       D4  = traza de la ISR        (normalmente INV_CMD, K4+K2)
 *       D9/D10 = HA/LA   D11/D3 = HB/LB
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// ======================= INTERRUPTORES DE LA PRUEBA ==========================

/*  SIN_MILLIS — apaga la interrupción del Timer0, o sea millis() y delay().
 *  Este sketch no los usa: su base de tiempo son los desbordes del propio
 *  SPWM, que son exactos por construcción. Ponerlo en 1 para la corrida 2.
 */
#define SIN_MILLIS              0

/*  Cada cuánto imprime. Subirlo a 10 calla el UART casi todo el tiempo y
 *  deja ver si era él quien robaba los ciclos (corrida 3).
 */
#define SEGUNDOS_POR_REPORTE    1

/*  Arriba de este valor, una entrada se cuenta como "tarde". El piso medido
 *  es 43 ciclos; 60 deja margen para variaciones de compilador.
 */
#define UMBRAL_TARDE            60

/*  PISO_CICLOS — la entrada cuando nadie estorba: vector + prólogo + traza.
 *  Medido 43 en banco, en dos tarjetas y en todos los reportes. De aquí sale
 *  la ESPERA de cada ISR: espera = entrada - PISO_CICLOS.
 *  El sketch verifica el piso contra el mínimo observado y avisa si difieren.
 */
#define PISO_CICLOS             43

/*  Traza de la sección crítica del lazo en D12 (PB4 = EN_HW).
 *  Sube al entrar al ATOMIC_BLOCK y baja al salir, para poder ver en
 *  PulseView si las entradas tardías caen justo encima.
 *  ⚠️ Requiere la etapa de potencia DESCONECTADA, igual que el resto.
 */
#define TRAZA_SECCION_CRITICA   1

/*  Ciclos que mi propia sección crítica tiene las interrupciones apagadas.
 *  Contados en el binario con:
 *      avr-objdump -d firmware.elf   y buscar el cli ... out 0x3f del lazo
 *  Sirve para que el sketch calcule cuánto robo se debe a sí mismo.
 */
#define CLI_PROPIO_CICLOS       18

/*  EPILOGO_CICLOS — lo que el compilador gasta DESPUÉS de la última lectura
 *  de TCNT1 y que por lo tanto no se puede medir desde adentro.
 *
 *      15 pop (2 c/u)               = 30
 *      out 0x3f, r0 (restaura SREG) =  1
 *      reti                         =  4   ← se cuenta: el AVR no atiende
 *                                             otra interrupción hasta acabarlo
 *      ---------------------------------
 *                                     35
 *  Medido sobre la ISR compilada con avr-gcc 7.3 a -Os. SI CAMBIA LA ISR,
 *  REHACER LA CUENTA, porque cambia el número de registros que salva:
 *      avr-objdump -d firmware.elf | awk '/<__vector_13>:/,/reti/'
 *  y contar los pop del bloque que termina en reti.
 */
#define EPILOGO_CICLOS          35

#define RELOJ_MHZ               16UL

// ========================= EL MÓDULO REAL, CON ENGANCHES =====================
//
// spwm_328p.cpp y sus cabeceras viven en ESTA MISMA CARPETA y el IDE los
// compila como cualquier otro archivo del sketch. El módulo encuentra el
// traza_isr.h de al lado con __has_include y de ahí toma las macros.
//
// No se incluye el .cpp desde el .ino: el IDE compila por su cuenta todo .cpp
// de la carpeta, así que el módulo quedaría compilado dos veces y el
// enlazador se cae con "multiple definition of __vector_13".

#include "spwm.h"
#include "config_spwm.h"
#include "traza_isr.h"

/*  Base de tiempo de la prueba: los desbordes del propio SPWM.
 *  F_CPU / (2 × TOP) = 16 000 000 / 666 = 24 024 por segundo.
 *  Se usa en vez de millis() para que el reporte siga cayendo cada segundo
 *  aunque SIN_MILLIS esté en 1 — y porque es exacta por construcción.
 */
#define ISR_POR_SEGUNDO   (F_CPU / (2UL * TOP_CUENTAS))

// Las variables que el enganche escribe. Las declara traza_isr.h y las
// define aquí, una sola vez.
volatile uint16_t isr_t0;
volatile uint16_t isr_t1;
volatile uint8_t  isr_lista;
volatile uint32_t isr_n;
volatile uint16_t isr_sobrepasos;

// ============================== ESTADÍSTICAS =================================

struct Serie {
  uint32_t n;
  uint32_t suma;
  uint16_t minimo;
  uint16_t maximo;
};

/*  HISTOGRAMA DE LA ESPERA (entrada - PISO_CICLOS).
 *
 *  Por qué sirve: un bloqueo de ancho W, si cae en cualquier momento, reparte
 *  las esperas UNIFORMEMENTE entre 1 y W y no produce ninguna mayor que W.
 *  O sea que el histograma se CORTA en W. Leer dónde se corta es medir el
 *  ancho del que bloquea, y el ancho lo identifica:
 *
 *      ~18 c   la sección crítica de este mismo lazo
 *      ~40-90  el desconocido que andamos buscando
 *      ~128    la ISR del Timer0 (millis)
 */
#define CAJONES  8
static const uint8_t TOPE[CAJONES] = { 0, 10, 20, 40, 70, 110, 160, 255 };
static uint16_t histo[CAJONES];

static void histoMete(uint16_t espera)
{
  for (uint8_t i = 0; i < CAJONES; i++) {
    if (espera <= TOPE[i]) { histo[i]++; return; }
  }
  histo[CAJONES - 1]++;
}

static Serie entrada;     // isr_t0            : piso fijo + espera ajena
static Serie cuerpo;      // isr_t1 - isr_t0   : costo propio del SPWM
static Serie ocupa;       // isr_t1 + epílogo  : lo que hay que caber
static uint32_t tarde;    // entradas por encima de UMBRAL_TARDE
static uint32_t vueltas;  // vueltas del lazo, para calcular el robo propio
static uint32_t isrPrev;  // isr_n del reporte anterior
static uint32_t isrCorte; // cuándo toca el siguiente reporte

static void cero(Serie &s)
{
  s.n = 0;
  s.suma = 0;
  s.minimo = 0xFFFF;
  s.maximo = 0;
}

static void mete(Serie &s, uint16_t v)
{
  s.n++;
  s.suma += v;
  if (v < s.minimo) s.minimo = v;
  if (v > s.maximo) s.maximo = v;
}

static uint16_t medio(const Serie &s)
{
  return s.n ? (uint16_t)((s.suma + s.n / 2) / s.n) : 0;
}

// Imprime ciclos como microsegundos con dos decimales, sin punto flotante.
static void us(uint32_t ciclos)
{
  const uint32_t x100 = (ciclos * 100UL + RELOJ_MHZ / 2) / RELOJ_MHZ;
  Serial.print(x100 / 100);
  Serial.print('.');
  const uint8_t d = x100 % 100;
  if (d < 10) Serial.print('0');
  Serial.print(d);
}

// Porcentaje del periodo de portadora, con un decimal.
static void pct(uint32_t ciclos)
{
  const uint32_t periodo = 2UL * TOP_CUENTAS;
  const uint32_t x10 = (ciclos * 1000UL + periodo / 2) / periodo;
  Serial.print(x10 / 10);
  Serial.print('.');
  Serial.print(x10 % 10);
}

static void trio(const Serie &s)
{
  Serial.print(s.minimo);
  Serial.print('/');
  Serial.print(medio(s));
  Serial.print('/');
  Serial.print(s.maximo);
}

// ================================= SETUP =====================================

void setup()
{
  Serial.begin(115200);
  while (!Serial) { }

  DDRD  |= _BV(TRZ_BIT);
  PORTD &= ~_BV(TRZ_BIT);
#if TRAZA_SECCION_CRITICA
  DDRB  |= _BV(PB4);          // D12 = EN_HW, traza de la seccion critica
  PORTB &= ~_BV(PB4);
#endif

  Serial.println();
  Serial.println(F("================================================================"));
  Serial.println(F("  ISLA DEL SOL - Parte C, ETAPA 1 v3: ISR del SPWM"));
  Serial.println(F("================================================================"));
  Serial.println(F("  !! J15 DESCONECTADO y etapa de potencia fuera. Verificar."));
  Serial.println();

  if (!spwm_preparar(DT_LEGACY_CUENTAS, LINEA_60_HZ)) {
    Serial.println(F("  ERROR: spwm_preparar fallo. Detenido."));
    for (;;) { }
  }

  Serial.print(F("  portadora : TOP = "));
  Serial.print(TOP_CUENTAS);
  Serial.print(F(" cuentas, periodo = "));
  us(2UL * TOP_CUENTAS);
  Serial.print(F(" us = "));
  Serial.print(2UL * TOP_CUENTAS);
  Serial.println(F(" ciclos"));

  Serial.print(F("  linea     : "));
  Serial.print(spwm_frecuencia_hz());
  Serial.print(F(" Hz, "));
  Serial.print(spwm_muestras_ciclo());
  Serial.print(F(" muestras/ciclo, "));
  Serial.print(ISR_POR_SEGUNDO);
  Serial.println(F(" ISR/s esperadas"));

  Serial.print(F("  epilogo   : "));
  Serial.print(EPILOGO_CICLOS);
  Serial.println(F(" ciclos sumados (no medibles desde adentro)"));

  Serial.print(F("  millis()  : "));
#if SIN_MILLIS
  TIMSK0 = 0;                      // fuera la ISR del Timer0
  Serial.println(F("APAGADO (SIN_MILLIS = 1)"));
#else
  Serial.println(F("encendido"));
#endif

  Serial.print(F("  reporte   : cada "));
  Serial.print(SEGUNDOS_POR_REPORTE);
  Serial.println(F(" s (base de tiempo: desbordes del SPWM, no millis)"));

  Serial.println();
  Serial.println(F("  Referencia v2 en banco: cuerpo determinista, entrada 43-176 c"));
  Serial.println(F("  Costo propio esperado : ~197 ciclos = 12.35 us = 29.7 %"));
  Serial.println(F("----------------------------------------------------------------"));

  cero(entrada);
  cero(cuerpo);
  cero(ocupa);
  tarde = 0;
  vueltas = 0;
  for (uint8_t i = 0; i < CAJONES; i++) histo[i] = 0;

  spwm_arrancar();
  spwm_fijar_amplitud(spwm_amplitud_max());   // caso peor: la tabla trabaja

  isrPrev  = 0;
  isrCorte = (uint32_t)ISR_POR_SEGUNDO * SEGUNDOS_POR_REPORTE;
}

// ================================== LAZO =====================================

void loop()
{
  uint16_t t0, t1, sob;
  uint32_t n;
  uint8_t  hay;

  vueltas++;

#if TRAZA_SECCION_CRITICA
  PORTB |= _BV(PB4);
#endif
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    hay = isr_lista;
    t0  = isr_t0;
    t1  = isr_t1;
    n   = isr_n;
    sob = isr_sobrepasos;
    isr_lista = 0;
  }
#if TRAZA_SECCION_CRITICA
  PORTB &= ~_BV(PB4);
#endif

  if (hay && t1 >= t0) {
    mete(entrada, t0);
    mete(cuerpo,  (uint16_t)(t1 - t0));
    mete(ocupa,   (uint16_t)(t1 + EPILOGO_CICLOS));
    histoMete((t0 > PISO_CICLOS) ? (uint16_t)(t0 - PISO_CICLOS) : 0);
    if (t0 > UMBRAL_TARDE) tarde++;
  }

  if (n < isrCorte) return;
  isrCorte += (uint32_t)ISR_POR_SEGUNDO * SEGUNDOS_POR_REPORTE;

  if (cuerpo.n == 0) {
    Serial.println(F("  sin muestras (la ISR no esta corriendo?)"));
    return;
  }

  // --- cuántas ISR corrieron de verdad, contra cuántas recogio el lazo -------
  const uint32_t corrieron = (n > isrPrev) ? (n - isrPrev) : 1;
  isrPrev = n;

  Serial.print(F("ISR="));
  Serial.print(corrieron);
  Serial.print(F(" recogidas="));
  Serial.print(cuerpo.n);
  Serial.print(F(" ("));
  Serial.print((uint16_t)((cuerpo.n * 1000UL) / corrieron));
  Serial.print(F("/1000)"));

  // --- las tres series, en ciclos -------------------------------------------
  Serial.print(F("  entrada="));
  trio(entrada);
  Serial.print(F("  cuerpo="));
  trio(cuerpo);

  // --- costo propio: piso + cuerpo + epilogo --------------------------------
  // El piso es el minimo de entrada visto en la corrida: ahi nadie estorbo.
  const uint32_t propio = (uint32_t)entrada.minimo + cuerpo.maximo + EPILOGO_CICLOS;
  Serial.print(F("  PROPIO="));
  Serial.print(propio);
  Serial.print(F("c "));
  us(propio);
  Serial.print(F("us "));
  pct(propio);
  Serial.print('%');

  // --- ocupacion real: lo que de verdad hay que caber -----------------------
  Serial.print(F("  OCUPA="));
  Serial.print(ocupa.maximo);
  Serial.print(F("c "));
  us(ocupa.maximo);
  Serial.print(F("us "));
  pct(ocupa.maximo);
  Serial.print('%');

  // --- robo ajeno y sobrepasos ----------------------------------------------
  Serial.print(F("  robo="));
  Serial.print(entrada.maximo - entrada.minimo);
  Serial.print(F("c tarde="));
  Serial.print(tarde);

  Serial.print(F("  SOBREPASOS="));
  Serial.print(sob);

  // --- veredicto contra los umbrales de la Parte C, seccion C1 --------------
  const uint32_t periodo = 2UL * TOP_CUENTAS;
  Serial.print(F("  -> "));
  if (sob)                                       Serial.println(F("FALLA (paso perdido)"));
  else if (ocupa.maximo * 10UL <= periodo * 7UL) Serial.println(F("PASA"));
  else if (ocupa.maximo * 10UL <= periodo * 9UL) Serial.println(F("AVISO"));
  else                                           Serial.println(F("FALLA"));

  // ---------- segunda linea: el histograma y el autoexamen ---------------
  Serial.print(F("   espera:"));
  for (uint8_t i = 0; i < CAJONES; i++) {
    Serial.print(' ');
    if (i == 0)               Serial.print(F("0="));
    else if (i == CAJONES - 1) { Serial.print('>'); Serial.print(TOPE[i - 1]); Serial.print('='); }
    else                      { Serial.print(TOPE[i]); Serial.print('='); }
    Serial.print(histo[i]);
  }

  // Cuanto robo le toca a MI PROPIA seccion critica:
  //    fraccion = CLI_PROPIO_CICLOS x vueltas / F_CPU
  // en partes por mil, con cuidado de no desbordar en 32 bits.
  const uint32_t vps  = vueltas / SEGUNDOS_POR_REPORTE;
  const uint32_t mio  = (CLI_PROPIO_CICLOS * (vps / 100UL) * 1000UL) / (F_CPU / 100UL);
  Serial.print(F("   vueltas/s="));
  Serial.print(vps);
  Serial.print(F(" robo_mio="));
  Serial.print(mio);
  Serial.print(F("/1000"));

  // El piso tiene que ser el que creemos; si no, todo lo demas se corre.
  if (entrada.minimo != PISO_CICLOS) {
    Serial.print(F("   !! PISO observado="));
    Serial.print(entrada.minimo);
    Serial.print(F(" esperado="));
    Serial.print(PISO_CICLOS);
  }
  Serial.println();

  cero(entrada);
  cero(cuerpo);
  cero(ocupa);
  tarde = 0;
  vueltas = 0;
  for (uint8_t i = 0; i < CAJONES; i++) histo[i] = 0;
}
