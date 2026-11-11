#!/usr/bin/env python3
"""
Auditoría térmica del cobre — tarjeta de transferencia de 5 relés (Isla del Sol)
Reutiliza el solucionador de hardware/puente_h/auditoria/auditoria_cobre.py:
reparto de corriente en las zonas reales (dos capas unidas por pines THT) y
calentamiento del cobre, más el calor de los contactos, bobinas y resistencias de 39 Ω.

Uso:  python3 auditoria_reles.py TARJETA.kicad_pcb RUTA_A/auditoria_cobre.py [--oz 1] [--amp 10] [--salida resultados]
Licencia: MIT
"""
import sys, os, argparse, json, importlib.util
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import BoundaryNorm, ListedColormap

ap = argparse.ArgumentParser()
ap.add_argument('pcb'); ap.add_argument('base')
ap.add_argument('--oz', type=float, default=1.0); ap.add_argument('--amp', type=float, default=10.0)
ap.add_argument('--salida', default='resultados'); ap.add_argument('--h', type=float, default=0.4)
ap.add_argument('--rc', type=float, default=5e-3, help='resistencia de contacto por relé [ohm]')
a = ap.parse_args(); os.makedirs(a.salida, exist_ok=True)
spec = importlib.util.spec_from_file_location('ac', a.base); ac = importlib.util.module_from_spec(spec); spec.loader.exec_module(ac)
I = a.amp

# ---- caminos de corriente por modo ----
# (net, fuentes, sumideros, anclas_de_otra_isla, etiqueta)
# Las "anclas" fijan a 0 V la otra isla de la misma red (unida por cable), por donde no pasa corriente en ese caso.
MODOS = {
 'RED → CARGA (ningún relé energizado; K4 y K2 por NC)': [
   ('L-GRID', [('J2', '2')], [('K4', '12')], [], 'L-GRID: J2 → K4 NC'),
   ('L-LOAD', [('K4', '11')], [('J2', '1')], [], 'L-LOAD: K4 COM → J2'),
   ('N-GRID', [('J1', '2')], [('K2', '12')], [('J13', None)], 'N-GRID: J1 → K2 NC'),
   ('N-LOAD', [('K2', '11')], [('J1', '1')], [], 'N-LOAD: K2 COM → J1')],
 'INVERSOR → CARGA (AISLAR: K4 y K2 por NO, K5 unión N–PE)': [
   ('L-INV', [('J7', '1')], [('J3', '1')], [('J8', '1')], 'L-INV: Faston J7 → puente J3'),
   ('L-INV', [('J8', '1')], [('K4', '14')], [('J7', '1')], 'L-INV: puente J8 → K4 NO'),
   ('L-LOAD', [('K4', '11')], [('J2', '1')], [], 'L-LOAD: K4 COM → J2'),
   ('N-INV', [('J4', '1')], [('K2', '14')], [('J5', '1')], 'N-INV: Faston J4 → K2 NO'),
   ('N-LOAD', [('K2', '11')], [('J1', '1')], [], 'N-LOAD: K2 COM → J1')],
 'CARGADOR (TRAFO: red → inversor por K4/K2 NC + K1/K3 NO)': [
   ('L-GRID', [('J2', '2')], [('K4', '12')], [], 'L-GRID: J2 → K4 NC'),
   ('L-LOAD', [('K4', '11')], [('K1', '14')], [], 'L-LOAD: K4 COM → K1 NO'),
   ('L-INV', [('K1', '11')], [('J7', '1')], [('J8', '1')], 'L-INV: K1 COM → Faston J7'),
   ('N-GRID', [('J1', '2')], [('K2', '12')], [('J13', None)], 'N-GRID: J1 → K2 NC'),
   ('N-LOAD', [('K2', '11')], [('K3', '14')], [], 'N-LOAD: K2 COM → K3 NO'),
   ('N-INV', [('K3', '11')], [('J5', '1')], [('J4', '1')], 'N-INV: K3 COM → puente J5'),
   ('N-INV', [('J6', '1')], [('J4', '1')], [('J5', '1')], 'N-INV: puente J6 → Faston J4')],
}
# por modo: contactos que conducen (relé, pin del contacto cerrado) y bobinas energizadas
CONTACTO = {list(MODOS)[0]: ([('K4', '12'), ('K2', '12')], []),
            list(MODOS)[1]: ([('K4', '14'), ('K2', '14')], ['K4', 'K2', 'K5']),
            list(MODOS)[2]: ([('K4', '12'), ('K2', '12'), ('K1', '14'), ('K3', '14')], ['K1', 'K3'])}
