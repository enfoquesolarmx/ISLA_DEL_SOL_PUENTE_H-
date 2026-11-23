/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_ETAPA4  -  Isla del Sol, micro CRÍTICO (A1)
 *  Protocolo de banco, Parte C — ETAPA 4: la red y el comparador
 * -----------------------------------------------------------------------------
 *  CAMBIA LA PREGUNTA
 *
 *    Las etapas 1 a 3 preguntaban **¿cabe en el tiempo?** y la respuesta fue
 *    sí, con margen: 218 ciclos de costo propio (32.7 %), 51 % de ocupación
 *    peor y cero pasos perdidos hasta 20× el tráfico de I2C.
 *
 *    Esta etapa pregunta otra cosa: **¿lo que medimos es la realidad?**
 *
 *  LO QUE ENTRA, Y NO CUESTA UN COMPONENTE
 *
 *    1) La red de verdad en A6, por el transformador de 12 V (DC-006). El
 *       canal ya venía corriendo desde la etapa 2 —1 488 muestras/s, reparto
 *       2:1:1 exacto— pero sin señal conectada y sin nadie que las use. Aquí
 *       se cierra la cadena.
 *
 *    2) El comparador analógico interno para el cruce por cero (DC-023), con
 *       AIN0 = D6 (el divisor de red) y AIN1 = D7 (NEUTRO-GRID). Las dos
 *       entradas son nodos que ya existen; por eso TN_CMD se mudó a D13.
 *
 *    3) El RMS de medio ciclo de los dos canales, acumulado DENTRO de la ISR
 *       del ADC. Va ahí por la lección de la etapa 3: el lazo se queda ciego
 *       mientras hay tráfico I2C, y un RMS al que le faltan muestras cuando
 *       hay bus no es un RMS. Ver rms_ventana.h.
 *
 *    Con esto corren **las cuatro interrupciones juntas** por primera vez
 *    —Timer1, ADC, TWI y comparador— que es la colisión de cuatro que las
 *    ocho puntas de C3 están puestas para cachar.
 *
 *  EL PRESUPUESTO QUE HAY QUE VIGILAR
 *
 *    La ISR del ADC crece: a los ~7.8 µs de antes se le suma la acumulación
 *    del RMS —dos sumas y una multiplicación de 16×16— en los canales del
 *    inversor y de la red, o sea en 3 de cada 4 muestras.
 *
 *        SPWM            218 c = 13.63 µs   MEDIDO, etapas 2 y 3
 *        ADC + RMS      ~186 c = 11.61 µs   estimado en el binario
 *        comparador     ~180 c = 11.25 µs   estimado en el binario
 *                      ──────────────────
 *                       ~584 c = 36.49 µs   de los 41.625 del periodo = 88 %
 *
 *    ⚠️ **88 % es el caso de que las tres caigan en el mismo periodo**, y el
 *    criterio de C1 avisa arriba del 70 %. No quiere decir que vaya a pasar
 *    —la del comparador corre 120 veces por segundo contra 24 024 del SPWM,
 *    así que la coincidencia es rara— pero sí que esta etapa es la primera
 *    donde el presupuesto puede no alcanzar.
 *
 *    Y los dos números nuevos son ESTIMADOS, no medidos. Salen de contar el
 *    binario sumando todas las ramas y escalar por el factor que el SPWM dio
 *    entre esa cuenta y su medición real (0.588). Es una calibración de un
 *    punto: sirve para saber el orden, no para firmar.
 *
 *    Lo que sí es exacto es el aviso: si no cabe, SOBREPASOS deja de ser +0.
 *    Y las trazas de D4, D5 y D13 dicen quién fue.
 *
 *  LOS CRITERIOS (sección C5.4 del protocolo)
 *
 *    (a) cruces = 120/s exactos (60 Hz × 2)
 *    (b) rechazados = 0
 *    (c) frecuencia dentro de 0.01 Hz del reporte de la Parte A
 *    (d) RMS dentro de ±0.3 V del Fluke — coherencia entre instrumentos, no
 *        exactitud: el Fluke sólo garantiza ±2.30 V a 120 V y no avala el
 *        dígito de las décimas
 *    (e) σ del RMS del orden de los 12.5 mV que dio el comparador en A6
 *
 *  ⚠️ SEGURIDAD — ES LA PRIMERA VEZ QUE LA RED TOCA EL BANCO
 *
 *    El transformador se enchufa a la pared: hay 127 V en el primario. El
 *    transformador ES la barrera de aislamiento, y el Fluke mide el
 *    secundario. Antes de conectar el Nano, el orden de A4 del protocolo:
 *    armar el divisor, medir el pico con el multímetro con el pin del ADC al
 *    aire, y sólo con el pico confirmado abajo de 4.60 V conectar el Nano.
 *
 *    J15 DESCONECTADO y etapa de potencia fuera, igual que siempre. Conmutar
 *    es la etapa 5, no ésta.
 *
 *  EL I2C SIGUE IGUAL QUE EN LA ETAPA 3-B
 *
 *    El CRÍTICO es maestro del bus y el AUX esclavo en 0x21.
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

