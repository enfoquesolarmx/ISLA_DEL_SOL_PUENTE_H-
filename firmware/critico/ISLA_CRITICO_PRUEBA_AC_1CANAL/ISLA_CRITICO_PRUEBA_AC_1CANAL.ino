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
 *      · el VOLTAJE DE LA RED en el primario, vía RELACION_X1000
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
 *    4. CALIBRAR LA RED: medir el primario con el multímetro, dividirlo entre
 *       los VAC que ESTE sketch reporta del secundario, × 1000, y poner eso en
 *       RELACION_X1000. Una sola medición calibra toda la cadena.
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
//
//   MEDIDO el 2026-11-21, no nominal. Con el Fluke en los DOS lados del
//   transformador la razón sale sin el error de ganancia del instrumento
//   (es un cociente del mismo aparato), y de ahí se despeja el divisor:
//
//     razón del transformador = 120.1 / 14.3        = 8.3986
//     secundario con la línea en 119.9              = 14.276 V
//     divisor = 14 276 mV / 1325 mV (pin del ADC)   = 10.774  -> 1077
//     Rb = 100k / (10.774 - 1)                      = 10 231 ohm
//          = 8.2k fijos + 2 031 de RV2 (41 % de un pot de 5 k)
//
//   Esto cerró la duda de los 2.9 kΩ que midió el Fluke en RV2: no era RV2
//   en circuito. La otra hipótesis (Rb = 11.1 k, divisor 1001) queda 7.1 %
//   fuera de lo medido, y la incertidumbre de la medición es 0.35 %.
//
//   ---------------------------------------------------------------------
//   ACTUALIZADO el 2026-11-21: Rt subió a 110 kΩ (100 k + 10 k en serie).
//
//   A3 lo pidió: con 100 kΩ el pico del pin llega a 4.69 V a 140 V de red —
//   con cresta senoidal, no sólo con la medida — y el techo son 4.60 V.
//
//     divisor = 14.3 V / 1220.7 mV (pin, media de 18 reportes) = 11.714
//     Rt = 10 231 × (11.714 − 1) = 109 619 Ω
//          -> el 10 k que se agregó vale 9 619 Ω, un 3.8 % bajo (dentro de ±5 %)
//
//   Comprobado por los dos lados: con 1171 el sketch reporta 14.29 V de
//   secundario (Fluke 14.3) y 120.06 V de red (Fluke 120.1).
//
//   Y RELACION_X1000 NO se tocó: el transformador es el mismo. Esa es la
//   ganancia de haberlas separado — antes habría que recalibrar las dos.
#define DIVISOR_X100           1171
//   Relación del transformador × 1000, para reportar el voltaje de la RED.
//
//   NO es la relación de placa ni el cociente de dos lecturas del multímetro:
//   es el PRIMARIO MEDIDO entre el SECUNDARIO QUE ESTE SKETCH REPORTA.
//
//     RELACION_X1000 = V_primario(multimetro) / VAC_secundario(este sketch) × 1000
//
//   Así una sola medición absorbe de un golpe el error del divisor, el del
//   riel de 5 V y la relación real del transformador. Y se calibra contra la
//   lectura BUENA: el multímetro da 4 dígitos en el primario (119.9 ±0.05 V,
//   ±0.04 %) y sólo 3 en el secundario (14.2 ±0.05 V, ±0.35 %). Calibrar con
//   el cociente 119.9/14.2 arrastraría ese ±0.35 % al número de la red.
//
//   AHORA ES LA RAZÓN DE VUELTAS DE VERDAD, no una calibración de punta a
//   punta. El 2026-11-21 se midió con el Fluke en LOS DOS LADOS:
//
//     120.1 V en el primario / 14.3 V en el secundario = 8.3986  -> 8399
//
//   Es un cociente del mismo instrumento, así que su error de ganancia se
//   cancela. Eso es lo que permitió separar esta constante de DIVISOR_X100.
//
//   POR QUÉ IMPORTA LA SEPARACIÓN. Hasta el 2026-11-20 esta constante valía
//   8379 y estaba calibrada de punta a punta contra el Fluke: absorbía el
//   error del divisor, el del riel de 5 V y la razón del transformador, todo
//   revuelto. La línea salía bien, pero ninguna de las dos constantes decía
//   lo que su nombre promete. Con eso:
//     · no se podía calcular el margen del pin contra el riel (criterio A3),
//       porque eso necesita el divisor solo;
//     · cambiar de transformador obligaba a recalibrar las dos.
//   Separadas, cada una se mide y se corrige por su cuenta.
//
//   El efecto en el voltaje de la línea es de -0.04 %: no se trata de
//   exactitud, se trata de que los números signifiquen algo.
//
//   Ojo con el dígito fino: el Fluke despliega 14.3 ±0.05 V en el secundario,
//   que son ±0.35 %, o sea ±29 cuentas de esta constante. No vale la pena
//   perseguir la cuarta cifra.
#define RELACION_X1000         8399
//   Frecuencia nominal de la fuente. Sólo sirve para descartar lecturas
//   absurdas; la frecuencia que se reporta sale de los cruces medidos.
#define HZ_NOMINAL             60

