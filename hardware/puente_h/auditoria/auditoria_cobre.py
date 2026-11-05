#!/usr/bin/env python3
"""
Auditoría de cobre del layout — Isla del Sol, Puente H
Lee un .kicad_pcb (KiCad 8/9), resuelve la distribución de corriente DC en las
zonas de cobre de cada net (dos capas unidas por pads THT y vías) y estima el
calentamiento del cobre. Genera mapas PNG y un resumen en Markdown.

Uso:  python3 auditoria_cobre.py TARJETA.kicad_pcb [--oz 1] [--salida carpeta]
Licencia: MIT
"""
import re, sys, math, argparse, os, json
import numpy as np
from matplotlib.path import Path
import scipy.sparse as sp, scipy.sparse.linalg as spl
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import BoundaryNorm, ListedColormap
from matplotlib.patches import Circle, Polygon as MPoly

# ---------------- lector s-expr ----------------
_tok = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')
def parse(text):
    st = [[]]
    for m in _tok.finditer(text):
        t = m.group(0)
        if t == '(': st.append([])
        elif t == ')': x = st.pop(); st[-1].append(x)
        else: st[-1].append(t[1:-1].replace('\\"', '"') if t[0] == '"' else t)
    return st[0][0]
def find(n, k): return [c for c in n if isinstance(c, list) and c and c[0] == k]
def first(n, k):
    r = find(n, k); return r[0] if r else None

# ---------------- escenario de operación ----------------
# Corrientes RMS y fracción de tiempo de cada camino (ver documento).
ESCENARIO = {
    'nombre': 'Inversor 1000 W, 127 V, primario ~33 A RMS, bus ~30 A RMS',
    'casos': [
        # (net, fuente(ref,pad), sumideros[(ref,pad|None)], amperes, fracción de tiempo, etiqueta)
        ('VS1', ('Q2', '2'), [('J24', None), ('J27', None)], 33, 0.5, 'VS1: Q2 (bajo) → bornes'),
        ('VS1', ('Q1', '3'), [('J24', None), ('J27', None)], 33, 0.5, 'VS1: Q1 (alto) → bornes'),
        ('VS2', ('Q4', '2'), [('J23', None), ('J22', None)], 33, 0.5, 'VS2: Q4 (bajo) → bornes'),
        ('VS2', ('Q3', '3'), [('J23', None), ('J22', None)], 33, 0.5, 'VS2: Q3 (alto) → bornes'),
        ('48V', ('Q1', '2'), [('J5', None), ('J6', None)], 30, 0.5, 'POS: Faston → Q1'),
        ('48V', ('Q3', '2'), [('J5', None), ('J6', None)], 30, 0.5, 'POS: Faston → Q3'),
        ('GND', ('Q2', '3'), [('R16', '2'), ('R28', '2')], 30, 0.5, 'NEG: Q2 → shunts'),
        ('GND', ('Q4', '3'), [('R16', '2'), ('R28', '2')], 30, 0.5, 'NEG: Q4 → shunts'),
        ('GND_IN', [('R16', '1'), ('R28', '1')], [('J1', None), ('J2', None)], 30, 1.0, 'GND_IN: shunts → Faston'),
    ]}
ETIQUETAS = {'Q1', 'Q2', 'Q3', 'Q4', 'J1', 'J2', 'J5', 'J6', 'J24', 'J27', 'J23', 'J22', 'R16', 'R28', 'R17',
             'C1', 'C2', 'C3', 'C4', 'C9', 'C10', 'J10', 'J9', 'J8', 'J3'}
LIM_OK, LIM_ATN = 1.35, 2.7   # A/mm de lámina (1 oz): ver documento
ALAMBRE_MM2 = 3.31            # 12 AWG

