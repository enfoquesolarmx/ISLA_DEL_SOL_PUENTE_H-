import sys
import schemdraw
import schemdraw.elements as elm
from schemdraw import logic
import matplotlib
matplotlib.rcParams['font.weight'] = 'bold'

BLUE = '#1f4e9a'
F = 22     # base font
FS = 20    # small
FP = 18    # pin numbers

def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.8, inches_per_unit=0.85)

        d += elm.Label().at((-4.0, 20.2)).label(
            'CIRCUITO COMPLETO DE CORTE: SENSOR_OK + EN_HW + 74HC08 → GATE DRIVERS A y B',
            loc='right', fontsize=31, halign='left')
        d += elm.Label().at((-4.0, 18.9)).label(
            'Cada salida al driver = PWM  AND  EN_HW.   EN_HW = 0  →  las 4 salidas en 0  →  los 4 MOSFET apagados.\n'
            'EN_HW solo queda en 1 si:  D12 = 1  Y  switch en ON  Y  SENSOR_OK en 0 (sensor bien)  Y  AUX_KILL libre.',
            loc='right', fontsize=F, halign='left')

        # ================= SENSOR BOARD =================
        d += elm.Label().at((-4.0, 16.4)).label('[1] TARJETA DEL SENSOR DC\n(junto a los shunts, puente H)',
                                                loc='right', fontsize=24.6, halign='left', color=BLUE)
        Y = 7.0
        d += elm.Dot(open=True).at((0.0, Y)).label('Salida del LM393\n(colector abierto,\nbaja con falla)', loc='left', fontsize=FS)
        d += elm.Line().at((0.0, Y)).right(2.4)
        d += (nb := elm.Dot())
        d += elm.Resistor().at(nb.center).up(2.4).label('R_B1\n10 kΩ', loc='bottom', fontsize=FS)
        d += elm.Vdd().label('5V del módulo\n(78L05)')
        d += elm.Line().at(nb.center).right(1.2)
        q1 = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q_OK\nBC547/\n2N2222', loc='right', fontsize=FS)
        d += q1
        d += elm.Line().at(q1.emitter).down(0.6)
        d += elm.Ground()
        d += elm.Line().at(q1.collector).up(1.0)
        d += elm.Line().right(4.4)
        d += (t3 := elm.Dot(open=True).label('J2-3\nSENSOR_OK', loc='top', fontsize=F))

        # cable
        d += elm.Line().at(t3.center).right(4.6).linestyle('--')
        d += elm.Label().at((t3.center[0] + 2.3, t3.center[1] - 0.55)).label('cable trenzado\ncon GND', fontsize=FS, color='gray', valign='top')

        # ================= CONTROL BOARD =================
        xc = t3.center[0] + 4.6
        ys = t3.center[1]
        d += elm.Label().at((xc - 0.6, 16.4)).label('[2] TARJETA DE CONTROL', loc='right', fontsize=24.6, halign='left', color=BLUE)
        d += (j6 := elm.Dot(open=True).at((xc, ys)).label('J6-3', loc='top', fontsize=F))
        d += elm.Line().at(j6.center).right(1.6)
        d += (nk := elm.Dot())
        d += elm.Resistor().at(nk.center).up(2.6).label('R26\n10 kΩ', loc='bottom', fontsize=FS)
        d += elm.Vdd().label('5V')
        d += elm.Line().at(nk.center).down(0.7)
        d += (nj := elm.Dot())
        d += elm.Switch().at(nj.center).down(1.8).label('J_RIESGO\n(jumper\nrojo)', loc='bottom', fontsize=FS)
        d += elm.Ground()
        d += elm.Line().at(nk.center).right(3.0)
        d += (nb2 := elm.Dot())
        d += elm.Resistor().right(2.6).label('R_B2 10 kΩ', loc='top', fontsize=FS)
        q2 = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q_EN\nBC547/\n2N2222', loc='right', fontsize=FS)
        d += q2
        d += elm.Line().at(q2.emitter).down(0.6)
        d += elm.Ground()
        # D3 AUX tap (down)
        d += elm.Line().at(nb2.center).down(3.4)
        d += elm.Dot(open=True).label('D3 (AUX)  INT1\nfalla = flanco\nde subida', loc='right', fontsize=FS)

        # EN_HW horizontal net
        Y_EN = 13.0
        d += elm.Line().at(q2.collector).toy(Y_EN)
        d += (n1 := elm.Dot())
        d += elm.Label().at((n1.center[0] + 0.25, Y_EN + 0.5)).label('EN_HW', loc='right', fontsize=24, halign='left', color='#a01818')
        x0 = n1.center[0]

        # R24 pull-down
        XA = x0 + 3.4
        d += elm.Line().at(n1.center).tox(XA)
        d += (na := elm.Dot())
        d += elm.Resistor().at(na.center).down(2.6).label('R24\n10 kΩ\npull-\ndown', loc='bottom', fontsize=FS)
        d += elm.Ground()

        # D12 chain: EN_HW <- switch <- R25 <- D12 (downwards)
        XB = XA + 3.6
        d += elm.Line().at(na.center).tox(XB)
        d += (nbx := elm.Dot())
        d += elm.Switch().at(nbx.center).down(2.4).label('SW ON/OFF\n(J_SW, por\ncable)', loc='bottom', fontsize=FS)
        d += elm.Resistor().down(2.6).label('R25\n1 kΩ', loc='bottom', fontsize=FS)
        d += elm.Dot(open=True).label('D12 (CRÍTICO)\nD12_ON_OF', loc='bottom', fontsize=FS)

        # D_K to AUX D7 (up). Anode on EN_HW, cathode to D7
        XC = XB + 4.6
        d += elm.Line().at(nbx.center).tox(XC)
        d += (nc := elm.Dot())
        d += elm.Diode().at(nc.center).up(2.4).label('D_K\n1N4148', loc='bottom', fontsize=FS)
        d += elm.Dot(open=True).label('D7 (AUX)\nAUX_KILL', loc='top', fontsize=FS)

        # R_SEN to CRITICO D8 (up)
        XD = XC + 4.0
        d += elm.Line().at(nc.center).tox(XD)
        d += (nd := elm.Dot())
        d += elm.Resistor().at(nd.center).up(2.4).label('R_SEN\n1 kΩ', loc='bottom', fontsize=FS)
        d += elm.Dot(open=True).label('D8 (CRÍTICO)\nEN_HW_SENSE', loc='top', fontsize=FS)

        # ================= 74HC08 =================
        XBUS = XD + 2.6
        d += elm.Line().at(nd.center).tox(XBUS)
        d += (nbus := elm.Dot())
        d += elm.Label().at((XBUS + 2.0, 16.4)).label('[3] 74HC08 (U_CORTE, DIP-14)\nPWM desde el CRÍTICO', loc='right', fontsize=24.6, halign='left', color=BLUE)

        XG = XBUS + 5.6
        gates = [
            ('D9',  'HA_IN', '1', '2', '3',  'HA → J1-1\n(driver A, alto)', 14.4),
            ('D10', 'LA_IN', '4', '5', '6',  'LA → J1-3\n(driver A, bajo)', 11.2),
            ('D11', 'HB_IN', '9', '10', '8', 'HB → J3-1\n(driver B, alto)', 8.0),
            ('D3',  'LB_IN', '12', '13', '11', 'LB → J3-3\n(driver B, bajo)', 4.8),
        ]
        in2s = []
        for pin, net, pa, pb, py, outlbl, y in gates:
            g = logic.And(inputs=2).theta(0).at((XG, y)).anchor('in1')
            d += g
            # PWM input from CRITICO
            xs = XBUS + 1.4
            d += elm.Dot(open=True).at((xs, g.in1[1]))
            d += elm.Label().at((xs - 0.1, g.in1[1] + 0.35)).label(f'{pin} {net}', loc='right', fontsize=FS, halign='left', valign='bottom')
            d += elm.Line().at((xs, g.in1[1])).tox(g.in1[0])
            # EN input
            d += elm.Line().at(g.in2).tox(XBUS)
            d += elm.Dot().at((XBUS, g.in2[1]))
            in2s.append(g.in2[1])
            # pin numbers
            d += elm.Label().at((g.in1[0] - 0.15, g.in1[1] + 0.05)).label(pa, fontsize=FP, loc='left', halign='right', valign='bottom', color='#555555')
            d += elm.Label().at((g.in2[0] - 0.15, g.in2[1] - 0.05)).label(pb, fontsize=FP, loc='left', halign='right', valign='top', color='#555555')
            # output
            d += elm.Line().at(g.out).right(1.6)
            d += elm.Dot(open=True).label(outlbl, loc='right', fontsize=F)
            d += elm.Label().at((g.out[0] + 0.15, g.out[1] + 0.1)).label(py, fontsize=FP, loc='right', halign='left', valign='bottom', color='#555555')
        d += elm.Line().at((XBUS, min(in2s))).toy(max(in2s))

        # power pins of 74HC08
        XP = XG + 4.0
        d += elm.Vdd().at((XP, 1.8)).label('5V')
        d += elm.Line().at((XP, 1.8)).down(0.6)
        d += (nv := elm.Dot())
        d += elm.Label().at((XP - 0.3, 1.2)).label('pin 14\nVCC', loc='left', fontsize=FS, halign='right')
        d += elm.Capacitor().at(nv.center).down(2.0).label('100 nF\npegado\nal pin 14', loc='bottom', fontsize=FS)
        d += elm.Ground()
        d += elm.Label().at((XP + 0.9, -2.0)).label('pin 7 = GND', loc='right', halign='left', fontsize=FS)

        # ================= TABLE =================
        TY = -2.2
        d += elm.Label().at((-4.0, TY)).label(
            'TABLA DE ESTADOS (lo que debe pasar):\n'
            '  Situación                          SENSOR_OK  Q_EN     EN_HW  Salidas 74HC08   Resultado\n'
            '  Sensor bien + SW ON + D12 = 1      0 V        apagado  1      = PWM            [OK] opera, con protección\n'
            '  Sobrecorriente (LM393 baja)        5 V        conduce  0      0 0 0 0          [X]  corte en ~3-4 µs + aviso D3 AUX\n'
            '  Sensor ausente / cable roto        5 V (R26)  conduce  0      0 0 0 0          [X]  no arranca\n'
            '  Sensor sin alimentación            5 V        conduce  0      0 0 0 0          [X]  no arranca\n'
            '  Switch OFF o cable del SW suelto   --         --       0(R24) 0 0 0 0          [X]  apagado\n'
            '  MCU en reset / cargando programa   --         --       0(R24) 0 0 0 0          [X]  apagado (D12 flota)\n'
            '  AUX_KILL = 0 (enclavado, AUX)      --         --       0      0 0 0 0          [X]  apagado hasta ENTER\n'
            '  J_RIESGO puesto                    0 V        apagado  1      = PWM            [!]  opera SIN protección OC',
            loc='right', fontsize=FS, halign='left', valign='top')

        d += elm.Label().at((-4.0, TY - 5.2)).label(
            'NOTAS PARA LA TRINCHERA:\n'
            '• Usar 74HC08 (AND).  NUNCA 74HC00 (NAND): con EN_HW = 0 pondría las 4 salidas en 1 y encendería los 4 MOSFET.\n'
            '• En KiCad: símbolo 74xx:74LS08 con valor "74HC08", footprint DIP-14 en zócalo.\n'
            '• R24 (10 kΩ) y R25 (1 kΩ) son OBLIGATORIAS: R24 apaga con cable suelto o micro en reset; R25 protege D12 cuando Q_EN o D_K jalan EN_HW.\n'
            '• Q_EN y Q_OK: BC547 o 2N2222.  Serigrafía "E-B-C" en el orden real del footprint.  D_K: cátodo hacia D7 del AUX.\n'
            '• Serigrafía junto a J_RIESGO: "PUENTE = SIN PROTECCIÓN OC · QUITAR SI HAY SENSOR".\n'
            '• Firmware del AUX al ver flanco de subida en D3: AUX_KILL = 0, aviso al CRÍTICO por I2C (D12 = 0), LCD + buzzer.  Rearme solo con ENTER.\n'
            '• El CRÍTICO lee EN_HW en D8: si D12 = 1 y D8 = 0 → algo cortó (switch, sensor o AUX); el AUX dice cuál por I2C.\n'
            '• Líneas lógicas de 0.25 mm; EN_HW y las 4 salidas lejos de la potencia.  Cable de SW > 30 cm: 10 nF de EN_HW a GND junto al 74HC08.',
            loc='right', fontsize=FS, halign='left', valign='top')

if __name__ == '__main__':
    dpi = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    build(sys.argv[1], dpi)
    print('ok')
