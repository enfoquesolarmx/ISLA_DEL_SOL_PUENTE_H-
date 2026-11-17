#!/usr/bin/env python3
"""
Auditoría de conmutación del layout — Isla del Sol, Puente H
=============================================================
La hermana de `auditoria_cobre.py`, que resuelve corriente DC y calentamiento.
Ésta no mira la corriente: mira **cómo está repartida la tarjeta**.

POR QUÉ EXISTE
    Un trazado puede pasar todas las pruebas de corriente y de temperatura y
    seguir siendo pésimo conmutando. La auditoría de cobre toma el trazado
    como viene y pregunta "¿aguanta la corriente sin calentarse?". Esta
    pregunta otra cosa: "¿el nodo que brinca 48 V a 24 kHz está donde debe,
    y del tamaño que debe?".

    Se escribió después de que una oportunidad de optimización —los dos
    MOSFET de la misma pierna a 74 mm uno del otro, con el nodo conmutado
    ocupando un tercio de la cara que ve al chasis— pasara inadvertida en
    una tarjeta por lo demás correcta, que aprobó la auditoría de cobre, el
    DRC y la térmica. La vio una persona mirando una imagen mientras cenaba.
    Nada estaba mal; simplemente podía estar mejor, y eso no se puede dejar
    a que alguien lo note de casualidad.

    IMPORTANTE SOBRE EL LENGUAJE: aquí no hay fallas. Las metas las pone
    esta herramienta, no el circuito. "FUERA META" significa que hay margen
    de mejora medible, no que la tarjeta esté mal.

PRUEBA DE ACEPTACIÓN DE ESTA HERRAMIENTA
    Corrida sobre la REV C debe señalar sola M2 y M3. Si no los señala, la
    herramienta no sirve y hay que arreglarla antes de confiar en ella.

QUÉ MIDE
    M1  Geometría del nodo conmutado. INFORMATIVA, sin veredicto.
    M2  Reparto de área de cobre por net y por capa (conmutado contra quieto).
    M3  Capacitancia al chasis del nodo conmutado y corriente de modo común.
    M4  Solapamiento del bus entre capas (sándwich 48V/GND).
    M4b Encimado del nodo conmutado sobre cobre quieto, pesado por área.
    M5  Qué plano queda debajo de cada red de compuerta.
    M6  Estimación del lazo de conmutación y del sobretiro al apagar,
        con el nH/mm que sale de M4b y de M4.
    M7  Distancia del desacoplo más cercano a cada medio puente.

CORRECCIÓN DEL 17 DE NOVIEMBRE DE 2026
    La primera versión tenía dos errores, los dos del mismo tipo: medir una
    distancia y hacerla pasar por lo que de verdad importa.

    M1 marcaba fuera de meta los 74 mm entre los MOSFET de una pierna, con un
    criterio de 25 mm tomado de una regla genérica. Esa regla supone UN borne
    de salida por pierna y NINGÚN encimado entre capas. Aquí hay DOS bornes,
    uno pegado a cada MOSFET (J27/J24 en la pierna A, J22/J23 en la B), que se
    juntan afuera en el transformador: la corriente de carga sale por el
    extremo donde está su MOSFET y nunca cruza la tarjeta. Y sí hay encimado.
    La distancia era un *proxy* de la inductancia del lazo, y aquí el proxy
    miente. M1 se quedó sin veredicto: informa y dice quién juzga cada cosa.

    M6 estimaba el lazo a 1.0 nH/mm, que es el valor de un conductor con el
    regreso lejos. Medido (M4b, nueva): el 46 % del nodo conmutado tiene 48V o
    GND directamente encima, a 1.6 mm. Ahí la inductancia por milímetro es de
    0.07 a 0.20 nH, de 5 a 15 veces menos. El lazo pasó de ~170 nH estimados a
    100-110 nH, y el margen al límite del MOSFET de negativo a +6 V.

    La lección, que vale para cualquier medida que se agregue después: una
    distancia sólo sirve de criterio si se sabe de qué es proxy y se verificó
    que el proxy aplica a ESTE layout.

Uso:  python3 auditoria_conmutacion.py TARJETA.kicad_pcb [--json salida.json]
                                       [--separacion-chasis 10] [--flanco 50]
Licencia: MIT (ver LICENSE.md del repositorio). Escrito desde cero.
"""
import re, sys, math, json, argparse
from shapely.geometry import Polygon
from shapely.ops import unary_union

