/*
 * =============================================================================
 *  ISLA_CRITICO_PRUEBA_AC_1CANAL  -  Isla del Sol, banco de pruebas
 *  Corrida A6: lectura de un solo canal AC contra el multímetro
 * -----------------------------------------------------------------------------
 *  QUÉ HACE
 *    Lee UN canal analógico con la disciplina de DC-020 (reloj del ADC a
 *    /128, conversión disparada desde el Timer1 en un punto fijo del periodo
 *    de portadora) y reporta una vez por segundo:
 *
 *      · el bias, en cuentas
 *      · el RMS, en cuentas, en milivolts del pin y en VAC del secundario
 *      · el FACTOR DE FORMA y, de ahí, el error del multímetro
 *      · el factor de cresta
 *      · la frecuencia de la fuente
 *      · los disparos perdidos
 *
 *  POR QUÉ UN SOLO CANAL
 *    Porque es lo que A6 necesita, y porque con dos canales hay una trampa:
 *
 *      ⚠️  UN PIN DE ADC AL AIRE NO LEE CERO: LEE EL CANAL ANTERIOR.
 *
 *    El capacitor de muestreo y retención del ADC guarda la carga de la
 *    conversión pasada. Si el canal que sigue está desconectado, no hay nada
 *    que descargue ese capacitor y la lectura sale casi idéntica a la
 *    anterior — un número perfectamente creíble que no mide nada. En banco
 *    se vio así: el canal conectado daba 14.26 VAC y el canal al aire daba
 *    14.12, el 99 %, con un retraso de exactamente un turno de muestreo.
 *
 *    Con un solo canal esa trampa no existe por construcción. Para medir dos
 *    canales está el otro sketch, y ahí los DOS tienen que estar conectados.
 *
 *  LO QUE NO HACE
 *    No corre el SPWM, no hay PLL, no mide el sesgo entre canales (eso es la
 *    corrida A7 y necesita el otro sketch). Esto sólo mide, para poder
 *    escribir el módulo de sensado con números medidos en vez de supuestos.
 *
 *  CÓMO SE USA
 *    1. Medir el riel de 5 V y ponerlo en VCC_MV.
 *    2. Poner la relación del divisor en DIVISOR_X100.
 *    3. Cargar, abrir el monitor serie a 115200.
 *
 *  Referencias: docs/protocolo_banco_control.md (corrida A6)
 *               docs/decisiones_criticas.md     (DC-006, DC-020)
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ======================== PERILLAS ===========================================
//   Qué canal analógico se lee. 6 = A6 (la red, J17). 0 = A0 (el inversor, J2).
#define CANAL                  6
//   El riel de 5 V medido con el multímetro, en milivolts.
#define VCC_MV                 5000
//   Relación del divisor AC × 100, es decir (Rt + Rb) / Rb × 100.
//     Rt = 100 k y Rb = 10.2 k  ->  110.2/10.2 = 10.80  ->  1080
//     Rt = 110 k y Rb = 10.2 k  ->  120.2/10.2 = 11.78  ->  1178
#define DIVISOR_X100           1080
//   Frecuencia nominal de la fuente. Sólo sirve para descartar lecturas
//   absurdas; la frecuencia que se reporta sale de los cruces medidos.
#define HZ_NOMINAL             60

// ======================== BASE DE TIEMPO =====================================
// Timer1 en modo 8 (phase and frequency correct), igual que el SPWM, para que
// la muestra caiga siempre en el mismo punto del periodo de portadora.
constexpr uint16_t TOP_CUENTAS = 333;                  // 24 024 Hz de portadora
constexpr uint8_t  PERIODOS_POR_DISPARO = 3;           // DC-020: 3 × 41.625 > 104 µs
constexpr uint32_t PERIODO_PORT_NS = 2UL * TOP_CUENTAS * 1000UL / 16UL;   // 41 625
constexpr uint32_t ENTRE_MUESTRAS_NS = PERIODO_PORT_NS * PERIODOS_POR_DISPARO;

// Ventana de 400 muestras = 49.95 ms ≈ 3 ciclos exactos a 60 Hz. Que caiga en
// un número entero de ciclos es lo que evita que el RMS respire.
constexpr uint16_t MUESTRAS_VENTANA = 400;
constexpr uint8_t  VENTANAS_POR_REPORTE = 20;          // ~1 s

// ======================== ESTADO =============================================
static volatile uint32_t suma, sumaCuad, sumaAbs;
static volatile uint16_t n, pico;
static volatile int16_t  dPrev;
static volatile bool     dPrevValida;
static volatile int16_t  mediaRef = 512;
// Histéresis del detector de cruce: hay que subir por encima de umbralArmar
// antes de aceptar el siguiente cruce. Se recalcula cada ventana a la mitad
// del RMS, que queda muy por arriba de cualquier pico y muy por debajo del
// pico de la onda.
static volatile int16_t  umbralArmar = 60;
static volatile bool     armado = true;

// Instantáneas que el lazo principal consume
static volatile uint32_t sSuma, sSumaCuad, sSumaAbs;
static volatile uint16_t sN, sPico;
static volatile bool     hayVentana;

// Frecuencia: tiempo del primer y último cruce del reporte, en 1/256 de muestra
static volatile uint32_t muestras;          // contador corrido de muestras
static volatile uint32_t cruceIni, cruceFin;
static volatile uint16_t nCruces;

static volatile uint32_t perdidos;
static volatile uint8_t  cuentaDisparo;

// ======================== INTERRUPCIONES =====================================
ISR(TIMER1_OVF_vect)
{
  if (++cuentaDisparo >= PERIODOS_POR_DISPARO) {
    cuentaDisparo = 0;
    if (ADCSRA & (1 << ADSC)) perdidos++;      // la conversión anterior no terminó
    else                      ADCSRA |= (1 << ADSC);
  }
}

ISR(ADC_vect)
{
  const uint16_t x = ADC;
  muestras++;

  suma     += x;
  sumaCuad += (uint32_t)x * x;
  n++;

  const int16_t  d  = (int16_t)x - mediaRef;
  const uint16_t ad = (d < 0) ? (uint16_t)(-d) : (uint16_t)d;
  sumaAbs += ad;
  if (ad > pico) pico = ad;

  // HISTÉRESIS. Sin ella un pico cerca del cruce produce cruces de más: ahí
  // la señal pasa de −18 a 0 a +18 cuentas por muestra, y veinte cuentas de
  // pico bastan para cruzar dos veces. Medido en banco: 62 a 71 cruces por
  // segundo donde debía haber 60, con el factor de cresta en 1.55.
  // Después de un cruce no se acepta otro hasta que la señal suba
  // claramente, y "claramente" se saca del RMS de la ventana anterior, así
  // que el umbral se adapta solo al nivel de señal que haya.
  if (!armado && d > umbralArmar) armado = true;

  // Cruce por cero ascendente, interpolado entre las dos muestras que lo
  // encierran. El tiempo queda en 1/256 de intervalo de muestreo.
  // Sin umbral sobre dPrev: pedirle a la muestra anterior que estuviera por
  // debajo de −3 PIERDE cruces buenos. Cerca del cruce la señal avanza ~18
  // cuentas por muestra, así que una de cada tres cae dentro de esa banda y
  // el cruce se descarta. Simulado con senoidal limpia: 52 cruces de 60.
  // La histéresis ya cubre el caso sin señal: con el umbral de armado en 10
  // cuentas como piso, el ruido dispara un cruce y nunca vuelve a armar.
  if (armado && dPrevValida && dPrev < 0 && d >= 0) {
    const int16_t salto = d - dPrev;                       // > 0 por construcción
    const uint32_t frac = ((uint32_t)(-dPrev) * 256UL) / (uint32_t)salto;
    const uint32_t t = (muestras - 1UL) * 256UL + frac;
    if (nCruces == 0) cruceIni = t;
    cruceFin = t;
    nCruces++;
    armado = false;
  }
  dPrev = d;
  dPrevValida = true;

  if (n >= MUESTRAS_VENTANA && !hayVentana) {
    sSuma = suma; sSumaCuad = sumaCuad; sSumaAbs = sumaAbs; sN = n; sPico = pico;
    suma = 0; sumaCuad = 0; sumaAbs = 0; n = 0; pico = 0;
    dPrevValida = false;
    hayVentana = true;
  }
}

// ======================== AUXILIARES =========================================
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
  int32_t resto = valor % escala;
  for (int32_t div = escala / 10; div >= 1; div /= 10) {
    Serial.print((char)('0' + (resto / div) % 10));
    if (--decimales == 0) break;
  }
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

  // La primera conversión tras habilitar tarda 25 ciclos: se tira.
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC)) { }
  (void)ADC;

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" Isla del Sol - lectura AC de un canal (corrida A6)"));
  Serial.println(F("=================================================="));
  Serial.print(F(" Canal              : A")); Serial.println(CANAL);
  Serial.print(F(" Reloj del ADC      : 125 kHz (/128), conversion 104 us"));
  Serial.println();
  Serial.print(F(" Entre muestras     : "));
  imprimeDec(ENTRE_MUESTRAS_NS, 1000, 3); Serial.print(F(" us  ("));
  Serial.print(PERIODOS_POR_DISPARO); Serial.println(F(" periodos de portadora)"));
  Serial.print(F(" Ventana            : ")); Serial.print(MUESTRAS_VENTANA);
  Serial.print(F(" muestras = "));
  imprimeDec((int32_t)((uint32_t)MUESTRAS_VENTANA * ENTRE_MUESTRAS_NS / 1000UL), 1000, 2);
  Serial.println(F(" ms"));
  Serial.print(F(" VCC / divisor      : ")); Serial.print(VCC_MV);
  Serial.print(F(" mV  /  x")); imprimeDec(DIVISOR_X100, 100, 2);
  Serial.println();
  Serial.println(F(" Un pin al aire NO lee cero: lee el canal anterior."));
  Serial.println(F(" Con un solo canal esa trampa no existe."));
  Serial.println();

  sei();
}

// ======================== LAZO PRINCIPAL =====================================
void loop()
{
  static uint8_t  ventanas = 0;
  static uint32_t rmsAcum = 0, biasAcum = 0, absAcum = 0;
  static uint16_t picoRep = 0;
  static uint32_t segundos = 0;

  if (!hayVentana) return;

  uint32_t vSuma, vCuad, vAbs;
  uint16_t vN, vPico;
  noInterrupts();
  vSuma = sSuma; vCuad = sSumaCuad; vAbs = sSumaAbs; vN = sN; vPico = sPico;
  hayVentana = false;
  interrupts();

  if (vN == 0) return;

  // RMS en una pasada: var = Σx²/n − (Σx/n)². La media sale del mismo
  // acumulador, así que el bias se quita solo: no se supone que valga 512.
  const uint32_t media = (vSuma + vN / 2) / vN;
  const uint32_t cuadMedia = (uint32_t)(((uint64_t)vSuma * vSuma) / vN);
  const uint32_t var = (vCuad > cuadMedia) ? (vCuad - cuadMedia) / vN : 0;

  // En dieciseisavos de cuenta: redondear a cuenta entera cada ventana le
  // metía ±0.4 % al factor de forma.
  rmsAcum  += raiz(var << 8);
  absAcum  += (vAbs * 16UL + vN / 2) / vN;
  biasAcum += media;
  if (vPico > picoRep) picoRep = vPico;

  const uint16_t rmsV = raiz(var);
  noInterrupts();
  mediaRef = (int16_t)media;            // referencia para la ventana siguiente
  // La mitad del RMS: con señal son ~135 cuentas, y el umbral se cae solo
  // si la señal se cae. Nunca abajo de 10 para que el ruido no arme.
  umbralArmar = (rmsV > 20) ? (int16_t)(rmsV / 2) : (int16_t)10;
  interrupts();

  if (++ventanas < VENTANAS_POR_REPORTE) return;
  segundos++;

  const uint32_t rms16 = rmsAcum / ventanas;
  const uint32_t mab16 = absAcum / ventanas;
  const uint32_t bias  = biasAcum / ventanas;
  const uint32_t mv    = (rms16 * VCC_MV) / (1023UL * 16UL);
  const uint32_t cv    = (mv * DIVISOR_X100) / 1000UL;

  Serial.print(F("t=")); Serial.print(segundos);
  Serial.print(F("s  bias ")); Serial.print(bias);
  Serial.print(F("  rms ")); imprimeDec(rms16, 16, 2);
  Serial.print(F(" cnt = ")); Serial.print(mv);
  Serial.print(F(" mV = ")); imprimeDec(cv, 100, 2);
  Serial.print(F(" VAC"));

  // Factor de forma: un multímetro de VALOR MEDIO mide la media de |v| y la
  // multiplica por 1.1107, el factor de forma de una senoidal. Si la onda no
  // es senoidal, se equivoca exactamente en 1.1107/FF − 1. Esto no estima la
  // distorsión: mide el error del patrón contra el que se va a calibrar.
  if (mab16 && rms16) {
    const uint32_t ff  = (rms16 * 10000UL) / mab16;
    const int32_t  err = (int32_t)((11107UL * 10000UL) / ff) - 10000;
    const uint32_t cf  = ((uint32_t)picoRep * 16000UL) / rms16;
    Serial.print(F(" | FF ")); imprimeDec(ff, 10000, 4);
    Serial.print(F(" -> multimetro "));
    if (err >= 0) Serial.print('+');
    imprimeDec(err, 100, 2); Serial.print(F(" %"));
    Serial.print(F(" | cresta ")); imprimeDec(cf, 1000, 3);
  }

  // Frecuencia: span entre el primer y el último cruce del reporte. Con ~60
  // cruces en un segundo, la resolución no la pone un ciclo sino todo el span.
  uint32_t ini, fin; uint16_t nc;
  noInterrupts();
  ini = cruceIni; fin = cruceFin; nc = nCruces;
  cruceIni = 0; cruceFin = 0; nCruces = 0;
  interrupts();
  if (nc >= 2 && fin > ini) {
    const uint32_t perS256 = (fin - ini) / (nc - 1);           // en 1/256 de muestra
    const uint32_t perNs   = (perS256 * ENTRE_MUESTRAS_NS) / 256UL;
    const uint32_t nomNs   = 1000000000UL / HZ_NOMINAL;
    if (perNs > (nomNs / 2) && perNs < (nomNs * 2)) {
      Serial.print(F(" | f = "));
      imprimeDec((int32_t)(1000000000000ULL / perNs), 1000, 3);
      Serial.print(F(" Hz"));
    } else {
      Serial.print(F(" | f fuera de rango"));
    }
    Serial.print(F(" (")); Serial.print(nc); Serial.print(F(" cruces)"));
  } else {
    Serial.print(F(" | sin cruces (senal ausente?)"));
  }

  uint32_t perd;
  noInterrupts(); perd = perdidos; perdidos = 0; interrupts();
  Serial.print(F(" | perdidos ")); Serial.println(perd);

  ventanas = 0; rmsAcum = 0; absAcum = 0; biasAcum = 0; picoRep = 0;
}
