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
    Corrida sobre la REV C debe señalar sola M1, M2 y M3. Si no los señala,
    la herramienta no sirve y hay que arreglarla antes de confiar en ella.

QUÉ MIDE
    M1  Separación entre los dos MOSFET de cada medio puente.
    M2  Reparto de área de cobre por net y por capa (conmutado contra quieto).
    M3  Capacitancia al chasis del nodo conmutado y corriente de modo común.
    M4  Solapamiento del bus entre capas (sándwich 48V/GND).
    M5  Qué plano queda debajo de cada red de compuerta.
    M6  Estimación del lazo de conmutación y del sobretiro al apagar.
    M7  Distancia del desacoplo más cercano a cada medio puente.

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
RIELES_QUIETOS = ("48V", "GND", "GND_IN")
CARA_AL_CHASIS = "B.Cu"      # la cara de soldadura es la que ve al chasis
CAP_DESACOPLO  = ("C1", "C2", "C3", "C4", "C9", "C10")

# Criterios. Cada uno trae su razón; cambiarlos exige cambiar la razón.
CRIT_PAREJA_OK    = 25.0   # mm. Dos TO-247 pegados quedan a ~20-25 mm de centro a centro
CRIT_PAREJA_ATN   = 50.0   # mm
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
    I.tit("M1. Separación entre los dos MOSFET de cada medio puente")
    I.di("En un medio puente el alto y el bajo van PEGADOS: el nodo conmutado es")
    I.di("el puente corto entre ellos. Si se separan, ese nodo tiene que viajar, y")
    I.di("para llevar la corriente no le queda más que volverse plano. El tamaño")
    I.di("del nodo conmutado es consecuencia de esta distancia, no causa aparte.")
    I.di()
    res = {}
    for nombre, alto, bajo, vs in PIERNAS:
        a = T.piezas.get(alto); b = T.piezas.get(bajo)
        if not (a and b):
            I.juicio("ATN", f"pierna {nombre}: no encontré {alto} o {bajo}"); continue
        d = math.dist(a['pos'], b['pos'])
        res[nombre] = d
        est = "OK" if d <= CRIT_PAREJA_OK else ("ATN" if d <= CRIT_PAREJA_ATN else "MAL")
        I.juicio(est, f"pierna {nombre}: {alto} en ({a['pos'][0]:.1f}, {a['pos'][1]:.1f}) y "
                      f"{bajo} en ({b['pos'][0]:.1f}, {b['pos'][1]:.1f}) -> {d:.1f} mm "
                      f"(criterio: <= {CRIT_PAREJA_OK:.0f} mm)")
    I.datos['separacion_medio_puente_mm'] = res
    return res

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

def m6_lazo(T, I):
    I.tit("M6. Lazo de conmutación y sobretiro al apagar (estimación)")
    I.di("Recorrido: 48V -> drenaje del alto -> source del alto -> drenaje del")
    I.di("bajo -> source del bajo -> GND -> regreso por el desacoplo. Se da como")
    I.di("BANDA, no como número: 0.3 nH/mm para plano apretado y 1.0 nH/mm para")
    I.di("pista con retorno lejano. La verdad está en medio y SE MIDE con sonda.")
    I.di("Sólo se marca falla si ni el extremo optimista deja margen.")
    I.di()
    for nombre, alto, bajo, vs in PIERNAS:
        pa2 = T.pad(alto, '2'); pa3 = T.pad(alto, '3')
        pb2 = T.pad(bajo, '2'); pb3 = T.pad(bajo, '3')
        if not all((pa2, pa3, pb2, pb3)):
            I.juicio("ATN", f"pierna {nombre}: faltan pads para estimar el lazo"); continue
        d1 = math.dist(pa3[:2], pb2[:2])          # el puente del nodo conmutado
        # el desacoplo más cercano al medio puente
        medio = ((pa3[0] + pb2[0]) / 2, (pa3[1] + pb2[1]) / 2)
        mejor = None
        for c in CAP_DESACOPLO:
            pz = T.piezas.get(c)
            if not pz: continue
            dd = math.dist(pz['pos'], medio)
            if mejor is None or dd < mejor[1]: mejor = (c, dd)
        d2 = mejor[1] if mejor else float('nan')
        perim = 2 * (d1 + d2)
        I.di(f"    pierna {nombre}: puente conmutado {d1:.1f} mm ; desacoplo más cercano "
             f"{mejor[0] if mejor else '?'} a {d2:.1f} mm ; perímetro del lazo ~{perim:.0f} mm")
        # Banda, no número único: plano apretado ~0.3 nH/mm, pista con retorno
        # lejano ~1.0 nH/mm. La verdad está en medio y se mide con sonda.
        picos = {}
        for flanco in (50, 100):
            didt = CORRIENTE_A / (flanco * 1e-9)
            lo = perim * 0.3e-9 * didt
            hi = perim * 1.0e-9 * didt
            picos[flanco] = (lo, hi)
            I.di(f"      flanco {flanco:3d} ns -> L entre {perim*0.3:.0f} y {perim*1.0:.0f} nH "
                 f"-> sobretiro entre {lo:.0f} y {hi:.0f} V "
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