/*  CALIBRACIÓN DE LOS CANALES AC — milivolts de línea por cuenta RMS, ×100.
 *
 *  Depende de dónde quedó el trimmer, así que no se puede saber de antemano:
 *  **la primera corrida calibra y la segunda valida.**
 *
 *  Con 0 el sketch imprime sólo cuentas. Para calibrar: correr así, anotar el
 *  RMS en cuentas y lo que marque el Fluke, y poner
 *
 *      MV_POR_CUENTA_X100 = redondear(mV_del_Fluke × 100 / cuentas_RMS)
 *
 *  Ejemplo: 127.0 V con 352 cuentas → 127000 × 100 / 352 = 36079.
 *  Es una calibración de un punto: vale para la zona donde se tomó.
 */
#define MV_POR_CUENTA_X100_RED  0
#define MV_POR_CUENTA_X100_INV  0

/*  Frecuencia de línea esperada, en milihertz, para el aviso de desvío.
 *  60.000 Hz nominales de la red; el inversor corre a 60.060 por diseño.
 */
#define RED_NOMINAL_MHZ         60000UL

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
#include "comparador_red.h"
#include "rms_ventana.h"
#include "traza_cmp.h"   // sólo por TRZ_CMP_BIT; las macros las usa el módulo

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
  DDRB  |= _BV(TRZ_CMP_BIT);  // D13 = TN_CMD, traza de la ISR del comparador
  PORTB &= ~_BV(TRZ_CMP_BIT);

  Serial.println();
  Serial.println(F("================================================================"));
  Serial.println(F("  ISLA DEL SOL - Parte C, ETAPA 4: + la red y el comparador"));
  Serial.println(F("  Las cuatro ISR juntas: SPWM + ADC/RMS + TWI + comparador"));
  Serial.println(F("================================================================"));
  Serial.println(F("  !! J15 DESCONECTADO y etapa de potencia fuera. Verificar."));
  Serial.println(F("  !! D13 es TN_CMD: con J15 puesto, esta traza castanetea K5."));
  Serial.println(F("  !! 127 V en el primario del transformador. Es la barrera."));
#if (MV_POR_CUENTA_X100_RED == 0) || (MV_POR_CUENTA_X100_INV == 0)
  Serial.println(F("  -> SIN CALIBRAR: se imprimen cuentas. Ver MV_POR_CUENTA_X100."));