# ============================ CONFIGURACIÓN =================================
# Las piernas del puente: (nombre, MOSFET alto, MOSFET bajo, nodo conmutado)
PIERNAS = [("A", "Q1", "Q2", "VS1"), ("B", "Q3", "Q4", "VS2")]
# Cada pierna saca su nodo conmutado por DOS bornes, uno pegado a cada MOSFET,
# y los dos cables se juntan en el transformador. Por eso la corriente de carga
# no cruza la tarjeta: entra y sale por el extremo donde esta su MOSFET.
BORNES = {"A": ("J27", "J24"), "B": ("J22", "J23")}
RIELES_QUIETOS = ("48V", "GND", "GND_IN")
CARA_AL_CHASIS = "B.Cu"      # la cara de soldadura es la que ve al chasis
CAP_DESACOPLO  = ("C1", "C2", "C3", "C4", "C9", "C10")

# Criterios. Cada uno trae su razón; cambiarlos exige cambiar la razón.
# Fraccion minima del nodo conmutado que debe tener cobre quieto encimado. Es
# lo que vuelve barata la distancia entre los MOSFET: con el retorno a 1.6 mm
# los campos se cancelan y la inductancia por milimetro cae de ~1 a ~0.1 nH.
CRIT_ENCIMADO = 0.50
CRIT_SOLAPE_BUS   = 0.60   # fracción mínima del bus que debe ser sándwich de dos capas
CRIT_FRAC_CONMUTADO = 0.15 # fracción máxima de una cara que puede ser nodo conmutado
CRIT_CM_MA        = 1.0    # mA de pico de modo común que se considera aceptable
CORRIENTE_A       = 21.0   # A de bus a 1000 W / 48 V, para el sobretiro
V_BUS             = 48.0
V_MOSFET          = 100.0  # IRF100P219

# ============================ LECTOR S-EXPR =================================
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
def num(x):
    try: return float(x)
    except Exception: return 0.0

def net_de(nodo):
    """Nombre del net, aguantando los dos formatos de KiCad.

    hasta 20241229 :  (net 2 "48V")   o   (net 2) + (net_name "48V")
    desde 20260206 :  (net "48V")

    Si esto devuelve None donde debería haber un net, la auditoría entera
    miente en silencio: mide áreas de nets que cree vacíos. Por eso vive
    aquí, en un solo lugar, y no repartido por el código.
    """
    nn = first(nodo, 'net_name')
    if nn and len(nn) > 1: return nn[1]
    n = first(nodo, 'net')
    if not n: return None
    if len(n) > 2: return n[2]                                   # (net 2 "48V")
    if len(n) > 1 and not re.fullmatch(r'\d+', str(n[1])):
        return n[1]                                              # (net "48V")
    return None

def gira(dx, dy, grados):
    """KiCad rota los pads con el footprint, en sentido horario."""
    a = math.radians(-grados)
    return dx * math.cos(a) - dy * math.sin(a), dx * math.sin(a) + dy * math.cos(a)

# ============================ LECTURA DE LA TARJETA =========================
class Tarjeta:
    def __init__(s, fn):
        s.arbol = parse(open(fn, encoding='utf-8').read())
        s.piezas = {}      # ref -> {pos, rot, valor, pads:{num:(x,y,net)}}
        s.cobre  = {}      # (net, capa) -> [Polygon, ...]  (cobre REAL, con huecos)
        s._piezas(); s._cobre()

    def _piezas(s):
        for fp in find(s.arbol, 'footprint'):
            at = first(fp, 'at')
            if not at: continue
            x, y = num(at[1]), num(at[2])
            rot = num(at[3]) if len(at) > 3 else 0.0
            ref = val = None
            for p in find(fp, 'property'):
                if len(p) > 2 and p[1] == 'Reference': ref = p[2]
                if len(p) > 2 and p[1] == 'Value':     val = p[2]
            if not ref: continue
            pads = {}
            for pad in find(fp, 'pad'):
                pat = first(pad, 'at')
                if not pat: continue
                dx, dy = gira(num(pat[1]), num(pat[2]), rot)
                pads.setdefault(pad[1], []).append((x + dx, y + dy, net_de(pad)))
            s.piezas[ref] = dict(pos=(x, y), rot=rot, valor=val, pads=pads)

    def _cobre(s):
        """Usa filled_polygon: es el cobre que queda DESPUÉS de los huecos."""
        for z in find(s.arbol, 'zone'):
            net = net_de(z) or ''
            for fp in find(z, 'filled_polygon'):
                lay = first(fp, 'layer')
                capa = lay[1] if lay and len(lay) > 1 else '?'
                pts = first(fp, 'pts')
                if not pts: continue
                P = [(num(c[1]), num(c[2])) for c in find(pts, 'xy')]
                if len(P) < 3: continue
                try:
                    g = Polygon(P)
                    if not g.is_valid: g = g.buffer(0)
                    if g.area > 0: s.cobre.setdefault((net, capa), []).append(g)
                except Exception:
                    pass

    def area(s, net, capa):
        return sum(g.area for g in s.cobre.get((net, capa), []))
    def union(s, net, capa):
        gs = s.cobre.get((net, capa), [])
        return unary_union(gs) if gs else None
    def net_bajo(s, pt, capa):
        """Qué net tiene cobre bajo un punto, en esa capa."""
        from shapely.geometry import Point
        p = Point(pt)
        for (net, c), gs in s.cobre.items():
            if c != capa: continue
            for g in gs:
                if g.contains(p): return net
        return None
    def pad(s, ref, numero):
        pz = s.piezas.get(ref)
        if not pz: return None
        v = pz['pads'].get(numero)
        return v[0] if v else None