class Tarjeta:
    def __init__(s, fn, h=0.4, oz=1.0):
        s.t = parse(open(fn, encoding='utf-8').read())
        s.h = h; s.tcu = 0.035 * oz; s.Rsq = 1.72e-5 / s.tcu   # ohm/cuadro (rho en ohm·mm)
        s.nets = {n[1]: n[2] for n in find(s.t, 'net')}
        xs = []; ys = []
        for g in s.t:
            if isinstance(g, list) and g and g[0].startswith('gr_') and first(g, 'layer') and first(g, 'layer')[1] == 'Edge.Cuts':
                for k in ('start', 'end'):
                    p = first(g, k)
                    if p: xs.append(float(p[1])); ys.append(float(p[2]))
        s.bx = (min(xs), max(xs), min(ys), max(ys))
        s.gx = np.arange(s.bx[0], s.bx[1] + h, h); s.gy = np.arange(s.bx[2], s.bx[3] + h, h)
        s.X, s.Y = np.meshgrid(s.gx, s.gy); s.P = np.c_[s.X.ravel(), s.Y.ravel()]
        s.ny, s.nx = s.X.shape
        s._pads(); s.vias = [(float(first(v, 'at')[1]), float(first(v, 'at')[2]), s.nn(v)) for v in find(s.t, 'via')]
        s.cache = {}
    def nn(s, x):
        n = first(x, 'net')
        if not n: return '-'
        return n[2] if len(n) > 2 else (s.nets.get(n[1], n[1]) if n[1].isdigit() else n[1])
    def _pads(s):
        s.pads = []; s.fps = []
        for f in find(s.t, 'footprint'):
            at = first(f, 'at'); x0, y0 = float(at[1]), float(at[2]); r = math.radians(float(at[3]) if len(at) > 3 else 0)
            ref = [p[2] for p in find(f, 'property') if p[1] == 'Reference'][0]
            s.fps.append((ref, x0, y0, first(f, 'layer')[1]))
            for pd in find(f, 'pad'):
                pa = first(pd, 'at'); px, py = float(pa[1]), float(pa[2])
                X = x0 + px * math.cos(r) + py * math.sin(r); Y = y0 - px * math.sin(r) + py * math.cos(r)
                sx, sy = map(float, first(pd, 'size')[1:3])
                s.pads.append(dict(ref=ref, num=pd[1], x=X, y=Y, r=max(sx, sy) / 2 * 0.9, net=s.nn(pd),
                                   tht=pd[2] == 'thru_hole', layers=first(pd, 'layers')[1:]))
    def mascara_net(s, net):
        if net in s.cache: return s.cache[net]
        m = {'F.Cu': np.zeros(len(s.P), bool), 'B.Cu': np.zeros(len(s.P), bool)}
        for z in find(s.t, 'zone'):
            if s.nn(z) != net: continue
            for fp in find(z, 'filled_polygon'):
                ly = first(fp, 'layer')[1]
                if ly in m:
                    xy = np.array([(float(p[1]), float(p[2])) for p in find(first(fp, 'pts'), 'xy')])
                    m[ly] |= Path(xy).contains_points(s.P)
        for sg in find(s.t, 'segment'):
            if s.nn(sg) != net: continue
            ly = first(sg, 'layer')[1]
            if ly not in m: continue
            a = np.array(list(map(float, first(sg, 'start')[1:3]))); b = np.array(list(map(float, first(sg, 'end')[1:3])))
            w = float(first(sg, 'width')[1]); d = b - a; L2 = d @ d + 1e-12
            tt = np.clip(((s.P - a) @ d) / L2, 0, 1); dist = np.hypot(*(s.P - (a + np.outer(tt, d))).T)
            m[ly] |= dist <= max(w / 2, s.h * 0.6)
        links = []
        for p in s.pads:
            if p['net'] != net: continue
            d = np.hypot(s.P[:, 0] - p['x'], s.P[:, 1] - p['y']) <= max(p['r'], s.h)
            for ly in m:
                if ly in p['layers'] or '*.Cu' in p['layers']: m[ly] |= d
            if p['tht']: links.append(d)
        for vx, vy, vn in s.vias:
            if vn == net: links.append(np.hypot(s.P[:, 0] - vx, s.P[:, 1] - vy) <= max(0.45, s.h))
        m = {k: v.reshape(s.ny, s.nx) for k, v in m.items()}
        s.cache[net] = (m, links); return m, links
    def refuerzo(s, ancho=2.5, nets=('VS1', 'VS2')):
        """Celdas bajo las ventanas de máscara donde se suelda alambre: factor de conductancia."""
        fac = {'F.Cu': np.ones(len(s.P)), 'B.Cu': np.ones(len(s.P))}
        for g in s.t:
            if not (isinstance(g, list) and g and g[0] in ('gr_line', 'gr_poly', 'zone')): continue
            if g[0] == 'zone':
                lys = (first(g, 'layer') or first(g, 'layers'))[1:]
                if not any('Mask' in l for l in lys): continue
                ly = [l for l in lys if 'Mask' in l][0]
                g = ['gr_poly', ['pts'] + find(first(first(g, 'polygon'), 'pts'), 'xy'), ['layer', ly]]
            ly = first(g, 'layer')[1]
            if ly not in ('F.Mask', 'B.Mask'): continue
            cu = 'F.Cu' if ly == 'F.Mask' else 'B.Cu'
            if g[0] == 'gr_line':
                a = np.array(list(map(float, first(g, 'start')[1:3]))); b = np.array(list(map(float, first(g, 'end')[1:3])))
                w = float(first(first(g, 'stroke'), 'width')[1]); d = b - a; L2 = d @ d + 1e-12
                tt = np.clip(((s.P - a) @ d) / L2, 0, 1); dist = np.hypot(*(s.P - (a + np.outer(tt, d))).T)
                sel = dist <= max(w, ancho) / 2; wl = max(w, ancho)
            else:
                xy = np.array([(float(p[1]), float(p[2])) for p in find(first(g, 'pts'), 'xy')])
                sel = Path(xy).contains_points(s.P)
                A = 0.5 * abs(np.dot(xy[:, 0], np.roll(xy[:, 1], 1)) - np.dot(xy[:, 1], np.roll(xy[:, 0], 1)))
                L = max(np.ptp(xy[:, 0]), np.ptp(xy[:, 1])); wl = max(A / max(L, 1e-6), 1.0)
            cuNet = np.zeros(len(s.P), bool)
            for n in nets: cuNet |= s.mascara_net(n)[0][cu].ravel()
            if sel.sum() == 0 or (sel & cuNet).sum() < 0.6 * sel.sum(): continue   # solo ventanas sobre VS
            f = 1 + ALAMBRE_MM2 / (s.tcu * wl)
            fac[cu][sel & cuNet] = np.maximum(fac[cu][sel & cuNet], f)
        return {k: v.reshape(s.ny, s.nx) for k, v in fac.items()}
    def resolver(s, net, src, sinks, amps, fac=None):
        m, links = s.mascara_net(net); N = s.nx * s.ny; ny, nx = s.ny, s.nx
        idx = {'F.Cu': np.arange(N).reshape(ny, nx), 'B.Cu': np.arange(N).reshape(ny, nx) + N}
        g0 = 1 / s.Rsq; R = []; C = []; V = []
        for ly in m:
            I = idx[ly]; F = fac[ly] if fac else np.ones((ny, nx))
            for dy, dx in ((0, 1), (1, 0)):
                a = m[ly][:ny - dy, :nx - dx] & m[ly][dy:, dx:]
                i = I[:ny - dy, :nx - dx][a]; j = I[dy:, dx:][a]
                g = g0 * 2 / (1 / F[:ny - dy, :nx - dx][a] + 1 / F[dy:, dx:][a])
                R += [i, j, i, j]; C += [i, j, j, i]; V += [g, g, -g, -g]
        both = m['F.Cu'] & m['B.Cu']
        for d in links:
            d2 = d.reshape(ny, nx) & both
            i = idx['F.Cu'][d2]; j = idx['B.Cu'][d2]; G = np.full(i.size, 1e4)
            R += [i, j, i, j]; C += [i, j, j, i]; V += [G, G, -G, -G]
        A = sp.csr_matrix((np.concatenate(V), (np.concatenate(R), np.concatenate(C))), shape=(2 * N, 2 * N))
        act = np.concatenate([m['F.Cu'].ravel(), m['B.Cu'].ravel()])
        def celdas(ref, num):
            o = np.zeros(2 * N, bool)
            for p in s.pads:
                if p['net'] == net and p['ref'] == ref and (num is None or p['num'] == num):
                    d = np.hypot(s.X - p['x'], s.Y - p['y']).ravel() <= max(p['r'], s.h)
                    o[:N] |= d & m['F.Cu'].ravel(); o[N:] |= d & m['B.Cu'].ravel()
            return o
        srcs = src if isinstance(src, list) else [src]
        cs = np.zeros(2 * N, bool)
        for k in srcs: cs |= celdas(*k)
        b = np.zeros(2 * N); b[cs] = amps / cs.sum()
        fix = np.zeros(2 * N, bool)
        for k in sinks: fix |= celdas(*k)
        fr = np.where(act & ~fix)[0]
        x = np.zeros(2 * N); x[fr] = spl.spsolve(A[fr][:, fr].tocsc(), b[fr])
        J = {}
        for ly in m:
            Vl = x[idx[ly]]; mm = m[ly]; F = fac[ly] if fac else np.ones((ny, nx))
            gx = np.zeros_like(Vl); gy = np.zeros_like(Vl)
            k = mm[:, 1:] & mm[:, :-1]; gx[:, 1:][k] = (Vl[:, 1:] - Vl[:, :-1])[k] / s.h
            k = mm[1:, :] & mm[:-1, :]; gy[1:, :][k] = (Vl[1:, :] - Vl[:-1, :])[k] / s.h
            J[ly] = np.where(mm, np.hypot(gx, gy) * F / s.Rsq, 0.0)   # A/mm (corriente total por mm de ancho)
        caida = x[cs].mean()
        return J, caida, m
    def termico(s, q, h_conv=10e-6):
        """q: W/mm2 por capa. Dos capas de cobre (donde haya) acopladas por FR-4 1.6 mm; convección por ambas caras."""
        N = s.nx * s.ny; ny, nx = s.ny, s.nx
        cuF = np.zeros((ny, nx), bool); cuB = np.zeros((ny, nx), bool)
        for z in find(s.t, 'zone'):
            for fp in find(z, 'filled_polygon'):
                ly = first(fp, 'layer')[1]
                xy = np.array([(float(p[1]), float(p[2])) for p in find(first(fp, 'pts'), 'xy')])
                msk = Path(xy).contains_points(s.P).reshape(ny, nx)
                if ly == 'F.Cu': cuF |= msk
                elif ly == 'B.Cu': cuB |= msk
        kfr = 0.3e-3; kcu = 0.39  # W/mm·K
        idx = {'F': np.arange(N).reshape(ny, nx), 'B': np.arange(N).reshape(ny, nx) + N}
        R = []; C = []; V = []
        for key, cu in (('F', cuF), ('B', cuB)):
            gs = np.where(cu, kcu * s.tcu, 0) + kfr * 0.8   # W/K por cuadro (cobre + media FR-4)
            I = idx[key]
            for dy, dx in ((0, 1), (1, 0)):
                i = I[:ny - dy, :nx - dx].ravel(); j = I[dy:, dx:].ravel()
                g = (2 / (1 / gs[:ny - dy, :nx - dx] + 1 / gs[dy:, dx:])).ravel()
                R += [i, j, i, j]; C += [i, j, j, i]; V += [g, g, -g, -g]
        gz = np.full(N, kfr / 1.6 * s.h * s.h)
        i = idx['F'].ravel(); j = idx['B'].ravel()
        R += [i, j, i, j]; C += [i, j, j, i]; V += [gz, gz, -gz, -gz]
        A = sp.csr_matrix((np.concatenate(V), (np.concatenate(R), np.concatenate(C))), shape=(2 * N, 2 * N))
        A = A + sp.diags(np.full(2 * N, h_conv * s.h * s.h))
        b = np.concatenate([q['F.Cu'].ravel(), q['B.Cu'].ravel()]) * s.h * s.h
        T = spl.spsolve(A.tocsc(), b)
        return {'F.Cu': T[:N].reshape(ny, nx), 'B.Cu': T[N:].reshape(ny, nx)}

