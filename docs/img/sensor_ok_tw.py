import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib
matplotlib.rcParams['font.weight']='bold'

def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=20.3, unit=3.0, font='FreeMono', lw=2.8, inches_per_unit=0.85)
        d += elm.Label().at((-1.0, 13.6)).label('SENSOR_OK — señal de seguridad con lógica invertida (fail-safe)', loc='right', fontsize=30.4, halign='left')
        d += elm.Label().at((-1.0, 12.4)).label('El sensor jala la línea a 0 V cuando TODO ESTÁ BIEN.\nFalla, sensor ausente, cable roto o sensor sin alimentación → la línea sube → corte por hardware.',
                                                loc='right', fontsize=20.3, halign='left')

        # ---------------- Sensor board side ----------------
        d += elm.Label().at((-1.0, 10.6)).label('TARJETA DEL SENSOR DC\n(junto a los shunts, en el puente H)', loc='right', fontsize=24.6, halign='left', color='#1f4e9a')
        # LM393 output
        d += elm.Dot(open=True).at((0.0, 6.0)).label('Salida del LM393\n(colector abierto,\nbaja con falla)', loc='left', fontsize=19.6)
        d += elm.Line().at((0.0, 6.0)).right(2.4)
        d += (nb := elm.Dot())
        d += elm.Resistor().at(nb.center).up(2.2).label('R_B1\n10 kΩ', loc='bottom', fontsize=19.6)
        d += elm.Vdd().label('5V del módulo\n(78L05)')
        d += elm.Line().at(nb.center).right(1.2)
        q1 = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q_OK\nBC547 / 2N2222', loc='right', fontsize=19.6)
        d += q1
        d += elm.Line().at(q1.emitter).down(0.6)
        d += elm.Ground()
        d += elm.Line().at(q1.collector).up(0.8)
        d += (nc1 := elm.Dot())
        d += elm.Line().right(4.0)
        d += (t3 := elm.Dot(open=True).label('J2-3 SENSOR_OK', loc='top', fontsize=20.3))

        # cable
        d += elm.Line().at(t3.center).right(4.2).linestyle('--')
        d += elm.Label().at((t3.center[0] + 2.1, t3.center[1] - 0.45)).label('cable trenzado con GND', fontsize=18.8, color='gray')

        # ---------------- Control board side ----------------
        xc = t3.center[0] + 4.2
        d += elm.Label().at((xc - 0.5, 11.0)).label('TARJETA DE CONTROL', loc='right', fontsize=24.6, halign='left', color='#1f4e9a')
        d += (j6 := elm.Dot(open=True).at((xc, t3.center[1])).label('J6-3', loc='top', fontsize=20.3))
        d += elm.Line().at(j6.center).right(1.4)
        d += (nk := elm.Dot())
        d += elm.Resistor().at(nk.center).up(2.2).label('R26\n10 kΩ', loc='bottom', fontsize=19.6)
        d += elm.Vdd().label('5V')
        # J_RIESGO to GND (down)
        d += elm.Line().at(nk.center).down(0.6)
        d += (nj := elm.Dot())
        d += elm.Switch().at(nj.center).down(1.6).label('J_RIESGO\n(puente)', loc='bottom', fontsize=19.6)
        d += elm.Ground()
        # to base of Q_EN
        d += elm.Line().at(nk.center).right(1.6)
        d += (nb2 := elm.Dot())
        d += elm.Resistor().right(2.2).label('R_B2  10 kΩ', loc='top', fontsize=19.6)
        q2 = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q_EN\nBC547 / 2N2222', loc='right', fontsize=19.6)
        d += q2
        d += elm.Line().at(q2.emitter).down(0.6)
        d += elm.Ground()
        d += elm.Line().at(q2.collector).up(1.0)
        d += elm.Dot(open=True).label('EN_HW  (74HC08, pines 2·5·10·13)', loc='right', fontsize=20.3)
        # D3 AUX
        d += elm.Line().at(nb2.center).up(2.6)
        d += elm.Dot(open=True).label('D3 (AUX)  INT1\nfalla = flanco de subida', loc='right', fontsize=20.3)

        # truth table
        d += elm.Label().at((-1.0, 2.9)).label(
            'Tabla de estados:\n'
            '  Situación                                  Q_OK        SENSOR_OK    Q_EN        EN_HW        Resultado\n'
            '  Sensor presente, sin falla                 conduce     0 V          apagado     libre        [OK] opera, con protección\n'
            '  Sobrecorriente                             apagado     5 V          conduce     0            [X]  corte en ~3-4 µs\n'
            '  Sensor ausente / cable roto                —           5 V (R26)    conduce     0            [X]  no arranca\n'
            '  Sensor sin alimentación (sin 15 V)         apagado     5 V          conduce     0            [X]  no arranca\n'
            '  J_RIESGO puesto (con o sin sensor)         —           0 V          apagado     libre        [!]  opera SIN protección de sobrecorriente',
            loc='right', fontsize=19.6, halign='left', valign='top')

        d += elm.Label().at((-1.0, -0.9)).label(
            'Notas:\n'
            '• En el módulo del sensor DC, el terminal 3 de J2 cambia de "FAULT" a "SENSOR_OK". Se agregan Q_OK y R_B1 junto al LM393.\n'
            '• En la tarjeta de control, Q_EN reemplaza al diodo D8. EN_HW conserva R_PD (10 kΩ), el switch ON/OFF en serie, D_K (AUX_KILL) y R_SEN.\n'
            '• J_RIESGO = riesgo autorizado por el usuario: con el puente se ignora TAMBIÉN la sobrecorriente, aunque el sensor esté conectado.\n'
            '  Serigrafía: "J_RIESGO: PUENTE = SIN PROTECCIÓN OC · QUITAR SI HAY SENSOR".  Jumper rojo.\n'
            '• R25 (1 kΩ) limita la corriente de D12 cuando Q_EN jala EN_HW.  BC547 o 2N2222: marcar "E-B-C"/"C-B-E" en la serigrafía.\n'
            '• Firmware del AUX: interrupción INT1 por flanco de subida en D3 → AUX_KILL = 0, aviso al CRÍTICO por I2C, LCD y buzzer.',
            loc='right', fontsize=19.6, halign='left', valign='top')

for f, dpi in [('/home/claude/github/esquematico_sensor_ok.png', 300), ('/home/claude/github/esquematico_sensor_ok.pdf', 300)]:
    build(f, dpi)
print('ok')