# ============================ INFORME =======================================
class Informe:
    def __init__(s): s.lineas = []; s.fallas = 0; s.avisos = 0; s.datos = {}
    def tit(s, t):
        s.lineas.append(""); s.lineas.append("=" * 74)
        s.lineas.append(t); s.lineas.append("=" * 74)
    def di(s, t=""): s.lineas.append(t)
    def juicio(s, estado, t):
        # No son veredictos sobre una tarjeta que funciona: son metas de
        # optimización que la revisión auditada todavía no alcanza.
        marca = {"OK": "  EN META    ", "ATN": "  CERCA      ", "MAL": "  FUERA META "}[estado]
        if estado == "MAL": s.fallas += 1
        if estado == "ATN": s.avisos += 1
        s.lineas.append(marca + t)
    def imprime(s): print("\n".join(s.lineas))

# ============================ MEDIDAS =======================================
def m1_parejas(T, I):
    I.tit("M1. Geometría del nodo conmutado (INFORMATIVA, sin veredicto)")
    I.di("Tres distancias, y esta medida NO juzga ninguna. Las reporta y dice")
    I.di("quién sí las juzga, porque cada una cuesta algo distinto:")
    I.di()
    I.di("  (a) Entre los dos MOSFET de la pierna. Es el tramo del nodo conmutado")
    I.di("      por donde se cierra la corriente que CONMUTA. Cuesta inductancia,")
    I.di("      pero cuánta depende de si va encimada sobre cobre quieto. Quien la")
    I.di("      juzga es M4b (encimado) y M6 (el lazo), no esta distancia.")
    I.di("  (b) De cada MOSFET a su borne de salida. Es el camino de la corriente")
    I.di("      de CARGA, y cada pierna tiene DOS bornes, uno pegado a cada")
    I.di("      MOSFET, que se juntan afuera en el transformador: la corriente de")
    I.di("      carga no cruza la tarjeta. Cuesta resistencia y calor, y quien lo")
    I.di("      juzga es auditoria_cobre.py midiendo el cobre real, no la línea")
    I.di("      recta entre dos centros.")
    I.di("  (c) El desbalance entre las dos mitades. Importa, pero tampoco se")
    I.di("      juzga aquí: un pad puede estar corrido a propósito para abrirle")
    I.di("      cobre al nodo, y entonces la línea recta se alarga mientras el")
    I.di("      cobre se ensancha. De nuevo: lo dice el cobre, no el centro.")
    I.di()
    I.di("  Esta medida existía antes con un criterio de 25 mm heredado de una")
    I.di("  regla genérica que supone un solo borne por pierna y ningún encimado.")
    I.di("  Aquí hay dos bornes y sí hay encimado, así que la regla no aplica y")
    I.di("  marcaba fuera de meta un layout que está bien. Se quitó el veredicto.")
    I.di()
    res = {}
    for nombre, alto, bajo, vs in PIERNAS:
        a = T.piezas.get(alto); b = T.piezas.get(bajo)
        if not (a and b):
            I.di(f"    pierna {nombre}: no encontré {alto} o {bajo}"); continue
        d = math.dist(a['pos'], b['pos'])
        I.di(f"    pierna {nombre}: (a) {alto} a {bajo} = {d:.1f} mm")
        salidas = {}
        for ref, pad in ((alto, '3'), (bajo, '2')):
            p = T.pad(ref, pad)
            if not p: continue
            opciones = []
            for bn in BORNES.get(nombre, ()):
                pz = T.piezas.get(bn)
                if not pz: continue
                opciones.append((bn, math.dist(p[:2], pz['pos'])))
            if not opciones: continue
            opciones.sort(key=lambda t: t[1])
            cerca, lejos = opciones[0], (opciones[1] if len(opciones) > 1 else None)
            salidas[ref] = {b_: round(d_, 1) for b_, d_ in opciones}
            extra = f"   (el otro, {lejos[0]}, a {lejos[1]:.1f} mm)" if lejos else ""
            I.di(f"                (b) {ref} pad {pad} -> {cerca[0]} a {cerca[1]:.1f} mm{extra}")
        if len(salidas) == 2:
            v = [min(d.values()) for d in salidas.values()]
            des = abs(v[0] - v[1]) / max(v) * 100
            I.di(f"                (c) desbalance de la línea recta: {des:.0f} % "
                 f"({v[0]:.1f} contra {v[1]:.1f} mm)")
            res[nombre] = dict(entre_mosfet=round(d, 1), salidas=salidas,
                               desbalance_recta_pct=round(des, 1))
        I.di()
    I.datos['geometria_nodo'] = res
    return res