def correr(s, fac=None):
    Jrms = {'F.Cu': np.zeros((s.ny, s.nx)), 'B.Cu': np.zeros((s.ny, s.nx))}
    filas = []
    for net, src, sinks, amps, frac, et in ESCENARIO['casos']:
        J, caida, m = s.resolver(net, src, sinks, amps, fac)
        # pérdida: J_total^2 * Rsq / F (el alambre baja la resistencia de la lámina)
        P = sum((J[ly] ** 2 * s.Rsq / (fac[ly] if fac else 1)).sum() * s.h * s.h for ly in J)
        for ly in J: Jrms[ly] += frac * (J[ly] / (fac[ly] if fac else 1)) ** 2   # carga equivalente de 1 oz
        filas.append(dict(caso=et, net=net, A=amps, frac=frac, P=P, caida_mV=caida * 1e3,
                          Jmax=max(J['F.Cu'].max(), J['B.Cu'].max()),
                          area_atn=sum(((J[ly] > LIM_OK) & (J[ly] <= LIM_ATN)).sum() for ly in J) * s.h ** 2,
                          area_roja=sum((J[ly] > LIM_ATN).sum() for ly in J) * s.h ** 2))
    Jrms = {k: np.sqrt(v) for k, v in Jrms.items()}
    q = {ly: Jrms[ly] ** 2 * s.Rsq * (fac[ly] if fac else 1) for ly in Jrms}   # (J/F)^2*R*F = J^2*R/F
    T = s.termico(q)
    return Jrms, T, filas

