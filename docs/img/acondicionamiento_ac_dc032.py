"""Acondicionamiento del canal de CA (red) — DC-032.

Muestra por qué el filtro antialias va DESPUÉS del nodo y no SOBRE el nodo:
el comparador de cruce por cero mira el nodo, así que cualquier capacitor
colgado ahí le mete fase. Subiendo R17 y bajando C30 se filtra A6 sin tocar
el nodo, y la tolerancia del capacitor deja de alcanzar al comparador.

    python3 acondicionamiento_ac_dc032.py salida.png [dpi]
"""
import sys
import schemdraw
import schemdraw.elements as elm
import matplotlib

matplotlib.rcParams['font.weight'] = 'bold'

BLUE = '#1f4e9a'
RED = '#a01818'
GREEN = '#1a6e1a'
F = 21
FS = 18

YS = 9.0      # riel de la señal
YB = 2.0      # riel del bias (NEUTRO-GRID)


def build(out, dpi):
    with schemdraw.Drawing(file=out, show=False, dpi=dpi) as d:
        d.config(fontsize=F, unit=3.0, font='FreeMono', lw=2.8,
                 inches_per_unit=0.72)

        # ------------------------------------------------------- títulos
        d += elm.Label().at((-10.5, YS + 6.6)).label(
            'CANAL DE CA DE LA RED  —  el filtro antialias va DESPUES del nodo, no SOBRE el nodo',
            loc='right', fontsize=30, halign='left')
        d += elm.Label().at((-10.5, YS + 5.0)).label(
            'El comparador de cruce por cero mira EL NODO, asi que todo capacitor colgado del nodo le mete fase.\n'
            'El filtro es el PRODUCTO R x C:  22k x 10nF = 1k x 100nF = 100 us.  Con R17 alta, la rama casi no\n'
            'carga el nodo: A6 queda igual de filtrada y el comparador se queda donde estaba.',
            loc='right', fontsize=FS, halign='left')

        # --------------------------------------------- cadena de la señal
        d += elm.Dot(open=True).at((-10.0, YS)).label(
            'J17-1  12VAC_GRID\n(secundario del\n transformador)',
            loc='left', fontsize=FS, color=BLUE)
        d += elm.Line().at((-10.0, YS)).to((-7.0, YS))
        d += elm.Resistor().at((-7.0, YS)).to((-4.0, YS)).label('R7\n110k', fontsize=FS)
        d += elm.Line().at((-4.0, YS)).to((0.0, YS))

        # --- C16, colgada del tramo R7-nodo
        d += elm.Dot().at((-2.0, YS))
        d += elm.Capacitor().at((-2.0, YS - 1.2)).to((-2.0, YS - 3.0))\
            .label('C16\n10nF', loc='bottom', fontsize=FS)
        d += elm.Line().at((-2.0, YS)).to((-2.0, YS - 1.2))
        d += elm.Ground().at((-2.0, YS - 3.0))

        # --- EL NODO
        d += elm.Dot(radius=0.20).at((0.0, YS)).color(RED)
        d += elm.Label().at((0.45, YS + 1.0)).label('EL NODO', fontsize=24, color=RED, halign='left')

        # --- rama del comparador, sale hacia arriba
        d += elm.Line().at((0.0, YS)).to((0.0, YS + 3.2)).color(RED)
        d += elm.Line().at((0.0, YS + 3.2)).to((4.0, YS + 3.2)).color(RED)
        d += elm.Dot(open=True).at((4.0, YS + 3.2)).color(RED).label(
            ' D6 = AIN0   entrada (+) del comparador',
            loc='right', fontsize=FS, color=RED)

        # --- rama de A6: R17 grande, DESPUES del nodo
        d += elm.Line().at((0.0, YS)).to((1.5, YS))
        d += elm.Resistor().at((1.5, YS)).to((4.5, YS))\
            .label('R17\n22k', fontsize=FS, color=GREEN)
        d += elm.Line().at((4.5, YS)).to((6.0, YS))
        d += elm.Dot().at((6.0, YS))
        d += elm.Line().at((6.0, YS)).to((8.2, YS))
        d += elm.Dot(open=True).at((8.2, YS)).label(
            ' A6  (ADC)', loc='right', fontsize=FS, color=GREEN)

        # --- C30 del pin de A6 al bias (NO a tierra)
        d += elm.Line().at((6.0, YS)).to((6.0, YS - 1.8))
        d += elm.Capacitor().at((6.0, YS - 1.8)).to((6.0, YS - 3.6))\
            .label('C30\n10nF', loc='bottom', fontsize=FS, color=GREEN)
        d += elm.Line().at((6.0, YS - 3.6)).to((6.0, YB))
        d += elm.Dot().at((6.0, YB))

        # ------------------------------------------- pata de abajo: R8 + RV1
        d += elm.Line().at((0.0, YS)).to((0.0, YS - 1.0))
        d += elm.Resistor().at((0.0, YS - 1.0)).to((0.0, YS - 4.0))
        d += elm.Label().at((0.45, YS - 2.5)).label('R8\n8.2k', fontsize=FS, halign='left')
        d += elm.Potentiometer().at((0.0, YS - 4.0)).to((0.0, YB))
        d += elm.Label().at((-0.75, YS - 5.5)).label('RV1  2k\n(reostato)', fontsize=FS, halign='right')
        d += elm.Dot().at((0.0, YB))

        # -------------------------------------- riel del bias NEUTRO-GRID
        d += elm.Line().at((-10.0, YB)).to((9.6, YB)).color(BLUE)
        d += elm.Label().at((9.6, YB - 1.5)).label(
            'NEUTRO-GRID = D7 = AIN1 = entrada (-) del comparador = J17-2',
            fontsize=19, halign='right', color=BLUE)

        # --- divisor del bias
        d += elm.Dot(radius=0.20).at((-10.0, YB)).color(BLUE)
        d += elm.Resistor().at((-10.0, YB)).to((-10.0, YB + 3.0))
        d += elm.Label().at((-10.45, YB + 1.5)).label('R5\n10k', fontsize=FS, halign='right')
        d += elm.Vdd().at((-10.0, YB + 3.0)).label('5V', fontsize=FS)
        d += elm.Resistor().at((-10.0, YB)).to((-10.0, YB - 3.0))
        d += elm.Label().at((-10.45, YB - 1.5)).label('R6\n10k', fontsize=FS, halign='right')
        d += elm.Ground().at((-10.0, YB - 3.0))

        # --- capacitores del bias
        d += elm.Dot().at((-7.0, YB))
        d += elm.Capacitor().at((-7.0, YB)).to((-7.0, YB - 2.4))\
            .label('C4\n100nF', loc='right', fontsize=FS)
        d += elm.Ground().at((-7.0, YB - 2.4))
        d += elm.Dot().at((-4.2, YB))
        d += elm.Capacitor2().at((-4.2, YB)).to((-4.2, YB - 2.4))\
            .label('C5\n10uF', loc='right', fontsize=FS)
        d += elm.Ground().at((-4.2, YB - 2.4))

        # ----------------------------------------------------- anotaciones
        YA = YB - 5.2
        d += elm.Label().at((-10.5, YA)).label(
            'POR QUE 22k Y NO 1k',
            loc='right', fontsize=26, halign='left', color=GREEN)
        d += elm.Label().at((-10.5, YA - 1.8)).label(
            '                       fase del comparador    antialias a 750 Hz    si C30 varia 9 %\n'
            '   R17 = 1k , C30 = 100nF      -19 a -24 grados        -13.3 dB         la fase se corre 1.6 grados\n'
            '   R17 = 22k, C30 = 10nF       -3.5 grados             -5.6 dB          la fase se corre 0.2 grados',
            loc='right', fontsize=FS, halign='left')
        d += elm.Label().at((-10.5, YA - 4.4)).label(
            'MEDIDO EN BANCO el 2026-11-24:  tres capacitores marcados "104" de la misma bolsa midieron\n'
            '126, 134 y 137 nF.  La etiqueta dice 100.  Con R17 = 1k esa dispersion mueve el cruce por cero;\n'
            'con R17 = 22k no lo alcanza. No se trata de conseguir un capacitor bueno, sino de que no importe.',
            loc='right', fontsize=FS, halign='left', color=RED)


if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else 'acondicionamiento_ac_dc032.png',
          int(sys.argv[2]) if len(sys.argv) > 2 else 150)
