/*
 * =============================================================================
 *  enlace_maestro.h  -  Isla del Sol, micro CRÍTICO (A1)
 *  El enlace con el AUX. El CRÍTICO es maestro DEL BUS; el que manda es el AUX.
 * -----------------------------------------------------------------------------
 *  POR QUÉ VOLTEARLO
 *
 *    DC-010 puso al CRÍTICO de esclavo con este argumento: «el CRÍTICO nunca
 *    espera al bus». Visto en banco, ese argumento resultó más débil de lo
 *    que parecía, y el precio más caro.
 *
 *    **Un esclavo no decide cuándo se abre la comunicación.** Tiene que estar
 *    siempre escuchando, y su máquina de estados queda a merced de los
 *    tiempos del otro. Si el maestro aborta a media transferencia, el esclavo
 *    se queda agarrando el bus y no se destraba solo.
 *
 *    **Un maestro abre cuando él quiere, manda, cierra, y vuelve a empezar.**
 *    Nunca se queda colgado esperando a nadie, porque nadie más inicia nada.
 *    Y si el bus se traba, es él quien tiene el reloj para destrabarlo.
 *
 *  LO QUE CUESTA, Y ES TODO LO QUE CUESTA
 *
 *        cada transferencia   2.3 ms   (medido en banco)
 *        10 por segundo       23 ms/s  =  2.3 % del lazo principal
 *
 *    El bloqueo es **del lazo, no de las interrupciones**: la ISR del SPWM
 *    sigue corriendo igual, porque va por interrupción. Eso es lo que hace
 *    barato el cambio — y es justo lo que DC-010 no tuvo en cuenta.
 *
 *  Y DE PASO SE LLEVA UN DEFECTO QUE NO BUSCÁBAMOS
 *
 *    Siendo esclavo, la foto de estado vivía en un búfer que la interrupción
 *    podía leer en cualquier momento, así que había que copiarla con las
 *    interrupciones apagadas:
 *
 *        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
 *          for (uint8_t i = 0; i < 16; i++) tx[i] = f[i];
 *        }
 *
 *    **212 ciclos con las interrupciones apagadas**, contados en el binario,
 *    y el bloqueo más largo de todo el sistema — más que `millis()`. Es lo
 *    que en la etapa 3 hacía perder pasos de la onda.
 *
 *    De maestro **ese bloque no existe**: la foto se arma y se manda en la
 *    misma llamada, nadie más la lee, y no hay carrera que proteger.
 *
 *    Un cambio, dos problemas.
 *
 *  ⚠️ MAESTRO DEL BUS NO ES QUIEN MANDA
 *
 *    Esto hay que leerlo antes de cualquier otra cosa, porque la palabra
 *    «maestro» confunde: **el que sigue mandando es el AUX.**
 *
 *      · El AUX es el panel. Él pide los perfiles de batería, él trae los
 *        permisos de carga y descarga que el BMS manda por CAN, y él tiene el
 *        AUX_KILL por hardware, que apaga el puente sin pedirle permiso a
 *        nadie ni pasar por este bus.
 *      · El CRÍTICO es maestro **del bus**, y nada más del bus. Lo único que
 *        hace en cada transferencia es **reportar su estado y recoger
 *        órdenes**. No decide nada del otro micro.
 *
 *    En una línea: **el CRÍTICO lleva el reloj, el AUX manda.**
 *
 *    Es maestro por una razón mecánica, no de jerarquía: el que abre la
 *    transferencia es el único que puede cerrarla, y el único que tiene el
 *    reloj para destrabar el bus si se atora. Antes el CRÍTICO era esclavo y
 *    se quedaba agarrado al bus cuando el otro abortaba — con el agravante de
 *    que es el único de los dos que no puede permitírselo.
 *
 *    Y hay un límite a lo que el AUX manda, que viene de DC-010 y no cambia:
 *    **el CRÍTICO nunca depende del enlace para protegerse.** Sobrecorriente,
 *    bajo voltaje y temperatura se deciden con sus propias lecturas. El AUX
 *    manda sobre lo que se puede conversar, no sobre lo que protege.
 *
 *  QUÉ PASA POR EL BUS
 *
 *    Cada 100 ms, el CRÍTICO:
 *      1) escribe la foto de estado (16 bytes, con CRC-8) al AUX  — reporta
 *      2) lee un comando (4 bytes, con CRC-8) del AUX            — obedece
 *
 *    Nótese el sentido: el CRÍTICO **entrega cuentas** y **recoge órdenes**.
 *    Abre él la conversación, pero el contenido va en la dirección de siempre.
 *
 *    Mismo contenido y mismo CRC que el protocolo de siempre: sólo cambia
 *    quién inicia. `enlace_protocolo.h` va igual, sin tocar.
 *
 *  ⚠️ El AUX ahora es esclavo **del bus**, en 0x21 — que no es lo mismo que
 *  estar subordinado: sigue siendo el que manda. Lo que se movió es la
 *  exposición a trabarse, y se movió a donde no duele: el AUX no tiene
 *  trabajo de tiempo real, así que si se cuelga se reinicia y no pasa nada.
 *  Si se colgaba el CRÍTICO, se perdían pasos de la onda.
 *
 *  Licencia: MIT (ver LICENSE.md del repositorio).
 * =============================================================================
 */

#pragma once

#include <stdint.h>

// Dirección del AUX cuando es esclavo. No reusamos 0x20 a propósito: así una
// tarjeta con firmware viejo no contesta por error.
#define ENLACE_DIRECCION_AUX   0x21

// Cada cuánto se abre una transferencia.
#define ENLACE_MS_ENTRE_ENVIOS 100

void enlace_iniciar(void);

/*  Se llama en cada vuelta del lazo. Cuando toca, abre la transferencia,
 *  manda la foto, lee el comando y cierra. El resto de las vueltas devuelve
 *  en seguida.
 */
void enlace_atender(uint8_t estadoP9, uint16_t adcP9, bool detenidoPorP9,
                    uint16_t bateriaDv, uint8_t estadoBateria,
                    uint8_t perfilBateria, bool perfilRechazado);

uint16_t enlace_envios_ok(void);      // transferencias completas y válidas
uint16_t enlace_envios_malos(void);   // el AUX contestó con CRC malo
uint16_t enlace_sin_respuesta(void);  // el AUX no contestó (sin ACK)
uint16_t enlace_us_max(void);         // la transferencia más lenta, en µs
bool     enlace_aux_presente(void);
bool     enlace_aux_cambio(void);
bool     enlace_pedido_perfil(uint8_t *p);
uint8_t  enlace_ultimo_seq(void);
void     enlace_reiniciar_contadores(void);
