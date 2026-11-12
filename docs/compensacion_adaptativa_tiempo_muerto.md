# Compensación adaptativa de tiempo muerto — etapa posterior

**Proyecto:** Puente H — prototipo SPWM "Isla del Sol" (inversor 48 V → 127 V / 60 Hz, 1000 W; modo cargador 1 kVA)
**Estado:** pendiente, para después de validar el prototipo en banco
**Fecha:** 2026-11-02

---

## 1. Resumen

La compensación de tiempo muerto corrige en el firmware el error de voltaje que el tiempo muerto mete en cada pulso, y elimina casi todos los armónicos impares (3°, 5°, 7°) de la salida. Se deja para después del prototipo porque no requiere cambiar el hardware de potencia ni los módulos de gate driver.

- **Problema:** durante el tiempo muerto conduce un diodo interno; si la corriente sale de la rama el pulso real se acorta, si entra se alarga.
- **Corrección:** ajustar el ciclo de trabajo antes de cargarlo al PWM:
  `d_comando = d_ideal + signo(i) · DT · f_sw`
- **Versión adaptativa:** el firmware mide el 3er armónico del voltaje de salida y ajusta la corrección solo, para cada módulo (IRS2186, UCC21520 o TLP250), temperatura y carga.
- **Aplica a:** inversor (modulación híbrida, rama A a 24 kHz) y cargador (unipolar, ambas ramas a 24 kHz).

### Contexto del proyecto (al momento de escribir esto)

| Elemento | Valor |
| --- | --- |
| Bus DC | 48 V (batería) |
| Conmutación | 24 kHz; rama A 24 kHz y rama B 60 Hz (inversor, híbrida); ambas 24 kHz (cargador, unipolar) |
| MOSFET | IRF100P219 (100 V) |
| Módulos de gate driver | IRS2186 (DT 500 ns), UCC21520DW (DT 500 ns + respaldo R_DT), TLP250H (DT 1 µs) |
| Filtro de salida | L total ≈ 2 mH en secundario (dispersión + inductor de ferrita E65 N87) + 4.7 µF MKP X2 |
| Transformador | 1 : 4.16 (48 V → 127 V) |
| Feedback de voltaje | Transformador 120 → 12 V + divisor de voltaje (ya existe) |
| Medición de corriente | Shunt en el bus DC (no da el signo de la corriente de salida) |

---

## 2. Resultados de referencia

Los armónicos dependen solo del producto **k · VBUS · DT · f_sw** (k = 1 en híbrida, k = 2 en unipolar y bipolar). Valores del simulador de armónicos, 1000 W, FP 1, VBUS 48 V, sin compensación:

| Driver y modo | DT | f_sw | ΔV puente | 3er armónico | THD salida |
| --- | --- | --- | --- | --- | --- |
| IRS2186 / UCC21520, híbrida | 500 ns | 24 kHz | 0.58 V | 0.58 % | 0.81 % |
| TLP250, híbrida | 1 µs | 24 kHz | 1.15 V | 1.17 % | 1.62 % |
| TLP250, unipolar | 1 µs | 12 kHz | 1.15 V | 1.4 % | — |
| UCC21520, unipolar | 500 ns | 24 kHz | 1.15 V | 1.4 % | — |
| TLP250, unipolar (cargador) | 1 µs | 24 kHz | 2.3 V | ~2.5–3 % (estimado) | — |

- Los armónicos son **impares** porque el error es una onda cuadrada con simetría de media onda; bajan como 1/n.
- Los armónicos **27° y 29°** crecen por la resonancia del filtro LC (f₀ ≈ 1.64 kHz con 2 mH y 4.7 µF) cuando la carga es ligera o inductiva.
- Con compensación y signo de corriente perfecto, el simulador baja todos los armónicos de tiempo muerto a casi cero; en la práctica queda un residuo cerca del cruce por cero.
- Bajar la portadora de 24 a 12 kHz reduce el error, pero trae silbido audible y menos atenuación del filtro (~35 dB contra ~47 dB). Se decidió **mantener 24 kHz** y usar compensación.

---

## 3. Hoja de ruta

Cada etapa se hace solo cuando la anterior está validada en banco.

