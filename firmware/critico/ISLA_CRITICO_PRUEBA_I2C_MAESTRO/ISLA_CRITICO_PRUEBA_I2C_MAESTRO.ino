/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_I2C_MAESTRO  -  Isla del Sol, micro CRÍTICO (A1)
 *  Protocolo de banco, Parte C — ETAPA 3-B: el I2C, pero al revés
 * -----------------------------------------------------------------------------
 *  QUÉ CAMBIA CONTRA LA ETAPA 3
 *
 *    Los papeles **del bus** están volteados: el CRÍTICO es maestro del bus
 *    y el AUX es esclavo en 0x21. Mismo protocolo, mismo CRC, mismo
 *    contenido: sólo cambia quién abre la conversación.
 *
 *    ⚠️ Y SÓLO ESO. **El que manda sigue siendo el AUX**: es el panel, pide
 *    los perfiles, trae los permisos del BMS y tiene el AUX_KILL por
 *    hardware. El CRÍTICO lleva el reloj del bus; en cada transferencia
 *    nada más reporta su estado y recoge órdenes. Ver `enlace_maestro.h`
 *    para el por qué completo; en corto son dos cosas:
 *
 *      1) un esclavo no decide cuándo se abre el bus y se queda agarrado
 *         si el otro aborta; un maestro abre, manda, cierra, y tiene el
 *         reloj para destrabar.
 *      2) de maestro **desaparece el ATOMIC_BLOCK de 212 ciclos** que
 *         copiaba la foto con las interrupciones apagadas. Era el bloqueo
 *         más largo de todo el sistema — más que millis() — y es el
 *         principal sospechoso de los pasos perdidos de la etapa 3.
 *
 *  LO QUE VENIMOS A VER
 *
 *    SOBREPASOS=+0 en todos los reportes. Nada más. Si de maestro siguen
 *    apareciendo, el culpable no era mi ATOMIC_BLOCK sino la ISR de TWI
 *    del núcleo, y entonces toca bajar el tráfico o mover la medición del
 *    cuerpo al Timer2.
 *
 *    ⚠️ Ahora SOBREPASOS se reporta como **+N nuevos (total M)**. En la
 *    etapa 3 el contador era acumulado y nunca se reiniciaba, así que un
 *    solo evento dejaba el veredicto en FALLA para siempre y parecía que
 *    «sólo pasan los primeros cuatro». El veredicto ahora juzga los
 *    eventos NUEVOS de cada reporte.
 * -----------------------------------------------------------------------------
 *  LA INCÓGNITA GRANDE
 *
 *    De las tres cosas que comparten el micro, ésta es la que no podemos
 *    medir directo. El SPWM y el ADC son módulos nuestros y les pusimos
 *    enganches. **La ISR de TWI es de la biblioteca Wire de Arduino**, vive
 *    en el núcleo, y no la vamos a tocar.
 *
 *  Y NO HACE FALTA TOCARLA
 *
 *    Para eso sirve el histograma. En la etapa 1 lo armamos para cazar un
 *    bloqueo mío, y de paso quedó calibrado en tres puntos: con bloqueos de
 *    20, 40 y 66 ciclos, el corte del histograma los siguió exactamente.
 *
 *    > Un bloqueo de ancho W reparte las esperas entre 1 y W y no produce
 *    > ninguna mayor que W. El histograma SE CORTA en W.
 *
 *    Así que la ISR de TWI se mide **por su efecto**, que además es lo único
 *    que importa: cuánto le estorba al SPWM.
 *
 *  DÓNDE VA A APARECER
 *
 *    Así quedó el reparto en la etapa 2, y éstos son los cajones "limpios":
 *
 *        0        ~48 %     nadie estorba
 *        1-20     ~50 %     atomicidad de instrucciones, inevitable
 *        21-40     ~0.2 %   la sección crítica del lazo
 *        41-110    ~0.5 %   millis()
 *        111-160   ~0.01 %  casi vacío
 *        161-220    0       VACÍO en los 24 reportes
 *
 *    La ISR de TWI del núcleo de Arduino anda por los 150 ciclos. **Si eso
 *    es cierto, va a llenar el cajón de 111-160**, que hoy tiene dos cuentas.
 *    Lo que aparezca ahí es de TWI y de nadie más.
 *
 *  LA CALIBRACIÓN, IGUAL QUE EN LA ETAPA 1
 *
 *    El AUX manda con periodo ajustable. Al subir el tráfico el cajón debe
 *    crecer EN PROPORCIÓN. Si crece proporcional, está medido; si no, hay
 *    algo más y hay que buscarlo.
 *
 *        AUX cada 100 ms  →   10 sondeos/s   referencia
 *        AUX cada  20 ms  →   50 sondeos/s
 *        AUX cada   5 ms  →  200 sondeos/s   el caso duro
 *
 *    Cada sondeo son 4 bytes de ida y 16 de vuelta, y la ISR de TWI corre
 *    una vez por byte más las de dirección y paro: unas 22 por sondeo.
 *
 *  EL PRESUPUESTO QUE VENIMOS A GASTAR
 *
 *        SPWM   13.63 µs   medido en la etapa 2
 *        ADC     7.81 µs   contado en el binario
 *                ───────
 *                21.44 µs  de los 41.625 disponibles
 *                20.18 µs  es lo que queda para TWI
 *
 *  ESTO MIDE EL MÓDULO REAL
 *
 *    `enlace_maestro.cpp` tal como va a ir en producción, con el protocolo
 *    de `enlace_protocolo.h`: foto de estado de 16 bytes con CRC-8 de ida y
 *    comando de 4 bytes con CRC-8 de vuelta, cada 100 ms. No es tráfico
 *    inventado para la prueba.
 *
 *  PINES  (⚠️ J15 DESCONECTADO y etapa de potencia fuera)
 *       D4  = traza de la ISR del SPWM    D5  = traza de la ISR del ADC
 *       D12 = traza de la sección crítica
 *       A4 = SDA   A5 = SCL   ← al AUX
 *
 *  CONEXIÓN ENTRE LOS DOS NANO
 *       A4 <-> A4     A5 <-> A5     GND <-> GND
 *       ⚠️ NO unir los 5 V: cada Nano por su propio USB.
 *       Pull-up EXTERNAS de 8.2 kΩ de A4 y A5 a 5 V, una por línea.
 *       Con las internas (~40 kΩ) el flanco de subida tarda ~4 µs contra el
 *       límite de 1000 ns del modo estándar: cuatro veces fuera de norma.
 *       Con 8.2 kΩ en paralelo quedan 6.8 kΩ, τ = 340 ns y subida ~410 ns.
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

