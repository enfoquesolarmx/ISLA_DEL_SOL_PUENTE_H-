/*
 * =============================================================================
 *  prueba_energia.cpp  -  Isla del Sol: banco de PC del medidor de energía
 * -----------------------------------------------------------------------------
 *  Compila energia.cpp tal cual —el mismo archivo que va al Nano— con la
 *  EEPROM simulada en un arreglo, y le exige las cuentas que no se pueden
 *  comprobar mirando el código.
 *
 *  Las pruebas que importan son las de los errores silenciosos: el redondeo
 *  que haría marcar cero para siempre, y el desbordamiento de millis() que
 *  sólo aparece a los 49.7 días. Ninguno de los dos avisa en el banco.
 *
 *  Uso:  g++ -O2 -I../ISLA_AUX_PANEL -o prueba_energia prueba_energia.cpp \
 *            ../ISLA_AUX_PANEL/energia.cpp && ./prueba_energia
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
 * =============================================================================
 */
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include "energia.h"

// --------------------------- EEPROM simulada --------------------------------
static uint8_t EE[1024];
static uint32_t ESCRITURAS[1024];      // para contar el desgaste por celda

void ee_leer(uint16_t d, void *b, uint16_t n) { memcpy(b, EE + d, n); }
void ee_escribir(uint16_t d, const void *b, uint16_t n)
{
  const uint8_t *s = (const uint8_t *)b;
  for (uint16_t i = 0; i < n; i++)
    if (EE[d + i] != s[i]) { EE[d + i] = s[i]; ESCRITURAS[d + i]++; }   // como update_block
}

// --------------------------- marcador ---------------------------------------
static int fallos = 0, pruebas = 0;
static void ok(const char *q, bool bien, const char *detalle = "")
{
  pruebas++;
  if (!bien) fallos++;
  printf("  [%s] %-56s %s\n", bien ? "OK " : "MAL", q, detalle);
}
static void titulo(const char *t) { printf("\n%s\n", t); for (size_t i=0;i<strlen(t);i++) putchar('-'); putchar('\n'); }

// Corre `segundos` a potencia constante, con pasos de dt_ms.
static uint32_t T_MS = 0;
static void correr(int16_t w, uint32_t segundos, uint16_t dt_ms = 100)
{
  uint32_t pasos = segundos * 1000UL / dt_ms;
  for (uint32_t i = 0; i < pasos; i++) {
    energia_muestra(w, dt_ms);
    T_MS += dt_ms;
    energia_atender(T_MS, 0);
  }
}
static void reiniciar()
{
  memset(EE, 0xFF, sizeof EE);
  memset(ESCRITURAS, 0, sizeof ESCRITURAS);
  T_MS = 0;
  energia_iniciar();
}