def m4b_encimado(T, I):
    """Cuánto del nodo conmutado tiene cobre quieto directamente encima."""
    I.tit("M4b. Encimado del nodo conmutado sobre cobre quieto")
    I.di("Si el nodo conmutado va en una cara y el riel quieto justo encima en la")
    I.di("otra, la ida y el regreso quedan a 1.6 mm (el grosor del FR-4) y los")
    I.di("campos se cancelan. Eso es lo que vuelve barata la distancia entre los")
    I.di("MOSFET: la inductancia por milímetro cae de ~1 nH a ~0.1 nH, de 5 a 15")
    I.di("veces menos. Es la medida que le da el número a M6.")
    I.di()
    I.di("Se pesa POR ÁREA, no por cara. Una cara con poco nodo conmutado y mal")
    I.di("encimada no vale lo mismo que la cara donde vive el nodo: juzgar por la")
    I.di("peor cara castiga a la que casi no lleva corriente.")
    I.di()
    quietos = {}
    for cara in ('F.Cu', 'B.Cu'):
        u = unary_union([g for (n, c), gs in T.cobre.items()
                         if c == cara and n in RIELES_QUIETOS for g in gs])
        quietos[cara] = u
    frac = {}
    for _, _, _, vs in PIERNAS:
        tot = enc = 0.0
        for cara, otra in (('B.Cu', 'F.Cu'), ('F.Cu', 'B.Cu')):
            u = T.union(vs, cara)
            q = quietos.get(otra)
            if u is None or u.area < 1: continue
            e = u.intersection(q).area if (q is not None and not q.is_empty) else 0.0
            tot += u.area; enc += e
            I.di(f"    {vs} en {cara}: {u.area:7.0f} mm², con cobre quieto encima "
                 f"{e:7.0f} mm² = {e/u.area*100:3.0f} %")
        if tot > 0:
            f = enc / tot
            frac[vs] = f
            I.di(f"    {vs} PESADO POR ÁREA: {enc:.0f} de {tot:.0f} mm² = {f*100:.0f} %")
            I.di()
    if frac:
        peor = min(frac.values())
        est = "OK" if peor >= CRIT_ENCIMADO else ("ATN" if peor >= CRIT_ENCIMADO*0.7 else "MAL")
        I.juicio(est, f"peor nodo conmutado: {peor*100:.0f} % encimado "
                      f"(criterio: >= {CRIT_ENCIMADO*100:.0f} %)")
        I.di()
        I.di("    Lo que falta para la meta no se gana ruteando más ancho: se gana")
        I.di("    poniendo 48V o GND en la cara opuesta justo donde el nodo")
        I.di("    conmutado todavía corre destapado.")
    I.datos['encimado'] = frac
    return frac