def dibujar_base(ax, s, ly):
    ax.add_patch(plt.Rectangle((s.bx[0], s.bx[2]), s.bx[1] - s.bx[0], s.bx[3] - s.bx[2], fc='#173b2a', ec='k', lw=1.5, zorder=0))
    for ref, x, y, l in s.fps:
        if ref in ETIQUETAS:
            ax.text(x, y, ref, fontsize=6.5, color='w', ha='center', va='center', zorder=5,
                    bbox=dict(boxstyle='round,pad=0.1', fc='k', ec='none', alpha=0.45), clip_on=True)
    ax.set_xlim(s.bx[0] - 1, s.bx[1] + 1); ax.set_ylim(s.bx[3] + 1, s.bx[2] - 1); ax.set_aspect('equal')
    ax.set_xticks([]); ax.set_yticks([])

def figura_J(s, Jrms, titulo, fn):
    cmap = ListedColormap(['#1b5e20', '#43a047', '#fdd835', '#fb8c00', '#c62828'])
    norm = BoundaryNorm([0.01, 0.5, LIM_OK, LIM_ATN, 4, 50], cmap.N)
    fig, axs = plt.subplots(1, 2, figsize=(16, 7.4))
    for ax, ly, nom in zip(axs, ['F.Cu', 'B.Cu'], ['Cara de componentes (F.Cu)', 'Cara de soldadura (B.Cu, vista desde arriba)']):
        dibujar_base(ax, s, ly)
        Jm = np.ma.masked_less(Jrms[ly], 0.01)
        im = ax.pcolormesh(s.gx, s.gy, Jm, cmap=cmap, norm=norm, shading='auto', zorder=2)
        ax.set_title(nom, fontsize=12)
    cb = fig.colorbar(im, ax=axs, orientation='horizontal', fraction=0.05, pad=0.04, spacing='uniform', ticks=[0.25, 0.9, 2.0, 3.3, 27])
    cb.ax.set_xticklabels(['< 0.5\nholgado', '0.5–1.35\nbien', '1.35–2.7\natención', '2.7–4\nzona caliente', '> 4\nconcentración\njunto a pines'])
    cb.set_label('Carga del conductor: corriente RMS por mm de ancho, equivalente a cobre de 1 oz [A/mm]  ·  referencia: 1.35 A/mm ≈ +20 °C (IPC-2221)')
    fig.suptitle(titulo, fontsize=14, weight='bold')
    plt.savefig(fn, dpi=110, bbox_inches='tight'); plt.close()

