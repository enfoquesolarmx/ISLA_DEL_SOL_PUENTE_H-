/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_AC_COMPARADOR  -  Isla del Sol, banco de pruebas
 *  Lectura AC con la ventana delimitada por el COMPARADOR ANALÓGICO (DC-023)
 * -----------------------------------------------------------------------------
 *  QUÉ CAMBIA CONTRA ISLA_CRITICO_PRUEBA_AC_1CANAL
 *
 *    Aquel sketch cierra la ventana cada 300 muestras. Eso da 3.00616 ciclos,
 *    no 3.00000, y el pedacito sobrante rota de fase: en banco se midió un
 *    BATIDO determinístico de 77 mV con periodo de 4 reportes, que explica el
 *    86 % de la dispersión del RMS. Promediar 20 ventanas no lo quita, porque
 *    los errores están correlacionados — y además promediar RAÍCES no es lo
 *    mismo que la raíz del total.
 *
 *    Aquí la ventana se cierra en un CRUCE POR CERO detectado por hardware,
 *    así que son ciclos enteros por construcción. Y los acumuladores corren
 *    sobre el reporte completo con UNA sola raíz al final.
 *
 *      sigma del RMS     82.8 mV  ->  ~30.6 mV esperados
 *      frecuencia        atada a millis()  ->  al nivel del cristal
 *      peor caso ISR ADC 28 us  ->  ~13 us  (se va la division de 32 bits)
 *
 *    El tercer renglón importa más de lo que parece: la interpolación del
 *    cruce vivía DENTRO de la ISR del ADC y llevaba una división de 32 bits,
 *    ~250 ciclos. Contra el plazo de 41.6 µs del SPWM, el comparador BAJA la
 *    latencia peor en vez de subirla.
 *
 *  CABLEADO — dos alambres, cero componentes
 *
 *      D6 (AIN0)  <-  el NODO del divisor, donde está el capacitor de 10 nF
 *                     (el mismo nodo que ve A6; NO del pin A6 del Nano)
 *      D7 (AIN1)  <-  la referencia de BIAS, el punto medio de 10k/10k
 *
 *    El comparador voltea justo cuando la componente AC cruza el bias. Las
 *    dos entradas ya existen en el circuito: no se agrega ni una pieza.
 *
 *    ⚠️ Tómela en el nodo del capacitor, no al final de una pista larga. El
 *    filtro protege el NODO, no el cable que sale de él: un trazo largo es
 *    una antena colgada de un punto de 9.4 kΩ. No es un problema de caída
 *    IR —por ahí no pasa corriente— así que no se arregla con una toma
 *    Kelvin: se arregla con un cable CORTO.
 *
 *  EL CANDADO POR SOFTWARE
 *    El comparador del ATmega328P NO tiene histéresis. Cerca del cruce la
 *    señal sube a ~0.8 mV/µs y cualquier ruido produce varios flancos. Tras
 *    aceptar uno se ignoran los siguientes durante ~70 % de un ciclo.
 *
 *  LO QUE NO HACE
 *    No corre el SPWM. No es un PLL. No mide el sesgo entre canales (A7).
 *
 *  CÓMO SE USA
 *    1. Copiar VCC_MV, DIVISOR_X100 y RELACION_X1000 del sketch de A6 ya
 *       calibrado — son las mismas perillas y los mismos valores.
 *    2. Cablear D6 y D7 como arriba.
 *    3. Cargar, monitor serie a 115200.
 *
 *    Para comparar contra el método viejo: corra los dos sketches sobre la
 *    misma carga. Si el factor de forma coincide, el número es de la onda;
 *    si no, es del método.
 *
 *  Referencias: docs/decisiones_criticas.md     (DC-020, DC-023)
 *               docs/protocolo_banco_control.md (registro de A6)
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ======================== PERILLAS ===========================================
//   Canal analógico. 6 = A6 (la red, J17). 0 = A0 (el inversor, J2).
#define CANAL                  6