// ======================== BASE DE TIEMPO =====================================
// Timer1 en modo 8 (phase and frequency correct), igual que el SPWM, para que
// la muestra caiga siempre en el mismo punto del periodo de portadora.
constexpr uint16_t TOP_CUENTAS = 333;                  // 24 024 Hz de portadora
constexpr uint32_t CONVERSION_NS = 104000;             // 13 ciclos del ADC a 125 kHz

// CUATRO periodos, no tres. Con tres el presupuesto entre muestras son
// 124.875 µs y la conversión se come 104: el ADC queda al 83 % y los 20.9 µs
// que sobran tienen que absorber TRES ISR del Timer1 (corre en cada desborde,
// no en cada disparo) más la ISR del ADC completa. No cabe — y eso medido sin
// SPWM, que en el firmware real vive dentro de esa misma ISR del Timer1.
//
// Con cuatro el ADC baja al 62 % y sobran 62.5 µs. Y no se pierde nada que
// sirva: el lazo de amplitud no puede ir más rápido que medio ciclo (8.33 ms),
// porque antes de eso no existe un RMS que medir, y 100 muestras por ciclo
// siguen siendo el doble de las ~50 que un RMS necesita.
constexpr uint8_t  PERIODOS_POR_DISPARO = 4;
constexpr uint32_t PERIODO_PORT_NS = 2UL * TOP_CUENTAS * 1000UL / 16UL;   // 41 625
constexpr uint32_t ENTRE_MUESTRAS_NS = PERIODO_PORT_NS * PERIODOS_POR_DISPARO;
constexpr uint16_t MUESTRAS_POR_S = (uint16_t)(1000000000UL / ENTRE_MUESTRAS_NS);

