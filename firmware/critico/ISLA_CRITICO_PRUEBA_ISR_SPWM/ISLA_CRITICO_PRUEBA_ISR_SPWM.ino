/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_ISR_SPWM  -  Isla del Sol, micro CRÍTICO (A1)
 *  Protocolo de banco, Parte C — ETAPA 1: el SPWM solo, y cuánto dura su ISR
 * -----------------------------------------------------------------------------
 *  QUÉ CONTESTA
 *    La única pregunta del presupuesto que seguimos estimando y nunca medimos:
 *    ¿cuánto dura de verdad la ISR del SPWM? De ese número cuelgan el margen
 *    de DC-024, el reparto de DC-025 y el presupuesto de DC-017.
 *
 *    Aquí corre el SPWM Y NADA MÁS. Sin ADC, sin I2C, sin comparador. El
 *    número que salga es el piso: todo lo que se agregue después se suma.
 *
 *  POR QUÉ NO ES UNA COPIA DE LA ISR
 *    Este sketch INCLUYE el módulo real (../ISLA_CRITICO/spwm_328p.cpp). No
 *    reimplementa nada. Mide la ISR que se va a producción, no una versión
 *    parecida que se desincroniza en tres semanas.
 *
 *    El módulo trae dos enganches, SPWM_TRAZA_ENTRA() y SPWM_TRAZA_SALE(),
 *    que en producción se expanden a NADA. Está verificado: el .o compilado
 *    con y sin ellos da el mismo md5. Aquí se definen antes de incluirlo.
 *
 *  LAS DOS MEDICIONES, Y POR QUÉ HACEN FALTA LAS DOS
 *
 *    1) POR SOFTWARE, con TCNT1 — es la buena.
 *       El Timer1 va en modo 8 y TOV1 se levanta en BOTTOM, con TCNT1 = 0.
 *       O sea que TCNT1 leído dentro de la ISR dice EXACTAMENTE cuántos ciclos
 *       pasaron desde que la interrupción se disparó, y eso YA INCLUYE la
 *       latencia del vector y el prólogo del compilador.
 *       Falta sumarle el epílogo, que es fijo: EPILOGO_CICLOS.
 *
 *    2) POR PIN, D4 — para el analizador lógico.
 *       ⚠️ EL PIN MIENTE POR ABAJO, Y NO POR POCO. No puede ver el prólogo
 *       (16 push + in + eor = 32 ciclos), ni el epílogo (16 pop + out + reti
 *       = 35 ciclos), ni la latencia del vector (7 ciclos). Son 74 ciclos =
 *       4.62 µs que NO aparecen en el pin — el 38 % del costo total.
 *       Quien mida sólo con el pin va a leer ~7.4 µs y creer que le sobra
 *       muchísimo, cuando el costo real es ~12 µs.
 *       El pin sirve para el JITTER y para ver colisiones, no para el costo.
 *
 *  LO QUE LA SIMULACIÓN YA PREDIJO (avr-gcc -Os + simavr, 22-nov-2026)
 *       mínimo   7.25 µs   116 ciclos   17.4 % del periodo
 *       medio   11.43 µs   183 ciclos   27.5 %
 *       máximo  12.07 µs   193 ciclos   29.0 %
 *    Si el banco da algo muy distinto, el que está mal es uno de los dos y
 *    hay que averiguar cuál ANTES de seguir con las etapas 2 a 5.
 *
 *  PINES  (⚠️ J15 DESCONECTADO — ver Parte C, sección C0)
 *       D4  = traza de la ISR        (normalmente INV_CMD, K4+K2)
 *       D12 = traza de la ISR#2      (normalmente EN_HW; con J15 fuera y la
 *             etapa de potencia desconectada, el 74HC08 no deja pasar nada)
 *       D9/D10 = HA/LA   D11/D3 = HB/LB   (las salidas del SPWM, para el
 *             analizador; se ven en el pin aunque EN_HW esté picado)
 *
 *  CÓMO SE USA
 *       1. J15 FUERA. Etapa de potencia desconectada. Verificar a ojo.
 *       2. Cargar, abrir el monitor serie a 115200.
 *       3. Dejarlo correr un minuto. Anotar la línea de REPORTE.
 *       4. Pasar los números a la hoja de registro de la Parte C, etapa 1.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// ======================= CONFIGURACIÓN DE LA PRUEBA ==========================

