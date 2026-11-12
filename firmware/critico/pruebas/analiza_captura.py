#!/usr/bin/env python3
"""
analiza_captura.py - Isla del Sol, firmware del CRÍTICO
Valida la SPWM híbrida REAL con una captura de analizador lógico.

ANALIZADOR: genérico de 24 MHz y 8 canales (clon de Saleae, driver fx2lafw)
con PulseView (sigrok) o con el software Logic de Saleae.

CONEXIÓN (sólo el Nano por USB, SIN etapa de potencia):
    canal 0 (D0) -> D9   HA   pierna A, lado alto
    canal 1 (D1) -> D10  LA   pierna A, lado bajo
    canal 2 (D2) -> D11  HB   pierna B, lado alto
    canal 3 (D3) -> D3   LB   pierna B, lado bajo
    canal 4 (D4) -> D12  EN_HW (debe quedar en 0)
    GND          -> GND del Nano (obligatorio)
  Muestreo 24 MHz, al menos 100 ms, disparo en flanco de subida del canal 3.

ARCHIVOS QUE ACEPTA
    .sr   sesión de PulseView (lo más cómodo: File > Save)
    .csv  exportación de PulseView (File > Export > CSV)
    .csv  exportación de Saleae Logic 1.x / 2.x (sólo cambios, con columna de tiempo)

USO
    python3 analiza_captura.py captura.sr
    python3 analiza_captura.py captura.csv --dt-ns 500 --hz 50
    python3 analiza_captura.py captura.csv --fs 24e6     (si el CSV no trae la frecuencia)
    python3 analiza_captura.py captura.sr --mapa 0,1,2,3,4   (canales de HA,LA,HB,LB,EN)
  Requiere Python 3 y numpy (pip install numpy).

QUÉ VERIFICA (contra lo que predice pruebas/prueba_spwm.cpp)
    1. HA y LA nunca altos juntos; tiempo muerto de la pierna A >= DT.
    2. HB y LB nunca altos juntos; hueco de la pierna B ~ 1 periodo (41.6 µs).
    3. Portadora ~24.02 kHz y salida ~60.06 / 50.05 Hz.
    4. Pulso más angosto >= 500 ns (menos 1 muestra de tolerancia).
    5. El ancho de pulso sigue al seno; amplitud en el pico y m.
    6. EN_HW (D12) en 0 todo el tiempo.
    7. Estado al inicio de la captura y primer flanco de cada señal.
  Termina con código 0 si todo pasa y 1 si algo falla.

Licencia: MIT (ver LICENSE.md del repositorio).
"""
import argparse
import configparser
import csv
import io
import math
import re
import sys
import zipfile

try:
    import numpy as np
except ImportError:
    sys.exit('Falta numpy: instale con  pip install numpy')

RELOJ_NANO_HZ = 16e6           # 1 cuenta del Timer1 = 62.5 ns
TOP = 333                      # config_portadora.h (328P)
NOMBRES = ['HA', 'LA', 'HB', 'LB', 'EN']


# ============================ LECTURA DE ARCHIVOS =============================
def frecuencia_texto(s):
    """'24 MHz' / '24000000' / '500 kHz' -> Hz"""
    m = re.match(r'\s*([\d.]+)\s*([kKmMgG]?)\s*[hH]?[zZ]?', s)
    if not m:
        raise ValueError(f'No entiendo la frecuencia de muestreo: {s!r}')
    mult = {'': 1, 'k': 1e3, 'K': 1e3, 'm': 1e6, 'M': 1e6, 'g': 1e9, 'G': 1e9}[m.group(2)]
    return float(m.group(1)) * mult


def bordes_desde_muestras(muestras, fs, canales):
    """Matriz de muestras (uint8 con un bit por canal) -> (inicial, [(t, nivel)...]) por canal."""
    res = []
    for c in canales:
        b = ((muestras >> c) & 1).astype(np.int8)
        cambios = np.nonzero(np.diff(b))[0] + 1
        res.append((int(b[0]), [(i / fs, int(b[i])) for i in cambios]))
    return res, len(muestras) / fs


def leer_sr(ruta, canales):
    with zipfile.ZipFile(ruta) as z:
        meta = configparser.ConfigParser()
        meta.read_string(z.read('metadata').decode())
        sec = [s for s in meta.sections() if s.startswith('device')][0]
        fs = frecuencia_texto(meta[sec]['samplerate'])
        unidad = int(meta[sec].get('unitsize', '1'))
        partes = sorted((n for n in z.namelist() if n.startswith('logic-1')),
                        key=lambda n: int(n.split('-')[-1]))
        crudo = b''.join(z.read(n) for n in partes)
    datos = np.frombuffer(crudo, dtype=np.uint8)
    if unidad > 1:
        datos = datos.reshape(-1, unidad)[:, 0]   # canales 0..7 en el primer byte
    return bordes_desde_muestras(datos, fs, canales) + (fs,)