def figura_T(s, T, titulo, fn, vmax=25):
    fig, axs = plt.subplots(1, 2, figsize=(16, 7.4))
    for ax, ly, nom in zip(axs, ['F.Cu', 'B.Cu'], ['Cara de componentes (F.Cu)', 'Cara de soldadura (B.Cu, vista desde arriba)']):
        dibujar_base(ax, s, ly)
        im = ax.pcolormesh(s.gx, s.gy, T[ly], cmap='turbo', vmin=0, vmax=vmax, shading='auto', zorder=2, alpha=0.9)
        cs = ax.contour(s.gx, s.gy, T[ly], levels=[5, 10, 15, 20], colors='k', linewidths=0.6, zorder=3)
        ax.clabel(cs, fmt='%d °C', fontsize=7)
        iy, ix = np.unravel_index(np.argmax(T[ly]), T[ly].shape)
        ax.plot(s.gx[ix], s.gy[iy], 'w*', ms=14, mec='k', zorder=6)
        ax.set_title(f'{nom}  ·  máx +{T[ly].max():.1f} °C', fontsize=12)
    cb = fig.colorbar(im, ax=axs, orientation='horizontal', fraction=0.05, pad=0.04)
    cb.set_label('Calentamiento del cobre sobre el ambiente [°C]  (solo pérdidas del cobre, convección natural, sin ventilador)')
    fig.suptitle(titulo, fontsize=14, weight='bold')
    plt.savefig(fn, dpi=110, bbox_inches='tight'); plt.close()