// Pin de traza para el analizador. PD4 = D4 (INV_CMD con J15 puesto).
#define TRZ_DDR    DDRD
#define TRZ_PORT   PORTD
#define TRZ_BIT    PD4

// Segunda traza, opcional, en D12 = PB4 (EN_HW). Poner en 0 para no usarla.
#define USA_TRAZA_D12   1

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
 *
 *  El número reportado trae además ~6 ciclos (0.37 µs) de la propia
 *  instrumentación —subir la traza y guardar isr_t0— así que peca de
 *  PESIMISTA, que es el lado correcto para un presupuesto.
 */
#define EPILOGO_CICLOS   35

#define REPORTE_MS       1000UL
#define RELOJ_MHZ        16UL

// ========================= EL MÓDULO REAL, CON ENGANCHES =====================

// Las dos macros tienen que quedar definidas ANTES de incluir el módulo.
// Nota sobre el orden: la traza del pin sube ANTES de leer TCNT1 y baja
// DESPUÉS de guardarlo, para que el pin nunca mida menos que el software.
volatile uint16_t isr_t0;          // TCNT1 al entrar (vector + prólogo ya idos)
volatile uint16_t isr_t1;          // TCNT1 al salir, antes del epílogo
volatile uint8_t  isr_lista;       // bandera: hay una muestra nueva

#define SPWM_TRAZA_ENTRA()                      \
  do {                                          \
    TRZ_PORT |= _BV(TRZ_BIT);                   \
    isr_t0 = TCNT1;                             \
  } while (0)

#define SPWM_TRAZA_SALE()                       \
  do {                                          \
    isr_t1 = TCNT1;                             \
    isr_lista = 1;                              \
    TRZ_PORT &= ~_BV(TRZ_BIT);                  \
  } while (0)

#include "../ISLA_CRITICO/spwm_328p.cpp"

// ============================== ESTADÍSTICAS =================================

struct Acum {
  uint32_t n;
  uint32_t suma;        // ciclos acumulados (cabe: 193 × 48000 × 1 s = 9.3e6)
  uint16_t minimo;
  uint16_t maximo;
  uint16_t entradaMin;  // el TCNT1 de entrada más chico visto = vector+prólogo
  uint16_t entradaMax;
};

static Acum ac;
static uint32_t tReporte;
static uint32_t perdidas;   // ISRs que corrieron sin que el lazo las recogiera

static void acumReset(Acum &a)
{
  a.n = 0;
  a.suma = 0;
  a.minimo = 0xFFFF;
  a.maximo = 0;
  a.entradaMin = 0xFFFF;
  a.entradaMax = 0;
}

static void acumMete(Acum &a, uint16_t ciclos, uint16_t entrada)
{
  a.n++;
  a.suma += ciclos;
  if (ciclos  < a.minimo)     a.minimo     = ciclos;
  if (ciclos  > a.maximo)     a.maximo     = ciclos;
  if (entrada < a.entradaMin) a.entradaMin = entrada;
  if (entrada > a.entradaMax) a.entradaMax = entrada;
}

// Imprime ciclos como microsegundos con dos decimales, sin punto flotante.
static void imprimeUs(uint32_t ciclos_x100)
{
  // ciclos_x100 = ciclos × 100;  µs × 100 = ciclos × 100 / 16
  const uint32_t us_x100 = (ciclos_x100 + RELOJ_MHZ / 2) / RELOJ_MHZ;
  Serial.print(us_x100 / 100);
  Serial.print('.');
  const uint8_t d = us_x100 % 100;
  if (d < 10) Serial.print('0');
  Serial.print(d);
}

// Imprime el porcentaje del periodo de portadora con un decimal.
static void imprimePorciento(uint32_t ciclos_x100)
{
  const uint32_t pc_x10 = (ciclos_x100 * 10UL + TOP_CUENTAS) / (2UL * TOP_CUENTAS);
  Serial.print(pc_x10 / 10);
  Serial.print('.');
  Serial.print(pc_x10 % 10);
}

// ================================= SETUP =====================================