| Etapa | Objetivo | Qué se necesita |
| --- | --- | --- |
| 1. Prototipo (actual) | Validar driver, bootstrap, térmico y filtro | Tiempo muerto fijo y seguro: 500 ns (IRS2186, UCC21520) o 1 µs (TLP250) |
| 2. Compensación fija | Bajar 3°, 5° y 7° | Signo de la corriente de salida + D_COMP calibrado en banco |
| 3. Amortiguamiento del filtro | Quitar el pico de 27° y 29° sin carga o con motores | Red RC: 22 Ω (3–5 W) en serie con 4.7 µF X2, en paralelo con el capacitor del filtro |
| 4. Compensación adaptativa | Que cada módulo se ajuste solo a su driver, temperatura y carga | Feedback AC existente + Goertzel del 3er armónico |
| 5. Lazo de voltaje resonante | Onda de calidad comercial también con cargas no lineales | Control instantáneo con controladores en 3°, 5° y 7° |

Sensar el nodo VS1 (ancho real de cada pulso) es una mejora opcional posterior; no es necesario para las etapas 2 a 5.

---

## 4. Hardware de medición

El feedback AC actual (transformador 120 → 12 V + divisor) ya sirve para medir el 3er armónico; solo falta el sensor de corriente de salida.

| Señal | Estado | Uso | Notas |
| --- | --- | --- | --- |
| Voltaje de salida | Existe (120 → 12 V + divisor) | Regulación RMS y Goertzel del 3er armónico | El transformador agrega un poco de 3er armónico propio: medir ese piso una vez contra un analizador |
| Signo de la corriente de salida | **Pendiente** | Compensación (etapas 2 y 4) | Sensor Hall en el primario (por ejemplo ACS758, 50 A) o transformador de corriente en el secundario. El shunt del bus DC no sirve para esto |
| Pulso real en VS1 | Opcional | Compensación ciclo por ciclo | Divisor + comparador a captura del temporizador; respetar separaciones de alto voltaje |

Acondicionamiento del voltaje hacia el ADC:

- Pico de entrada ≈ 17 V (12 V × √2) más ~10 % de margen.
- Señal centrada en VREF/2 (1.65 V con 3.3 V), excursión de ±1.2–1.4 V.
- Filtro antialias con corte de 2–3 kHz: deja pasar hasta el 13°–15° armónico y atenúa los 24 kHz.
- Protección del pin: resistencia serie y diodos de enclavamiento.
- Muestreo dentro de la interrupción del PWM, a mitad del pulso, con un número entero de muestras por ciclo (por ejemplo 100 = una cada 4 periodos de 24 kHz).

---

## 5. Firmware

El tiempo muerto y D_COMP se dejan como **parámetros por tipo de módulo**; así el mismo código sirve para los tres drivers.

### 5.1 Compensación con signo de corriente (etapa 2)

Cerca del cruce por cero la corrección crece en proporción a la corriente para no equivocar el signo con el rizo:

```c
#define DT_S     1.0e-6f          // 500e-9 para IRS2186 / UCC21520, 1e-6 para TLP250
#define FSW      24000.0f
#define D_COMP   (DT_S * FSW)     // 0.024 con 1 us
#define I_TH     1.5f             // A, zona lineal (~ rizo pico del primario)
#define D_MIN    0.0f
#define D_MAX    0.95f            // deja tiempo al bootstrap para recargar

static float d_comp = D_COMP;     // valor inicial; la etapa 4 lo ajusta en linea

static float limitar(float x, float lo, float hi){ return x < lo ? lo : (x > hi ? hi : x); }

// i_rama_A > 0 cuando la corriente SALE del nodo VS1
static float comp_dt(float i_rama_A){
    return d_comp * limitar(i_rama_A / I_TH, -1.0f, 1.0f);
}

void isr_pwm_24khz(void){
    float s  = tabla_seno[indice];                 // referencia de 60 Hz
    float dA = (s >= 0.0f) ? M * s : 1.0f + M * s; // rama A (24 kHz), modulacion hibrida
    dA += comp_dt(i_medida);
    dA  = limitar(dA, D_MIN, D_MAX);
    pwm_rama_A(dA);
    rama_B(s >= 0.0f ? BAJO_ENCENDIDO : ALTO_ENCENDIDO);  // rama B a 60 Hz
}
```