def m2_reparto(T, I):
    I.tit("M2. Reparto de área de cobre por net y por capa")
    I.di("Regla: el nodo conmutado se hace lo más CHICO que la corriente y el")
    I.di("térmico permitan; los rieles quietos se hacen GRANDES. Si un nodo")
    I.di("conmutado tiene más cobre que un riel quieto en la misma capa, el")
    I.di("reparto está invertido.")
    I.di()
    capas = sorted({c for (_, c) in T.cobre})
    nets = sorted({n for (n, _) in T.cobre if n})
    conmutados = [vs for _, _, _, vs in PIERNAS]
    I.di(f"    {'NET':<10}" + "".join(f"{c:>12}" for c in capas) + "   tipo")
    tabla = {}
    for n in nets:
        fila = [T.area(n, c) for c in capas]
        tabla[n] = dict(zip(capas, fila))
        tipo = "CONMUTADO" if n in conmutados else ("quieto" if n in RIELES_QUIETOS else "")
        I.di(f"    {n:<10}" + "".join(f"{v:>12.0f}" for v in fila) + f"   {tipo}")
    I.di("    (mm² de cobre real, ya descontados los huecos)")
    I.di()
    for capa in capas:
        if not capa.endswith('.Cu'): continue
        peor_q = max((T.area(r, capa) for r in RIELES_QUIETOS), default=0)
        for vs in conmutados:
            a = T.area(vs, capa)
            if a <= 0: continue
            if a > peor_q:
                I.juicio("MAL", f"{capa}: {vs} (conmutado) tiene {a:.0f} mm² y el riel quieto "
                                f"más grande sólo {peor_q:.0f} mm². Reparto invertido.")
            elif a > 0.5 * peor_q:
                I.juicio("ATN", f"{capa}: {vs} tiene {a:.0f} mm², más de la mitad del riel "
                                f"quieto más grande ({peor_q:.0f} mm²).")
            else:
                I.juicio("OK", f"{capa}: {vs} = {a:.0f} mm² contra {peor_q:.0f} mm² del riel quieto.")
    # Qué fracción de cada capa se la lleva el nodo conmutado. Un nodo puede
    # ser más chico que GND y aun así ocupar un tercio de la cara: eso es
    # superficie de dv/dt que no debería estar ahí.
    I.di()
    for capa in capas:
        if not capa.endswith('.Cu'): continue
        total = sum(T.area(n, capa) for n in nets)
        if total <= 0: continue
        conm = sum(T.area(vs, capa) for vs in conmutados)
        f = conm / total
        est = "OK" if f <= CRIT_FRAC_CONMUTADO else ("ATN" if f <= 2 * CRIT_FRAC_CONMUTADO else "MAL")
        I.juicio(est, f"{capa}: el nodo conmutado ocupa {conm:.0f} mm² de {total:.0f} mm² "
                      f"= {f*100:.0f} % de la cara (criterio: <= {CRIT_FRAC_CONMUTADO*100:.0f} %)")
    I.datos['fraccion_conmutado'] = {
        c: (sum(T.area(vs, c) for vs in conmutados) / max(sum(T.area(n, c) for n in nets), 1e-9))
        for c in capas if c.endswith('.Cu')}

    # el riel quieto que desaparece de una capa
    I.di()
    for r in RIELES_QUIETOS:
        for capa in capas:
            if not capa.endswith('.Cu'): continue
            otra = [c for c in capas if c.endswith('.Cu') and c != capa]
            a = T.area(r, capa); ao = max((T.area(r, c) for c in otra), default=0)
            if ao > 500 and a < 0.05 * ao:
                I.juicio("MAL", f"{r} tiene {ao:.0f} mm² en la otra cara pero sólo {a:.0f} mm² "
                                f"en {capa}: el bus no es sándwich, es una sola cara.")
    I.datos['area_mm2'] = tabla
    return tabla

def m3_chasis(T, I, sep_mm, flanco_ns):
    I.tit("M3. Capacitancia al chasis del nodo conmutado")
    I.di(f"El cobre de la cara {CARA_AL_CHASIS} ve al chasis. Si ahí vive el nodo que")
    I.di("brinca 48 V, esa área es un capacitor que inyecta corriente de modo común")
    I.di("en el chasis cada flanco. Es la vía principal de ruido del aparato y la")
    I.di("que decide qué tan difícil será la corriente de fuga (DC-015).")
    I.di()
    e0 = 8.854e-12
    dvdt = V_BUS / (flanco_ns * 1e-9)
    tot = 0.0; det = {}
    for _, _, _, vs in PIERNAS:
        a = T.area(vs, CARA_AL_CHASIS)
        tot += a; det[vs] = a
    C = e0 * (tot * 1e-6) / (sep_mm * 1e-3)
    icm = C * dvdt * 1e3
    for vs, a in det.items():
        I.di(f"    {vs} en {CARA_AL_CHASIS}: {a:.0f} mm²")
    I.di(f"    total conmutado: {tot:.0f} mm²")
    I.di(f"    separación al chasis supuesta: {sep_mm:.0f} mm ; flanco: {flanco_ns:.0f} ns "
         f"(dv/dt = {V_BUS/flanco_ns:.2f} V/ns)")
    I.di(f"    C = {C*1e12:.2f} pF  ->  {icm:.2f} mA de pico de modo común")
    est = "OK" if icm <= CRIT_CM_MA else ("ATN" if icm <= 3 * CRIT_CM_MA else "MAL")
    I.juicio(est, f"{icm:.2f} mA de pico (criterio: <= {CRIT_CM_MA:.1f} mA)")
    I.datos['chasis'] = dict(area_mm2=tot, C_pF=C*1e12, i_cm_mA=icm,
                             separacion_mm=sep_mm, flanco_ns=flanco_ns)