void setup()
{
  Serial.begin(115200);
  while (!Serial) { }

  TRZ_DDR |= _BV(TRZ_BIT);
  TRZ_PORT &= ~_BV(TRZ_BIT);
#if USA_TRAZA_D12
  DDRB  |= _BV(PB4);
  PORTB &= ~_BV(PB4);
#endif

  Serial.println();
  Serial.println(F("================================================================"));
  Serial.println(F("  ISLA DEL SOL - Parte C, ETAPA 1: duracion de la ISR del SPWM"));
  Serial.println(F("================================================================"));
  Serial.println(F("  !! J15 DESCONECTADO y etapa de potencia fuera. Verificar."));
  Serial.println();

  if (!spwm_preparar(DT_LEGACY_CUENTAS, LINEA_60_HZ)) {
    Serial.println(F("  ERROR: spwm_preparar fallo. Detenido."));
    for (;;) { }
  }

  Serial.print(F("  portadora   : TOP = "));
  Serial.print(TOP_CUENTAS);
  Serial.print(F(" cuentas, periodo = "));
  imprimeUs((uint32_t)2 * TOP_CUENTAS * 100);
  Serial.println(F(" us"));

  Serial.print(F("  linea       : "));
  Serial.print(spwm_frecuencia_hz());
  Serial.print(F(" Hz, "));
  Serial.print(spwm_muestras_ciclo());
  Serial.println(F(" muestras por ciclo"));

  Serial.print(F("  tiempo muerto: "));
  Serial.print(spwm_dt_cuentas());
  Serial.print(F(" cuentas   amplitud max: "));
  Serial.println(spwm_amplitud_max());

  Serial.print(F("  epilogo sumado: "));
  Serial.print(EPILOGO_CICLOS);
  Serial.println(F(" ciclos (no medible desde adentro)"));
  Serial.println();
  Serial.println(F("  Prediccion de simavr: min 116 / med 183 / max 193 ciclos"));
  Serial.println(F("----------------------------------------------------------------"));

  acumReset(ac);

  spwm_arrancar();
  spwm_fijar_amplitud(spwm_amplitud_max());   // caso peor: la tabla trabaja

  tReporte = millis();
}

// ================================== LAZO =====================================

void loop()
{
  // Recoger la última muestra de la ISR.
  uint16_t t0, t1;
  uint8_t  hay;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    hay = isr_lista;
    t0  = isr_t0;
    t1  = isr_t1;
    isr_lista = 0;
  }

  if (hay) {
    // t1 es TCNT1 al final del cuerpo; como TOV1 se levanta en BOTTOM con
    // TCNT1 = 0, t1 YA trae la latencia del vector y el prologo adentro.
    // Solo falta el epilogo, que es fijo.
    if (t1 >= t0) {                       // defensa: si el timer dio la vuelta
      acumMete(ac, t1 + EPILOGO_CICLOS, t0);
    } else {
      perdidas++;
    }
  }

  if (millis() - tReporte < REPORTE_MS) return;
  tReporte += REPORTE_MS;

  if (ac.n == 0) {
    Serial.println(F("  sin muestras (la ISR no esta corriendo?)"));
    return;
  }

  const uint32_t medio_x100 = (ac.suma * 100UL + ac.n / 2) / ac.n;

  Serial.print(F("REPORTE  n="));
  Serial.print(ac.n);
  Serial.print(F("  ciclos min/med/max = "));
  Serial.print(ac.minimo);
  Serial.print('/');
  Serial.print((uint16_t)(medio_x100 / 100));
  Serial.print('/');
  Serial.print(ac.maximo);

  Serial.print(F("   us = "));
  imprimeUs((uint32_t)ac.minimo * 100);
  Serial.print('/');
  imprimeUs(medio_x100);
  Serial.print('/');
  imprimeUs((uint32_t)ac.maximo * 100);

  Serial.print(F("   max = "));
  imprimePorciento((uint32_t)ac.maximo * 100);
  Serial.print(F(" % del periodo"));

  Serial.print(F("   [vector+prologo "));
  Serial.print(ac.entradaMin);
  Serial.print('-');
  Serial.print(ac.entradaMax);
  Serial.print(F(" c]"));

  if (perdidas) {
    Serial.print(F("   PERDIDAS="));
    Serial.print(perdidas);
  }

  // Veredicto contra los umbrales de la Parte C, seccion C1.
  const uint16_t periodo = 2 * TOP_CUENTAS;          // 666 ciclos
  Serial.print(F("   -> "));
  if (ac.maximo * 10UL <= periodo * 7UL)        Serial.println(F("PASA"));
  else if (ac.maximo * 10UL <= periodo * 9UL)   Serial.println(F("AVISO"));
  else                                          Serial.println(F("FALLA"));

  acumReset(ac);
}