/*  CUERPO_PISO — el cuerpo más corto que la ISR del SPWM puede tener.
 *  Medido: 49 sin ADC, 57 con ADC. Se usa 40 para dejar margen.
 *
 *  Sirve para DESDOBLAR la lectura del cronómetro. En modo 8 el contador
 *  SUBE hasta TOP (333) y luego BAJA, así que una lectura r puede venir de
 *  dos tiempos distintos:
 *
 *      tau = r            si la ISR todavía no pasa el TOP
 *      tau = 666 - r      si ya lo pasó y el contador va de regreso
 *
 *  Como el cuerpo nunca baja de CUERPO_PISO, cualquier lectura menor que
 *  t0 + CUERPO_PISO sólo puede venir del regreso.
 *
 *  ⚠️ Esto apareció en la etapa 3: con entradas de 272 ciclos y cuerpo de
 *  140 el total pasa de 333, y el reporte mostraba cuerpos de 0, 2, 6, 15...
 *  cuando el real es 57.
 */
#define CUERPO_PISO             40

/*  Traza de la sección crítica del lazo en D12 (PB4 = EN_HW).
 *  Sube al entrar al ATOMIC_BLOCK y baja al salir, para poder ver en
 *  PulseView si las entradas tardías caen justo encima.
 *  ⚠️ Requiere la etapa de potencia DESCONECTADA, igual que el resto.
 */
#define TRAZA_SECCION_CRITICA   1

/*  Ciclos que mi propia sección crítica tiene las interrupciones apagadas,
 *  para que el sketch calcule cuánto robo se debe a sí mismo. Se cuentan en
 *  el binario:  avr-objdump -d firmware.elf  y buscar el cli ... out 0x3f.
 *  Con la sección mínima de la v5 son 10, y cada relleno agrega 14.
 *  Los dos números están contados en el binario, no estimados.
 */