P_BOB = 0.52                      # W: relé de 15 V (375 Ω) con ~14 V; sin resistencia en serie
F_BOB, F_CON = 0.30, 0.6          # fracción que entra a la tarjeta: bobina = repartida bajo el cuerpo del relé; contacto = por sus pines

s = ac.Tarjeta(a.pcb, h=a.h, oz=a.oz)

def resolver(net, src, sinks, anclas, amps):
    # el solucionador acepta fuentes múltiples y sumideros; las anclas se agregan como sumideros sin conexión
    J, caida, m = s.resolver(net, src, sinks + anclas, amps)
    return J, caida

def calor_pads(refs_pads, W, q):
    """reparte W watts en las celdas de los pads indicados (ambas caras)."""
    sel = np.zeros((s.ny, s.nx), bool)
    for p in s.pads:
        if (p['ref'], p['num']) in refs_pads or (p['ref'], None) in refs_pads:
            sel |= np.hypot(s.X - p['x'], s.Y - p['y']) <= max(p['r'], s.h)
    n = sel.sum()
    if n:
        for ly in q: q[ly][sel] += W / 2 / (n * s.h * s.h)

def donde(T):
    ly = max(T, key=lambda k: T[k].max()); iy, ix = np.unravel_index(np.argmax(T[ly]), T[ly].shape)
    x, y = s.gx[ix], s.gy[iy]
    ref = min(s.pads, key=lambda p: (p['x'] - x) ** 2 + (p['y'] - y) ** 2)
    return f"{ly} ({x:.1f}, {y:.1f}) junto a {ref['ref']}.{ref['num']} [{ref['net']}]"

def calor_cuerpo(ref, W, q):
    """calor del cuerpo del relé hacia la tarjeta: repartido en el rectángulo que cubren sus pines (+2 mm), cara de componentes."""
    ps = [p for p in s.pads if p['ref'] == ref]
    x0, x1 = min(p['x'] for p in ps) - 2, max(p['x'] for p in ps) + 2; y0, y1 = min(p['y'] for p in ps) - 2, max(p['y'] for p in ps) + 2
    sel = (s.X >= x0) & (s.X <= x1) & (s.Y >= y0) & (s.Y <= y1)
    q['F.Cu'][sel] += W / (sel.sum() * s.h * s.h)

res = {}; mapas = {}
for modo, casos in MODOS.items():
    Jrms = {'F.Cu': np.zeros((s.ny, s.nx)), 'B.Cu': np.zeros((s.ny, s.nx))}; filas = []
    for net, src, sinks, anclas, et in casos:
        J, caida = resolver(net, src, sinks, anclas, I)
        P = sum((J[ly] ** 2 * s.Rsq).sum() * s.h * s.h for ly in J)
        for ly in J: Jrms[ly] += J[ly] ** 2
        filas.append(dict(camino=et, P=P, caida_mV=caida * 1e3, Jmax=max(J['F.Cu'].max(), J['B.Cu'].max())))
    Jrms = {k: np.sqrt(v) for k, v in Jrms.items()}
    q_cu = {ly: Jrms[ly] ** 2 * s.Rsq for ly in Jrms}
    T_cu = s.termico(q_cu)
    q = {ly: v.copy() for ly, v in q_cu.items()}
    con, bob = CONTACTO[modo]
    for k, pin in con:
        calor_pads([(k, '11'), (k, pin)], F_CON * I * I * a.rc, q)
    for k in bob:
        calor_cuerpo(k, F_BOB * P_BOB, q)
    T_tot = s.termico(q)
    Pcu = sum(f['P'] for f in filas)
    res[modo] = dict(filas=filas, P_cobre=Pcu, Tcu_max={k: float(v.max()) for k, v in T_cu.items()},
                     Ttot_max={k: float(v.max()) for k, v in T_tot.items()},
                     P_contactos=len(con) * I * I * a.rc, P_bobinas=len(bob) * P_BOB,
                     caliente=donde(T_tot), caliente_cu=donde(T_cu))
    mapas[modo] = (Jrms, T_cu, T_tot)

