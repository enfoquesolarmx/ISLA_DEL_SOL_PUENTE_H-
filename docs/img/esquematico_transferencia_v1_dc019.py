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
            'Tres estados: RED (todo apagado) · INVERSOR (canal AISLAR RED) · CARGADOR (canal TRAFO).  Nunca los dos canales a la vez.  Unión N–PE (RL_TN) en canal propio, solo con INVERSOR activo (Q2).\n'
            'K2 + K_L2 = conmutador de 2 polos y 2 tiros: la carga no puede tener las dos fuentes.  Sin energía en las bobinas, la casa queda en la RED.',
            loc='right', fontsize=FS, halign='left')

        # ================= CONTROL BOARD SIDE =================
        d += elm.Label().at((-12.0, 23.0)).label('[A] TARJETA DE CONTROL', loc='right', fontsize=22, halign='left', color=BLUE)
        for (a, b, c, e) in [(-12, 1.6, -1.2, 1.6), (-1.2, 1.6, -1.2, 22.2), (-1.2, 22.2, -12, 22.2), (-12, 22.2, -12, 1.6)]:
            d += elm.Line().at((a, b)).to((c, e)).linestyle('--').color(GRAY)
        # D5 with EN_HW diode
        d += elm.Dot(open=True).at((-10.6, 16.0)).label('CRÍTICO D5', loc='left', fontsize=FS)
        d += elm.Resistor().at((-10.6, 16.0)).right(2.6).label('1 kΩ', loc='top', fontsize=FP)
        d += (ne := elm.Dot())
        d += elm.Line().at(ne.center).tox(2.0)
        d += elm.Diode().at(ne.center).down(2.2).reverse().label('1N4148', loc='bottom', fontsize=FP - 1)
        d += elm.Dot(open=True).label('EN_HW', loc='bottom', fontsize=FS, color=RED)
        d += elm.Label().at((-7.4, 10.6)).label('EN_HW = 0 → K2 se suelta\n→ la casa cae a la RED', loc='right', fontsize=FP - 1, halign='left', color=GRAY)
        # D6
        d += elm.Dot(open=True).at((-10.6, 8.0)).label('CRÍTICO D6', loc='left', fontsize=FS)
        d += elm.Line().at((-10.6, 8.0)).tox(2.0)
        # AUX D10 = orden TN (salida)
        d += elm.Dot(open=True).at((-10.6, 12.0)).label('AUX D10', loc='left', fontsize=FS)
        d += elm.Line().at((-10.6, 12.0)).tox(2.0)
        d += elm.Label().at((-10.4, 11.6)).label('salida TN (unión N–PE según escenario)', loc='right', fontsize=FP - 3, halign='left', valign='top', color=GRAY)
        # CRÍTICO D4 = MODO_TN (entrada con pull-up)
        d += elm.Dot(open=True).at((-10.6, 2.6)).label('CRÍTICO D4', loc='left', fontsize=FS)
        d += elm.Line().at((-10.6, 2.6)).tox(2.0)
        d += elm.Label().at((-10.4, 3.0)).label('entrada, pull-up 10k + 1 nF', loc='right', fontsize=FP - 3, halign='left', valign='bottom', color=GRAY)

        # ================= J15 =================
        for y, t in [(16.0, 'J15-3 AISLAR RED'), (8.0, 'J15-5 TRAFO'), (12.0, 'J15-7 TN')]:
            d += elm.Dot(open=True).at((2.0, y))
            d += elm.Label().at((2.0, y + 0.35)).label(t, fontsize=FP - 1, color=GREEN, valign='bottom')
        d += elm.Label().at((2.0, 0.4)).label('J15 (IDC 2×4): 1 15V · 2,4,6 GND · 3 AISLAR · 5 TRAFO · 7 TN · 8 MODO_TN', fontsize=FP - 2, color=GRAY)
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
        d += elm.Label().at((4.8, 6.4)).label('candado:\nAISLAR\nbloquea\nTRAFO', loc='left', fontsize=FP - 2, halign='right', color=ORG)
        # channel TN: J15-7 -> 1k -> Q2 colector; base (arriba) <- AISLAR por 10k; emisor -> I3   (NPN horizontal, AND por hardware)
        q2 = elm.BjtNpn(circle=True).theta(-90).flip().anchor('base').at((10.0, 12.75))
        d += q2
        d += elm.Label().at((10.0, 11.0)).label('Q2 BC547 (NPN)\nC ← TN · B ← AISLAR · E → I3', fontsize=FP - 3, color=ORG)
        d += elm.Line().at((2.0, 12.0)).tox(6.0)
        d += elm.Resistor().right(2.0).label('1 kΩ', loc='top', fontsize=FP - 2, color=ORG)
        d += elm.Line().tox(q2.collector[0])
        d += elm.Dot().at((10.0, 16.0))
        d += elm.Resistor().at((10.0, 16.0)).toy(q2.base[1]).label('10k', loc='bottom', fontsize=FP - 3, color=ORG)
        d += elm.Line().at(q2.emitter).tox(UX0)
        for x, el, lab in [(12.0, 'R', '10k'), (13.8, 'C', '10nF')]:
            d += elm.Dot().at((x, 12.0))
            e = elm.Resistor() if el == 'R' else elm.Capacitor()
            d += e.at((x, 12.0)).down(1.3).label(lab, loc='bottom', fontsize=FP - 2)
            d += elm.Ground()
        d += elm.Label().at((5.4, 14.4)).label('Q2: TN pasa SOLO si\nAISLAR está activo', loc='right', fontsize=FP - 3, halign='left', color=ORG)
        d += elm.Label().at((UX0 - 0.1, 12.1)).label('3', loc='left', fontsize=FP - 2, halign='right', valign='bottom', color=GRAY)
        d += elm.Label().at((UX0 - 0.1, 16.1)).label('1', loc='left', fontsize=FP - 2, halign='right', valign='bottom', color=GRAY)
        d += elm.Label().at((UX0 - 0.1, 8.1)).label('2', loc='left', fontsize=FP - 2, halign='right', valign='bottom', color=GRAY)

        # rail 15V + COM
        # power LED next to the ULN2003
        d += elm.Dot().at((15.6, 21.0))
        d += elm.Resistor().at((15.6, 21.0)).down(1.4).label('4.7k', loc='bottom', fontsize=FP - 3)
        d += elm.LED().down(1.4).color(GREEN).label('LED\n15V', loc='bottom', fontsize=FP - 3, color=GREEN)
        d += elm.Ground()
        RAIL = 21.0
        d += elm.Line().at((19.0, 18.0)).toy(RAIL)
        d += elm.Label().at((19.15, 18.15)).label('9 COM', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Line().at((15.6, RAIL)).tox(38.0)
        d += elm.Dot(open=True).at((38.0, RAIL)).label('15V\n(J15-1)', loc='top', fontsize=FP)
        d += elm.Dot().at((19.0, 20.0))
        d += elm.Capacitor().at((19.0, 20.0)).left(1.6).label('C3 100n', loc='top', fontsize=FP - 2, color=GREEN)
        d += elm.Ground()
        for xc, lab, pol in [(36.4, 'C1\n100µ', True), (37.6, 'C2\n100n', False)]:
            d += elm.Dot().at((xc, RAIL))
            d += elm.Capacitor(polar=pol).at((xc, RAIL)).down(1.4).label(lab, loc='bottom', fontsize=FP - 2, color=GREEN)
            d += elm.Ground()

        # coils
        coils1 = [('K2', 21.6), ('K_L2', 24.6), ('RL_TN', 27.6)]
        coils2 = [('K3', 30.6), ('K_L3', 33.6)]
        d += elm.Line().at((UX1, 16.0)).tox(26.0)
        d += elm.Line().at((UX1, 12.0)).tox(27.6)
        d += elm.Label().at((UX1 + 0.1, 12.1)).label('14', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Label().at((UX1 + 0.1, 16.1)).label('16', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Line().at((UX1, 8.0)).tox(35.0)
        d += elm.Label().at((UX1 + 0.1, 8.1)).label('15', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        for name, x in coils1 + coils2:
            yb = (12.0 if name == 'RL_TN' else 16.0) if (name, x) in coils1 else 8.0
            r = '39Ω'
            d += elm.Dot().at((x, RAIL))
            d += elm.Resistor().at((x, RAIL)).down(1.4).label(r, loc='bottom', fontsize=FP - 3)
            d += elm.RBox(w=0.7, h=0.45).down(1.4).label(name, loc='bottom', fontsize=FP - 2, color=BLUE if yb != 8.0 else RED)
            if name == 'RL_TN':
                d += elm.Dot()
                ntn = d.here
                d += elm.Switch().down(1.6).label('SW_TN-A', loc='left', fontsize=FP - 3, color=ORG)
            d += elm.Line().toy(yb)
            d += elm.Dot().at((x, yb))
            # status LED per relay: 15V -> 2.2k -> LED -> coil low side
            xl = x + 1.4
            col = BLUE if yb != 8.0 else ORG
            d += elm.Dot().at((xl, RAIL))
            d += elm.Resistor().at((xl, RAIL)).down(1.4).label('2.2k', loc='bottom', fontsize=FP - 4)
            d += elm.LED().down(1.4).color(col)
            if name == 'RL_TN':
                d += elm.Line().toy(ntn[1])
                d += elm.Line().tox(x)
            else:
                d += elm.Line().toy(yb)
                d += elm.Line().tox(x)
        d += elm.Label().at((21.0, 3.6)).label('Canal 1 (16): K2 + K_L2 = MODO INVERSOR\nCanal 2 (15): K3 + K_L3 = MODO CARGADOR\nCanal 3 (14): RL_TN = unión N–PE (AUX D10, solo con INVERSOR por Q2)\nTodos Rayex L90S 12 V: 39 Ω ½ W en serie (bobina ≈ 11.3 V)\nKiCad: K_L2 = K4 · K2 = K2 · K_L3 = K1 · K3 = K3 · RL_TN = K5\nLED por relé (a la derecha de cada bobina): 2.2 kΩ ¼ W (~6 mA) · azul = INVERSOR · ámbar = CARGADOR\nLED_RL_TN después de SW_TN-A: apagado en modo CARGA (sin unión)\nLED 15V verde junto al ULN: 4.7 kΩ (~3 mA)',
                                              loc='right', fontsize=FP - 1, halign='left', color=GRAY)

        # ================= SLOT =================
        d += elm.Line().at((40.0, 1.0)).toy(24.0).linestyle('--').color(RED)
        d += elm.Label().at((40.0, 24.4)).label('RANURA · ≥ 5 mm', fontsize=FP, color=RED)

        # ================= POWER =================
        X0 = 49.0
        d += elm.Label().at((X0 - 2.0, 23.0)).label('[C] TARJETA DE RELÉS — lado 127 V', loc='right', fontsize=22, halign='left', color=RED)

        def spdt_block(y, pole, grid, inv, load, ref):
            s = elm.SwitchSpdt2().scale(2.2).left().flip().anchor('a').at((X0 + 12.0, y)).label(f'{ref}\nL90S' + (' (KiCad K4)' if ref == 'K_L2' else ''), loc='top', fontsize=FP, color=BLUE)
            d.add(s)
            # grid to b (NC), inv to c (NO)
            d.add(elm.Dot(open=True).at((X0, s.b[1])).label(grid, loc='left', fontsize=FS))
            d.add(elm.Line().at((X0, s.b[1])).tox(s.b[0]))
            d.add(elm.Label().at((s.b[0] - 0.2, s.b[1] + 0.15)).label('NC 12', loc='left', fontsize=FP - 3, halign='right', valign='bottom', color=GRAY))
            d.add(elm.Label().at((s.c[0] - 0.2, s.c[1] - 0.15)).label('NO 14', loc='left', fontsize=FP - 3, halign='right', valign='top', color=GRAY))
            d.add(elm.Label().at((s.a[0] + 0.2, s.a[1] + 0.15)).label('11', loc='right', fontsize=FP - 3, halign='left', valign='bottom', color=GRAY))
            return s

        # filtro EMI de la red (como el OutBack FX): CX_RED + descarga entre L-GRID y N-GRID; CY_RED de N-GRID a PE
        XE = X0 + 17.0
        d += elm.Label().at((XE - 0.6, 24.4)).label('FILTRO EMI DE LA RED (junto a J2/J1, lado RED de K_L2/K2):', loc='right', fontsize=FP - 2, halign='left', color=ORG)
        d += elm.Line().at((XE, 23.2)).tox(XE + 6.0).color(RED)
        d += elm.Label().at((XE - 0.2, 23.2)).label('L-GRID', loc='left', fontsize=FP - 1, halign='right', color=RED)
        d += elm.Line().at((XE, 20.8)).tox(XE + 8.0).color(BLUE)
        d += elm.Label().at((XE - 0.2, 20.8)).label('N-GRID', loc='left', fontsize=FP - 1, halign='right', color=BLUE)
        d += elm.Dot().at((XE + 1.5, 23.2)); d += elm.Dot().at((XE + 1.5, 20.8))
        d += elm.Capacitor().at((XE + 1.5, 23.2)).toy(20.8).color(ORG).label('CX_RED\n0.1 µF X2', loc='bottom', fontsize=FP - 3, color=ORG)
        d += elm.Dot().at((XE + 4.6, 23.2)); d += elm.Dot().at((XE + 4.6, 20.8))
        d += elm.Resistor().at((XE + 4.6, 23.2)).toy(20.8).label('2×470k\n½ W', loc='bottom', fontsize=FP - 3)
        d += elm.Capacitor().at((XE + 8.0, 20.8)).right(2.4).color(ORG).label('CY_RED 0.1 µF Y2', loc='top', fontsize=FP - 3, color=ORG)
        d += elm.Ground().color(GREEN)
        d += elm.Label().at((XE + 10.6, 20.2)).label('PE', loc='right', fontsize=FP - 2, halign='left', color=GREEN)
        d += elm.Label().at((XE + 6.4, 23.2)).label('nunca un Y en un vivo\nni en el lado del inversor', loc='right', fontsize=FP - 4, halign='left', color=RED)

        # L pole
        sL = spdt_block(18.0, 'L', 'L-GRID', 'L-INV', 'L-LOAD', 'K_L2')
        # N pole
        sN = spdt_block(8.0, 'N', 'N-GRID', 'N-INV', 'N-LOAD', 'K2')
        # inverter sources
        # L directo del trafo; N pasa por el inductor de filtro L_F (en el neutro, antes de C_F)
        d += elm.Dot(open=True).at((X0 - 4.4, 14.2)).label('L-TRAFO', loc='left', fontsize=FS)
        d += elm.Line().at((X0 - 4.4, 14.2)).tox(X0)
        d += elm.Dot(open=True).at((X0 - 4.4, 4.2)).label('N-TRAFO', loc='left', fontsize=FS)
        d += elm.Line().at((X0 - 4.4, 4.2)).tox(X0 - 3.8)
        d += elm.Inductor2(loops=4).right(3.0).color(BLUE).label('L_F  E65 N87', loc='top', fontsize=FP - 1, color=BLUE)
        d += elm.Line().tox(X0)
        d += elm.Label().at((X0 - 2.3, 3.4)).label('(fuera de tarjeta,\nen el chasis, fastons)', fontsize=FP - 4, color=GRAY)
        d += elm.Label().at((X0 + 0.15, 14.5)).label('L-INV', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        d += elm.Label().at((X0 + 0.15, 4.5)).label('N-INV', loc='right', fontsize=FP - 2, halign='left', valign='bottom', color=GRAY)
        for s, name, y in [(sL, 'L-INV', 14.2), (sN, 'N-INV', 4.2)]:
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
            'FILTRO DE SALIDA (lado del transformador, antes de los relés), igual que el OutBack FX:\n'
            'L_F E65 N87 en el NEUTRO · C_F 4.7 µF MKP X2 310 VAC · R_B 2 × 100 kΩ ½ W · R_D 22 Ω 5 W + C_D 4.7 µF X2 (huella opcional, DNP)',
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
        d += elm.Switch().right(2.4).label('K_L3 (NO)\nL90S (KiCad K1)', loc='top', fontsize=FP - 1, color=RED)
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
        # TN bond
        d += elm.Dot().at((X0 + 3.5, 4.2))
        d += elm.Line().at((X0 + 3.5, 4.2)).toy(1.6)
        d += elm.Switch().right(2.4).label('RL_TN = K5 (NO) L90S', loc='bottom', fontsize=FP - 2, color=BLUE)
        d += elm.Line().right(0.6)
        d += elm.Ground()
        d += elm.Label().at((X0 + 7.6, 1.0)).label('PE (TIERRA): unión N–PE solo con INVERSOR activo (Q2) + orden TN (AUX D10) + SW_TN en CASA', loc='right', fontsize=FP - 2, halign='left', color=GRAY)

        # ================= TABLE / NOTES =================
        TY = -1.2
        d += elm.Label().at((-12.0, TY)).label(
            'ESTADOS:\n'
            '  Modo        D5 AISLAR  D6 TRAFO  D10 TN  K2 + K_L2     K3 + K_L3   RL_TN              Casa en           Transformador\n'
            '  RED         0          0         x       NC (red)      abiertos    abierto (Q2)       RED               aislado\n'
            '  INV. CASA   1          0         1       NO (trafo)    abiertos    cerrado            TRANSFORMADOR     alimenta la casa (fuente principal)\n'
            '  INV. CARGA  1          0         0       NO (trafo)    abiertos    abierto            TRANSFORMADOR     alimenta cargas críticas (UPS)\n'
            '  CARGADOR    0          1         x       NC (red)      cerrados    abierto (Q2)       RED               conectado a la red (cargando)\n'
            '  D5 y D6     1          1         —       NO (trafo)    BLOQUEADOS por Q_A             = INVERSOR (seguro)\n'
            '  TN sin INV  0          —         1       —             —           BLOQUEADO por Q2   sin doble unión con CFE\n'
            '\n'
            'SECUENCIA A INVERSOR:  EN_HW = 0 → D6 = 0 → pausa 50 ms → D5 = 1 → pausa 50 ms → AUX: D10 según el escenario (CASA = 1, CARGA = 0) → EN_HW = 1 → SPWM.\n'
            'SALIDA DE INVERSOR:  EN_HW = 0 → AUX: D10 = 0 → D5 = 0.   Si se pierde el I2C, el AUX suelta D10.\n'
            'SECUENCIA A CARGADOR:  EN_HW = 0 → D5 = 0 → pausa 50 ms → verificar red con AC_FB_GRID → D6 = 1 → cargar (puente sincronizado con la red).\n'
            'SIN VERIFICACIÓN AUTOMÁTICA DE RELÉ (PC817 eliminado): un contacto soldado no se detecta solo → prueba manual periódica con los LED y el multímetro\n'
            '  (con orden INV: 0 V entre L-GRID y L-LOAD).  Huella del PC817 opcional, sin poblar (DNP).',
            loc='right', fontsize=FS - 1, halign='left', valign='top')
        d += elm.Label().at((-12.0, TY - 7.0)).label(
            'REGLAS:\n'
            '  • Falla de UN relé no forma paralelo: K2 soldado en NC → el NO no cierra y Q_A impide cerrar K3; K3 soldado → K2 sigue aislando la red.\n'
            '  • Con K2 apagado el puente nunca genera voltaje de inversor; en modo cargador solo rectifica sincronizado con la red.\n'
            '  • Separación: 3 mm entre pours de distinto origen (GRID / INV / LOAD) · 5 mm a bobinas y lógica · ranura entre zonas.\n'
            '  • J15: 1 15V · 2 GND · 3 AISLAR RED (CRÍTICO D5) · 4 GND · 5 TRAFO (CRÍTICO D6) · 6 GND · 7 TN (AUX D10) · 8 MODO_TN (CRÍTICO D4).\n'
            '  • Q2 (BC547, seguidor de emisor) = AND por hardware: RL_TN solo cierra con TN = 1 Y AISLAR = 1.  En modo cargador K2 sigue en la red: Q2 impide la doble unión.\n'
            '    Con AUX en reset o colgado, el 10k de I3 mantiene TN en 0.  Contacto de RL_TN: TIERRA en el NO (14); NC sin conectar.\n'
            '  • LED de diagnóstico: 15V apagado = sin energía (cable o XL7015) · LED de relé apagado con orden = J15-3/J15-5 abierto ·\n'
            '    LED de relé encendido SIN orden (LCD en RED) = línea pelada contra 15 V: revisar el cable IDC.  El LED indica bobina mandada, no contacto cerrado.\n'
            '  • SW_TN (DPDT): CASA = unión N–PE (polo A en la bobina de RL_TN) · CARGA = sin unión.  El polo B avisa al CRÍTICO; si no coincide con el menú, NO invierte.\n'
            '  • Dos puentes faston con seguro (L y N del trafo hacia K_L3/K3) = seccionador de servicio del cargador; colores distintos.\n'
            '  • C_F 4.7 µF MKP X2 (polipropileno, ≥ 310 VAC) entre L-INV y N-INV, ANTES de los relés: forma el LC con los ~2 mH del secundario (f₀ ≈ 1.64 kHz).\n'
            '    No poliéster (MKT): calienta con el rizo de 24 kHz.  R_B 2 × 100 kΩ ½ W descarga C_F a < 60 V en ~1 s.  R_D + C_D: huella opcional, poblar solo si el banco muestra oscilación.\n'
            '  • Filtro EMI de la red (como el OutBack FX): CX_RED 0.1 µF X2 entre L-GRID y N-GRID con descarga 2 × 470 kΩ ½ W (τ ≈ 0.1 s), y CY_RED 0.1 µF Y2 entre N-GRID y PE.\n'
            '    N-GRID siempre está referido a la unión de CFE (casi 0 V a PE): CY_RED no fuga, y en modo inversor K2 lo separa de la salida.  Nunca un capacitor Y en un vivo ni en N-INV.\n'
            '  • L_F (E65 N87) en serie con el NEUTRO del trafo, ANTES de C_F (como el OutBack FX): L_F + dispersión ≈ 2 mH.  Fuera de la tarjeta (peso y calor), con fastons N-TRAFO → L_F → N-INV.\n'
            '    Lleva toda la corriente de carga y de falla: entrehierro para no saturar con el pico de arranque (~18 A) y alambre que nunca se abra (el neutro no se interrumpe).',
            loc='right', fontsize=FS - 1, halign='left', valign='top')


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