#define CLI_PROPIO_CICLOS       (10 + 14 * CLI_RELLENO)

/*  CLI_RELLENO — lecturas de relleno DENTRO de la sección crítica, para
 *  alargarla a propósito.
 *
 *  Para qué: el histograma se corta en el ancho del bloqueo. Si corremos con
 *  0, 4 y 8 de relleno y el corte se mueve proporcionalmente, queda probado
 *  que el bloqueo es la sección crítica y, de paso, sale medido LO QUE
 *  CUESTA CADA BYTE LEÍDO BAJO cli() — que es el número que el firmware de
 *  producción necesita para presupuestar sus propias secciones críticas.
 *
 *  Anchos contados en el binario, exactamente lineales:
 *
 *      relleno 0 ->  10 ciclos        relleno 4 ->  66 ciclos
 *      relleno 2 ->  38 ciclos        relleno 8 -> 122 ciclos
 *
 *  El corte del histograma debe caer en esos valores. Si los sigue, el
 *  mecanismo queda probado y de paso sale la pendiente en ciclos por byte.
 */
#define CLI_RELLENO             0

/*  Cada cuántas muestras recogidas se leen los contadores de 32 bits.
 *  Leerlos en cada vuelta era lo que inflaba la sección crítica: ahora se
 *  consultan 46 veces por segundo en vez de 55 000.
 */
#define VERIFICA_CADA           512

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
#include "adc_portadora.h"
#include "traza_adc.h"   // sólo por TRZ_ADC_BIT; las macros las usa el módulo
#include "enlace_maestro.h"

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
#define CAJONES  9
static const uint8_t TOPE[CAJONES] = { 0, 10, 20, 40, 70, 110, 160, 220, 255 };
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
static uint32_t dobladas; // lecturas que pasaron el TOP y hubo que desdoblar
static uint16_t sobPrev;  // sobrepasos ya reportados, para contar los nuevos
static uint32_t vueltas;  // vueltas del lazo, para calcular el robo propio
static uint16_t desdeCheck;        // muestras desde la última lectura de isr_n
volatile uint16_t relleno;         // destino de las lecturas de relleno
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
  DDRD  |= _BV(TRZ_ADC_BIT);  // D5 = CARG_CMD, traza de la ISR del ADC
  PORTD &= ~_BV(TRZ_ADC_BIT);

  Serial.println();
  Serial.println(F("================================================================"));
  Serial.println(F("  ISLA DEL SOL - Parte C, ETAPA 3-B: SPWM + ADC + I2C MAESTRO"));
  Serial.println(F("  El CRITICO abre el bus: reporta al AUX (0x21) cada 100 ms"));
  Serial.println(F("  (maestro del bus; el que manda sigue siendo el AUX)"));
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

  Serial.print(F("  cli propio: "));
  Serial.print(CLI_PROPIO_CICLOS);
  Serial.print(F(" ciclos nominales, relleno = "));
  Serial.println(CLI_RELLENO);

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
  dobladas = 0;
  sobPrev  = 0;
  vueltas = 0;
  for (uint8_t i = 0; i < CAJONES; i++) histo[i] = 0;

  /*  El ADC se prepara ANTES de arrancar el SPWM: su primera conversión
   *  tarda 25 ciclos en vez de 13 porque despierta la lógica analógica, y
   *  esa la hacemos aquí a mano para que no le cueste al lazo ya corriendo.
   */
  adc_preparar();
  adc_reiniciar_contadores();

  /*  El maestro I2C. De aquí en adelante es el CRÍTICO quien abre el bus,
   *  cuando él quiere. Cada byte sigue disparando una ISR de TWI que compite
   *  con el SPWM — eso no cambia; lo que cambia es que ya no hay nadie
   *  preguntando a destiempo, y que la foto se arma sin apagar nada.
   */
  enlace_iniciar();

  spwm_arrancar();
  spwm_fijar_amplitud(spwm_amplitud_max());   // caso peor: la tabla trabaja

  isrPrev  = 0;
  isrCorte = (uint32_t)ISR_POR_SEGUNDO * SEGUNDOS_POR_REPORTE;
}

// ================================== LAZO =====================================