def m4_sandwich(T, I):
    I.tit("M4. Solapamiento del bus entre capas (sándwich 48V / GND)")
    I.di("La inductancia del lazo se desploma cuando la ida y el regreso van")
    I.di("encimados en capas opuestas: sus campos se cancelan. Si el 48 V vive")
    I.di("en una cara y GND en la otra pero no se traslapan, no hay cancelación.")
    I.di()
    a_f = T.union('48V', 'F.Cu'); g_b = T.union('GND', 'B.Cu')
    a_b = T.union('48V', 'B.Cu'); g_f = T.union('GND', 'F.Cu')
    frac = {}
    for nom, uno, otro in (("48V(F.Cu) bajo GND(B.Cu)", a_f, g_b),
                           ("48V(B.Cu) bajo GND(F.Cu)", a_b, g_f)):
        if uno is None or otro is None or uno.area == 0:
            I.di(f"    {nom}: sin cobre que comparar"); continue
        inter = uno.intersection(otro).area
        f = inter / uno.area
        frac[nom] = f
        I.di(f"    {nom}: {inter:.0f} mm² de {uno.area:.0f} mm²  ->  {f*100:.0f} % encimado")
    if frac:
        mejor = max(frac.values())
        est = "OK" if mejor >= CRIT_SOLAPE_BUS else ("ATN" if mejor >= 0.3 else "MAL")
        I.juicio(est, f"mejor solapamiento del bus: {mejor*100:.0f} % "
                      f"(criterio: >= {CRIT_SOLAPE_BUS*100:.0f} %)")
    I.datos['sandwich_bus'] = frac

def m5_compuertas(T, I):
    I.tit("M5. Qué plano queda debajo de cada red de compuerta")
    I.di("El lazo de compuerta de cada MOSFET se cierra por su propio SOURCE. El")
    I.di("plano debajo de esa red debe ser el net de ese source. Si debajo hay")
    I.di("OTRO nodo conmutado, ese nodo le acopla dv/dt al gate: es el camino")
    I.di("clásico al encendido parásito y a la conducción cruzada.")
    I.di()
    conmutados = [vs for _, _, _, vs in PIERNAS]
    for _, alto, bajo, vs in PIERNAS:
        for ref in (alto, bajo):
            pz = T.piezas.get(ref)
            if not pz: continue
            src = None
            p3 = pz['pads'].get('3')
            if p3: src = p3[0][2]
            gate_net = None
            p1 = pz['pads'].get('1')
            if p1: gate_net = p1[0][2]
            I.di(f"    {ref}: source = {src} ; gate = {gate_net}")
            if not gate_net: continue
            # piezas que tocan la net del gate = su red de compuerta
            for r2, pz2 in sorted(T.piezas.items()):
                if r2 == ref: continue
                toca = any(n == gate_net for lst in pz2['pads'].values() for (_, _, n) in lst)
                if not toca: continue
                for capa in ('F.Cu', 'B.Cu'):
                    bajo_net = T.net_bajo(pz2['pos'], capa)
                    if bajo_net is None: continue
                    if bajo_net in conmutados and bajo_net != src:
                        I.juicio("MAL", f"{ref}: su pieza de compuerta {r2} está sobre el plano "
                                        f"{bajo_net} en {capa}, que es un nodo conmutado AJENO "
                                        f"(su referencia es {src}).")
                    elif bajo_net == src:
                        I.juicio("OK", f"{ref}: {r2} sobre {bajo_net} en {capa} (su propia referencia).")
                    else:
                        I.juicio("ATN", f"{ref}: {r2} sobre {bajo_net} en {capa}, y su referencia "
                                        f"es {src}.")
            I.di()

# Inductancia por milímetro de un par encimado en FR-4 de 1.6 mm, según el
# ancho efectivo del par (placa paralela, L/l = mu0*h/w):
#   10 mm -> 0.201 nH/mm ; 20 mm -> 0.101 ; 30 mm -> 0.067
# Y de un conductor cuyo regreso queda lejos: ~1.0 nH/mm. La diferencia entre
# los dos casos es de 5 a 15 veces, así que suponer el malo cuando el bueno es
# el real exagera el lazo en un orden de magnitud. Por eso M6 ya no supone:
# usa el encimado que midió M4b.
NH_MM_ENCIMADO_OPT = 0.067   # par ancho, 30 mm
NH_MM_ENCIMADO_PES = 0.201   # par angosto, 10 mm
NH_MM_SUELTO       = 1.000   # regreso lejano