En unipolar (modo cargador) se aplica la misma corrección a la rama B con su corriente, que es la de la rama A con signo contrario.

### 5.2 Medición del 3er armónico con Goertzel (etapa 4)

Un ciclo de 60 Hz, 100 muestras, sin el nivel de DC:

```c
#define N_MUESTRAS 100
static const float COEF = 1.9645745f;      // 2*cos(2*pi*3/100)

float goertzel_h3(const float *x){
    float s1 = 0.0f, s2 = 0.0f;
    for(int n = 0; n < N_MUESTRAS; n++){
        float s = x[n] + COEF * s1 - s2;
        s2 = s1; s1 = s;
    }
    float mag2 = s1*s1 + s2*s2 - COEF*s1*s2;
    return 2.0f * sqrtf(mag2) / N_MUESTRAS;  // amplitud del 3er armonico
}
```

Con K = 1 (COEF = 2·cos(2π/100) ≈ 1.9960534) se obtiene la fundamental, para reportar el 3° en porcentaje (h3 / h1 × 100).

### 5.3 Ajuste adaptativo de D_COMP (etapa 4)

Búsqueda del mínimo del 3er armónico, lenta y estable:

```c
// cada 10 ciclos de 60 Hz
float h3 = goertzel_h3(muestras_vout);
if (h3 > h3_anterior) paso = -paso;       // empeoró: invertir dirección
d_comp += paso;                           // por ejemplo paso = 0.0005
d_comp  = limitar(d_comp, 0.0f, 0.04f);
h3_anterior = h3;
```

Opcional: guardar el `d_comp` aprendido en memoria no volátil por módulo para arrancar ya calibrado.

---

## 6. Ajuste en banco y precauciones

1. Medir la salida con analizador de armónicos o FFT del osciloscopio y anotar el 3er armónico sin compensación; compararlo con la tabla de la sección 2.
2. Activar la compensación. Si el 3° sube en lugar de bajar, la convención de signo de la corriente está invertida.
3. Ajustar D_COMP en pasos de ±10 % hasta el mínimo del 3°.
4. Ajustar I_TH mirando el cruce por cero: si aparece un escalón u oscilación, subirlo.
5. Repetir con carga resistiva, sin carga y con motor (FP bajo).

Precauciones:

- **Piso de tiempo muerto, nunca por debajo:** ~390 ns con IRS2186 e IRF100P219; lo que fije R_DT en el UCC21520; ~0.8–1 µs con TLP250 por la variación de retardo entre piezas (±150 ns). La compensación corrige armónicos; reducir el tiempo muerto solo ahorra conducción de diodo y arriesga conducción cruzada.
- **Resonancia del filtro:** con poca carga o con motores, los armónicos 27° y 29° se amplifican. La red RC de amortiguamiento no es opcional si se arrancan motores.
- **Armónicos pares o componente de DC en la medición:** indican asimetría entre ramas (dos TLP250 con retardos distintos, MOSFET distintos). La DC puede saturar el transformador.
- **Bootstrap:** mantener el ciclo de trabajo máximo en 95 % aunque la compensación lo empuje hacia arriba.

---

## 7. Material relacionado del proyecto

- Simulador de armónicos por tiempo muerto (artefacto): compara 0 ns, 500 ns y 1 µs; forma de onda, error, armónicos 2°–40°, espectro y flancos.
- Simulador del bootstrap del IRS2186 (artefacto).
- Documento en línea: "Compensación adaptativa de tiempo muerto — etapa posterior" (mismo contenido que este archivo).
- Hojas de justificación en PNG: estudio de MOSFET, red de compuerta, térmico (30 / 40 / 60 °C), capacitores, Rubycon 820 µF y gate driver IRS2186.

## 8. Bitácora de seguimiento

| Fecha | Etapa | Qué se hizo | Resultado | Siguiente paso |
| --- | --- | --- | --- | --- |
| 2026-11-02 | 1 | Definición de la ruta de compensación | Documento creado | Validar prototipo en banco |
| | | | | |