//   Riel de 5 V MEDIDO, en milivolts.
#define VCC_MV                 5000

//   Relación del divisor × 100. Medida, no nominal: es el secundario entre el
//   voltaje del pin. Banco 2026-11-21 con Rt = 110 kΩ: 14.3 V / 1220.7 mV.
#define DIVISOR_X100           1171

//   Razón de vueltas del transformador × 1000, medida con el multímetro en
//   LOS DOS lados (el error de ganancia del instrumento se cancela en el
//   cociente). Banco 2026-11-21: 120.1 / 14.3 = 8.3986.
#define RELACION_X1000         8399

//   Frecuencia nominal. Sólo dimensiona el candado y la ventana; la que se
//   reporta sale de los cruces medidos.
#define HZ_NOMINAL             60

//   Ciclos por reporte. 60 ≈ un segundo. Para cazar la forma de un escalón de
//   carga (compresor, cafetera, aspiradora) bájelo a 3: salen 20 reportes por
//   segundo, que es la resolución temporal que el instrumento de verdad tiene.
#define CICLOS_VENTANA         60

// ======================== CONSTANTES DERIVADAS ===============================
constexpr uint16_t TOP_CUENTAS = 333;                 // 24 024 Hz de portadora
constexpr uint32_t CONVERSION_NS = 104000;            // 13 ciclos del ADC a 125 kHz
constexpr uint8_t  PERIODOS_POR_DISPARO = 4;          // DC-020 + margen (ver A6)
constexpr uint32_t PERIODO_PORT_NS = 2UL * TOP_CUENTAS * 1000UL / 16UL;   // 41 625
constexpr uint32_t ENTRE_MUESTRAS_NS = PERIODO_PORT_NS * PERIODOS_POR_DISPARO;
constexpr uint16_t MUESTRAS_POR_S = (uint16_t)(1000000000UL / ENTRE_MUESTRAS_NS);
constexpr uint32_t DESBORDES_POR_S = 1000000000UL / PERIODO_PORT_NS;      // 24 024

// Candado del comparador: ~70 % de un ciclo. Suficiente para matar el rebote
// cerca del cruce y bastante menos que un ciclo, para no perder el siguiente.
constexpr uint32_t CANDADO_DESB = (DESBORDES_POR_S / HZ_NOMINAL) * 7UL / 10UL;

// ======================== ESTADO COMPARTIDO ==================================
// Acumuladores del REPORTE COMPLETO, no de sub-ventanas. Todo contra mediaRef,
// que es el bias de la ventana anterior: así d cabe en 16 bits con holgura y
// d² no desborda. Peor caso 6000 muestras × 512² = 1.57e9 de los 4.29e9 que da
// un uint32; en operación real anda en 3.8e8.
static volatile int32_t  sumaD;
static volatile uint32_t sumaCuadD, sumaAbs;
static volatile uint16_t n, pico;
static volatile int16_t  mediaRef = 512;

// Instantánea para el lazo principal
static volatile int32_t  sSumaD;
static volatile uint32_t sSumaCuadD, sSumaAbs;
static volatile uint16_t sN, sPico;
static volatile uint32_t sCierre;          // desbordes en el cruce de cierre
static volatile bool     hayVentana;

// Base de tiempo: desbordes del Timer1, uno cada 41.625 µs
static volatile uint32_t desbordes;
static volatile uint32_t ultimoCruce;
static volatile uint16_t ciclos;
static volatile bool     cerrar;
static volatile uint32_t cruces;           // contador corrido, para diagnóstico

static volatile uint32_t muestras, perdidos;
static volatile uint8_t  cuentaDisparo;

// ======================== INTERRUPCIONES =====================================
ISR(TIMER1_OVF_vect)
{
  desbordes++;
  if (++cuentaDisparo >= PERIODOS_POR_DISPARO) {
    cuentaDisparo = 0;
    if (ADCSRA & (1 << ADSC)) perdidos++;      // la conversión anterior no terminó
    else                      ADCSRA |= (1 << ADSC);
  }
}