void loop()
{
  uint16_t t0, t1;
  uint8_t  hay;

  vueltas++;

  /*  El enlace se atiende en cada vuelta, como en producción. Cuando toca
   *  (cada 100 ms) abre la transferencia completa y cierra; el resto de las
   *  vueltas regresa en seguida. Los valores van fijos: esta etapa mide el
   *  COSTO del tráfico, no el contenido.
   */
  enlace_atender(0, 0, false, 480, 0, 0, false);

  /*  SECCIÓN CRÍTICA MÍNIMA — ésta es la diferencia de la v5.
   *
   *  La v4 leía aquí también los dos contadores (isr_n de 32 bits y
   *  isr_sobrepasos), once bytes en total, y lo hacía 55 000 veces por
   *  segundo. Eso era el bloqueo que llenaba el cajón de 21-40 ciclos.
   *
   *  Ahora sólo salen las tres cosas que cambian en cada ISR: cinco bytes.
   *  Los contadores se consultan cada VERIFICA_CADA muestras, más abajo.
   */
#if TRAZA_SECCION_CRITICA
  PORTB |= _BV(PB4);
#endif
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    hay = isr_lista;
    t0  = isr_t0;
    t1  = isr_t1;
    isr_lista = 0;
#if CLI_RELLENO >= 1
    relleno += isr_t1;
#endif
#if CLI_RELLENO >= 2
    relleno += isr_t0;
#endif
#if CLI_RELLENO >= 3
    relleno += isr_t1;
#endif
#if CLI_RELLENO >= 4
    relleno += isr_t0;
#endif
#if CLI_RELLENO >= 5
    relleno += isr_t1;
#endif
#if CLI_RELLENO >= 6
    relleno += isr_t0;
#endif
#if CLI_RELLENO >= 7
    relleno += isr_t1;
#endif
#if CLI_RELLENO >= 8
    relleno += isr_t0;
#endif
  }
#if TRAZA_SECCION_CRITICA
  PORTB &= ~_BV(PB4);