// Ventana de 300 muestras = 49.95 ms ≈ 3 ciclos exactos a 60 Hz. Que caiga en
// un número entero de ciclos es lo que evita que el RMS respire.
//
// Son los MISMOS 49.95 ms que con 3 periodos y 400 muestras, porque
// 400/8008 = 300/6006. La ventana no se movió: sólo se aflojó el ADC.
constexpr uint16_t MUESTRAS_VENTANA = 300;
// 20 ventanas ≈ 1 s por reporte.
//
// MODO CAPTURA: ponerlo en 1 y cada ventana sale sola — 20 lecturas de RMS por
// segundo, que es la resolución temporal que el instrumento DE VERDAD tiene. El
// promedio de un segundo es una decisión de impresión, no un límite.
//
// Sirve para agarrar la forma de un escalón de carga (el compresor del
// refrigerador arrancando, la cafetera, la aspiradora): profundidad, duración y
// tiempo de recuperación. Con el promedio de un segundo, un hundimiento de
// 200 ms se diluye a una quinta parte y se ve plano.
//
// En captura la frecuencia NO se imprime: con ~3 cruces por ventana no hay con
// qué, y 'dms' de 50 ms hace que el milisegundo entero de millis() valga 2 %.
constexpr uint8_t  VENTANAS_POR_REPORTE = 20;
constexpr uint16_t CRUCES_MINIMOS = 20;                // para fiarse de la f

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
    // dPrev NO se invalida: la onda es continua entre ventanas y sólo los
    // acumuladores se reinician. Invalidarla ciega una muestra por ventana,
    // veinte por reporte, y si un cruce cae ahí se pierde. Medido en banco:
    // eso pasaba en el 15 % de los reportes, y un cruce perdido a media
    // medición deja el divisor corto en uno y la frecuencia 1.4 % baja
    // (59.17 en vez de 60.02).
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
  Serial.print(PERIODOS_POR_DISPARO); Serial.print(F(" periodos) = "));
  Serial.print(MUESTRAS_POR_S); Serial.println(F(" muestras/s"));
  Serial.print(F(" Ocupacion del ADC  : "));
  imprimeDec((int32_t)(CONVERSION_NS * 1000UL / ENTRE_MUESTRAS_NS), 10, 1);
  Serial.print(F(" %   (sobran "));
  imprimeDec((int32_t)((ENTRE_MUESTRAS_NS - CONVERSION_NS) / 100UL), 10, 1);
  Serial.println(F(" us por muestra)"));
  Serial.print(F(" Ventana            : ")); Serial.print(MUESTRAS_VENTANA);
  Serial.print(F(" muestras = "));
  imprimeDec((int32_t)((uint32_t)MUESTRAS_VENTANA * ENTRE_MUESTRAS_NS / 1000UL), 1000, 2);
  Serial.print(F(" ms = "));
  imprimeDec((int32_t)((uint32_t)MUESTRAS_VENTANA * ENTRE_MUESTRAS_NS / 1000UL
                        * HZ_NOMINAL / 1000UL), 1000, 3);
  Serial.println(F(" ciclos"));
  Serial.print(F(" VCC / divisor      : ")); Serial.print(VCC_MV);
  Serial.print(F(" mV  /  x")); imprimeDec(DIVISOR_X100, 100, 2);
  Serial.println();
  Serial.print(F(" Relacion a la red  : x"));
  imprimeDec(RELACION_X1000, 1000, 3);
  Serial.println(F("   (primario medido / secundario de este sketch)"));
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
  static uint32_t msAnt = 0, muestrasAnt = 0;

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
  // ---------------------------------------------------------------------------
  // VOLTAJE: de rms16 a la línea en UNA sola expresión.
  //
  // La cadena anterior truncaba tres veces — cuentas → mV entero → cV entero →
  // línea entera — y cada truncada tiraba resolución. Peor: cv = mv × 1.08
  // avanza 1.08 por paso, así que la división entera SE SALTA un cv cada doce
  // pasos. Con RELACION 8379 el cv 1430 quedaba inalcanzable y 119.8 V NO SE
  // PODÍA DESPLEGAR: la lectura tenía que brincar 119.7 ↔ 119.9 aunque la línea
  // estuviera perfectamente quieta en 119.8. Se vio en banco el 2026-11-20 con
  // el Fluke marcando 119.8 fijo.
  //
  // El paso visible quedaba en ~200 mV, SIETE VECES más grueso que los 27.6 mV
  // que el promedio de 6000 muestras realmente resuelve.
  //
  //   sec[mV]   = rms16/16 × VCC_MV/1023 × DIVISOR_X100/100
  //             = rms16 × VCC_MV × DIVISOR_X100 / (16 × 1023 × 100)
  //   linea[cV] = sec[mV] × RELACION_X1000/1000 / 10
  //
  // El numerador pasa de 32 bits (~1.9e14), así que va en 64. Corre una vez por
  // reporte, no en la ISR: el costo no importa.
  constexpr uint64_t DEN_SEC = 16ULL * 1023ULL * 100ULL;       // -> mV del secundario
  constexpr uint64_t DEN_RED = DEN_SEC * 1000ULL * 10ULL;      // -> cV de la línea
  const uint64_t numSec = (uint64_t)rms16 * VCC_MV * DIVISOR_X100;
  const uint32_t sec = (uint32_t)((numSec + DEN_SEC / 2) / DEN_SEC);
  const uint32_t red = (uint32_t)((numSec * RELACION_X1000 + DEN_RED / 2) / DEN_RED);
  const uint32_t mv  = (uint32_t)((rms16 * VCC_MV + 8184UL) / (1023UL * 16UL));

  // Y el RMS en cuentas. imprimeDec supone que la escala es potencia de diez;
  // con escala 16 imprimía el RESTO módulo diez, no un decimal: "270.5" quería
  // decir 270 + 5/16 = 270.31. Se escala a centésimas antes de imprimir.
  const uint32_t rmsCent = (rms16 * 100UL + 8UL) / 16UL;

  // Con 20 ventanas el contador son segundos; en modo captura son reportes, y
  // etiquetarlos "s" mentiría. El compilador resuelve la comparación en frío.
  if (VENTANAS_POR_REPORTE == 20) { Serial.print(F("t=")); Serial.print(segundos);
                                    Serial.print(F("s")); }
  else                            { Serial.print(F("#")); Serial.print(segundos); }
  Serial.print(F("  bias ")); Serial.print(bias);
  Serial.print(F("  rms ")); imprimeDec(rmsCent, 100, 2);
  Serial.print(F(" cnt = ")); Serial.print(mv);
  // Tres decimales en el secundario y dos en la línea: es la resolución que de
  // verdad hay (3.3 mV y 27.6 mV). Son dígitos de RESOLUCIÓN, no de exactitud
  // — la exactitud absoluta la limita el patrón, ±1.9 % con el Fluke 324.
  Serial.print(F(" mV = ")); imprimeDec(sec, 1000, 3);
  Serial.print(F(" VAC  ->  RED ")); imprimeDec(red, 100, 2);
  Serial.print(F(" V"));

  // Factor de forma: un multímetro de VALOR MEDIO mide la media de |v| y la
  // multiplica por 1.1107, el factor de forma de una senoidal. Si la onda no
  // es senoidal se equivoca en 1.1107/FF − 1.
  //
  // OJO, ESTE NÚMERO NO ES FINO. Al pasar de 3 a 4 periodos por disparo —
  // misma onda, mismo transformador, mismo divisor, misma fuente — el FF se
  // movió de 1.1096 a 1.1140 y el error deducido brincó de +0.10 % a −0.30 %.
  // Un corrimiento de 0.40 % habiendo cambiado SÓLO la tasa de muestreo: es
  // un sistemático del método, no una propiedad de la onda.
  //
  // Hipótesis (sin medir): a 6006 muestras/s y 60.06 Hz salen 100.00 muestras
  // por ciclo EXACTAS. El muestreo quedó coherente con la onda y cada ciclo
  // cae en los mismos 100 puntos de fase, lo que congela el sesgo de esa
  // rejilla. A 8008/60.02 = 133.4 la rejilla se corría y promediaba.
  //
  // Así que el FF sirve para decir "la onda está dentro de ~0.4 % de la forma
  // de una senoidal", y NO para certificar un patrón al 0.1 %. Esa afirmación
  // se retiró el 2026-11-20 al ver el corrimiento.
  //
  // Lo que la coherencia sí regala: repetibilidad. Vsec σ = 0.011 V (0.077 %),
  // FF σ = 0.0004, cresta σ = 0.0019 en 22 reportes. Para un lazo de control
  // la varianza baja vale más que la exactitud absoluta, porque lo absoluto lo
  // arregla RELACION_X1000 — pero obliga a medir esa constante a la MISMA tasa
  // a la que va a correr el firmware.
  if (mab16 && rms16) {
    const uint32_t ff  = (rms16 * 10000UL) / mab16;
    const int32_t  err = (int32_t)((11107UL * 10000UL) / ff) - 10000;
    const uint32_t cf  = ((uint32_t)picoRep * 16000UL) / rms16;
    Serial.print(F(" | FF ")); imprimeDec(ff, 10000, 4);
    Serial.print(F(" (sen 1.1107) -> patron "));
    if (err >= 0) Serial.print('+');
    imprimeDec(err, 100, 2); Serial.print(F(" % +-0.4"));
    Serial.print(F(" | cresta ")); imprimeDec(cf, 1000, 3);
  }

  // Frecuencia. El span entre el primer y el ultimo cruce se mide en
  // MUESTRAS, y el tiempo real de una muestra NO es el supuesto: en banco se
  // perdia el 17 % de las conversiones con 3 periodos y el intervalo real
  // resulto de 151 us en vez de 124.875 (de ahi el cambio a 4 periodos, que
  // baja el ADC del 83 al 62 %; si con eso la tasa llega a MUESTRAS_POR_S, la
  // perdida era falta de margen). El intervalo se sigue midiendo contra
  // millis(), que
  // corre del Timer0 y no depende del ADC. Asi la frecuencia sale bien
  // aunque se pierdan muestras.
  uint32_t ini, fin; uint16_t nc;
  noInterrupts();
  ini = cruceIni; fin = cruceFin; nc = nCruces;
  cruceIni = 0; cruceFin = 0; nCruces = 0;
  interrupts();

  // DIAGNOSTICO DE BASE DE TIEMPO. El contador de muestras avanza una por
  // conversion ENTREGADA; si alguna se pierde sin que 'perdidos' la vea, el
  // tiempo deducido de las muestras se queda corto y la frecuencia sale
  // inflada. millis() corre del Timer0, que es independiente del ADC, asi
  // que comparar los dos relojes separa "cruces de mas" de "base de tiempo
  // mal": si las muestras/s no son MUESTRAS_POR_S, el problema es el reloj.
  uint32_t perd, muAhora;
  noInterrupts(); perd = perdidos; perdidos = 0; muAhora = muestras; interrupts();
  const uint32_t ms = millis();
  Serial.print(F(" | perdidos ")); Serial.print(perd);
  if (msAnt && ms > msAnt) {
    const uint32_t dms = ms - msAnt;
    const uint32_t dmu = muAhora - muestrasAnt;
    const uint32_t tasa = (dmu * 1000UL) / dms;
    Serial.print(F(" | ")); Serial.print(tasa);
    Serial.print(F(" muestras/s de ")); Serial.print(MUESTRAS_POR_S);
    if (nc >= CRUCES_MINIMOS && fin > ini && dmu) {
      const uint32_t intervaloNs = (dms * 1000000UL) / dmu;   // ns por muestra, REAL
      const uint32_t perS256 = (fin - ini) / (nc - 1);        // en 1/256 de muestra
      const uint32_t perNs = (uint32_t)(((uint64_t)perS256 * intervaloNs) / 256ULL);
      Serial.print(F(" ("));
      imprimeDec(intervaloNs, 1000, 2); Serial.print(F(" us c/u) | f = "));
      if (perNs) imprimeDec((int32_t)(1000000000000ULL / perNs), 1000, 3);
      Serial.print(F(" Hz (")); Serial.print(nc); Serial.print(F(" cruces)"));
    } else if (nc == 0) {
      Serial.print(F(" | sin cruces (senal ausente?)"));
    } else {
      // Pocos cruces: en modo captura es lo normal, no es falla.
      Serial.print(F(" | f omitida (")); Serial.print(nc);
      Serial.print(F(" cruces, se piden ")); Serial.print(CRUCES_MINIMOS);
      Serial.print(F(")"));
    }
  }
  msAnt = ms; muestrasAnt = muAhora;
  Serial.println();

  ventanas = 0; rmsAcum = 0; absAcum = 0; biasAcum = 0; picoRep = 0;
}
