# Etapa 2 — el ADC entra

Este sketch **mide el módulo real**, no una copia. Por eso el repositorio
guarda aquí nada más lo propio de la prueba:

```
ISLA_CRITICO_PRUEBA_ADC_PORTADORA.ino
traza_isr.h     enganches de la ISR del SPWM
traza_adc.h     enganches de la ISR del ADC
```

Para compilarlo hay que **copiar aquí** estos ocho archivos de
`../ISLA_CRITICO/`:

```
spwm_328p.cpp   spwm.h   config_spwm.h   config_portadora.h   tabla_seno.h
adc_portadora.cpp   adc_portadora.h   gancho_spwm.h
```

No se guardan copias en el repositorio **a propósito**: si el módulo cambia y
aquí quedara una copia vieja, estaríamos midiendo algo que ya no es lo que se
va a producción — que es justamente lo que esta prueba existe para evitar.

El ZIP que se reparte para el banco ya viene armado con los ocho adentro.

## Antes de conectar

- [ ] **J15 desconectado.** D4 es `INV_CMD`, D5 es `CARG_CMD` y D12 es `EN_HW`.
- [ ] **Etapa de potencia desconectada.**

## Qué mirar

| | Esperado |
| --- | --- |
| `PROPIO` | ~214 ciclos (eran 204 sin ADC) |
| `ADC inv/red/bat` | razón **2 : 1 : 1** |
| `esperadas` | una conversión cada 4 desbordes |
| **`PERDIDAS`** | **0** |
| Cajón de 161-220 en el histograma | vacío, o se explica |

Con el analizador, en paralelo: el ancho de D5 (la ISR del ADC) y el hueco
entre el inicio de D4 y el muestreo — **12 µs según DC-020**, que es la
afirmación que nadie ha comprobado.
