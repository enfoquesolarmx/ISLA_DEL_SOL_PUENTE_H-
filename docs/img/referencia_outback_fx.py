"""Diagrama de referencia: arquitectura AC del OutBack FX (120 V) según las mediciones de banco.
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
        d += elm.Label().at((-1.0, 17.2)).label(
            'REFERENCIA INDUSTRIAL: OutBack FX 120 V, etapa AC (medido en banco, 2026-11-09)',
            loc='right', fontsize=27, halign='left')
        d += elm.Label().at((-1.0, 15.9)).label(
            'Neutro común · tierra de chasis pasante · SIN unión N–PE interna (la decide el instalador)',
            loc='right', fontsize=FS, halign='left', color=GRAY)

        YH, YN, YP = 12.0, 6.0, 1.0
        XL, XR = 0.0, 26.0

        # equipment box
        for (a, b, c, e) in [(2.0, -0.4, 24.0, -0.4), (24.0, -0.4, 24.0, 14.6), (24.0, 14.6, 2.0, 14.6), (2.0, 14.6, 2.0, -0.4)]:
            d += elm.Line().at((a, b)).to((c, e)).color(GRAY).linestyle('--').linewidth(1.6)
        d += elm.Label().at((2.3, 14.2)).label('INVERSOR FX (gabinete = chasis)', loc='right', fontsize=FP, halign='left', color=GRAY)

        # terminals
        for x, side in [(XL, 'IN'), (XR, 'OUT')]:
            for y, nm, col in [(YH, 'AC HOT ' + side, RED), (YN, 'AC NEUTRAL ' + side, BLUE), (YP, 'CHASSIS GND/PE', GREEN)]:
                d += elm.Dot(open=True).at((x, y)).label(nm, loc='left' if side == 'IN' else 'right', fontsize=FS, color=col)

        # HOT: transfer relay (hypothesis: 3 poles in parallel)
        d += elm.Line().at((XL, YH)).tox(5.0).color(RED)
        sw = elm.Switch().at((5.0, YH)).right(3.0).color(RED).label('K  Deltrol 375P\n3PDT 30 A/polo, 1 HP', loc='top', fontsize=FP, color=RED)
        d += sw
        d += elm.Label().at((6.5, YH - 0.9)).label('polos en paralelo\n(HIPÓTESIS, por medir)', fontsize=FP - 3, color=ORG)
        d += elm.Line().at((8.0, YH)).tox(XR).color(RED)
        d += elm.Dot().at((17.0, YH))
        d += elm.Dot().at((13.0, YH))

        # transformer secondary, always on the output
        d += elm.Line().at((17.0, YH)).toy(10.2).color(RED)
        d += elm.Inductor2(loops=4).at((17.0, 10.2)).down(3.0).label('secundario\ntransformador\n(XFMR HOT/NEU)', loc='bottom', fontsize=FP - 1)
        d += elm.Line().at((17.0, 7.2)).toy(YN).color(BLUE)

        # C1 across XFMR HOT / NEU
        d += elm.Line().at((13.0, YH)).toy(10.0).color(RED)
        d += elm.Capacitor().at((13.0, 10.0)).down(2.6).label('C1  EFC 505J\n5 µF 400 V', loc='bottom', fontsize=FP - 1, color=ORG)
        d += elm.Line().at((13.0, 7.4)).toy(YN).color(BLUE)
        d += elm.Dot().at((13.0, YN))
        d += elm.Dot().at((17.0, YN))

        # NEUTRAL: common, toroid in series (position to confirm)
        d += elm.Line().at((XL, YN)).tox(9.0).color(BLUE)
        d += elm.Dot().at((5.0, YN))
        d += elm.Line().at((9.0, YN)).tox(13.0).color(BLUE)
        d += elm.Line().at((13.0, YN)).tox(17.0).color(BLUE)
        d += elm.Inductor2(loops=3).at((17.0, YN)).right(3.0).color(BLUE).label('toroide rojo\n(inductor)', loc='top', fontsize=FP - 1, color=BLUE)
        d += elm.Line().at((20.0, YN)).tox(XR).color(BLUE)
        d += elm.Label().at((14.8, YN - 1.0)).label('NEUTRO COMÚN: ningún relé lo abre\nIN = OUT = XFMR NEU (continuidad)',
                                                   fontsize=FP - 2, color=BLUE)
        d += elm.Label().at((18.5, YN - 1.0)).label('', fontsize=FP - 3)
        d += elm.Label().at((20.6, YN + 1.5)).label('posición exacta\n(antes/después de C1)\npor confirmar', loc='right',
                                                   fontsize=FP - 4, halign='left', color=ORG)

        # PE: pass-through to chassis
        d += elm.Line().at((XL, YP)).tox(XR).color(GREEN)
        d += elm.Dot().at((13.0, YP))
        d += elm.Ground().at((13.0, YP)).color(GREEN)
        d += elm.Label().at((13.0, YP + 0.7)).label('chasis', fontsize=FP - 2, color=GREEN)
        d += elm.Label().at((6.5, YP + 0.7)).label('2 bornes PE: tierra pasante', fontsize=FP - 2, color=GREEN)

        # NO internal bond: show open gap
        d += elm.Line().at((22.0, YN)).toy(4.3).color(GRAY).linestyle(':')
        d += elm.Line().at((22.0, 2.7)).toy(YP).color(GRAY).linestyle(':')
        d += elm.Label().at((22.0, 3.5)).label('X sin unión\nN–PE interna', fontsize=FP - 2, color=RED)
        d += elm.Dot().at((22.0, YN))
        d += elm.Dot().at((22.0, YP))

        # notes
        d += elm.Label().at((-1.0, -1.6)).label(
            'LO QUE SE MIDIÓ (sin energía):\n'
            '  • AC NEUTRAL IN ↔ AC NEUTRAL OUT ↔ faston XFMR NEU: continuidad   → el neutro nunca se conmuta.\n'
            '  • XFMR HOT y XFMR NEU llegan a C1 (5 µF); el neutro pasa por el toroide rojo.\n'
            '  • Chasis/PE separado del neutro; dos bornes PE (entrada y salida)  → la unión N–PE NO está en el equipo.\n'
            '\n'
            'UNIÓN N–PE: LA HACE EL INSTALADOR, UNA SOLA VEZ (NOM-001-SEDE-2012 250-24, 250-30):\n'
            '  • Hay CFE en la entrada AC (respaldo, o principal con red)  → NO unir: la unión ya está en la acometida.\n'
            '  • Sitio aislado, sin red ni planta unida                      → SÍ unir, en el tablero de cargas + electrodo.\n'
            '  • Planta en la entrada AC                                     → quitar la unión de la planta (si la trae).',
            loc='right', fontsize=FS - 1, halign='left', valign='top')


if __name__ == '__main__':
    build(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    print('ok')