def figura_zoom(s, pares, region, titulo, fn):
    """pares: lista de (Jrms, T, subtitulo). region: (x0,x1,y0,y1)."""
    cmap = ListedColormap(['#1b5e20', '#43a047', '#fdd835', '#fb8c00', '#c62828'])
    norm = BoundaryNorm([0.01, 0.5, LIM_OK, LIM_ATN, 4, 50], cmap.N)
    fig, axs = plt.subplots(2, len(pares), figsize=(4.6 * len(pares) + 1.8, 11))
    for k, (J, T, sub) in enumerate(pares):
        for r, (data, kind) in enumerate(((J, 'J'), (T, 'T'))):
            ax = axs[r, k]; dibujar_base(ax, s, 'B.Cu')
            if kind == 'J':
                im = ax.pcolormesh(s.gx, s.gy, np.ma.masked_less(np.maximum(data['F.Cu'], data['B.Cu']), 0.01), cmap=cmap, norm=norm, shading='auto', zorder=2)
                ax.set_title(f'{sub}\ncorriente (máx. de las dos caras)', fontsize=11)
            else:
                Tm = np.maximum(data['F.Cu'], data['B.Cu'])
                im2 = ax.pcolormesh(s.gx, s.gy, Tm, cmap='turbo', vmin=0, vmax=20, shading='auto', zorder=2)
                cs = ax.contour(s.gx, s.gy, Tm, levels=[5, 8, 10, 12, 15], colors='k', linewidths=0.6, zorder=3); ax.clabel(cs, fmt='%d °C', fontsize=8)
                ax.set_title(f'calentamiento · máx +{Tm[(s.X>=region[0])&(s.X<=region[1])&(s.Y>=region[2])&(s.Y<=region[3])].max():.1f} °C', fontsize=11)
            ax.set_xlim(region[0], region[1]); ax.set_ylim(region[3], region[2])
    fig.colorbar(im, ax=axs[0, :], fraction=0.04, spacing='uniform', ticks=[0.25, 0.9, 2.0, 3.3, 27]).ax.set_yticklabels(['< 0.5', '0.5–1.35', '1.35–2.7', '2.7–4', '> 4 A/mm'])
    fig.colorbar(im2, ax=axs[1, :], fraction=0.04, label='°C sobre ambiente')
    fig.suptitle(titulo, fontsize=14, weight='bold')
    plt.savefig(fn, dpi=110, bbox_inches='tight'); plt.close()