def _banda_nh_mm(frac):
    """nH/mm de un tramo del que una fracción `frac` va encimada."""
    f = max(0.0, min(1.0, frac))
    return (f * NH_MM_ENCIMADO_OPT + (1 - f) * NH_MM_SUELTO,
            f * NH_MM_ENCIMADO_PES + (1 - f) * NH_MM_SUELTO)

def m6_lazo(T, I):
    I.tit("M6. Lazo de conmutación y sobretiro al apagar (estimación)")
    I.di("Recorrido: 48V -> drenaje del alto -> source del alto -> drenaje del")
    I.di("bajo -> source del bajo -> GND -> regreso por el desacoplo.")
    I.di()
    I.di("El lazo se parte en dos tramos y cada uno cobra según SU encimado:")
    I.di("  · el puente del nodo conmutado, con el encimado que midió M4b")
    I.di("  · el camino al desacoplo, con el sándwich 48V/GND que midió M4")
    I.di("Un tramo encimado sobre cobre quieto a 1.6 mm cuesta ~0.07-0.20 nH/mm;")
    I.di("uno con el regreso lejos cuesta ~1.0. Son 5 a 15 veces, así que esto")
    I.di("no se supone: se mide. Sigue siendo BANDA, y la verdad SE MIDE con")
    I.di("sonda. Sólo se marca fuera de meta si ni el extremo optimista deja")
    I.di("margen.")
    I.di()
    enc = I.datos.get('encimado', {})
    snd = I.datos.get('sandwich_bus', {})
    f_bus = max(snd.values()) if snd else 0.0
    for nombre, alto, bajo, vs in PIERNAS:
        pa2 = T.pad(alto, '2'); pa3 = T.pad(alto, '3')
        pb2 = T.pad(bajo, '2'); pb3 = T.pad(bajo, '3')
        if not all((pa2, pa3, pb2, pb3)):
            I.juicio("ATN", f"pierna {nombre}: faltan pads para estimar el lazo"); continue
        d1 = math.dist(pa3[:2], pb2[:2])          # el puente del nodo conmutado
        medio = ((pa3[0] + pb2[0]) / 2, (pa3[1] + pb2[1]) / 2)
        mejor = None
        for c in CAP_DESACOPLO:
            pz = T.piezas.get(c)
            if not pz: continue
            dd = math.dist(pz['pos'], medio)
            if mejor is None or dd < mejor[1]: mejor = (c, dd)
        d2 = mejor[1] if mejor else float('nan')
        # encimado de ESTE nodo conmutado (el mejor de sus caras)
        f_vs = enc.get(vs, 0.0)
        o1, p1 = _banda_nh_mm(f_vs)
        o2, p2 = _banda_nh_mm(f_bus)
        # el lazo recorre cada tramo de ida y de vuelta
        L_opt = 2 * (d1 * o1 + d2 * o2)
        L_pes = 2 * (d1 * p1 + d2 * p2)
        I.di(f"    pierna {nombre}:")
        I.di(f"      puente conmutado  {d1:5.1f} mm, encimado {f_vs*100:3.0f} %  "
             f"-> {o1:.2f} a {p1:.2f} nH/mm")
        I.di(f"      al desacoplo {mejor[0] if mejor else '?':<4s} {d2:5.1f} mm, sándwich "
             f"{f_bus*100:3.0f} %  -> {o2:.2f} a {p2:.2f} nH/mm")
        I.di(f"      lazo completo (ida y vuelta): entre {L_opt:.0f} y {L_pes:.0f} nH")
        picos = {}
        for flanco in (50, 100):
            didt = CORRIENTE_A / (flanco * 1e-9)
            lo = L_opt * 1e-9 * didt
            hi = L_pes * 1e-9 * didt
            picos[flanco] = (lo, hi)
            I.di(f"      flanco {flanco:3d} ns -> sobretiro entre {lo:.0f} y {hi:.0f} V "
                 f"-> pico total {V_BUS+lo:.0f} a {V_BUS+hi:.0f} V de {V_MOSFET:.0f} V")
        lo50, hi50 = picos[50]
        m_opt = V_MOSFET - (V_BUS + lo50)
        m_pes = V_MOSFET - (V_BUS + hi50)
        if m_pes > 0:   est = "OK"
        elif m_opt > 0: est = "ATN"
        else:           est = "MAL"
        I.juicio(est, f"pierna {nombre}: margen al límite del MOSFET entre {m_pes:+.0f} V "
                      f"(pesimista) y {m_opt:+.0f} V (optimista). "
                      + ("Hay que MEDIRLO con sonda antes de confiar."
                         if est != "OK" else "Aguanta en los dos extremos."))
        I.di()