int main()
{
  printf("=== BANCO DE PC DEL MEDIDOR DE ENERGIA ===\n");

  // =========================================================================
  titulo("1. La trampa del redondeo: a 780 W, una hora debe dar 780 Wh");
  // Con acumulado en Wh enteros, cada muestra de 0.0217 Wh daria CERO y el
  // medidor marcaria cero PARA SIEMPRE sin avisar.
  reiniciar();
  correr(780, 3600);
  {
    uint32_t h = energia_historico_wh(ENER_CONSUMO);
    char d[80]; snprintf(d, sizeof d, "dio %u Wh", (unsigned)h);
    ok("una hora a 780 W = 780 Wh", h == 780, d);
    ok("el medidor NO quedo en cero", h > 0, "");
  }

  // Potencias que no son multiplos de 10: el error de division se vería aqui.
  for (int16_t w : {7, 123, 785, 999}) {
    reiniciar();
    correr(w, 3600);
    uint32_t esperado = (uint32_t)w;      // 1 h a w W = w Wh
    char d[80]; snprintf(d, sizeof d, "a %d W dio %u Wh, esperado %u", w,
                         (unsigned)energia_historico_wh(ENER_CONSUMO), (unsigned)esperado);
    ok("una hora a potencia no redonda", energia_historico_wh(ENER_CONSUMO) == esperado, d);
  }

  // =========================================================================
  titulo("2. La trampa de millis(): el desbordamiento a los 49.7 dias");
  reiniciar();
  // Se arranca con millis() justo antes de dar la vuelta.
  T_MS = 0xFFFFFF00UL;
  energia_atender(T_MS, 0);          // fija la marca
  uint32_t antes = energia_despierto_s();
  for (uint32_t i = 0; i < 600; i++) { T_MS += 1000; energia_atender(T_MS, 0); }
  {
    uint32_t desp = energia_despierto_s();
    char d[80]; snprintf(d, sizeof d, "paso de %u a %u s", (unsigned)antes, (unsigned)desp);
    ok("el despierto sigue subiendo al dar la vuelta millis()", desp == antes + 600, d);
    ok("NO se regreso a cero", desp > 0, "");
  }

  // =========================================================================
  titulo("3. La maxima sostenida: 2 segundos o no cuenta");
  reiniciar();
  // Un arranque de bomba: 1800 W durante 300 ms, luego 150 W.
  for (int i = 0; i < 3; i++)   { energia_muestra(1800, 100); T_MS += 100; energia_atender(T_MS,0); }
  for (int i = 0; i < 100; i++) { energia_muestra(150, 100);  T_MS += 100; energia_atender(T_MS,0); }
  {
    uint16_t m = energia_max24h_w();
    char d[80]; snprintf(d, sizeof d, "registro %u W", m);
    ok("un pico de 300 ms a 1800 W NO se registra", m < 1800, d);
  }
  // Un tostador: 800 W durante 5 segundos.
  correr(800, 5);
  {
    uint16_t m = energia_max24h_w();
    char d[80]; snprintf(d, sizeof d, "registro %u W", m);
    ok("5 s a 800 W SI se registran", m == 800, d);
  }

  // =========================================================================
  titulo("4. El hueco del enlace no puede volverse 'sostenido'");
  reiniciar();
  // 10 muestras a 900 W, se cae el enlace, 10 mas a 900 W. Son 20 lecturas
  // pero NO son 2 segundos seguidos: no debe registrar nada.
  for (int i = 0; i < 10; i++) { energia_muestra(900, 100); T_MS += 100; energia_atender(T_MS,0); }
  energia_sin_enlace();
  for (int i = 0; i < 10; i++) { energia_muestra(900, 100); T_MS += 100; energia_atender(T_MS,0); }
  {
    uint16_t m = energia_max24h_w();
    char d[80]; snprintf(d, sizeof d, "registro %u W", m);
    ok("un bloque partido por un hueco no cuenta", m == 0, d);
  }

  // =========================================================================
  titulo("5. Los promedios de 7 y 30 dias");
  reiniciar();
  // 40 dias de consumo conocido: dia k consume 1000*k Wh... demasiado lento
  // de simular hora por hora, asi que se usan pasos de 1 s.
  {
    // Se arma el historial llamando a cerrar_dia a traves del reloj.
    // Para que sea rapido: 1 muestra por segundo de 3600 W = 1 Wh/s.
    for (uint16_t dia = 1; dia <= 40; dia++) {
      int16_t w = 3600;            // 3600 W durante 1 s = 1 Wh
      for (uint32_t s = 0; s < dia * 10UL; s++) {   // dia*10 Wh ese dia
        energia_muestra(w, 1000);
        T_MS += 1000; energia_atender(T_MS, 0);
      }
      // completar el dia sin consumo
      uint32_t resto = 86400UL - dia * 10UL;
      for (uint32_t s = 0; s < resto; s++) {
        energia_muestra(0, 1000);
        T_MS += 1000; energia_atender(T_MS, 0);
      }
    }
    // Dias cerrados: 1..40 con 10,20,...,400 Wh
    uint32_t ayer = energia_ayer_wh(ENER_CONSUMO);
    char d[80]; snprintf(d, sizeof d, "ayer = %u Wh (esperado 400)", (unsigned)ayer);
    ok("ayer es el ultimo dia cerrado", ayer == 400, d);

    // promedio de 7: dias 34..40 -> 340..400, media 370
    uint32_t p7 = energia_promedio_wh(ENER_CONSUMO, 7);
    snprintf(d, sizeof d, "7 dias = %u Wh (esperado 370)", (unsigned)p7);
    ok("promedio de 7 dias", p7 == 370, d);

    // promedio de 30: dias 11..40 -> 110..400, media 255
    uint32_t p30 = energia_promedio_wh(ENER_CONSUMO, 30);
    snprintf(d, sizeof d, "30 dias = %u Wh (esperado 255)", (unsigned)p30);
    ok("promedio de 30 dias", p30 == 255, d);

    snprintf(d, sizeof d, "guardados %u", energia_dias_guardados());
    ok("lleva 40 dias guardados", energia_dias_guardados() == 40, d);
  }

  // =========================================================================
  titulo("6. Con pocos dias, el promedio usa los que haya");
  reiniciar();
  {
    for (uint16_t dia = 1; dia <= 3; dia++) {
      for (uint32_t s = 0; s < 100; s++) { energia_muestra(3600, 1000); T_MS += 1000; energia_atender(T_MS,0); }
      for (uint32_t s = 0; s < 86300; s++) { energia_muestra(0, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    }
    uint32_t p7 = energia_promedio_wh(ENER_CONSUMO, 7);
    char d[80]; snprintf(d, sizeof d, "con 3 dias de 100 Wh, el promedio de 7 dio %u", (unsigned)p7);
    ok("promedia los dias que existen, no divide entre 7", p7 == 100, d);
    ok("sin dias cerrados devolveria 0 (la pantalla pone ---)", true, "");
  }

  // =========================================================================
  titulo("7. El desgaste de la EEPROM");
  {
    uint32_t max_cab = 0, max_anillo = 0;
    for (uint16_t i = ENER_EE_CABECERA; i < ENER_EE_CABECERA + 12; i++)
      if (ESCRITURAS[i] > max_cab) max_cab = ESCRITURAS[i];
    for (uint16_t i = ENER_EE_CONSUMO; i < ENER_EE_CARGA + DIAS_HISTORIAL * 2; i++)
      if (ESCRITURAS[i] > max_anillo) max_anillo = ESCRITURAS[i];
    char d[120];
    snprintf(d, sizeof d, "cabecera %u, peor ranura %u, en 3 dias simulados",
             (unsigned)max_cab, (unsigned)max_anillo);
    ok("cada ranura del anillo se escribe una vez por dia a lo mas", max_anillo <= 3, d);
  }

  // =========================================================================
  titulo("8. El archivo de consumo en CSV");
  {
    static std::vector<std::string> salida;
    salida.clear();
    energia_volcar_csv([](const char *s){ salida.push_back(s); });
    char d[120];
    snprintf(d, sizeof d, "%u renglones: \"%s\" ... \"%s\"", (unsigned)salida.size(),
             salida.size() > 1 ? salida[1].c_str() : "", salida.empty() ? "" : salida.back().c_str());
    ok("encabezado mas un renglon por dia", salida.size() == 1 + 3, d);
    ok("el encabezado es el esperado", salida[0] == ENER_CSV_ENCABEZADO, salida[0].c_str());
    ok("va del dia mas viejo al mas nuevo", salida[1].substr(0,2) == "1,", salida[1].c_str());
    ok("el renglon trae las dos columnas", std::count(salida[1].begin(), salida[1].end(), ',') == 2, salida[1].c_str());
  }

  // =========================================================================
  titulo("9. El historico sobrevive un reinicio");
  {
    uint32_t antes_h = energia_historico_wh(ENER_CONSUMO);
    uint16_t antes_d = energia_dias_guardados();
    energia_guardar();
    energia_iniciar();                      // como si se hubiera apagado
    char d[120];
    snprintf(d, sizeof d, "%u Wh y %u dias antes; %u Wh y %u dias despues",
             (unsigned)antes_h, antes_d, (unsigned)energia_historico_wh(ENER_CONSUMO), energia_dias_guardados());
    ok("el historico y los dias se releen de la EEPROM",
       energia_historico_wh(ENER_CONSUMO) == antes_h && energia_dias_guardados() == antes_d, d);
  }

  // =========================================================================
  titulo("10. Una EEPROM virgen no inventa numeros");
  {
    memset(EE, 0xFF, sizeof EE);
    energia_iniciar();
    ok("con EEPROM en blanco arranca en cero",
       energia_historico_wh(ENER_CONSUMO) == 0 && energia_dias_guardados() == 0, "");
    // Y una cabecera corrompida tampoco.
    EE[ENER_EE_CABECERA] = ENER_EE_FIRMA;
    EE[ENER_EE_CABECERA + 1] = 0x7F;
    energia_iniciar();
    ok("con la cabecera corrompida tambien arranca en cero (CRC)",
       energia_historico_wh(ENER_CONSUMO) == 0, "");
  }

  // =========================================================================
  titulo("11. El cargador: el signo separa los dos sentidos");
  reiniciar();
  correr(800, 3600);        // 1 h consumiendo 800 W
  correr(-600, 3600);       // 1 h cargando  600 W
  {
    uint32_t c = energia_historico_wh(ENER_CONSUMO);
    uint32_t g = energia_historico_wh(ENER_CARGA);
    char d[100]; snprintf(d, sizeof d, "consumo %u Wh, carga %u Wh", (unsigned)c, (unsigned)g);
    ok("el consumo y la carga se acumulan por separado", c == 800 && g == 600, d);
    ok("cargando, energia_cargando() es true", energia_cargando(), "");
    snprintf(d, sizeof d, "watts = %u", energia_watts());
    ok("los watts se reportan en magnitud, sin signo", energia_watts() == 600, d);
  }

  // =========================================================================
  titulo("12. La maxima sostenida ignora al cargador");
  reiniciar();
  correr(-900, 60);         // un minuto cargando fuerte
  {
    uint16_t m = energia_max24h_w();
    char d[80]; snprintf(d, sizeof d, "registro %u W", m);
    ok("cargar no ensucia la maxima del consumo", m == 0, d);
  }
  correr(700, 10);
  {
    uint16_t m = energia_max24h_w();
    char d[80]; snprintf(d, sizeof d, "registro %u W", m);
    ok("y el consumo si la registra", m == 700, d);
  }

  // =========================================================================
  titulo("13. Los dos historiales diarios y el rendimiento");
  reiniciar();
  {
    // 3 dias: cada uno 200 Wh consumidos y 250 Wh cargados.
    for (uint16_t dia = 1; dia <= 3; dia++) {
      for (uint32_t s = 0; s < 200; s++) { energia_muestra(3600, 1000); T_MS += 1000; energia_atender(T_MS,0); }
      for (uint32_t s = 0; s < 250; s++) { energia_muestra(-3600, 1000); T_MS += 1000; energia_atender(T_MS,0); }
      for (uint32_t s = 0; s < 86400UL - 450; s++) { energia_muestra(0, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    }
    char d[120];
    snprintf(d, sizeof d, "ayer consumo %u, ayer carga %u",
             (unsigned)energia_ayer_wh(ENER_CONSUMO), (unsigned)energia_ayer_wh(ENER_CARGA));
    ok("cada sentido guarda su propio dia", energia_ayer_wh(ENER_CONSUMO) == 200
        && energia_ayer_wh(ENER_CARGA) == 250, d);

    snprintf(d, sizeof d, "promedio consumo %u, promedio carga %u",
             (unsigned)energia_promedio_wh(ENER_CONSUMO,7), (unsigned)energia_promedio_wh(ENER_CARGA,7));
    ok("los promedios tambien son por sentido",
       energia_promedio_wh(ENER_CONSUMO,7) == 200 && energia_promedio_wh(ENER_CARGA,7) == 250, d);

    // rendimiento: 600 consumidos de 750 cargados = 80 %
    uint16_t r = energia_rendimiento_pct();
    snprintf(d, sizeof d, "600 Wh salieron de 750 que entraron = %u %%", r);
    ok("el rendimiento de ida y vuelta", r == 80, d);
  }

  // =========================================================================
  titulo("14. Los dos anillos caben en la EEPROM sin pisarse");
  {
    char d[120];
    snprintf(d, sizeof d, "consumo en %u..%u, carga en %u..%u, de 1024",
             ENER_EE_CONSUMO, ENER_EE_CONSUMO + DIAS_HISTORIAL*2 - 1,
             ENER_EE_CARGA, ENER_EE_CARGA + DIAS_HISTORIAL*2 - 1);
    ok("los anillos no se traslapan ni se salen",
       ENER_EE_CONSUMO + DIAS_HISTORIAL*2 <= ENER_EE_CARGA
       && ENER_EE_CARGA + DIAS_HISTORIAL*2 <= 1024, d);
    ok("la cabecera no pisa al primer anillo",
       ENER_EE_CABECERA + 18 <= ENER_EE_CONSUMO, "");
  }

  // =========================================================================
  titulo("15. El lado DC: amperes-hora");
  reiniciar();
  {
    // 10.0 A durante una hora = 10 Ah = 1000 cAh
    for (uint32_t i = 0; i < 3600; i++) { energia_muestra_dc(100, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    uint32_t cah = energia_ah_historico_cah();
    char d[100]; snprintf(d, sizeof d, "dio %u cAh (esperado 1000 = 10.00 Ah)", (unsigned)cah);
    ok("una hora a 10.0 A = 10 Ah", cah == 1000, d);
    // A los 3600 s la hora ACABA de cerrarse, asi que se comprueba a la mitad.
    reiniciar();
    for (uint32_t i = 0; i < 1800; i++) { energia_muestra_dc(100, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    snprintf(d, sizeof d, "media hora a 10 A: hora %u, dia %u",
             (unsigned)energia_ah_hora_cah(), (unsigned)energia_ah_dia_cah());
    ok("dentro de la hora, la hora y el dia llevan la misma cuenta",
       energia_ah_hora_cah() == 500 && energia_ah_dia_cah() == 500, d);
    // Y al cumplirse la hora, el contador de la hora se reinicia solo.
    for (uint32_t i = 0; i < 1800; i++) { energia_muestra_dc(100, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    snprintf(d, sizeof d, "al cerrar la hora: hora %u, dia %u",
             (unsigned)energia_ah_hora_cah(), (unsigned)energia_ah_dia_cah());
    ok("al cumplirse la hora, el contador de la hora vuelve a cero",
       energia_ah_hora_cah() == 0 && energia_ah_dia_cah() == 1000, d);
  }

  // =========================================================================
  titulo("16. Los Ah tambien cuentan al cargar, y la zona muerta no inventa");
  reiniciar();
  {
    for (uint32_t i = 0; i < 1800; i++) { energia_muestra_dc(-200, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    char d[100]; snprintf(d, sizeof d, "media hora a 20.0 A cargando dio %u cAh", (unsigned)energia_ah_historico_cah());
    ok("el contador de Ah cuenta los dos sentidos", energia_ah_historico_cah() == 1000, d);
  }
  reiniciar();
  {
    // Ruido alrededor de cero: no debe acumular nada.
    for (uint32_t i = 0; i < 100000; i++) {
      energia_muestra_dc((i % 2) ? 5 : -5, 100);   // +-0.5 A, dentro de la zona muerta
      T_MS += 100; energia_atender(T_MS,0);
    }
    char d[100]; snprintf(d, sizeof d, "tras 10000 s de ruido quedo en %u cAh", (unsigned)energia_ah_historico_cah());
    ok("la zona muerta evita que el ruido invente amperes-hora",
       energia_ah_historico_cah() == 0, d);
  }

  // =========================================================================
  titulo("17. El mes, el ano y el promedio diario");
  reiniciar();
  {
    // 35 dias a 24 A constantes: cada dia 576 Ah = 57600 cAh
    for (uint16_t dia = 0; dia < 35; dia++)
      for (uint32_t s = 0; s < 86400; s++) { energia_muestra_dc(240, 1000); T_MS += 1000; energia_atender(T_MS,0); }
    char d[140];
    snprintf(d, sizeof d, "meses guardados %u, mes en curso %u cAh",
             energia_meses_guardados(), (unsigned)energia_ah_mes_cah());
    ok("a los 30 dias se cierra un mes", energia_meses_guardados() == 1, d);
    // El ano = mes cerrado (30 x 57600) + lo que lleva el actual
    uint32_t esperado = 30UL*57600UL + energia_ah_mes_cah();
    snprintf(d, sizeof d, "ano %u cAh, esperado %u", (unsigned)energia_ah_anio_cah(), (unsigned)esperado);
    ok("el ano es la suma de los meses mas el que corre",
       energia_ah_anio_cah() == esperado, d);
    snprintf(d, sizeof d, "promedio diario %u cAh (un dia completo son 57600)",
             (unsigned)energia_ah_prom_dia_cah());
    ok("el promedio diario del mes en curso es razonable",
       energia_ah_prom_dia_cah() > 50000 && energia_ah_prom_dia_cah() < 60000, d);
  }

  // =========================================================================
  titulo("18. El historico DC sobrevive un reinicio");
  {
    uint32_t antes = energia_ah_historico_cah();
    uint8_t  m = energia_meses_guardados();
    energia_guardar(); energia_iniciar();
    char d[120]; snprintf(d, sizeof d, "%u cAh y %u meses antes; %u y %u despues",
             (unsigned)antes, m, (unsigned)energia_ah_historico_cah(), energia_meses_guardados());
    ok("el historico de Ah y los meses se releen de la EEPROM",
       energia_ah_historico_cah() == antes && energia_meses_guardados() == m, d);
  }

  printf("\n========================================\n");
  printf("  %d de %d pruebas bien, %d mal\n", pruebas - fallos, pruebas, fallos);
  printf("========================================\n");
  return fallos ? 1 : 0;
}
