#!/usr/bin/env python3
"""
Extensión de la auditoría: rizo en los capacitores de bus y calor de los MOSFET.
- Reparte la corriente de rizo de alta frecuencia entre los Rubycon según el cobre real + su ESR.
- Calcula pérdidas de los MOSFET (conducción, conmutación, recuperación, tiempo muerto).
- Resuelve una red térmica acoplada: tarjeta (2 capas) + uniones + disipadores + Rubycon.
Licencia: MIT
"""
import sys, math, json, argparse, os
import numpy as np
import scipy.sparse as sp, scipy.sparse.linalg as spl
from auditoria_cobre import Tarjeta, find, first, dibujar_base, LIM_OK, LIM_ATN
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.colors import BoundaryNorm, ListedColormap
from matplotlib.path import Path

# ---------- parámetros (fuente entre paréntesis) ----------
OP = dict(P=1000, Vdc=48.0, Vpri_rms=30.5, f_sw=24e3, t_dt=1e-6)   # tiempo muerto de trinchera (peor caso)
MOS = dict(Rds25=1.4e-3, k_T=1.6, tr=150e-9, tf=100e-9, Qrr=150e-9, Vf=0.9,  # IRF100P219: Rds 1.4 typ (DS); tr/tf estimados con nuestro driver
           RthJC=0.44, RthCS=0.24, R_dis=5.3, R_patas=9.0, R_pines_dis=10.0)  # DS; SK129 50.8 STS 5.3 K/W; patas y pines estimados
CAP = dict(ESR=0.030, I_nom_24k=3.4, A_cm2=25.1, beta=2.3e-3, R_patas=13.0)   # ZLH 820/100 (DS Rubycon): Z=0.026 Ω@100k/20°C; 3510 mA@105°C/100k; coef. 0.96@10k, 1.00@100k -> ~3.4 A@24k
PIERNA = {'A': dict(alto='Q1', bajo='Q2', dis_alto='HS5', dis_bajo='HS6', hf=True),
          'B': dict(alto='Q3', bajo='Q4', dis_alto='HS7', dis_bajo='HS8', hf=False)}
RUBYCON = ['C1', 'C2', 'C3', 'C4']

def corrientes():
    m = OP['Vpri_rms'] * math.sqrt(2) / OP['Vdc']; Io = OP['P'] / OP['Vpri_rms']
    Ibus_rms = math.sqrt(8 * m * Io ** 2 / (3 * math.pi))
    Idc = m * math.sqrt(2) * Io / 2; I120 = Idc / math.sqrt(2)
    Ihf = math.sqrt(max(Ibus_rms ** 2 - Idc ** 2 - I120 ** 2, 0))
    return dict(m=m, Io=Io, Ibus_rms=Ibus_rms, Idc=Idc, I120=I120, Ihf=Ihf)

def perdidas_mosfet(c):
    Io = c['Io']; Iavg = 2 * math.sqrt(2) / math.pi * Io
    Rhot = MOS['Rds25'] * MOS['k_T']
    P = {}
    for k, p in PIERNA.items():
        cond = (Io / math.sqrt(2)) ** 2 * Rhot
        if p['hf']:
            sw = 0.5 * OP['Vdc'] * Iavg * (MOS['tr'] + MOS['tf']) * OP['f_sw'] / 2      # cada MOSFET conmuta duro medio ciclo
            rr = MOS['Qrr'] * OP['Vdc'] * OP['f_sw'] / 2
            dt = MOS['Vf'] * Iavg * 2 * OP['t_dt'] * OP['f_sw'] / 2
        else: sw = rr = dt = 0.0
        for q in (p['alto'], p['bajo']):
            P[q] = dict(cond=cond, sw=sw, rr=rr, dt=dt, total=cond + sw + rr + dt)
    return P