# ---- figuras ----
ETQ = {'K1', 'K2', 'K3', 'K4', 'K5', 'J1', 'J2', 'J3', 'J4', 'J5', 'J6', 'J7', 'J8', 'J10', 'J11', 'J12', 'J13', 'R1', 'R2', 'R3', 'R4', 'R12', 'U1'}
ac.ETIQUETAS = ETQ
cmap = ListedColormap(['#1b5e20', '#43a047', '#fdd835', '#fb8c00', '#c62828'])
norm = BoundaryNorm([0.01, 0.5, ac.LIM_OK, ac.LIM_ATN, 4, 50], cmap.N)
nombres = list(MODOS)
cortos = ['red', 'inversor', 'cargador']
for modo, corto in zip(nombres, cortos):
    Jrms, T_cu, T_tot = mapas[modo]
    fig, axs = plt.subplots(2, 2, figsize=(17, 14))
    for c, ly, nom in ((0, 'F.Cu', 'F.Cu (componentes)'), (1, 'B.Cu', 'B.Cu (soldadura, vista desde arriba)')):
        ax = axs[0, c]; ac.dibujar_base(ax, s, ly)
        im = ax.pcolormesh(s.gx, s.gy, np.ma.masked_less(Jrms[ly], 0.01), cmap=cmap, norm=norm, shading='auto', zorder=2)
        ax.set_title(f'Corriente · {nom}', fontsize=12)
        ax = axs[1, c]; ac.dibujar_base(ax, s, ly)
        im2 = ax.pcolormesh(s.gx, s.gy, T_tot[ly], cmap='turbo', vmin=0, vmax=30, shading='auto', zorder=2, alpha=0.9)
        cs = ax.contour(s.gx, s.gy, T_tot[ly], levels=[5, 10, 15, 20, 25], colors='k', linewidths=0.6, zorder=3); ax.clabel(cs, fmt='%d °C', fontsize=7)
        iy, ix = np.unravel_index(np.argmax(T_tot[ly]), T_tot[ly].shape); ax.plot(s.gx[ix], s.gy[iy], 'w*', ms=15, mec='k', zorder=6)
        ax.set_title(f'Calentamiento total · {nom} · máx +{T_tot[ly].max():.1f} °C', fontsize=12)
    cb = fig.colorbar(im, ax=axs[0, :], orientation='horizontal', fraction=0.04, pad=0.03, ticks=[0.25, 0.9, 2.0, 3.3, 27])
    cb.ax.set_xticklabels(['< 0.5\nholgado', '0.5–1.35\nbien', '1.35–2.7\natención', '2.7–4\ncaliente', '> 4\njunto a pines'], fontsize=9)
    cb.set_label(f'Corriente por mm de ancho [A/mm], cobre {a.oz:g} oz · 1.35 A/mm ≈ +20 °C (IPC-2221)')
    fig.colorbar(im2, ax=axs[1, :], orientation='horizontal', fraction=0.04, pad=0.03,
                 label='°C sobre el ambiente: cobre + contactos + bobinas (relés de 15 V, sin resistencia), convección natural')
    fig.suptitle(f'{modo} · {I:g} A RMS a 127 V ({I*127:.0f} VA) · cobre {a.oz:g} oz', fontsize=15, weight='bold')
    plt.savefig(f'{a.salida}/modo_{corto}_{a.oz:g}oz.png', dpi=100, bbox_inches='tight'); plt.close()

json.dump(res, open(f'{a.salida}/resumen_{a.oz:g}oz.json', 'w'), indent=1, ensure_ascii=False, default=float)
for m, r in res.items():
    print(f'  más caliente total: {r["caliente"]} · solo cobre: {r["caliente_cu"]}')
    print(f'\n== {m}\n  P_cobre {r["P_cobre"]:.2f} W · Tcu {max(r["Tcu_max"].values()):.1f} °C · Ttot {max(r["Ttot_max"].values()):.1f} °C · contactos {r["P_contactos"]:.2f} W · bobinas {r["P_bobinas"]:.2f} W')
    for f in r['filas']: print(f'   {f["camino"]:34s} {f["P"]*1e3:7.0f} mW  {f["caida_mV"]:6.1f} mV  Jmax {f["Jmax"]:.1f} A/mm')
