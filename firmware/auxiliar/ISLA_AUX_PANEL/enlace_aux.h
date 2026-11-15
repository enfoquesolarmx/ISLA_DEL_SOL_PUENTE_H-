/*
 * =============================================================================
 *  enlace_aux.h  -  Isla del Sol, AUX (A2): maestro I2C hacia el CRÍTICO
 * -----------------------------------------------------------------------------
 *  Lo mismo que la prueba validada ISLA_AUX_PRUEBA_I2C, hecho módulo:
 *  cada 100 ms manda PING y pide la foto de 16 bytes; revisa largo, CRC-8,
 *  versión y eco. Nunca entrega una foto de más de 300 ms.
 *
 *  RECURSOS: A4 (SDA) y A5 (SCL), TWI (Wire). La pantalla NO va aquí (D4/D5).
 *  Protocolo: enlace_protocolo.h (idéntico al del CRÍTICO).
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */
#pragma once
#include <stdint.h>

enum EstadoEnlace : uint8_t { ENLACE_NUNCA, ENLACE_OK, ENLACE_PERDIDO, ENLACE_CONGELADO };

// Lo que el panel necesita de la foto del CRÍTICO (ya decodificado).
struct FotoCritico {
  bool     spwmActiva;
  bool     detenidoPorP9;
  uint8_t  frecuenciaHz;   // 50 o 60
  uint8_t  estadoP9;       // EstadoP9 del CRÍTICO (0..5)
  uint8_t  dtCuentas;      // 8 = 500 ns, 16 = 1 µs
  uint16_t ciclos;
  uint16_t adcP9;
  uint16_t amplitudMax;
  uint16_t bateriaDv;      // batería en décimas de V (protocolo v2)
  uint8_t  estadoBateria;  // 0 normal, 1 aviso, 2 corte bajo, 3 corte alto, 4 fuera de rango
  uint8_t  seq;
  uint8_t  eco;            // secuencia del último comando que el CRÍTICO aceptó
  uint8_t  perfilBateria;  // PerfilBateria activo en el CRÍTICO
  bool     perfilRechazado;// el último cambio de tecnología fue rechazado
};

void enlace_aux_iniciar();
// Llamar en cada vuelta de loop(). Devuelve true una vez cada que cambia el estado.
bool enlace_aux_atender();
EstadoEnlace enlace_aux_estado();
// true y copia la foto si hay una válida de menos de 300 ms.
bool enlace_aux_foto(FotoCritico *f);
// Manda (en el siguiente turno, en lugar del PING) el comando para cambiar la
// tecnología de batería. Devuelve la secuencia con la que sale, para
// reconocer la respuesta en FotoCritico::eco.
uint8_t enlace_aux_pedir_perfil(uint8_t perfil);
const __FlashStringHelper *enlace_aux_texto(EstadoEnlace e);
uint32_t enlace_aux_tramas_ok();
uint32_t enlace_aux_tramas_total();