def figura_resumen(res, fn):
    a = res['sin_refuerzo']['filas']; b = res['con_alambre']['filas']
    et = [f['caso'] for f in a]; y = np.arange(len(et))
    fig, axs = plt.subplots(1, 2, figsize=(15, 0.55 * len(et) + 2.5))
    axs[0].barh(y + 0.2, [f['P'] for f in a], 0.4, label='cobre solo', color='#fb8c00')
    axs[0].barh(y - 0.2, [f['P'] for f in b], 0.4, label='con alambre 12 AWG', color='#43a047')
    axs[0].set_yticks(y, et); axs[0].invert_yaxis(); axs[0].set_xlabel('Pérdida en el cobre mientras ese camino conduce [W]'); axs[0].legend(); axs[0].grid(axis='x', alpha=.3)
    axs[1].barh(y + 0.2, [f['caida_mV'] for f in a], 0.4, color='#fb8c00'); axs[1].barh(y - 0.2, [f['caida_mV'] for f in b], 0.4, color='#43a047')
    axs[1].set_yticks(y, ['' for _ in et]); axs[1].invert_yaxis(); axs[1].set_xlabel('Caída de voltaje en el cobre [mV]'); axs[1].grid(axis='x', alpha=.3)
    Pa = res['sin_refuerzo']['P']; Pb = res['con_alambre']['P']
    fig.suptitle(f'Resumen por camino · pérdida total promedio: {Pa:.1f} W (cobre solo) → {Pb:.1f} W (con alambre) · '
                 f'máx. +{max(res["sin_refuerzo"]["Tmax"].values()):.0f} °C → +{max(res["con_alambre"]["Tmax"].values()):.0f} °C', fontsize=12, weight='bold')
    plt.savefig(fn, dpi=110, bbox_inches='tight'); plt.close()

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('pcb'); ap.add_argument('--oz', type=float, default=1.0)
    ap.add_argument('--salida', default='resultados'); ap.add_argument('--h', type=float, default=0.4)
    a = ap.parse_args(); os.makedirs(a.salida, exist_ok=True)
    s = Tarjeta(a.pcb, h=a.h, oz=a.oz)
    res = {}; mapas = {}
    for clave, fac, nom in (('sin_refuerzo', None, 'cobre solo'), ('con_alambre', s.refuerzo(), 'con alambre 12 AWG en las ventanas')):
        Jrms, T, filas = correr(s, fac)
        figura_J(s, Jrms, f'Densidad de corriente — {ESCENARIO["nombre"]} — {a.oz:g} oz, {nom}', f'{a.salida}/corriente_{clave}.png')
        figura_T(s, T, f'Calentamiento estimado del cobre — {a.oz:g} oz, {nom}', f'{a.salida}/temperatura_{clave}.png')
        mapas[clave] = (Jrms, T)
        res[clave] = dict(filas=filas, Tmax={k: float(v.max()) for k, v in T.items()}, P=sum(f['P'] * f['frac'] for f in filas))
    for nombre, reg in (('pierna_A', (78, 126, 44, 140)), ('pierna_B', (142, 190, 44, 140))):
        figura_zoom(s, [(*mapas['sin_refuerzo'], 'Cobre solo (1 oz)'), (*mapas['con_alambre'], 'Con alambre 12 AWG en las ventanas')],
                    reg, f'Acercamiento {nombre.replace("_", " ")}: salida VS', f'{a.salida}/zoom_{nombre}.png')
    figura_resumen(res, f'{a.salida}/resumen_caminos.png')
    json.dump(res, open(f'{a.salida}/resumen.json', 'w'), indent=1, ensure_ascii=False, default=float)
    print(json.dumps({k: dict(Tmax=v['Tmax'], P=v['P']) for k, v in res.items()}, indent=1, default=float))
    for f in res['sin_refuerzo']['filas']: print(f)