#endif

  if (hay && t1 >= t0) {
    /*  Desdoblado: ver CUERPO_PISO. Sin esto, cuando la ISR pasa el TOP el
     *  cronómetro lee de regreso y el cuerpo sale absurdamente corto.
     */
    uint16_t tau1 = t1;
    if (tau1 < (uint16_t)(t0 + CUERPO_PISO)) {
      tau1 = (uint16_t)(2UL * TOP_CUENTAS - t1);
      dobladas++;
    }

    mete(entrada, t0);
    mete(cuerpo,  (uint16_t)(tau1 - t0));
    mete(ocupa,   (uint16_t)(tau1 + EPILOGO_CICLOS));
    histoMete((t0 > PISO_CICLOS) ? (uint16_t)(t0 - PISO_CICLOS) : 0);
    if (t0 > UMBRAL_TARDE) tarde++;
  }

  // Los contadores de 32 bits, fuera de la ruta caliente: 46 veces por
  // segundo en vez de 55 000.
  if (++desdeCheck < VERIFICA_CADA) return;
  desdeCheck = 0;

  uint32_t n;
  uint16_t sob;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    n   = isr_n;
    sob = isr_sobrepasos;
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

  // Se lee ANTES del veredicto y antes de reiniciar los contadores.
  const uint16_t adcPerdidas = adc_perdidas();

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
  Serial.print(F("  dobladas="));
  Serial.print(dobladas);

  Serial.print(F("  robo="));
  Serial.print(entrada.maximo - entrada.minimo);
  Serial.print(F("c tarde="));
  Serial.print(tarde);

  /*  Nuevos de ESTE reporte, no el acumulado. El contador de la ISR nunca
   *  se reinicia (reiniciarlo pediría apagar interrupciones), así que la
   *  resta se hace aquí.
   */
  const uint16_t sobNuevos = (uint16_t)(sob - sobPrev);
  sobPrev = sob;

  Serial.print(F("  SOBREPASOS=+"));
  Serial.print(sobNuevos);
  Serial.print(F(" (total "));
  Serial.print(sob);
  Serial.print(')');

  // --- veredicto contra los umbrales de la Parte C, seccion C1 --------------
  const uint32_t periodo = 2UL * TOP_CUENTAS;
  Serial.print(F("  -> "));
  if (sobNuevos)                                 Serial.println(F("FALLA (paso perdido)"));
  else if (adcPerdidas)                          Serial.println(F("FALLA (disparo de ADC perdido)"));
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

  // El CORTE: donde el histograma acumula 99.5 %. Es el ancho del bloqueo
  // mas grande que ocurre con frecuencia apreciable, y es lo que identifica
  // al culpable. Con 0 relleno deberia caer mucho mas abajo que en la v4.
  {
    const uint32_t meta = ((uint32_t)cuerpo.n * 995UL) / 1000UL;
    uint32_t ac = 0;
    uint8_t  i  = 0;
    for (; i < CAJONES; i++) { ac += histo[i]; if (ac >= meta) break; }
    Serial.print(F("   corte<="));
    if (i >= CAJONES - 1) Serial.print(F(">160"));
    else                  Serial.print(TOPE[i]);
    Serial.print('c');
  }

  // Fraccion que sufrio ALGUN retraso, en partes por mil.
  Serial.print(F(" bloqueadas="));
  Serial.print((uint16_t)(((cuerpo.n - histo[0]) * 1000UL) / cuerpo.n));
  Serial.print(F("/1000"));

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

  // ---------- tercera linea: el ADC --------------------------------------
  {
    const uint32_t mi = adc_muestras(ADC_RANURA_INV);
    const uint32_t mr = adc_muestras(ADC_RANURA_RED);
    const uint32_t mb = adc_muestras(ADC_RANURA_BAT);
    const uint16_t pe = adcPerdidas;

    Serial.print(F("   ADC inv/red/bat="));
    Serial.print(mi); Serial.print('/');
    Serial.print(mr); Serial.print('/');
    Serial.print(mb);

    // El reparto de DC-025 debe salir 2:1:1. Se reporta en centesimas para
    // no necesitar punto flotante: 200 = exactamente el doble.
    if (mr) { Serial.print(F(" razon=")); Serial.print((uint16_t)((mi * 100UL) / mr)); Serial.print(F("/100")); }

    // Esperadas: una conversion cada 4 desbordes.
    const uint32_t esp = corrieron / ADC_DESBORDES_POR_DISPARO;
    Serial.print(F(" esperadas=")); Serial.print(esp);

    Serial.print(F(" PERDIDAS=")); Serial.print(pe);

    Serial.print(F("  cuentas=")); Serial.print(adc_cuentas(ADC_RANURA_INV));
    Serial.print('/');             Serial.print(adc_cuentas(ADC_RANURA_RED));
    Serial.print('/');             Serial.print(adc_cuentas(ADC_RANURA_BAT));

    if (pe) Serial.print(F("   <<< NO CABE: revisar ADC_DESBORDES_POR_DISPARO"));
    adc_reiniciar_contadores();
  }

  // ---------- cuarta linea: el enlace ------------------------------------
  Serial.print(F("   I2C(maestro) ok="));
  Serial.print(enlace_envios_ok());
  Serial.print(F(" CRCmalo="));
  Serial.print(enlace_envios_malos());
  Serial.print(F(" sinResp="));
  Serial.print(enlace_sin_respuesta());
  Serial.print(F(" usMax="));
  Serial.print(enlace_us_max());
  Serial.print(F(" seq="));
  Serial.print(enlace_ultimo_seq());
  Serial.print(enlace_aux_presente() ? F(" AUX-OK") : F(" AUX-AUSENTE"));

  // El piso tiene que ser el que creemos; si no, todo lo demas se corre.
  /*  Si la ENTRADA misma se acerca al TOP, el instrumento se queda sin
   *  margen: pasando 333 ni siquiera t0 se puede desdoblar, porque no hay
   *  un piso conocido contra el cual compararlo.
   */
  if (entrada.maximo > 300) {
    Serial.print(F("   !! entrada="));
    Serial.print(entrada.maximo);
    Serial.print(F(" cerca de TOP="));
    Serial.print(TOP_CUENTAS);
    Serial.print(F(": el instrumento pierde margen"));
  }

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
  dobladas = 0;
  vueltas = 0;
  for (uint8_t i = 0; i < CAJONES; i++) histo[i] = 0;
}