// Cruce por cero ascendente, por hardware. AIN0 (la señal) sube por encima de
// AIN1 (el bias) exactamente en el cruce de la componente AC.
ISR(ANALOG_COMP_vect)
{
  const uint32_t t = desbordes;
  if (t - ultimoCruce < CANDADO_DESB) return;   // candado: el comparador no
  ultimoCruce = t;                              // tiene histéresis propia
  cruces++;
  if (++ciclos >= CICLOS_VENTANA) { sCierre = t; cerrar = true; }
}

ISR(ADC_vect)
{
  const int16_t  d  = (int16_t)ADC - mediaRef;
  const uint16_t ad = (d < 0) ? (uint16_t)(-d) : (uint16_t)d;
  muestras++;

  sumaD     += d;
  sumaCuadD += (uint32_t)((int32_t)d * d);
  sumaAbs   += ad;
  if (ad > pico) pico = ad;
  n++;

  // Aquí ya NO hay detección de cruce ni interpolación: eso se llevaba una
  // división de 32 bits (~250 ciclos = 15.6 µs) que estiraba esta ISR hasta
  // 28 µs de los 41.6 que tiene el SPWM. Ahora el peor caso es plano.
  if (cerrar && !hayVentana) {
    sSumaD = sumaD; sSumaCuadD = sumaCuadD; sSumaAbs = sumaAbs;
    sN = n; sPico = pico;
    sumaD = 0; sumaCuadD = 0; sumaAbs = 0; n = 0; pico = 0;
    ciclos = 0; cerrar = false;
    hayVentana = true;
  }
}

// ======================== AUXILIARES =========================================
static uint16_t raiz(uint32_t v)
{
  uint32_t r = 0, b = 1UL << 30;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
    else            { r >>= 1; }
    b >>= 2;
  }
  return (uint16_t)r;
}

static void imprimeDec(int32_t valor, int32_t escala, uint8_t decimales)
{
  if (valor < 0) { Serial.print('-'); valor = -valor; }
  Serial.print(valor / escala);
  if (!decimales) return;
  Serial.print('.');
  int32_t resto = valor % escala, div = escala / 10;
  while (decimales-- && div) { Serial.print((char)('0' + (resto / div) % 10)); div /= 10; }
}

