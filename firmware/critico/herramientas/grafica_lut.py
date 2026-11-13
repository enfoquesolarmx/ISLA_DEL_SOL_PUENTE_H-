#!/usr/bin/env python3
"""
grafica_lut.py - Isla del Sol, firmware del CRÍTICO
Dibuja cómo la LUT de 90° (100 muestras) arma la senoidal completa de 60 Hz,
con un color por cuadrante. Genera img/lut_cuarto_de_onda.png.
Uso (desde firmware/critico/herramientas):  python3 grafica_lut.py
Requiere matplotlib. Licencia: MIT.
"""
import math, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

S, C = 200, 100
tabla = [round(65535 * math.sin((k + 0.5) * math.pi / S)) for k in range(C)]
COL = ['#2a78d6', '#eb6834', '#1baf7a', '#eda100']
SURF, INK, INK2, GRID = '#fcfcfb', '#1f1e1c', '#5f5e5a', '#e6e5e0'
nombres = ['Q1  0°–90°', 'Q2  90°–180°', 'Q3  180°–270°', 'Q4  270°–360°']
lectura = ['tabla[m]\ntal cual', 'tabla[199 − m]\nen espejo', 'tabla[m − 200]\ntal cual', 'tabla[399 − m]\nen espejo']
signo = ['+  (LB encendido)', '+  (LB encendido)', '−  (HB encendido)', '−  (HB encendido)']

n, val, idx, cuad = [], [], [], []
for m in range(400):
    k = m % S; neg = m >= S
    q = k if k < C else S - 1 - k
    n.append(m); idx.append(q); cuad.append((2 if neg else 0) + (0 if k < C else 1))
    val.append((-1 if neg else 1) * tabla[q] / 65535)

plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'axes.edgecolor': GRID,
                     'axes.labelcolor': INK2, 'xtick.color': INK2, 'ytick.color': INK2})
fig, (a1, a2) = plt.subplots(2, 1, figsize=(12, 8.2), sharex=True,
                             gridspec_kw={'height_ratios': [1.6, 1], 'hspace': 0.12}, facecolor=SURF)
for a in (a1, a2):
    a.set_facecolor(SURF); a.grid(True, color=GRID, lw=0.8); a.set_axisbelow(True)
    for s in ('top', 'right'): a.spines[s].set_visible(False)
    for x in (100, 200, 300): a.axvline(x, color=INK2, lw=0.8, ls=(0, (3, 3)), zorder=1)

for c in range(4):
    xs = [n[i] for i in range(400) if cuad[i] == c]
    ys = [val[i] for i in range(400) if cuad[i] == c]
    a1.fill_between(xs, 0, ys, color=COL[c], alpha=0.16, lw=0, zorder=2)
    a1.plot(xs, ys, color=COL[c], lw=2.2, zorder=3, solid_capstyle='round')
    iy = [idx[i] for i in range(400) if cuad[i] == c]
    a2.plot(xs, iy, color=COL[c], lw=2.2, zorder=3, solid_capstyle='round')
    xm = c * 100 + 50
    a1.text(xm, 1.27 if c < 2 else -1.27, nombres[c], ha='center', va='center', fontsize=11.5, fontweight='bold', color=INK)
    a1.text(xm, 0.5 if c < 2 else -0.5, lectura[c] + '\n' + 'signo ' + signo[c], ha='center', va='center',
            fontsize=9.3, color=INK, bbox=dict(boxstyle='round,pad=0.35', fc=SURF, ec=COL[c], lw=1.6))
    a2.text(xm, 112, '0 → 99' if c % 2 == 0 else '99 → 0', ha='center', va='center', fontsize=10, color=INK, fontweight='bold')

a1.axhline(0, color=INK2, lw=1)
a1.set_ylim(-1.45, 1.45); a1.set_yticks([-1, -0.5, 0, 0.5, 1])
a1.set_ylabel('Salida (fracción de la amplitud)')
a1.set_title('Senoidal completa armada con una LUT de 90° (100 muestras)', loc='left', fontsize=14, fontweight='bold', color=INK, pad=26)
a1.text(0, 1.62, 'Isla del Sol · CRÍTICO · 60 Hz: 400 muestras por ciclo, una por periodo de la portadora de 24 kHz. '
        'La pierna A usa la magnitud; la pierna B pone el signo.', transform=a1.get_yaxis_transform(), fontsize=9.5, color=INK2)

a2.set_ylim(-8, 124); a2.set_yticks([0, 25, 50, 75, 99])
a2.set_ylabel('Índice leído de la tabla\n(q = 0 … 99)')
a2.set_xlabel('Muestra del ciclo, m = 0 … 399', labelpad=8)
a2.set_xlim(-4, 403); a2.set_xticks([0, 50, 100, 150, 200, 250, 300, 350, 399])
sec = a2.secondary_xaxis('top', functions=(lambda x: x * 0.9, lambda d: d / 0.9))
sec.set_xticks([0, 90, 180, 270, 360]); sec.set_xticklabels(['0°', '90°', '180°', '270°', '360°']); sec.tick_params(colors=INK2)
fig.text(0.5, -0.165, 'Maximizando la SPWM de trinchera sin coprocesador matemático ni hardware dedicado\n'
         '(como un DSP o un microcontrolador ARM con periféricos de PWM avanzados):\n'
         'las matemáticas básicas maximizan los recursos.\n'
         'sen(180° − x) = sen(x)  →  el espejo        sen(180° + x) = −sen(x)  →  el signo\n'
         'Dos identidades de bachillerato ahorran el 75 % de la tabla: 100 valores en lugar de 400 (200 B en lugar de 800 B).',
         ha='center', va='bottom', fontsize=10.5, color=INK, linespacing=1.55,
         bbox=dict(boxstyle='round,pad=0.7', fc='#f3f2ee', ec=GRID, lw=1))
fig.text(0.5, 0.005, 'Una sola tabla de 100 valores se lee 4 veces por ciclo: ida, vuelta, ida, vuelta.  '
         'Muestra al centro del periodo: tabla[q] = sen((q + 0.5)·180°/200).', ha='center', va='bottom', fontsize=9, color=INK2)

leg = [Line2D([0], [0], color=COL[c], lw=3, label=nombres[c]) for c in range(4)]
fig.legend(handles=leg, loc='upper right', bbox_to_anchor=(0.985, 0.985), ncol=4, frameon=False, fontsize=9.5, labelcolor=INK)
fig.subplots_adjust(bottom=0.1)
fig.savefig('../img/lut_cuarto_de_onda.png', dpi=160, bbox_inches='tight', facecolor=SURF)
print('ok')
