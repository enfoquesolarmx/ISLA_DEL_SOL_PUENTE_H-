import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib
matplotlib.rcParams['font.weight'] = 'bold'

BLUE = '#1f4e9a'; RED = '#a01818'; GREEN = '#1a6e1a'; NEW = '#b35900'
F = 22; FS = 19; FP = 16

def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.8, inches_per_unit=0.85)

        def net(x, y, name, side='left', color='black'):
            d.add(elm.Dot(open=True).at((x, y)).label(name, loc=side, fontsize=FS, color=color))

        d += elm.Label().at((-4.0, 25.2)).label(
            'MÓDULO SENSOR DC: INA240A1 + U3 (DISPARO, LM393) + U4 (LÍMITE, LM393) + Q_OK',
            loc='right', fontsize=30, halign='left')
        d += elm.Label().at((-4.0, 23.9)).label(
            '3 shunts de 1 mΩ en paralelo = 0.333 mΩ · INA240A1 (G = 20) → 6.67 mV/A · referencia 2.5 V.   '
            'En NARANJA: lo nuevo contra el esquemático actual.',
            loc='right', fontsize=FS, halign='left')

        # ============ [1] INA ============
        d += elm.Label().at((-4.0, 22.2)).label('[1] AMPLIFICADOR (sin cambios)', loc='right', fontsize=23, halign='left', color=BLUE)
        ina = elm.Opamp(leads=True).theta(0).at((4.6, 19.6)).anchor('in1').label('U2\nINA240A1\nG = 20', loc='center', ofst=(-0.3, 0), fontsize=FP)
        d += ina
        # inputs
        d += elm.Line().at(ina.in1).left(0.6)
        d += (n_m := elm.Dot())
        d += elm.Resistor().left(2.4).label('R4 0 Ω', loc='top', fontsize=FP)
        d += elm.Dot(open=True).label('IN− (Kelvin)\nJ1-3', loc='left', fontsize=FS)
        d += elm.Line().at(ina.in2).left(0.6)
        d += (n_p := elm.Dot())
        d += elm.Resistor().left(2.4).label('R3 0 Ω', loc='bottom', fontsize=FP)
        d += elm.Dot(open=True).label('IN+ (Kelvin)\nJ1-2', loc='left', fontsize=FS)
        d += elm.Capacitor().at(n_m.center).to(n_p.center).label('C3\n10 nF', loc='top', fontsize=FP)
        d += elm.Label().at((ina.out[0] - 1.6, 16.6)).label('VS = 5V · REF = 2.5 V\n(REF1 a 5V, REF2 a GND)\nC2 100 nF en VS', fontsize=FP, color='#555')
        # output
        d += elm.Line().at(ina.out).right(0.8)
        d += (n_o := elm.Dot().label('INA_OUT', loc='top', fontsize=FS))
        # I_OUT branch
        d += elm.Resistor().at(n_o.center).right(2.6).label('R1 1 kΩ', loc='top', fontsize=FP)
        d += (n_i := elm.Dot())
        d += elm.Capacitor().at(n_i.center).down(1.8).label('C1\n10 nF', loc='bottom', fontsize=FP)
        d += elm.Ground()
        d += elm.Line().at(n_i.center).right(1.4)
        d += (n_i2 := elm.Dot())
        d += elm.Resistor().at(n_i2.center).down(1.8).label('R2\n2 kΩ', loc='bottom', fontsize=FP)
        d += elm.Ground()
        d += elm.Line().at(n_i2.center).right(1.2)
        d += elm.Dot(open=True).label('I_OUT → J2-1\n(2/3 de INA_OUT)', loc='right', fontsize=FS)
        # INA_RC branch
        d += elm.Line().at(n_o.center).up(3.0)
        d += elm.Resistor().right(2.6).label('R8 1 kΩ', loc='top', fontsize=FP)
        d += (n_rc := elm.Dot())
        d += elm.Line().at(n_rc.center).right(1.4)
        d += (n_rc2 := elm.Dot())
        d += elm.Capacitor().at(n_rc2.center).down(1.2).label('C4 1 nF', loc='right', fontsize=FP)
        d += elm.Ground()
        d += elm.Line().at(n_rc2.center).right(1.2)
        d += elm.Dot(open=True).label('INA_RC\n(τ = 1 µs)', loc='right', fontsize=FS)

        # ============ [2] ESCALERA ============
        XL = 33.0
        d += elm.Label().at((XL - 4.0, 22.2)).label('[2] ESCALERA DE UMBRALES', loc='right', fontsize=23, halign='left', color=BLUE)
        d += elm.Vdd().at((XL, 20.6)).label('5V')
        nodes = [('R5', '3.6 kΩ', 'V_ALTO', '3.20 V · +105 A', 'C7', RED, 'black'),
                 ('R9', '390 Ω', 'V_LIM_ALTO', '3.00 V · +75 A', 'C8', GREEN, NEW),
                 ('R10', '2.0 kΩ', 'V_LIM_BAJO', '2.00 V · −75 A', 'C9', GREEN, NEW),
                 ('R11', '390 Ω', 'V_BAJO', '1.80 V · −105 A', 'C6', RED, NEW)]
        y = 20.6
        for ref, val, nname, vtxt, cref, ncol, rcol in nodes:
            d += elm.Resistor().at((XL, y)).down(2.4).label(f'{ref}\n{val}', loc='bottom', fontsize=FP, color=rcol)
            y -= 2.4
            d += elm.Dot().at((XL, y))
            d += elm.Label().at((XL - 0.25, y + 0.05)).label(f'{nname}\n{vtxt}', loc='left', fontsize=FS - 1, halign='right', color=ncol)
            d += elm.Line().at((XL, y)).right(1.6)
            ccol = NEW if cref in ('C8', 'C9') else 'black'
            d += elm.Capacitor().down(1.2).label(f'{cref} 1 nF', loc='bottom', ofst=(0.6, 0), fontsize=FP, color=ccol)
            d += elm.Ground()
        d += elm.Resistor().at((XL, y)).down(2.4).label('R7\n3.6 kΩ', loc='bottom', fontsize=FP)
        d += elm.Ground()
        d += elm.Label().at((XL - 4.0, 6.6)).label('R6 (2.7 kΩ) SE QUITA:\nla reemplazan R9 + R10 + R11', loc='right', fontsize=FP, halign='left', color=NEW)

        # ============ [3] REGULADOR ============
        XR = 42.0
        d += elm.Label().at((XR, 22.2)).label('[3] REGULADOR', loc='right', fontsize=23, halign='left', color=BLUE)
        d += elm.Dot(open=True).at((XR, 19.6)).label('15V\nJ1-6', loc='left', fontsize=FS)
        d += elm.Line().at((XR, 19.6)).right(1.0)
        d += (r_in := elm.Dot())
        d += elm.Capacitor().at(r_in.center).down(1.6).label('C15\n1 µF', loc='bottom', fontsize=FP)
        d += elm.Ground()
        d += elm.Line().at(r_in.center).right(0.8)
        bx0 = XR + 1.8
        for (a, b, c, e) in [(bx0, 18.9, bx0 + 3.0, 18.9), (bx0 + 3.0, 18.9, bx0 + 3.0, 20.3), (bx0 + 3.0, 20.3, bx0, 20.3), (bx0, 20.3, bx0, 18.9)]:
            d += elm.Line().at((a, b)).to((c, e))
        d += elm.Label().at((bx0 + 1.5, 19.6)).label('U1\n78L05', fontsize=FP)
        d += elm.Line().at((bx0 + 1.5, 18.9)).down(0.6)
        d += elm.Ground()
        d += elm.Line().at((bx0 + 3.0, 19.6)).right(0.8)
        d += (r_out := elm.Dot())
        d += elm.Capacitor().at(r_out.center).down(1.6).label('C14\n1 µF', loc='bottom', fontsize=FP, color=NEW)
        d += elm.Ground()
        d += elm.Line().at(r_out.center).right(1.0)
        d += elm.Dot(open=True).label('5V', loc='right', fontsize=FS)
        d += elm.Label().at((XR - 0.4, 15.4)).label('C14 va en 5V\n(el esquemático actual\nlo tiene en "6V")', loc='right', fontsize=FP, halign='left', color=NEW)

        # ============ [4] COMPARADORES ============
        d += elm.Label().at((-4.0, 13.4)).label('[4] COMPARADORES (dos ventanas sobre la misma INA_RC)', loc='right', fontsize=23, halign='left', color=BLUE)

        def comp(x, y, ref, minus, plus, outn, col, newc):
            o = elm.Opamp(leads=True).theta(0).at((x, y)).anchor('in1').label(ref, loc='top', fontsize=FS, color=newc)
            d.add(o)
            d.add(elm.Line().at(o.in1).left(0.5))
            d.add(elm.Dot(open=True).label(minus, loc='left', fontsize=FS - 1))
            d.add(elm.Line().at(o.in2).left(0.5))
            d.add(elm.Dot(open=True).label(plus, loc='left', fontsize=FS - 1))
            d.add(elm.Line().at(o.out).right(0.6))
            d.add(elm.Dot(open=True).label(outn, loc='right', fontsize=FS, color=col))
            return o

        comp(3.0, 11.2, 'U3A · pines 1-2-3', 'INA_RC (2)', 'V_ALTO (3)', 'FAULT (1)', RED, 'black')
        comp(3.0, 6.6, 'U3B · pines 5-6-7', 'V_BAJO (6)', 'INA_RC (5)', 'FAULT (7)', RED, 'black')
        comp(17.0, 11.2, 'U4A · pines 1-2-3  (NUEVO)', 'INA_RC (2)', 'V_LIM_ALTO (3)', 'I_LIM (1)', GREEN, NEW)
        comp(17.0, 6.6, 'U4B · pines 5-6-7  (NUEVO)', 'V_LIM_BAJO (6)', 'INA_RC (5)', 'I_LIM (7)', GREEN, NEW)
        d += elm.Label().at((-4.0, 3.4)).label(
            'U3 = DISPARO DURO (±105 A): salidas unidas en FAULT.   U4 = LÍMITE (±75 A): salidas unidas en I_LIM.\n'
            'U3 y U4: pin 8 = 5V con 100 nF pegado (C5 y C16 nuevo) · pin 4 = GND.   Salidas de colector abierto: '
            'en 0 V cuando la corriente sale de la ventana.',
            loc='right', fontsize=FS - 1, halign='left')

        # ============ [5] SALIDAS ============
        d += elm.Label().at((-4.0, 1.4)).label('[5] SALIDAS A LA TARJETA DE CONTROL (J2)', loc='right', fontsize=23, halign='left', color=BLUE)
        d += elm.Dot(open=True).at((0.0, -2.6)).label('FAULT', loc='left', fontsize=FS, color=RED)
        d += elm.Line().at((0.0, -2.6)).right(1.6)
        d += (nq := elm.Dot())
        d += elm.Resistor().at(nq.center).up(2.2).label('R12 (R_B1)\n10 kΩ', loc='bottom', fontsize=FP, color=NEW)
        d += elm.Vdd().label('5V')
        d += elm.Line().at(nq.center).right(0.9)
        q = elm.BjtNpn(circle=True).theta(0).anchor('base').label('Q1 (Q_OK)\nBC547', loc='right', fontsize=FP, color=NEW)
        d += q
        d += elm.Line().at(q.emitter).down(0.5)
        d += elm.Ground()
        d += elm.Line().at(q.collector).up(0.9)
        d += elm.Line().right(5.0)
        d += elm.Dot(open=True).label('J2-3  SENSOR_OK\n(0 V = sensor bien)', loc='right', fontsize=FS, color=RED)

        d += elm.Dot(open=True).at((19.0, -2.6)).label('I_LIM', loc='left', fontsize=FS, color=GREEN)
        d += elm.Line().at((19.0, -2.6)).right(3.0)
        d += elm.Dot(open=True).label('J2-5  I_LIM\n(0 V = en el límite;\npull-up en la tarjeta de control)', loc='right', fontsize=FS, color=GREEN)

        # ============ TABLE / NOTES ============
        TY = -6.4
        d += elm.Label().at((-4.0, TY)).label(
            'CONECTORES:\n'
            '  J1 INPUT_INA (desde el puente H):  1 GND · 2 IN+ (Kelvin) · 3 IN− (Kelvin) · 4 GND · 5 LLAVE · 6 15V\n'
            '  J2 SALIDA (hacia J6 de control):   1 I_OUT · 2 GND · 3 SENSOR_OK · 4 GND · 5 I_LIM\n'
            '\n'
            'CAMBIOS CONTRA EL ESQUEMÁTICO ACTUAL (en naranja):\n'
            '  1. R6 2.7 kΩ se quita; entran R9 390 Ω, R10 2.0 kΩ, R11 390 Ω (escalera de 5 resistencias).\n'
            '  2. C8 y C9 (1 nF, "102") en V_LIM_ALTO y V_LIM_BAJO.\n'
            '  3. U4 = segundo LM393 (SOIC-8, igual que U3) + C16 100 nF en su pin 8.\n'
            '  4. FAULT ya no va directo a J2-3: pasa por R12 (10 kΩ a 5V) y Q1 BC547 → J2-3 = SENSOR_OK (DC-014).\n'
            '  5. J2-5 = I_LIM (antes vacío).   6. C14 en la red 5V (antes "6V").\n'
            '\n'
            'NOTAS:\n'
            '  • Corriente = (V − 2.5 V) / 6.67 mV/A.  Ventana dura ±0.70 V = ±105 A.  Ventana de límite ±0.50 V = ±75 A (72 %).\n'
            '  • Sin Q1, la tarjeta de control NUNCA arranca: R26 vería la línea en alto y Q_EN cortaría siempre.\n'
            '  • LM393: basta que UNA entrada esté dentro de su rango (hasta VCC − 1.5 V = 3.5 V); los umbrales siempre lo están.\n'
            '  • Histéresis opcional: 1 MΩ de cada salida a su entrada (+) si el límite oscila en el borde (verificar en banco).\n'
            '  • Kelvin con 3 shunts en paralelo: cobre simétrico hacia los tres, o calibrar I_OUT con una corriente conocida.',
            loc='right', fontsize=FS - 1, halign='left', valign='top')

if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
