import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib
matplotlib.rcParams['font.weight'] = 'bold'

BLUE = '#1f4e9a'; RED = '#a01818'; GREEN = '#1a6e1a'; GRAY = '#555555'; ORG = '#b35900'; PE = '#1a7a1a'
F = 21; FS = 18; FP = 15


def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.6, inches_per_unit=0.85)
        d += elm.Label().at((-12.0, 27.0)).label('TARJETA DE TRANSFERENCIA V1 — RED / INVERSOR / CARGADOR (DC-019)', loc='right', fontsize=30, halign='left')
        d += elm.Label().at((-12.0, 25.6)).label(
            'Tres estados: RED (todo apagado) · INVERSOR (canal AISLAR RED) · CARGADOR (canal TRAFO).  Nunca los dos canales a la vez.\n'
            'K2 + K_L2 = conmutador de 2 polos y 2 tiros: la carga no puede tener las dos fuentes.  Sin energía en las bobinas, la casa queda en la RED.',
            loc='right', fontsize=FS, halign='left')

        # ================= CONTROL BOARD SIDE =================
        d += elm.Label().at((-12.0, 23.0)).label('[A] TARJETA DE CONTROL', loc='right', fontsize=22, halign='left', color=BLUE)
        for (a, b, c, e) in [(-12, 3.0, -1.2, 3.0), (-1.2, 3.0, -1.2, 22.2), (-1.2, 22.2, -12, 22.2), (-12, 22.2, -12, 3.0)]:
            d += elm.Line().at((a, b)).to((c, e)).linestyle('--').color(GRAY)
        # D5 with EN_HW diode
        d += elm.Dot(open=True).at((-10.6, 16.0)).label('D5', loc='left', fontsize=FS)
        d += elm.Resistor().at((-10.6, 16.0)).right(2.6).label('1 kΩ', loc='top', fontsize=FP)
        d += (ne := elm.Dot())
        d += elm.Line().at(ne.center).tox(2.0)
        d += elm.Diode().at(ne.center).down(2.2).reverse().label('1N4148', loc='bottom', fontsize=FP - 1)
        d += elm.Dot(open=True).label('EN_HW', loc='bottom', fontsize=FS, color=RED)
        d += elm.Label().at((-11.6, 11.0)).label('EN_HW = 0 → K2 se suelta\n→ la casa cae a la RED', loc='right', fontsize=FP - 1, halign='left', color=GRAY)
        # D6
        d += elm.Dot(open=True).at((-10.6, 8.0)).label('D6', loc='left', fontsize=FS)
        d += elm.Line().at((-10.6, 8.0)).tox(2.0)
        # AUX D10 presence
        d += elm.Dot(open=True).at((-10.6, 5.0)).label('AUX D10', loc='left', fontsize=FS)
        d += elm.Line().at((-10.6, 5.0)).tox(2.0)
        d += elm.Dot().at((-7.4, 5.0))
        d += elm.Resistor().at((-7.4, 5.0)).up(1.6).label('10k', loc='bottom', fontsize=FP - 1)
        d += elm.Vdd().label('5V')
        d += elm.Dot().at((-4.6, 5.0))
        d += elm.Capacitor().at((-4.6, 5.0)).down(1.2).label('1 µF', loc='bottom', fontsize=FP - 1)
        d += elm.Ground()

        # ================= J15 =================
        for y, t in [(16.0, 'J15-3 AISLAR RED'), (8.0, 'J15-5 TRAFO'), (5.0, 'J15-7 PRES_CARGA')]:
            d += elm.Dot(open=True).at((2.0, y))
            d += elm.Label().at((2.0, y + 0.35)).label(t, fontsize=FP - 1, color=GREEN, valign='bottom')
        d += elm.Label().at((2.0, 0.4)).label('J15 (IDC 2×4): 1 15V · 2,4,6 GND · 8 MODO_TN', fontsize=FP - 2, color=GRAY)
        # MODO_TN sense: SW_TN pole B
        d += elm.Dot(open=True).at((2.0, 2.6))
        d += elm.Label().at((2.0, 2.95)).label('J15-8 MODO_TN', fontsize=FP - 1, color=GREEN, valign='bottom')
        d += elm.Line().at((2.0, 2.6)).tox(5.0)
        d += elm.Switch().right(2.0).label('SW_TN-B', loc='top', fontsize=FP - 2, color=ORG)
        d += elm.Line().right(0.4)
        d += elm.Ground()
        d += elm.Label().at((8.0, 2.6)).label('CASA = cerrado → 0\nCARGA = abierto → 1\n(→ CRÍTICO D4, pull-up)', loc='right', fontsize=FP - 3, halign='left', color=ORG)

        # ================= RELAY BOARD LOGIC =================
        d += elm.Label().at((3.4, 23.0)).label('[B] TARJETA DE RELÉS — lado 15 V', loc='right', fontsize=22, halign='left', color=BLUE)
        UX0, UX1 = 17.0, 20.0
        for (a, b, c, e) in [(UX0, 6.4, UX1, 6.4), (UX1, 6.4, UX1, 18.0), (UX1, 18.0, UX0, 18.0), (UX0, 18.0, UX0, 6.4)]:
            d += elm.Line().at((a, b)).to((c, e))
        d += elm.Label().at((18.5, 10.8)).label('U1\nULN\n2003A', fontsize=FS)
        # channel AISLAR
        d += elm.Line().at((2.0, 16.0)).tox(UX0)
        d += elm.Dot().at((5.0, 16.0))
        for x, el, lab in [(12.0, 'R', '10k'), (13.8, 'C', '1nF')]:
            d += elm.Dot().at((x, 16.0))
            e = elm.Resistor() if el == 'R' else elm.Capacitor()
            d += e.at((x, 16.0)).down(1.3).label(lab, loc='bottom', fontsize=FP - 2)
            d += elm.Ground()
        # channel TRAFO with series 1k
        d += elm.Line().at((2.0, 8.0)).tox(4.0)
        d += elm.Resistor().at((4.0, 8.0)).right(2.6).label('1 kΩ', loc='top', fontsize=FP - 1, color=ORG)
        d += elm.Line().tox(UX0)
        for x, el, lab in [(12.0, 'R', '10k'), (13.8, 'C', '1nF')]:
            d += elm.Dot().at((x, 8.0))
            e = elm.Resistor() if el == 'R' else elm.Capacitor()
            d += e.at((x, 8.0)).down(1.3).label(lab, loc='bottom', fontsize=FP - 2)
            d += elm.Ground()
        # Q_A interlock: AISLAR blocks TRAFO
        d += elm.Dot().at((9.0, 8.0))
        d += elm.Line().at((9.0, 8.0)).down(0.8)
        qa = elm.BjtNpn(circle=True).theta(0).anchor('collector').at((9.0, 7.2)).label('Q_A\nBC547', loc='right', fontsize=FP - 1, color=ORG)
        d += qa
        d += elm.Line().at(qa.emitter).down(0.3)
        d += elm.Ground()
        d += elm.Resistor().at(qa.base).tox(5.8).label('10 kΩ', loc='bottom', fontsize=FP - 2, color=ORG)
        d += elm.Line().tox(5.0)
        d += elm.Line().toy(16.0)
        d += elm.Label().at((4.8, 12.0)).label('candado:\nAISLAR\nbloquea\nTRAFO', loc='left', fontsize=FP - 2, halign='right', color=ORG)
        # PRES line passes to opto
        d += elm.Line().at((2.0, 5.0)).tox(4.2)
        d += elm.Label().at((4.4, 5.0)).label('← colector del PC817 (lado 127 V)', loc='right', fontsize=FP - 1, halign='left', color=GRAY)
        d += elm.Label().at((UX0 - 0.1, 16.1)).label('1', loc='left', fontsize=FP - 2, halign='right', valign='bottom', color=GRAY)
        d += elm.Label().at((UX0 - 0.1, 8.1)).label('2', loc='left', fontsize=FP - 2, halign='right', valign='bottom', color=GRAY)

        # rail 15V + COM
        RAIL = 21.0
        d += elm.Line().at((19.0, 18.0)).toy(RAIL)
        d += elm.Label().at((19.15, 18.15)).label('9 COM', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Line().at((19.0, RAIL)).tox(35.0)
        d += elm.Dot(open=True).at((35.0, RAIL)).label('15V\n(J15-1)', loc='top', fontsize=FP)
        d += elm.Dot().at((19.0, 20.0))
        d += elm.Capacitor().at((19.0, 20.0)).left(1.6).label('C3 100n', loc='top', fontsize=FP - 2, color=GREEN)
        d += elm.Ground()
        for xc, lab, pol in [(32.6, 'C1\n100µ', True), (34.0, 'C2\n100n', False)]:
            d += elm.Dot().at((xc, RAIL))
            d += elm.Capacitor(polar=pol).at((xc, RAIL)).down(1.4).label(lab, loc='bottom', fontsize=FP - 2, color=GREEN)
            d += elm.Ground()

        # coils
        coils1 = [('K2', 21.6), ('K_L2', 24.0), ('RL_TN', 26.4)]
        coils2 = [('K3', 28.8), ('K_L3', 31.2)]
        d += elm.Line().at((UX1, 16.0)).tox(26.4)
        d += elm.Label().at((UX1 + 0.1, 16.1)).label('16', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Line().at((UX1, 8.0)).tox(31.2)
        d += elm.Label().at((UX1 + 0.1, 8.1)).label('15', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        for name, x in coils1 + coils2:
            yb = 16.0 if (name, x) in coils1 else 8.0
            r = '100Ω' if name == 'RL_TN' else '39Ω'
            d += elm.Dot().at((x, RAIL))
            d += elm.Resistor().at((x, RAIL)).down(1.4).label(r, loc='bottom', fontsize=FP - 3)
            d += elm.RBox(w=0.7, h=0.45).down(1.4).label(name, loc='bottom', fontsize=FP - 2, color=BLUE if yb == 16.0 else RED)
            if name == 'RL_TN':
                d += elm.Switch().down(1.6).label('SW_TN-A', loc='left', fontsize=FP - 3, color=ORG)
            d += elm.Line().toy(yb)
            d += elm.Dot().at((x, yb))
        d += elm.Label().at((21.0, 3.6)).label('Canal 1 (16): K2 + K_L2 + RL_TN = MODO INVERSOR\nCanal 2 (15): K3 + K_L3 = MODO CARGADOR\nL90S: 39 Ω ½ W · SRD (RL_TN): 100 Ω',
                                              loc='right', fontsize=FP - 1, halign='left', color=GRAY)

        # ================= SLOT =================
        d += elm.Line().at((37.0, 1.0)).toy(24.0).linestyle('--').color(RED)
        d += elm.Label().at((37.0, 24.4)).label('RANURA · ≥ 5 mm', fontsize=FP, color=RED)

        # ================= POWER =================
        X0 = 41.0
        d += elm.Label().at((X0 - 2.0, 23.0)).label('[C] TARJETA DE RELÉS — lado 127 V', loc='right', fontsize=22, halign='left', color=RED)

        def spdt_block(y, pole, grid, inv, load, ref):
            s = elm.SwitchSpdt2().scale(2.2).left().flip().anchor('a').at((X0 + 12.0, y)).label(f'{ref}\nL90S', loc='top', fontsize=FP, color=BLUE)
            d.add(s)
            # grid to b (NC), inv to c (NO)
            d.add(elm.Dot(open=True).at((X0, s.b[1])).label(grid, loc='left', fontsize=FS))
            d.add(elm.Line().at((X0, s.b[1])).tox(s.b[0]))
            d.add(elm.Label().at((s.b[0] - 0.2, s.b[1] + 0.15)).label('NC 12', loc='left', fontsize=FP - 3, halign='right', valign='bottom', color=GRAY))
            d.add(elm.Label().at((s.c[0] - 0.2, s.c[1] - 0.15)).label('NO 14', loc='left', fontsize=FP - 3, halign='right', valign='top', color=GRAY))
            d.add(elm.Label().at((s.a[0] + 0.2, s.a[1] + 0.15)).label('11', loc='right', fontsize=FP - 3, halign='left', valign='bottom', color=GRAY))
            return s

        # L pole
        sL = spdt_block(18.0, 'L', 'L-GRID', 'L-INV', 'L-LOAD', 'K_L2')
        # N pole
        sN = spdt_block(8.0, 'N', 'N-GRID', 'N-INV', 'N-LOAD', 'K2')
        # inverter sources
        for s, name, y in [(sL, 'L-INV (trafo)', 14.2), (sN, 'N-INV (trafo, J6)', 4.2)]:
            d += elm.Dot(open=True).at((X0, y)).label(name, loc='left', fontsize=FS)
            d += elm.Line().at((X0, y)).tox(X0 + 4.0)
            d += elm.Dot()
            d += elm.Line().at((X0 + 4.0, y)).toy(s.c[1])
            d += elm.Line().tox(s.c[0])
        # output filter capacitor + damper (DNP) + bleeder, across L-INV / N-INV
        for x, kind, tag in [(X0 + 0.7, 'C', 'C_F'), (X0 + 1.9, 'RC', 'R_D+C_D'), (X0 + 3.1, 'R', 'R_B')]:
            d += elm.Dot().at((x, 14.2))
            d += elm.Dot().at((x, 4.2))
            if kind == 'C':
                d += elm.Capacitor().at((x, 14.2)).toy(11.0)
                d += elm.Line().toy(4.2)
            elif kind == 'R':
                d += elm.Resistor().at((x, 14.2)).toy(10.8)
                d += elm.Line().toy(4.2)
            else:
                d += elm.Resistor().at((x, 14.2)).toy(12.0)
                d += elm.Capacitor().toy(9.9)
                d += elm.Line().toy(4.2)
            d += elm.Label().at((x + 0.12, 12.6 if kind != 'RC' else 13.2)).label(tag, loc='right', fontsize=FP - 4, halign='left', color=ORG)
        d += elm.Label().at((X0 + 0.2, 16.0)).label(
            'FILTRO DE SALIDA (lado del transformador, antes de los relés):\n'
            'C_F 4.7 µF MKP X2 310 VAC · R_D 22 Ω 5 W + C_D 4.7 µF X2 (sin poblar, etapa 3) · R_B 2 × 100 kΩ ½ W',
            loc='right', fontsize=FP - 3, halign='left', color=ORG)
        # load buses
        XB = X0 + 15.5
        d += elm.Line().at(sL.a).tox(XB)
        d += elm.Dot()
        d += elm.Line().at(sN.a).tox(XB + 1.6)
        d += elm.Dot()
        # K_L3: L-INV -> fuse -> faston -> K_L3 -> L-LOAD
        d += elm.Line().at((X0 + 4.0, 14.2)).tox(X0 + 5.0)
        d += elm.Fuse().right(2.6).label('F_CARG\n10–12 A lento', loc='bottom', fontsize=FP - 2)
        d += elm.Line().right(0.6)
        d += elm.Dot(open=True).label('faston', loc='bottom', fontsize=FP - 3, color=GRAY)
        d += elm.Switch().right(2.4).label('K_L3 (NO)\nL90S', loc='top', fontsize=FP - 1, color=RED)
        d += elm.Line().tox(XB)
        d += elm.Dot()
        d += elm.Line().at((XB, sL.a[1])).toy(14.2)
        # K3: N-INV -> faston -> K3 -> N-LOAD
        d += elm.Line().at((X0 + 4.0, 4.2)).tox(X0 + 7.4)
        d += elm.Dot(open=True).label('faston J4–J5', loc='bottom', fontsize=FP - 3, color=GRAY)
        d += elm.Switch().right(2.4).label('K3 (NO)\nL90S', loc='top', fontsize=FP - 1, color=RED)
        d += elm.Line().tox(XB + 1.6)
        d += elm.Dot()
        d += elm.Line().at((XB + 1.6, sN.a[1])).toy(4.2)
        # outputs
        XO = X0 + 27.0
        d += elm.Line().at((XB, sL.a[1])).tox(XB + 2.8)
        d += elm.Fuse().right(2.8).label('breaker\n20 A', loc='top', fontsize=FP - 2)
        d += elm.Line().tox(XO)
        d += elm.Dot(open=True).label('J_LOAD L', loc='right', fontsize=FS)
        d += elm.Line().at((XB + 1.6, sN.a[1])).tox(XO)
        d += elm.Dot(open=True).label('J_LOAD N', loc='right', fontsize=FS)
        # MOV
        xm = XO - 1.6
        d += elm.Dot().at((xm, sL.a[1])); d += elm.Dot().at((xm, sN.a[1]))
        d += elm.RBox(w=0.5, h=0.9).at((xm, sL.a[1])).to((xm, sN.a[1])).label('MOV\n14D241K', loc='right', fontsize=FP - 2)
        # PC817 presence
        xo = XO - 5.4
        d += elm.Dot().at((xo, sL.a[1]))
        d += elm.Resistor().at((xo, sL.a[1])).down(2.6).label('2 × 47k 1 W', loc='right', fontsize=FP - 2)
        d += (pa := elm.Dot())
        op = elm.Optocoupler().anchor('anode').at(pa.center).label('PC817', loc='top', ofst=(1.5, 0.2), fontsize=FP - 1)
        d += op
        d += elm.Line().at(op.cathode).toy(sN.a[1])
        d += elm.Dot()
        d += elm.Diode().at((pa.center[0] - 2.2, pa.center[1])).down(1.5).label('1N4007', loc='left', fontsize=FP - 3)
        d += elm.Line().at((pa.center[0] - 2.2, pa.center[1])).tox(pa.center[0])
        d += elm.Line().at((pa.center[0] - 2.2, pa.center[1] - 1.5)).tox(op.cathode[0])
        d += elm.Line().at(op.collector).right(1.2)
        d += elm.Dot(open=True).label('→ J15-7\nPRES_CARGA', loc='right', fontsize=FP - 1, color=GREEN)
        d += elm.Line().at(op.emitter).right(0.6)
        d += elm.Ground()
        # TN bond
        d += elm.Dot().at((X0 + 3.5, 4.2))
        d += elm.Line().at((X0 + 3.5, 4.2)).toy(1.6)
        d += elm.Switch().right(2.4).label('RL_TN (NO) SRD', loc='bottom', fontsize=FP - 2, color=BLUE)
        d += elm.Line().right(0.6)
        d += elm.Ground()
        d += elm.Label().at((X0 + 7.6, 1.0)).label('PE: unión N–PE solo en INVERSOR CASA (SW_TN cerrado)', loc='right', fontsize=FP - 2, halign='left', color=GRAY)

        # ================= TABLE / NOTES =================
        TY = -1.2
        d += elm.Label().at((-12.0, TY)).label(
            'ESTADOS:\n'
            '  Modo        D5 AISLAR  D6 TRAFO   K2 + K_L2     K3 + K_L3   RL_TN   Casa en           Transformador\n'
            '  RED         0          0          NC (red)      abiertos    abierto RED               aislado\n'
            '  INV. CASA   1          0          NO (trafo)    abiertos    cerrado TRANSFORMADOR     alimenta la casa (fuente principal)\n'
            '  INV. CARGA  1          0          NO (trafo)    abiertos    abierto TRANSFORMADOR     alimenta cargas críticas (UPS)\n'
            '  CARGADOR    0          1          NC (red)      cerrados    abierto RED               conectado a la red (cargando)\n'
            '  1 y 1       —          —          NO (trafo)    BLOQUEADOS por Q_A                    = INVERSOR (seguro)\n'
            '\n'
            'SECUENCIA A INVERSOR (con autoprueba):  EN_HW = 0 → D6 = 0 → pausa 50 ms → D5 = 1 → pausa 50 ms →\n'
            '  si había red: PRES_CARGA debe indicar "sin voltaje" (si no: "K2 SOLDADO", soltar D5, quedarse en RED) → EN_HW = 1 → SPWM.\n'
            'SECUENCIA A CARGADOR:  EN_HW = 0 → D5 = 0 → pausa 50 ms → verificar red con AC_FB_GRID → D6 = 1 → cargar (puente sincronizado con la red).\n'
            'PRES_CARGA: 0 = hay voltaje en la carga.  Autochequeo: en modo RED con red presente, PRES_CARGA debe dar 0 (si no: sensor o cable J15-7 dañado).',
            loc='right', fontsize=FS - 1, halign='left', valign='top')
        d += elm.Label().at((-12.0, TY - 7.0)).label(
            'REGLAS:\n'
            '  • Falla de UN relé no forma paralelo: K2 soldado en NC → el NO no cierra y Q_A impide cerrar K3; K3 soldado → K2 sigue aislando la red.\n'
            '  • Con K2 apagado el puente nunca genera voltaje de inversor; en modo cargador solo rectifica sincronizado con la red.\n'
            '  • Separación: 3 mm entre pours de distinto origen (GRID / INV / LOAD) · 5 mm a bobinas y lógica · ranura entre zonas.\n'
            '  • J15: 1 15V · 2 GND · 3 AISLAR RED (D5) · 4 GND · 5 TRAFO (D6) · 6 GND · 7 PRES_CARGA (AUX D10) · 8 MODO_TN (CRÍTICO D4).\n'
            '  • SW_TN (DPDT): CASA = unión N–PE (polo A en la bobina de RL_TN) · CARGA = sin unión.  El polo B avisa al CRÍTICO; si no coincide con el menú, NO invierte.\n'
            '  • Dos puentes faston con seguro (L y N del trafo hacia K_L3/K3) = seccionador de servicio del cargador; colores distintos.\n'
            '  • C_F 4.7 µF MKP X2 (polipropileno, ≥ 310 VAC) entre L-INV y N-INV, ANTES de los relés: forma el LC con los ~2 mH del secundario (f₀ ≈ 1.64 kHz).\n'
            '    No poliéster (MKT): calienta con el rizo de 24 kHz.  R_B 2 × 100 kΩ ½ W descarga C_F a < 60 V en ~1 s.  R_D + C_D: amortiguamiento, sin poblar (etapa 3).',
            loc='right', fontsize=FS - 1, halign='left', valign='top')


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
