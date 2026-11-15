// Prueba en PC de bateria.cpp (limites y retardos). Compilar desde esta carpeta:
//   g++ -std=gnu++11 -I. -I../../ISLA_CRITICO -include Arduino.h prueba_bateria.cpp ../../ISLA_CRITICO/bateria.cpp -o prueba && ./prueba
#include <stdio.h>
#include "Arduino.h"
#include "bateria.h"
#include "config_bateria.h"
uint32_t g_ms=0; uint16_t g_adc=0;
static uint16_t adcDe(uint16_t dv){ return (uint32_t)dv*1023/730; }
static const char* n(EstadoBateria e){const char*t[]={"NORMAL","AVISO","CORTE_BAJO","CORTE_ALTO","FUERA"};return t[e];}
static void corre(uint16_t dv, uint32_t ms){ g_adc=adcDe(dv); uint32_t fin=g_ms+ms; EstadoBateria a=bateria_estado();
  for(;g_ms<fin;g_ms+=10){ bateria_atender(); if(bateria_estado()!=a){a=bateria_estado(); printf("  t=%6u ms  V=%u.%u -> %s\n",g_ms,bateria_dv()/10,bateria_dv()%10,n(a));} } }
int main(){
  g_adc=adcDe(510); bateria_leer_ahora(); printf("inicio 51.0 V: %s\n",n(bateria_estado()));
  puts("44.0 V 3 s (aviso):"); corre(440,3000);
  puts("41.5 V 5 s (motor, no corta):"); corre(415,5000);
  puts("45 V 1 s:"); corre(450,1000);
  puts("41.5 V 12 s (corta a los 10 s):"); corre(415,12000);
  puts("rebote 47 V 40 s (NO reconecta):"); corre(470,40000);
  puts("50.4 V 20 s y 46 V (no llega a 30 s):"); corre(504,20000); corre(460,1000);
  puts("50.4 V 35 s (reconecta a 30 s):"); corre(504,35000);
  puts("36 V 2 s (emergencia 0.5 s):"); corre(360,2000);
  puts("50.4 V 31 s:"); corre(504,31000);
  puts("67 V 3 s (alto a 2 s):"); corre(670,3000);
  puts("63 V 31 s (vuelve):"); corre(630,31000);
  puts("A3 = 1023 (R4 abierta):"); g_adc=1023; for(int i=0;i<10;i++){g_ms+=50;bateria_atender();} printf("  %s\n",n(bateria_estado()));
  puts("vuelve a 50 V 40 s (debe seguir FUERA):"); corre(500,40000); printf("  %s\n",n(bateria_estado()));
}