def leer_csv(ruta, canales, fs_arg):
    with open(ruta, encoding='utf-8', errors='replace') as f:
        texto = f.read()
    fs = fs_arg
    lineas = []
    for ln in texto.splitlines():
        if ln.startswith(';'):
            m = re.search(r'[Ss]amplerate:\s*(.+)', ln)
            if m and not fs:
                fs = frecuencia_texto(m.group(1))
            continue
        if ln.strip():
            lineas.append(ln)
    filas = list(csv.reader(io.StringIO('\n'.join(lineas))))
    cab = [c.strip() for c in filas[0]]
    if cab and re.match(r'\d', cab[0]):          # sin encabezado
        cab, datos = None, filas
    else:
        datos = filas[1:]
    con_tiempo = cab is not None and 'time' in cab[0].lower()
    columnas = list(range(1, len(datos[0]))) if con_tiempo else list(range(len(datos[0])))

    if con_tiempo:
        # Saleae (sólo cambios) o sigrok con columna de tiempo: se trabaja con eventos.
        t = np.array([float(r[0]) for r in datos])
        estados = np.array([[int(float(r[i])) for i in columnas] for r in datos], dtype=np.uint8)
        res = []
        for c in canales:
            b = estados[:, c]
            cambios = np.nonzero(np.diff(b.astype(np.int8)))[0] + 1
            res.append((int(b[0]), [(t[i] - t[0], int(b[i])) for i in cambios]))
        dur = t[-1] - t[0]
        if not fs:
            # Este formato sólo guarda los cambios: no se puede deducir el muestreo.
            fs = 24e6
            print('Aviso: el CSV no dice la frecuencia de muestreo; se supone 24 MHz (use --fs si fue otra)')
        return res, dur, fs

    if not fs:
        sys.exit('El CSV no trae la frecuencia de muestreo: use --fs 24e6')
    m = np.array([[int(float(r[i])) for i in columnas] for r in datos], dtype=np.uint8)
    paquete = np.zeros(len(m), dtype=np.uint8)
    for i in range(m.shape[1]):
        paquete |= (m[:, i] & 1) << i
    return bordes_desde_muestras(paquete, fs, canales) + (fs,)