class Circuito:
    """Varias nets en la misma cuadrícula + ramas concentradas (ESR) entre pads."""
    def __init__(s, T, nets):
        s.T = T; s.nets = nets; N = T.nx * T.ny; s.N = N
        s.m = {n: T.mascara_net(n)[0] for n in nets}
        s.links = {n: T.mascara_net(n)[1] for n in nets}
        s.off = {}; k = 0
        for n in nets:
            for ly in ('F.Cu', 'B.Cu'): s.off[(n, ly)] = k; k += N
        s.nv = k; s.extra = 0; s.R = []; s.C = []; s.V = []
        g0 = 1 / T.Rsq
        for n in nets:
            for ly in ('F.Cu', 'B.Cu'):
                mm = s.m[n][ly]; I = np.arange(N).reshape(T.ny, T.nx) + s.off[(n, ly)]
                for dy, dx in ((0, 1), (1, 0)):
                    a = mm[:T.ny - dy, :T.nx - dx] & mm[dy:, dx:]
                    s._g(I[:T.ny - dy, :T.nx - dx][a], I[dy:, dx:][a], g0)
            both = s.m[n]['F.Cu'] & s.m[n]['B.Cu']
            for d in s.links[n]:
                d2 = d.reshape(T.ny, T.nx) & both
                s._g(np.arange(N).reshape(T.ny, T.nx)[d2] + s.off[(n, 'F.Cu')], np.arange(N).reshape(T.ny, T.nx)[d2] + s.off[(n, 'B.Cu')], 1e4)
    def _g(s, i, j, g):
        g = np.broadcast_to(g, np.shape(i)).astype(float)
        s.R += [i, j, i, j]; s.C += [i, j, j, i]; s.V += [g, g, -g, -g]
    def celdas(s, net, ref, num):
        T = s.T; out = []
        for p in T.pads:
            if p['net'] == net and p['ref'] == ref and (num is None or p['num'] == num):
                d = (np.hypot(T.X - p['x'], T.Y - p['y']) <= max(p['r'], T.h))
                for ly in ('F.Cu', 'B.Cu'):
                    sel = d & s.m[net][ly]
                    out.append(np.arange(s.N).reshape(T.ny, T.nx)[sel] + s.off[(net, ly)])
        return np.concatenate(out) if out else np.array([], int)
    def nodo(s, cells):
        k = s.nv + s.extra; s.extra += 1
        s._g(cells, np.full(cells.size, k), 1e4); return k
    def resolver(s, fuentes, tierra):
        n = s.nv + s.extra
        A = sp.csr_matrix((np.concatenate(s.V), (np.concatenate(s.R), np.concatenate(s.C))), shape=(n, n))
        act = np.zeros(n, bool)
        for (net, ly), o in s.off.items(): act[o:o + s.N] = s.m[net][ly].ravel()
        act[s.nv:] = True
        b = np.zeros(n)
        for cells, I in fuentes: b[cells] += I / cells.size
        fix = np.zeros(n, bool); fix[tierra] = True
        fr = np.where(act & ~fix)[0]; x = np.zeros(n)
        x[fr] = spl.spsolve(A[fr][:, fr].tocsc(), b[fr]); s.x = x; return x
    def J(s, net):
        T = s.T; out = {}
        for ly in ('F.Cu', 'B.Cu'):
            o = s.off[(net, ly)]; Vl = s.x[o:o + s.N].reshape(T.ny, T.nx); mm = s.m[net][ly]
            gx = np.zeros_like(Vl); gy = np.zeros_like(Vl)
            k = mm[:, 1:] & mm[:, :-1]; gx[:, 1:][k] = (Vl[:, 1:] - Vl[:, :-1])[k] / T.h
            k = mm[1:, :] & mm[:-1, :]; gy[1:, :][k] = (Vl[1:, :] - Vl[:-1, :])[k] / T.h
            out[ly] = np.where(mm, np.hypot(gx, gy) / T.Rsq, 0.0)
        return out

