import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib
matplotlib.rcParams['font.weight'] = 'bold'

BLUE = '#1f4e9a'; RED = '#a01818'; GREEN = '#1a6e1a'
F = 22; FS = 20

def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.8, inches_per_unit=0.85)
        d += elm.Label().at((-6.0, 19.6)).label(
            'LM393: UN SOLO CHIP CON DOS COMPARADORES ADENTRO (canal A y canal B)', loc='right', fontsize=31, halign='left')
        d += elm.Label().at((-6.0, 18.2)).label(
            'Los dos miran la MISMA señal del INA, cada uno con su propio umbral.\n'
            'Canal A = DISPARO DURO (corta y enclava).  Canal B = LÍMITE DE CORRIENTE (avisa al CRÍTICO, sin candado).',
            loc='right', fontsize=F, halign='left')

        d += elm.Label().at((-6.0, 15.6)).label('[1] TARJETA DEL SENSOR DC (junto a los shunts)', loc='right', fontsize=24.6, halign='left', color=BLUE)

        # chip outline (dashed)
        X0, X1, Y0, Y1 = 1.2, 10.6, 1.2, 14.0
        for (a, b, c, e) in [(X0, Y0, X1, Y0), (X1, Y0, X1, Y1), (X1, Y1, X0, Y1), (X0, Y1, X0, Y0)]:
            d += elm.Line().at((a, b)).to((c, e)).linestyle('--').color('gray')
        d += elm.Label().at((X0 + 0.2, Y1 - 0.3)).label('U: LM393 (DIP-8)\npin 8 = VCC 5V (78L05) · pin 4 = GND',
                                                        loc='right', fontsize=FS, halign='left', valign='top', color='gray')

        # op amps
        oa = elm.Opamp(leads=True).theta(0).at((4.2, 10.0)).anchor('in1').label('CANAL A\n(pines 1-2-3)', loc='top', fontsize=FS)
        d += oa
        ob = elm.Opamp(leads=True).theta(0).at((4.2, 4.6)).anchor('in1').label('CANAL B\n(pines 5-6-7)', loc='top', fontsize=FS)
        d += ob

        # INA signal bus to both (-) inputs
        d += elm.Dot(open=True).at((-2.6, 7.4)).label('Salida del INA\n(V_I, sube con\nla corriente)', loc='left', fontsize=FS)
        d += elm.Line().at((-2.6, 7.4)).tox(2.4)
        d += (nbus := elm.Dot())
        d += elm.Line().at(nbus.center).toy(oa.in1[1])
        d += elm.Line().tox(oa.in1[0])
        d += elm.Line().at(nbus.center).toy(ob.in1[1])
        d += elm.Line().tox(ob.in1[0])
        d += elm.Label().at((oa.in1[0] - 0.15, oa.in1[1] + 0.15)).label('2 (−)', fontsize=FS - 3, loc='left', halign='right', valign='bottom', color='#555')
        d += elm.Label().at((ob.in1[0] - 0.15, ob.in1[1] + 0.15)).label('6 (−)', fontsize=FS - 3, loc='left', halign='right', valign='bottom', color='#555')

        # thresholds on (+)
        d += elm.Line().at(oa.in2).left(0.9)
        d += elm.Dot(open=True).label('V_DISPARO\n(umbral ALTO)', loc='bottom', fontsize=FS, color=RED)
        d += elm.Label().at((oa.in2[0] - 0.15, oa.in2[1] + 0.12)).label('3 (+)', fontsize=FS - 3, loc='left', halign='right', valign='bottom', color='#555')
        d += elm.Line().at(ob.in2).left(0.9)
        d += elm.Dot(open=True).label('V_LÍMITE\n(umbral BAJO)', loc='bottom', fontsize=FS, color=GREEN)
        d += elm.Label().at((ob.in2[0] - 0.15, ob.in2[1] + 0.12)).label('5 (+)', fontsize=FS - 3, loc='left', halign='right', valign='bottom', color='#555')

        # ---- canal A output -> Q_OK -> J2-3
        d += elm.Line().at(oa.out).right(0.8)
        d += (na := elm.Dot())
        d += elm.Label().at((oa.out[0] + 0.1, oa.out[1] - 0.25)).label('1', fontsize=FS - 3, loc='right', halign='left', valign='top', color='#555')
        d += elm.Resistor().at(na.center).up(2.2).label('R_B1\n10 kΩ', loc='bottom', fontsize=FS)
        d += elm.Vdd().label('5V')
        d += elm.Line().at(na.center).right(0.9)
        q1 = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q_OK', loc='right', fontsize=FS)
        d += q1
        d += elm.Line().at(q1.emitter).down(0.5)
        d += elm.Ground()
        d += elm.Line().at(q1.collector).up(0.9)
        XJ = 13.6
        d += elm.Line().tox(XJ)
        ya = q1.collector[1] + 0.9
        d += (j23 := elm.Dot(open=True).label('J2-3', loc='top', fontsize=F))

        # ---- canal B output -> J2-5
        d += elm.Line().at(ob.out).tox(XJ)
        d += elm.Label().at((ob.out[0] + 0.1, ob.out[1] - 0.25)).label('7', fontsize=FS - 3, loc='right', halign='left', valign='top', color='#555')
        d += (j25 := elm.Dot(open=True).label('J2-5', loc='top', fontsize=F))

        # cable
        XC = XJ + 5.0
        d += elm.Line().at(j23.center).tox(XC).linestyle('--')
        d += elm.Line().at(j25.center).tox(XC).linestyle('--')
        d += elm.Label().at((XJ + 2.5, (ya + ob.out[1]) / 2)).label('cable trenzado\ncon GND\n(J2 ↔ J6)', fontsize=FS, color='gray')

        # ================= CONTROL BOARD =================
        d += elm.Label().at((XC - 0.4, 15.6)).label('[2] TARJETA DE CONTROL', loc='right', fontsize=24.6, halign='left', color=BLUE)
        d += (j63 := elm.Dot(open=True).at((XC, ya)).label('J6-3', loc='top', fontsize=F))
        d += elm.Line().at(j63.center).right(2.0)
        d += elm.Dot(open=True).label('SENSOR_OK → R26 / Q_EN → EN_HW\n(ya existe: diagrama del corte)', loc='right', fontsize=FS, color=RED)

        d += (j65 := elm.Dot(open=True).at((XC, ob.out[1])).label('J6-5', loc='top', fontsize=F))
        d += elm.Line().at(j65.center).right(1.6)
        d += (np := elm.Dot())
        d += elm.Resistor().at(np.center).up(2.4).label('R_PU\n10 kΩ', loc='bottom', fontsize=FS)
        d += elm.Vdd().label('5V')
        d += elm.Resistor().at(np.center).right(2.8).label('R_S 1 kΩ', loc='top', fontsize=FS)
        d += (nf := elm.Dot())
        d += elm.Capacitor().at(nf.center).down(2.0).label('C_F\n1 nF ("102")', loc='bottom', fontsize=FS)
        d += elm.Ground()
        d += elm.Line().at(nf.center).right(1.6)
        d += elm.Dot(open=True).label('D2 (CRÍTICO)\nINT0 · red CRIT_D2_I_LIM', loc='right', fontsize=FS, color=GREEN)

        # ================= TABLE =================
        TY = -0.8
        d += elm.Label().at((-6.0, TY)).label(
            'QUÉ HACE CADA CANAL SEGÚN LA CORRIENTE:\n'
            '  Corriente                      Canal A (pin 1)   Canal B (pin 7)   Resultado\n'
            '  Normal (menor que V_LÍMITE)    suelto (alto)     suelto (alto)     opera normal\n'
            '  Entre V_LÍMITE y V_DISPARO     suelto (alto)     A 0 V             D2 avisa → el CRÍTICO baja el índice\n'
            '  (arranque de motor, corto                                          de modulación; corren las huellas de\n'
            '   naciendo)                                                         corto/arco/motor (DC-016). SIN candado\n'
            '  Mayor que V_DISPARO            A 0 V             A 0 V             Q_OK suelta SENSOR_OK → Q_EN → EN_HW = 0\n'
            '  (MOSFET en peligro)                                                → corte en ~2-4 µs + candado (DC-014)\n'
            '  Sensor ausente / sin 5V        —                 —                 SENSOR_OK corta; I_LIM queda alto (no importa)',
            loc='right', fontsize=FS, halign='left', valign='top')

        d += elm.Label().at((-6.0, TY - 4.9)).label(
            'NOTAS:\n'
            '• "Segundo comparador" = el CANAL B del MISMO LM393. No se agrega otro chip: el LM393 siempre trae dos.\n'
            '• Regla: V_LÍMITE < V_DISPARO.  Valor inicial: V_LÍMITE ≈ 70 % de V_DISPARO; el final se fija con la medición\n'
            '  de la inductancia de dispersión (DC-016).  Los dos umbrales salen de divisores del 5 V del 78L05 del módulo.\n'
            '• Salida a 0 V cuando V_I (pin 2 o 6) sube por arriba del umbral (pin 3 o 5).  Histéresis: 1 MΩ de la salida a su (+).\n'
            '• LM393 a 5 V: sus entradas solo funcionan hasta ~3.5 V (VCC − 1.5 V).  V_I y los dos umbrales deben quedar abajo de 3.5 V.\n'
            '• R_PU (10 kΩ) es el pull-up del colector abierto del canal B: va en la tarjeta de control, no en la del sensor.\n'
            '• C_F 1 nF con R_S 1 kΩ: τ = 1 µs, filtra el ruido de conmutación (equivale al blanking de 1 µs).\n'
            '• Serigrafía de J6: 1 I_OUT · 2 GND · 3 SENSOR_OK · 4 GND · 5 I_LIM.',
            loc='right', fontsize=FS, halign='left', valign='top')

if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