// ======================== ARRANQUE ===========================================
void setup()
{
  Serial.begin(115200);
  while (!Serial) { }

  // Timer1 modo 8, TOP = ICR1, preescalador 1. Sin salidas: aquí no hay puente.
  TCCR1A = 0;
  TCCR1B = (1 << WGM13);
  ICR1   = TOP_CUENTAS;
  TCNT1  = 0;
  TIMSK1 = (1 << TOIE1);
  TCCR1B |= (1 << CS10);

  // ADC: referencia AVCC, canal fijo, /128 = 125 kHz (DC-020).
  ADMUX  = (1 << REFS0) | (CANAL & 0x0F);
  ADCSRA = (1 << ADEN) | (1 << ADIE) |
           (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  ADCSRA |= (1 << ADSC);                   // la primera tarda 25 ciclos: se tira
  while (ADCSRA & (1 << ADSC)) { }
  (void)ADC;

  // COMPARADOR ANALÓGICO (DC-023).
  //   AIN0 = PD6 = D6 (señal), AIN1 = PD7 = D7 (bias). Están cableados en
  //   silicio y no se pueden mover: por eso TN_CMD se mudó a D13 en la tarjeta.
  //
  //   DIDR1 apaga los buffers de entrada digital de esos dos pines. Importa:
  //   van a vivir en 2.5 V, el peor punto para una entrada digital — consume
  //   corriente de cortocircuito y mete ruido.
  DIDR1 = (1 << AIN1D) | (1 << AIN0D);
  //   ACME = 0 para que AIN1 sea el PIN y no el multiplexor del ADC (además
  //   ese modo exige el ADC apagado, y aquí corre). ACBG = 0 para que AIN0 sea
  //   el pin y no la referencia interna de 1.1 V, que daría un disparo que se
  //   mueve con la amplitud.
  ADCSRB &= (uint8_t)~(1 << ACME);
  //   La hoja de datos pide desarmar ACIE antes de tocar ACIS, limpiar ACI y
  //   recién entonces armar: cambiar ACIS con la interrupción viva puede
  //   generar un flanco fantasma.
  ACSR = (1 << ACIS1) | (1 << ACIS0);      // sólo flanco de SUBIDA, ACIE abajo
  ACSR |= (1 << ACI);                      // limpiar bandera pendiente
  ACSR |= (1 << ACIE);                     // armar

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" Isla del Sol - AC con ventana por COMPARADOR"));
  Serial.println(F("=================================================="));
  Serial.print(F(" Canal              : A")); Serial.println(CANAL);
  Serial.println(F(" Comparador         : AIN0 = D6 (senal), AIN1 = D7 (bias)"));
  Serial.print(F(" Candado            : "));
  imprimeDec((int32_t)(CANDADO_DESB * PERIODO_PORT_NS / 1000UL), 1000, 2);
  Serial.println(F(" ms tras cada cruce"));
  Serial.print(F(" Entre muestras     : "));
  imprimeDec(ENTRE_MUESTRAS_NS, 1000, 3); Serial.print(F(" us = "));
  Serial.print(MUESTRAS_POR_S); Serial.println(F(" muestras/s"));
  Serial.print(F(" Ocupacion del ADC  : "));
  imprimeDec((int32_t)(CONVERSION_NS * 1000UL / ENTRE_MUESTRAS_NS), 10, 1);
  Serial.println(F(" %"));
  Serial.print(F(" Ventana            : ")); Serial.print(CICLOS_VENTANA);
  Serial.print(F(" ciclos EXACTOS (~"));
  imprimeDec((int32_t)((uint32_t)CICLOS_VENTANA * 1000UL / HZ_NOMINAL), 1000, 3);
  Serial.println(F(" s)"));
  Serial.print(F(" VCC / divisor      : ")); Serial.print(VCC_MV);
  Serial.print(F(" mV  /  x")); imprimeDec(DIVISOR_X100, 100, 2);
  Serial.println();
  Serial.print(F(" Relacion a la red  : x")); imprimeDec(RELACION_X1000, 1000, 3);
  Serial.println();
  Serial.println(F(" Base de tiempo     : desbordes del Timer1, 41.625 us"));
  Serial.println();

  sei();
}