def rizo_capacitores(T, c):
    """Lazo de alta frecuencia: Rubycon -> 48V -> MOSFET alto ... MOSFET bajo -> GND -> Rubycon."""
    res = {k: [] for k in RUBYCON}; Jsum = {n: {'F.Cu': 0, 'B.Cu': 0} for n in ('48V', 'GND')}
    for alto, bajo in (('Q1', 'Q4'), ('Q3', 'Q2')):          # semiciclo positivo / negativo (modulación híbrida)
        ci = Circuito(T, ['48V', 'GND'])
        nodos = {}
        for cap in RUBYCON:
            a = ci.nodo(ci.celdas('48V', cap, '1')); b = ci.nodo(ci.celdas('GND', cap, '2'))
            ci._g(np.array([a]), np.array([b]), 1 / CAP['ESR']); nodos[cap] = (a, b)
        src = ci.celdas('48V', alto, '2'); snk = ci.celdas('GND', bajo, '3')
        # la corriente sale del capacitor hacia el MOSFET alto y regresa por el bajo: inyectamos en el drenaje del alto (+) y fijamos el source del bajo
        x = ci.resolver([(src, -c['Ihf'])], snk)
        for cap, (a, b) in nodos.items(): res[cap].append(abs(x[a] - x[b]) / CAP['ESR'])
        for n in ('48V', 'GND'):
            J = ci.J(n)
            for ly in J: Jsum[n][ly] = Jsum[n][ly] + 0.5 * J[ly] ** 2
    Icap = {k: math.sqrt(0.5 * v[0] ** 2 + 0.5 * v[1] ** 2) for k, v in res.items()}
    return Icap, Jsum

def termico_total(T, q_cobre, Pmos, Pcap):
    """Red térmica: tarjeta (2 capas) + uniones + disipadores + Rubycon. Devuelve mapas y temperaturas."""
    ny, nx = T.ny, T.nx; N = nx * ny
    cuF = np.zeros((ny, nx), bool); cuB = np.zeros((ny, nx), bool)
    for z in find(T.t, 'zone'):
        for fp in find(z, 'filled_polygon'):
            ly = first(fp, 'layer')[1]
            xy = np.array([(float(p[1]), float(p[2])) for p in find(first(fp, 'pts'), 'xy')])
            msk = Path(xy).contains_points(T.P).reshape(ny, nx)
            if ly == 'F.Cu': cuF |= msk
            elif ly == 'B.Cu': cuB |= msk
    kfr = 0.3e-3; kcu = 0.39; hc = OP.get('h_conv', 10e-6)
    R = []; C = []; V = []
    def g(i, j, gg):
        gg = np.broadcast_to(gg, np.shape(i)).astype(float); R.extend([i, j, i, j]); C.extend([i, j, j, i]); V.extend([gg, gg, -gg, -gg])
    I = np.arange(N).reshape(ny, nx)
    for off, cu in ((0, cuF), (N, cuB)):
        gs = np.where(cu, kcu * T.tcu, 0) + kfr * 0.8
        for dy, dx in ((0, 1), (1, 0)):
            g(I[:ny - dy, :nx - dx].ravel() + off, I[dy:, dx:].ravel() + off, (2 / (1 / gs[:ny - dy, :nx - dx] + 1 / gs[dy:, dx:])).ravel())
    g(I.ravel(), I.ravel() + N, np.full(N, kfr / 1.6 * T.h ** 2))
    n = 2 * N; nombres = []; b = np.zeros(n + 64)
    def nodo(nombre):
        nonlocal n; nombres.append((nombre, n)); n += 1; return n - 1
    def celdas_ref(ref, nums=None):
        out = []
        for p in T.pads:
            if p['ref'] == ref and (nums is None or p['num'] in nums):
                d = (np.hypot(T.X - p['x'], T.Y - p['y']) <= max(p['r'], T.h)).ravel()
                out.append(np.where(d)[0]); out.append(np.where(d)[0] + N)
        return np.unique(np.concatenate(out))
    amb = []   # (nodo, conductancia a ambiente)
    for k, p in PIERNA.items():
        for q, hs in ((p['alto'], p['dis_alto']), (p['bajo'], p['dis_bajo'])):
            j = nodo(f'{q} unión'); d = nodo(f'{hs} disipador')
            b[j] += Pmos[q]['total']
            g(np.array([j]), np.array([d]), 1 / (MOS['RthJC'] + MOS['RthCS']))
            amb.append((d, 1 / MOS['R_dis']))
            cp = celdas_ref(q); g(np.full(cp.size, j), cp, 1 / MOS['R_patas'] / cp.size)
            ch = celdas_ref(hs); g(np.full(ch.size, d), ch, 1 / MOS['R_pines_dis'] / ch.size)
    for cap in RUBYCON:
        k = nodo(f'{cap} Rubycon'); b[k] += Pcap[cap]
        amb.append((k, CAP['beta'] * CAP['A_cm2']))
        cc = celdas_ref(cap); g(np.full(cc.size, k), cc, 1 / CAP['R_patas'] / cc.size)
    b = b[:n]; b[:N] += q_cobre['F.Cu'].ravel() * T.h ** 2; b[N:2 * N] += q_cobre['B.Cu'].ravel() * T.h ** 2
    A = sp.csr_matrix((np.concatenate(V), (np.concatenate(R), np.concatenate(C))), shape=(n, n))
    dg = np.full(n, 0.0); dg[:2 * N] = hc * T.h ** 2
    for k, gg in amb: dg[k] += gg
    A = A + sp.diags(dg)
    x = spl.spsolve(A.tocsc(), b)
    return {'F.Cu': x[:N].reshape(ny, nx), 'B.Cu': x[N:2 * N].reshape(ny, nx)}, {nm: x[k] for nm, k in nombres}

