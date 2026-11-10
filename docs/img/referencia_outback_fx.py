"""Diagrama de referencia: arquitectura AC del OutBack FX (120 V), según las mediciones de banco
del 2026-11-09 y 2026-11-10.
Uso: python3 referencia_outback_fx.py salida.png [dpi]
"""
import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib
matplotlib.rcParams['font.weight'] = 'bold'

RED = '#a01818'; BLUE = '#1f4e9a'; GREEN = '#1a6e1a'; GRAY = '#555555'; ORG = '#b35900'
F = 20; FS = 17; FP = 15


def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.6, inches_per_unit=0.85)
        d += elm.Label().at((-1.0, 19.6)).label(
            'REFERENCIA INDUSTRIAL: OutBack FX 120 V, etapa AC (medido en banco, 2026-11-09 y 10)',
            loc='right', fontsize=27, halign='left')
        d += elm.Label().at((-1.0, 18.3)).label(
            'Dos relés solo en el VIVO · neutro común · tierra de chasis pasante · sin unión N–PE interna · filtro EMI sin capacitor vivo–chasis',
            loc='right', fontsize=FS, halign='left', color=GRAY)

        YH, YXH, YXN, YN, YP = 14.0, 11.5, 8.5, 5.0, 1.0
        XL, XR = 0.0, 30.0

        # equipment box
        for (a, b, c, e) in [(1.6, -0.4, 28.4, -0.4), (28.4, -0.4, 28.4, 16.6), (28.4, 16.6, 1.6, 16.6), (1.6, 16.6, 1.6, -0.4)]:
            d += elm.Line().at((a, b)).to((c, e)).color(GRAY).linestyle('--').linewidth(1.6)
        d += elm.Label().at((1.9, 16.2)).label('INVERSOR FX (gabinete = chasis)', loc='right', fontsize=FP, halign='left', color=GRAY)

        # terminals
        for x, side in [(XL, 'IN'), (XR, 'OUT')]:
            for y, nm, col in [(YH, 'AC HOT ' + side, RED), (YN, 'AC NEUTRAL ' + side, BLUE), (YP, 'CHASSIS GND/PE', GREEN)]:
                d += elm.Dot(open=True).at((x, y)).label(nm, loc='left' if side == 'IN' else 'right', fontsize=FS, color=col)

        # HOT IN -> CX node -> Deltrol (NO) -> Hall -> HOT bus -> HOT OUT
        d += elm.Line().at((XL, YH)).tox(5.0).color(RED)
        d += elm.Dot().at((3.4, YH))
        d += elm.Switch().at((5.0, YH)).right(3.4).color(RED).label('K_RED  Deltrol 375P\n3 polos NO en paralelo', loc='top', fontsize=FP - 1, color=RED)
        d += elm.Line().at((8.4, YH)).tox(9.4).color(RED)
        d += elm.RBox(w=1.6, h=0.7).right(1.8).color(ORG).label('Hall (I)', loc='top', fontsize=FP - 2, color=ORG)
        d += elm.Line().at((11.2, YH)).tox(XR).color(RED)
        d += elm.Label().at((10.3, YH - 0.9)).label('→ IDC (corriente)', fontsize=FP - 4, color=ORG)
        d += elm.Dot().at((22.0, YH))
        d += elm.Label().at((22.4, YH + 0.9)).label('R1–R8 → IDC (voltaje)', loc='right', fontsize=FP - 3, halign='left', color=ORG)
        d += elm.Line().at((22.0, YH)).up(0.7).color(ORG)

        # Zettler: HOT bus -> XFMR HOT
        d += elm.Dot().at((19.0, YH))
        d += elm.Switch().at((19.0, YH)).down(2.5).color(RED).label('K_INV  Zettler AZ2850-2A\n2 polos NO en paralelo\n(pines 2,6 ↔ HOT OUT · 4,8 ↔ XFMR HOT)', loc='bottom', fontsize=FP - 2, color=RED)
        d += elm.Line().at((19.0, YXH)).tox(13.5).color(RED)
        d += elm.Label().at((15.0, YXH + 0.4)).label('XFMR HOT', fontsize=FP - 2, color=GRAY)

        # transformer secondary + C1
        d += elm.Dot().at((17.0, YXH))
        d += elm.Inductor2(loops=4).at((17.0, YXH)).down(3.0).label('secundario\ntransformador', loc='bottom', fontsize=FP - 3)
        d += elm.Dot().at((13.5, YXH))
        d += elm.Capacitor().at((13.5, YXH)).down(3.0).label('C1 505J\n5 µF', loc='bottom', fontsize=FP - 2, color=ORG)
        d += elm.Line().at((13.5, YXN)).tox(17.0).color(BLUE)
        d += elm.Dot().at((13.5, YXN))
        d += elm.Label().at((15.3, YXN - 0.45)).label('XFMR NEU', fontsize=FP - 2, color=GRAY)

        # toroid: neutral bus -> XFMR NEU
        d += elm.Line().at((13.5, YXN)).tox(11.0).color(BLUE)
        d += elm.Inductor2(loops=3).at((11.0, YXN)).down(3.5).color(BLUE).label('toroide\nrojo', loc='top', fontsize=FP - 2, color=BLUE)

        # neutral common bus
        d += elm.Line().at((XL, YN)).tox(XR).color(BLUE)
        for x in (3.4, 7.0, 11.0):
            d += elm.Dot().at((x, YN))
        d += elm.Label().at((21.0, YN - 0.8)).label('NEUTRO COMÚN: IN = OUT, ningún relé lo abre', fontsize=FP - 2, color=BLUE)

        # CX hot-neutral, CY neutral-chassis
        d += elm.Capacitor().at((3.4, YH)).toy(YN).color(ORG).label('CX 0.1 µF\nX1/Y2', loc='bottom', fontsize=FP - 3, color=ORG)
        d += elm.Capacitor().at((7.0, YN)).toy(YP).color(ORG).label('CY 0.1 µF\nX1/Y2', loc='bottom', fontsize=FP - 3, color=ORG)
        d += elm.Dot().at((7.0, YP))

        # PE
        d += elm.Line().at((XL, YP)).tox(XR).color(GREEN)
        d += elm.Dot().at((15.0, YP))
        d += elm.Ground().at((15.0, YP)).color(GREEN)
        d += elm.Label().at((15.0, YP + 0.6)).label('chasis', fontsize=FP - 2, color=GREEN)
        d += elm.Label().at((21.5, YP + 0.6)).label('ningún capacitor VIVO–chasis · sin unión N–PE interna', fontsize=FP - 3, color=RED)

        # notes
        d += elm.Label().at((-1.0, -1.6)).label(
            'ESTADOS (M7–M9):\n'
            '  Estado            K_RED (Deltrol)   K_INV (Zettler)   La salida tiene\n'
            '  Sin energía       abierto           abierto           NADA (el FX apagado no deja pasar la red)\n'
            '  Inversor          abierto           cerrado           transformador\n'
            '  Red, sin cargar   cerrado           abierto           red\n'
            '  Red, cargando     cerrado           cerrado           red + transformador (puente sincronizado)\n'
            '\n'
            'FILTRO: C1 5 µF (XFMR HOT–NEU) + toroide en el neutro = LC de salida · CX vivo–neutro (EMI diferencial) · CY neutro–chasis\n'
            '  ("unión capacitiva": 26.5 kΩ a 60 Hz, 1.6 Ω a 1 MHz).  Sensado: corriente con sensor Hall en el vivo; voltaje con R1–R8 en serie.',
            loc='right', fontsize=FS - 1, halign='left', valign='top')


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