// ======================== LAZO PRINCIPAL =====================================
void loop()
{
  static uint32_t reporte = 0;
  static uint32_t cierreAnt = 0;
  static bool     hayAnt = false;

  if (!hayVentana) return;

  int32_t  vSumaD;
  uint32_t vCuadD, vAbs, vCierre;
  uint16_t vN, vPico;
  noInterrupts();
  vSumaD = sSumaD; vCuadD = sSumaCuadD; vAbs = sSumaAbs;
  vN = sN; vPico = sPico; vCierre = sCierre;
  hayVentana = false;
  interrupts();

  if (vN == 0) return;
  reporte++;

  // RMS en una pasada, contra mediaRef: var = Σd²/n − (Σd/n)². Una sola raíz
  // para TODA la ventana. El sketch anterior sacaba una raíz por sub-ventana y
  // promediaba las raíces, que no es la raíz del total y deja residuo.
  const int32_t  mediaD = (vSumaD >= 0) ? (vSumaD + vN / 2) / (int32_t)vN
                                        : (vSumaD - vN / 2) / (int32_t)vN;
  const uint32_t cuadMediaD = (uint32_t)((int32_t)mediaD * mediaD);
  const uint32_t varN = vCuadD / vN;
  const uint32_t var  = (varN > cuadMediaD) ? (varN - cuadMediaD) : 0;

  const uint32_t rms16 = raiz(var << 8);            // RMS en 1/16 de cuenta
  const uint32_t mab16 = (vAbs * 16UL + vN / 2) / vN;
  const int32_t  bias  = (int32_t)mediaRef + mediaD;

  noInterrupts();
  mediaRef = (int16_t)bias;                         // referencia de la siguiente
  interrupts();

  // VOLTAJE: de rms16 a la línea en UNA sola expresión, sin truncadas
  // intermedias. Encadenar divisiones enteras abría huecos: con el divisor a
  // ×1.08 el paso visible quedaba en ~200 mV y había valores IMPOSIBLES de
  // desplegar (se vio con el Fluke fijo en 119.8 y la lectura brincando).
  constexpr uint64_t DEN_SEC = 16ULL * 1023ULL * 100ULL;
  constexpr uint64_t DEN_RED = DEN_SEC * 1000ULL * 10ULL;
  const uint64_t numSec = (uint64_t)rms16 * VCC_MV * DIVISOR_X100;
  const uint32_t sec = (uint32_t)((numSec + DEN_SEC / 2) / DEN_SEC);
  const uint32_t red = (uint32_t)((numSec * RELACION_X1000 + DEN_RED / 2) / DEN_RED);
  const uint32_t rmsCent = (rms16 * 100UL + 8UL) / 16UL;

  Serial.print(F("#")); Serial.print(reporte);
  Serial.print(F("  bias ")); Serial.print(bias);
  Serial.print(F("  rms ")); imprimeDec(rmsCent, 100, 2);
  Serial.print(F(" cnt = ")); imprimeDec(sec, 1000, 3);
  Serial.print(F(" VAC  ->  RED ")); imprimeDec(red, 100, 2);
  Serial.print(F(" V"));

  // Factor de forma. OJO: este número trae un sistemático de ~0.4 % que
  // depende de la tasa de muestreo — se midió al pasar de 3 a 4 periodos con
  // la misma onda. Sirve para decir "la onda está dentro de ~0.4 % de una
  // senoidal", NO para certificar un patrón al 0.1 %.
  if (mab16 && rms16) {
    const uint32_t ff = (rms16 * 10000UL) / mab16;
    const uint32_t cf = ((uint32_t)vPico * 16000UL) / rms16;
    Serial.print(F(" | FF ")); imprimeDec(ff, 10000, 4);
    Serial.print(F(" | cresta ")); imprimeDec(cf, 1000, 3);
  }

  // FRECUENCIA, del comparador y el Timer1. Ya no pasa por millis(): la
  // ventana son CICLOS_VENTANA ciclos exactos y el span se mide en desbordes
  // de 41.625 µs. Sobre ~1 s eso da ±0.004 %, o sea ±0.0025 Hz — el límite
  // pasa a ser el cristal y no el instrumento.
  if (hayAnt && vCierre > cierreAnt) {
    const uint32_t span = vCierre - cierreAnt;      // desbordes
    const uint64_t perNs = (uint64_t)span * PERIODO_PORT_NS / CICLOS_VENTANA;
    if (perNs) {
      Serial.print(F(" | f = "));
      imprimeDec((int32_t)(1000000000000ULL / perNs), 1000, 3);
      Serial.print(F(" Hz"));
    }
  }
  cierreAnt = vCierre; hayAnt = true;

  // Diagnóstico: muestras de la ventana y disparos perdidos. La ventana son
  // ciclos exactos, así que las muestras que caen dentro dicen directamente
  // cuántas por ciclo hubo — y si faltan, se nota aquí.
  uint32_t perd;
  noInterrupts(); perd = perdidos; perdidos = 0; interrupts();
  Serial.print(F(" | ")); Serial.print(vN);
  Serial.print(F(" muestras/")); Serial.print(CICLOS_VENTANA);
  Serial.print(F(" ciclos = "));
  imprimeDec((int32_t)((uint32_t)vN * 100UL / CICLOS_VENTANA), 100, 2);
  Serial.print(F(" por ciclo | perdidos ")); Serial.print(perd);
  Serial.println();
}
