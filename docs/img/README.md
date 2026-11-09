# Diagramas de `docs/`

Cada diagrama se genera con su script de Python (schemdraw), así se puede corregir y volver a generar: `python3 <script>.py <salida>.png [dpi]`.

| Diagrama | Script | Lo usa |
| --- | --- | --- |
| [esquematico_corte_completo.png](esquematico_corte_completo.png) | [completo_tw.py](completo_tw.py) | [corte_por_hardware.md](../corte_por_hardware.md), DC-014 |
| [esquematico_sensor_ok.png](esquematico_sensor_ok.png) | [sensor_ok_tw.py](sensor_ok_tw.py) | [corte_por_hardware.md](../corte_por_hardware.md) |
| [esquematico_lm393_dos_canales.png](esquematico_lm393_dos_canales.png) | [lm393_dos.py](lm393_dos.py) | DC-016 |
| [esquematico_modulo_sensor_dc_u4.png](esquematico_modulo_sensor_dc_u4.png) | [sensor_mod.py](sensor_mod.py) | DC-016, módulo sensor V0.2 |
| [esquematico_transferencia_v1_dc019.png](esquematico_transferencia_v1_dc019.png) | [esquematico_transferencia_v1_dc019.py](esquematico_transferencia_v1_dc019.py) | DC-019 (transferencia, SW_TN, filtro C_F) |
| [referencia_outback_fx.png](referencia_outback_fx.png) | [referencia_outback_fx.py](referencia_outback_fx.py) | [referencia_outback_fx.md](../referencia_outback_fx.md) |

`outback_fx/`: fotos de banco del OutBack FX (evidencia del estudio de referencia).