# ============================ ANÁLISIS =======================================
def analizar(senales, duracion, fs, dt_ns, hz):
    res_s = 1.0 / fs
    fallas = []
    eventos = []
    for idx, (ini, bordes) in enumerate(senales):
        for t, v in bordes:
            eventos.append((t, idx, v))
    eventos.sort()
    est = [ini for ini, _ in senales]
    HA, LA, HB, LB, EN = range(5)
    ult_baja = [-1e9] * 5
    sube = [None] * 5
    gaps_a, gaps_b, anchos = [], [], {HA: [], LA: []}
    traslape = {('A'): 0.0, ('B'): 0.0}
    en_alto = 0.0
    t_ant = 0.0
    pulsos = {HA: [], LA: []}          # (centro, ancho, LB, HB al centro)
    sube_lb = []
    for t, c, v in eventos:
        dt_t = t - t_ant
        if est[HA] and est[LA]:
            traslape['A'] += dt_t
        if est[HB] and est[LB]:
            traslape['B'] += dt_t
        if est[EN]:
            en_alto += dt_t
        t_ant = t
        if v == est[c]:
            continue
        if v == 1:
            sube[c] = t
            if c == HA and not est[LA]:
                gaps_a.append(t - ult_baja[LA])
            if c == LA and not est[HA]:
                gaps_a.append(t - ult_baja[HA])
            if c == HB and not est[LB]:
                gaps_b.append(t - ult_baja[LB])
            if c == LB:
                sube_lb.append(t)
                if not est[HB]:
                    gaps_b.append(t - ult_baja[HB])
        else:
            ult_baja[c] = t
            if c in (HA, LA) and sube[c] is not None:
                w = t - sube[c]
                anchos[c].append(w)
                pulsos[c].append(((t + sube[c]) / 2, w, est[LB], est[HB]))
        est[c] = v
    if est[EN]:
        en_alto += duracion - t_ant

    dt_s = dt_ns * 1e-9
    tol = res_s * 1.01
    print(f'Muestreo: {fs / 1e6:.3f} MHz (resolución {res_s * 1e9:.1f} ns); captura de {duracion * 1e3:.1f} ms')
    print(f'Esperado: DT {dt_ns} ns, línea {hz} Hz, portadora 24.02 kHz\n')

    def renglon(nombre, ok, texto):
        print(f'  [{"PASA " if ok else "FALLA"}] {nombre}: {texto}')
        if not ok:
            fallas.append(nombre)

    # 1. Pierna A
    renglon('HA y LA nunca juntos', traslape['A'] == 0, f'traslape total {traslape["A"] * 1e9:.0f} ns')
    if gaps_a:
        g = np.array(gaps_a)
        g = g[g < 20e-6]                 # sólo los flancos complementarios
        if len(g):
            renglon('Tiempo muerto pierna A', g.min() >= dt_s - tol,
                    f'mínimo {g.min() * 1e9:.0f} ns, mediana {np.median(g) * 1e9:.0f} ns, '
                    f'{len(g)} flancos (esperado >= {dt_ns} ns)')
    # 2. Pierna B
    renglon('HB y LB nunca juntos', traslape['B'] == 0, f'traslape total {traslape["B"] * 1e9:.0f} ns')
    if gaps_b:
        g = np.array([x for x in gaps_b if x < 1e-3])
        if len(g):
            renglon('Hueco pierna B', g.min() >= 30e-6,
                    f'mínimo {g.min() * 1e6:.1f} µs, máximo {g.max() * 1e6:.1f} µs (esperado ~41.6 µs)')
    # 3. Frecuencias
    if len(sube_lb) >= 2:
        f_lin = 1.0 / np.median(np.diff(sube_lb))
        renglon('Frecuencia de salida', abs(f_lin - hz) / hz < 0.005,
                f'{f_lin:.3f} Hz (esperado {hz * 1.001:.3f})')
    centros_la = np.array([p[0] for p in pulsos[LA] if p[2]])   # LA durante semiciclo +
    if len(centros_la) > 10:
        d = np.diff(centros_la)
        d = d[d < 60e-6]
        f_sw = 1.0 / np.median(d)
        renglon('Portadora', abs(f_sw - 24024) / 24024 < 0.005, f'{f_sw:.0f} Hz (esperado 24 024)')
    # 4. Pulso mínimo
    todos = np.array(anchos[HA] + anchos[LA])
    if len(todos):
        renglon('Pulso más angosto', todos.min() >= 500e-9 - tol,
                f'{todos.min() * 1e9:.0f} ns (esperado >= 500 ns)')
    # 5. Seno: HA en semiciclo + y LA en semiciclo - deben seguir |sen|
    muestras_ciclo = 24000 // hz
    for nombre, c, en_sem in (('semiciclo +', HA, 2), ('semiciclo -', LA, 3)):
        ps = [p for p in pulsos[c] if p[en_sem]]
        if len(ps) < 50 or len(sube_lb) < 1:
            continue
        cen = np.array([p[0] for p in ps])
        w = np.array([p[1] for p in ps])
        semiper = muestras_ciclo / 2 / 24024.0
        ref = np.array(sube_lb)
        if c == LA:
            ref = ref + semiper                              # el semiciclo - empieza medio ciclo después
        k = np.searchsorted(ref, cen) - 1
        validos = k >= 0
        fase = (cen[validos] - ref[k[validos]]) / semiper
        w = w[validos]
        dentro = (fase > 0.02) & (fase < 0.98)
        s = np.sin(np.pi * fase[dentro])
        wv = w[dentro]
        if len(s) < 20:
            continue
        a = (wv @ s) / (s @ s) / 2 * RELOJ_NANO_HZ         # amplitud en cuentas
        resid = wv / 2 * RELOJ_NANO_HZ - a * s
        err = np.sqrt(np.mean(resid ** 2)) / a
        print(f'  [INFO ] Seno {nombre}: amplitud {a:.1f} cuentas (m = {a / TOP:.3f}), '
              f'desviación RMS {100 * err:.1f} % de la amplitud, {len(s)} pulsos')
        if err > 0.05:
            renglon(f'Forma senoidal {nombre}', False, f'desviación {100 * err:.1f} % (> 5 %)')
    # 6. EN_HW
    renglon('EN_HW (D12) en 0', en_alto == 0, f'tiempo en alto {en_alto * 1e3:.3f} ms')
    # 7. Inicio
    ini = ', '.join(f'{NOMBRES[i]}={senales[i][0]}' for i in range(5))
    primeros = ', '.join(f'{NOMBRES[i]} {senales[i][1][0][0] * 1e3:.3f} ms' if senales[i][1]
                         else f'{NOMBRES[i]} sin flancos' for i in range(5))
    print(f'  [INFO ] Estado al inicio de la captura: {ini}')
    print(f'  [INFO ] Primer flanco: {primeros}')
    print()
    print('RESULTADO: ' + ('todo correcto' if not fallas else f'{len(fallas)} FALLAS: ' + '; '.join(fallas)))
    return not fallas


def main():
    ap = argparse.ArgumentParser(description='Valida la SPWM híbrida del CRÍTICO con una captura de analizador lógico')
    ap.add_argument('archivo')
    ap.add_argument('--dt-ns', type=int, default=1000, help='tiempo muerto esperado (1000 sin P9; 500 con P9 a GND)')
    ap.add_argument('--hz', type=int, default=60, choices=[50, 60])
    ap.add_argument('--fs', type=float, default=None, help='frecuencia de muestreo si el archivo no la trae')
    ap.add_argument('--mapa', default='0,1,2,3,4', help='canales de HA,LA,HB,LB,EN')
    a = ap.parse_args()
    canales = [int(x) for x in a.mapa.split(',')]
    if a.archivo.lower().endswith('.sr'):
        senales, dur, fs = leer_sr(a.archivo, canales)
    else:
        senales, dur, fs = leer_csv(a.archivo, canales, a.fs)
    sys.exit(0 if analizar(senales, dur, fs, a.dt_ns, a.hz) else 1)


if __name__ == '__main__':
    main()