def m7_desacoplo(T, I):
    I.tit("M7. Distancia del desacoplo más cercano a cada medio puente")
    I.di("El capacitor que cierra el lazo de alta frecuencia debe estar pegado al")
    I.di("medio puente, no en la banda central con los electrolíticos.")
    I.di()
    for nombre, alto, bajo, vs in PIERNAS:
        pa3 = T.pad(alto, '3'); pb2 = T.pad(bajo, '2')
        if not (pa3 and pb2): continue
        medio = ((pa3[0] + pb2[0]) / 2, (pa3[1] + pb2[1]) / 2)
        for c in CAP_DESACOPLO:
            pz = T.piezas.get(c)
            if not pz: continue
            d = math.dist(pz['pos'], medio)
            if d < 60:
                I.di(f"    pierna {nombre}: {c} ({pz['valor']}) a {d:.1f} mm del medio puente")

# ============================ PRINCIPAL =====================================
def main():
    ap = argparse.ArgumentParser(description="Auditoría de conmutación del layout")
    ap.add_argument("pcb")
    ap.add_argument("--json")
    ap.add_argument("--separacion-chasis", type=float, default=10.0, help="mm")
    ap.add_argument("--flanco", type=float, default=50.0, help="ns")
    a = ap.parse_args()

    T = Tarjeta(a.pcb)

    # CANDADO contra el falso aprobado. Si el lector no entiende el formato del
    # archivo, los nets salen vacíos, todas las áreas dan cero, y M2/M3/M4
    # reportan "EN META" sin haber medido nada. Eso es peor que no correr la
    # herramienta: es correrla y creerle. Pasó al abrir un archivo guardado con
    # KiCad 20260206, donde el net cambió de (net 2 "48V") a (net "48V").
    con_cobre = sorted({n for (n, _) in T.cobre if n})
    if not con_cobre:
        print(f"ABORTADO: no leí ningún net con cobre en {a.pcb}.", file=sys.stderr)
        print("No es que la tarjeta esté vacía: es que no entendí el formato.",
              file=sys.stderr)
        print("Revise net_de() contra la versión de KiCad del archivo antes de",
              file=sys.stderr)
        print("creerle una sola cifra a esta herramienta.", file=sys.stderr)
        return 2
    faltan = [n for n in (list(RIELES_QUIETOS) + [v for _, _, _, v in PIERNAS])
              if n not in con_cobre]
    if faltan:
        print(f"AVISO: no encontré cobre de estos nets esperados: {', '.join(faltan)}",
              file=sys.stderr)

    I = Informe()
    I.di("AUDITORÍA DE CONMUTACIÓN — Isla del Sol, Puente H")
    I.di(f"tarjeta: {a.pcb}")
    I.di(f"piezas leídas: {len(T.piezas)} ; polígonos de cobre: {sum(len(v) for v in T.cobre.values())}")

    m1_parejas(T, I)
    m2_reparto(T, I)
    m3_chasis(T, I, a.separacion_chasis, a.flanco)
    m4_sandwich(T, I)
    m4b_encimado(T, I)
    m5_compuertas(T, I)
    m6_lazo(T, I)
    m7_desacoplo(T, I)

    I.tit("RESUMEN")
    I.di(f"    fuera de meta: {I.fallas}    cerca: {I.avisos}")
    I.di()
    I.di("    Esto NO dice que la tarjeta esté mal. Una tarjeta puede ser correcta,")
    I.di("    pasar DRC, pasar corriente y pasar térmica, y aun así tener margen de")
    I.di("    optimización en la conmutación. Eso es lo que mide esta herramienta.")
    I.di()
    I.di("    Lo que sí conviene saber: las metas fuera de rango son estructurales.")
    I.di("    No se alcanzan ensanchando pistas. Casi siempre dependen de dónde")
    I.di("    están puestas las piezas, así que se ganan moviendo, no ruteando.")
    I.imprime()

    if a.json:
        with open(a.json, 'w', encoding='utf-8') as f:
            json.dump(dict(fallas=I.fallas, avisos=I.avisos, **I.datos), f,
                      indent=2, ensure_ascii=False)
        print(f"\n[guardado] {a.json}")
    return 1 if I.fallas else 0

if __name__ == "__main__":
    sys.exit(main())