POS = {}
def _pos(T):
    for ref, x, y, l in T.fps: POS[ref] = (x, y)

def figuras(T, a, tag, Tmap, Tn, Icap, Pcap, Pm, Jhf, out):
    _pos(T); nom = 'con alambre 12 AWG' if tag == 'con_alambre' else 'cobre solo'
    # 1) mapa térmico completo con componentes
    fig, ax = plt.subplots(figsize=(13, 10.5))
    dibujar_base(ax, T, 'F.Cu'); Tm = np.maximum(Tmap['F.Cu'], Tmap['B.Cu'])
    im = ax.pcolormesh(T.gx, T.gy, Tm + a.tamb, cmap='turbo', vmin=a.tamb, vmax=a.tamb + 25, shading='auto', zorder=2)
    cs = ax.contour(T.gx, T.gy, Tm + a.tamb, levels=[a.tamb + d for d in (5, 10, 15, 20)], colors='k', linewidths=0.6, zorder=3)
    ax.clabel(cs, fmt='%d °C', fontsize=8)
    for nm, v in Tn.items():
        ref = nm.split()[0]; x, y = POS[ref]
        lab = {'unión': 'Tj', 'disipador': 'disip.', 'Rubycon': 'Rubycon'}[nm.split()[1]]
        dy = {'unión': -4.5, 'disipador': 4.5, 'Rubycon': 0}[nm.split()[1]]
        ax.text(x, y + dy, f'{ref} {lab}\n{a.tamb + v:.0f} °C', ha='center', va='center', fontsize=8.5, zorder=8, weight='bold',
                bbox=dict(boxstyle='round,pad=0.25', fc='white', ec='k', alpha=0.85))
    fig.colorbar(im, ax=ax, fraction=0.035, label=f'Temperatura del cobre [°C] con {a.tamb:.0f} °C de ambiente')
    ax.set_title(f'Temperatura de la tarjeta con MOSFET, Rubycon y cobre · {nom}\n'
                 f'inversor 1000 W, convección natural sin ventilador · tiempo muerto 1 µs (trinchera)', fontsize=12, weight='bold')
    plt.savefig(f'{a.salida}/temperatura_total_{tag}.png', dpi=105, bbox_inches='tight'); plt.close()
    if tag != 'sin_refuerzo': return
    # 2) lazo de rizo en los planos
    cmap = ListedColormap(['#1b5e20', '#43a047', '#fdd835', '#fb8c00', '#c62828']); norm = BoundaryNorm([0.01, 0.5, LIM_OK, LIM_ATN, 4, 50], cmap.N)
    fig, axs = plt.subplots(1, 2, figsize=(16, 7.4))
    for ax, ly, t in zip(axs, ('F.Cu', 'B.Cu'), ('Cara de componentes (F.Cu)', 'Cara de soldadura (B.Cu, vista desde arriba)')):
        dibujar_base(ax, T, ly); im = ax.pcolormesh(T.gx, T.gy, np.ma.masked_less(Jhf[ly], 0.01), cmap=cmap, norm=norm, shading='auto', zorder=2); ax.set_title(t)
    cb = fig.colorbar(im, ax=axs, orientation='horizontal', fraction=0.05, pad=0.04, spacing='uniform', ticks=[0.25, 0.9, 2.0, 3.3, 27])
    cb.ax.set_xticklabels(['< 0.5', '0.5–1.35', '1.35–2.7', '2.7–4', '> 4']); cb.set_label('Corriente de rizo de 24 kHz en los planos 48 V y GND [A/mm, RMS]')
    fig.suptitle(f'Lazo de rizo: Rubycon ↔ MOSFET ({out["corrientes"]["Ihf"]:.1f} A RMS de alta frecuencia que la batería no entrega)', fontsize=13, weight='bold')
    plt.savefig(f'{a.salida}/rizo_planos.png', dpi=105, bbox_inches='tight'); plt.close()
    # 3) capacitores
    fig, axs = plt.subplots(1, 2, figsize=(13, 4.8))
    ks = RUBYCON; x = np.arange(len(ks))
    axs[0].bar(x, [Icap[k] for k in ks], color=['#1565c0', '#1565c0', '#6a1b9a', '#6a1b9a'])
    axs[0].axhline(CAP['I_nom_24k'], color='r', ls='--'); axs[0].text(-0.45, CAP['I_nom_24k'] + 0.45, f'nominal del fabricante a 105 °C\n(~{CAP["I_nom_24k"]} A a 24 kHz)', ha='left', color='r', fontsize=9)
    axs[0].set_xticks(x, [f'{k}\n(pierna {"A" if k in ("C1", "C2") else "B"})' for k in ks]); axs[0].set_ylabel('Rizo RMS de alta frecuencia [A]'); axs[0].set_ylim(0, 4.2)
    for i, k in enumerate(ks): axs[0].text(i, Icap[k] / 2, f'{Icap[k]:.2f} A\n{100 * Icap[k] / CAP["I_nom_24k"]:.0f} %', ha='center', color='w', weight='bold')
    axs[0].set_title('Corriente de rizo por Rubycon (reparto por cobre + ESR)')
    tc = [a.tamb + Tn[f'{k} Rubycon'] for k in ks]
    axs[1].bar(x, tc, color='#ef6c00'); axs[1].axhline(105, color='r', ls='--'); axs[1].text(3.45, 101, 'máximo 105 °C', ha='right', color='r')
    for i, v in enumerate(tc): axs[1].text(i, v + 1.5, f'{v:.0f} °C\n{Pcap[ks[i]]:.2f} W', ha='center', fontsize=9)
    axs[1].set_xticks(x, ks); axs[1].set_ylim(0, 115); axs[1].set_ylabel('°C'); axs[1].set_title(f'Temperatura del Rubycon con {a.tamb:.0f} °C de ambiente')
    plt.tight_layout(); plt.savefig(f'{a.salida}/capacitores.png', dpi=105, bbox_inches='tight'); plt.close()
    # 4) balance de calor
    fig, axs = plt.subplots(1, 2, figsize=(14, 5), gridspec_kw=dict(width_ratios=[1.5, 1]))
    qs = ['Q1', 'Q2', 'Q3', 'Q4']; bot = np.zeros(4)
    for comp, col, et in (('cond', '#1565c0', 'conducción'), ('sw', '#c62828', 'conmutación'), ('dt', '#fb8c00', 'tiempo muerto (diodo)'), ('rr', '#8e24aa', 'recuperación del diodo')):
        v = np.array([Pm[q][comp] for q in qs]); axs[0].bar(qs, v, bottom=bot, color=col, label=et); bot += v
    for i, q in enumerate(qs): axs[0].text(i, bot[i] + 0.08, f'{bot[i]:.1f} W\nTj {a.tamb + Tn[q + " unión"]:.0f} °C', ha='center', fontsize=9)
    axs[0].set_ylim(0, bot.max() * 1.35); axs[0].set_ylabel('W'); axs[0].legend(loc='upper right', fontsize=9)
    axs[0].set_title('Pérdidas por MOSFET (pierna A conmuta a 24 kHz, pierna B a 60 Hz)')
    partes = [sum(Pm[q]['total'] for q in qs), out['Pcobre'], sum(Pcap.values())]
    axs[1].pie(partes, labels=[f'MOSFET\n{partes[0]:.1f} W', f'Cobre\n{partes[1]:.1f} W', f'Rubycon\n{partes[2]:.1f} W'], colors=['#c62828', '#fb8c00', '#1565c0'], startangle=90, wedgeprops=dict(width=0.45))
    axs[1].set_title(f'¿De dónde sale el calor? total {sum(partes):.1f} W ({sum(partes) / 10:.1f} % de 1000 W)')
    plt.tight_layout(); plt.savefig(f'{a.salida}/balance_calor.png', dpi=105, bbox_inches='tight'); plt.close()

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('pcb'); ap.add_argument('--salida', default='resultados')
    ap.add_argument('--alambre', action='store_true'); ap.add_argument('--tamb', type=float, default=40)
    a = ap.parse_args(); os.makedirs(a.salida, exist_ok=True)
    import auditoria_cobre as AC
    T = Tarjeta(a.pcb, h=0.4)
    c = corrientes(); Pm = perdidas_mosfet(c)
    Icap, Jhf = rizo_capacitores(T, c)
    Pcap = {k: v ** 2 * CAP['ESR'] for k, v in Icap.items()}
    # cobre: caminos de salida + batería (DC + 120 Hz), más el lazo HF de capacitores
    Ibat = math.sqrt(c['Idc'] ** 2 + (0.9 * c['I120']) ** 2)
    AC.ESCENARIO['casos'] = [x for x in AC.ESCENARIO['casos'] if x[0] in ('VS1', 'VS2')] + [
        ('48V', ('Q1', '2'), [('J5', None), ('J6', None)], Ibat, 0.5, 'POS batería → Q1'),
        ('48V', ('Q3', '2'), [('J5', None), ('J6', None)], Ibat, 0.5, 'POS batería → Q3'),
        ('GND', ('Q2', '3'), [('R16', '2'), ('R28', '2')], Ibat, 0.5, 'NEG Q2 → shunts'),
        ('GND', ('Q4', '3'), [('R16', '2'), ('R28', '2')], Ibat, 0.5, 'NEG Q4 → shunts'),
        ('GND_IN', [('R16', '1'), ('R28', '1')], [('J1', None), ('J2', None)], Ibat, 1.0, 'GND_IN shunts → Faston')]
    fac = T.refuerzo() if a.alambre else None
    Jrms, _, filas = AC.correr(T, fac)
    J2 = {ly: Jrms[ly] ** 2 for ly in Jrms}
    for nn in ('48V', 'GND'):
        for ly in J2: J2[ly] = J2[ly] + Jhf[nn][ly]      # RMS de frecuencias distintas se suman en cuadratura
    q = {ly: J2[ly] * T.Rsq * (fac[ly] if fac else 1) for ly in J2}
    Tmap, Tn = termico_total(T, q, Pm, Pcap)
    tag = 'con_alambre' if a.alambre else 'sin_refuerzo'
    out = dict(corrientes=c, Ibat=Ibat, Icap=Icap, Pcap=Pcap, Pmos=Pm, Tnodos=Tn, Tmax={k: float(v.max()) for k, v in Tmap.items()},
               Pcobre=sum(float((q[ly]).sum()) * T.h ** 2 for ly in q))
    json.dump(out, open(f'{a.salida}/termica_{tag}.json', 'w'), indent=1, default=float, ensure_ascii=False)
    Jhf_tot = {ly: np.sqrt(Jhf['48V'][ly] + Jhf['GND'][ly]) for ly in ('F.Cu', 'B.Cu')}
    figuras(T, a, tag, Tmap, Tn, Icap, Pcap, Pm, Jhf_tot, out)
    print(json.dumps({k: out[k] for k in ('corrientes', 'Ibat', 'Icap', 'Pcap', 'Tmax', 'Pcobre')}, indent=1, default=float))
    for k, v in Pm.items(): print(k, {a: round(b, 2) for a, b in v.items()})
    for k, v in Tn.items(): print(f'{k}: +{v:.1f} °C  → {a.tamb + v:.0f} °C con {a.tamb:.0f} °C ambiente')