#endif
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

  /*  El comparador y el RMS. El comparador se prepara antes de arrancar el
   *  SPWM para que los pines queden como entradas sin pull-up desde el
   *  principio: un pull-up sobre el divisor movería el bias, y con él el
   *  cruce que venimos a medir.
   */
  cmp_preparar();
  rms_preparar();
  est_cero(est[RMS_INV]);
  est_cero(est[RMS_RED]);

  spwm_arrancar();
  spwm_fijar_amplitud(spwm_amplitud_max());   // caso peor: la tabla trabaja

  cmp_arrancar();                             // de aquí en adelante, 120 ISR/s
  cmp_reiniciar();

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

  /*  Las ventanas de RMS ya cerradas. Se recogen en cada vuelta: cierran 120
   *  veces por segundo y el lazo gira más de 10 000, así que sobra margen
   *  incluso con el lazo bloqueado por el I2C la mitad del tiempo.
   */
  {
    uint16_t r, cd;
    uint8_t  n;
    if (rms_recoger(RMS_INV, &r, &cd, &n)) est_mete(est[RMS_INV], r, cd, n);
    if (rms_recoger(RMS_RED, &r, &cd, &n)) est_mete(est[RMS_RED], r, cd, n);
  }

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

  // ---------- quinta linea: la red, el cruce y el RMS ---------------------
  {
    const uint16_t cruces = cmp_cruces(), rech = cmp_rechazados(), hue = cmp_huecos();
    const uint32_t fmhz = cmp_frecuencia_mhz();

    Serial.print(F("   RED "));
    Serial.print(cmp_red_presente() ? F("presente") : F("AUSENTE "));
    Serial.print(F(" cruces="));
    Serial.print(cruces);
    Serial.print(F(" rech="));
    Serial.print(rech);
    Serial.print(F(" huecos="));
    Serial.print(hue);

    Serial.print(F("  f="));
    if (fmhz) {
      Serial.print(fmhz / 1000);
      Serial.print('.');
      const uint16_t d = (uint16_t)(fmhz % 1000);
      if (d < 100) Serial.print('0');
      if (d < 10)  Serial.print('0');
      Serial.print(d);
      Serial.print(F(" Hz"));
      const int32_t desvio = (int32_t)fmhz - (int32_t)RED_NOMINAL_MHZ;
      Serial.print(F(" ("));
      if (desvio >= 0) Serial.print('+');
      Serial.print(desvio);
      Serial.print(F(" mHz)"));
    } else {
      Serial.print(F("---"));
    }

    Serial.print(F("  semiper="));
    Serial.print(cmp_semiperiodo_us());
    Serial.print(F("us"));

    // Criterio (a): 120 cruces por segundo exactos, y (b) rechazados en 0.
    const uint16_t crucesEsperados = (uint16_t)(120 * SEGUNDOS_POR_REPORTE);
    if (cmp_red_presente() && (cruces < crucesEsperados - 1 || cruces > crucesEsperados + 1))
      Serial.print(F("   <<< CRUCES FUERA DE 120/s"));
    if (rech) Serial.print(F("   <<< CRUCE CON RUIDO: revisar antes de cerrar el lazo"));

    Serial.println();
    cmp_reiniciar();
  }

  // ---------- sexta linea: las dos ventanas de RMS ------------------------
  {
    uint32_t prom, sigma;

    Serial.print(F("   RMS red "));
    est_resumen(est[RMS_RED], &prom, &sigma);
    if (est[RMS_RED].vent) {
      imprime_ac(prom, MV_POR_CUENTA_X100_RED);
      Serial.print(F("  sigma="));
      imprime_ac(sigma, MV_POR_CUENTA_X100_RED);
      Serial.print(F("  CD="));
      Serial.print(est[RMS_RED].cdUlt);
      Serial.print(F(" n="));
      Serial.print(est[RMS_RED].nUlt);
      Serial.print(F(" vent="));
      Serial.print(est[RMS_RED].vent);
    } else {
      Serial.print(F("sin ventanas"));
    }

    Serial.print(F(" | inv "));
    est_resumen(est[RMS_INV], &prom, &sigma);
    if (est[RMS_INV].vent) {
      imprime_ac(prom, MV_POR_CUENTA_X100_INV);
      Serial.print(F("  CD="));
      Serial.print(est[RMS_INV].cdUlt);
      Serial.print(F(" n="));
      Serial.print(est[RMS_INV].nUlt);
      Serial.print(F(" vent="));
      Serial.print(est[RMS_INV].vent);
      // El inversor debe dar 25 muestras por ventana, exacto por construcción.
      if (est[RMS_INV].nUlt != RMS_MUESTRAS_INV)
        Serial.print(F("   <<< VENTANA DESALINEADA"));
    } else {
      Serial.print(F("sin ventanas"));
    }

    const uint16_t desb = rms_desbordes_red();
    if (desb) {
      Serial.print(F("   <<< ventanas de red sin cerrar: "));
      Serial.print(desb);
      Serial.print(F(" (comparador sin cruces?)"));
    }
    Serial.println();

    est_cero(est[RMS_INV]);
    est_cero(est[RMS_RED]);
    rms_reiniciar();
  }

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
