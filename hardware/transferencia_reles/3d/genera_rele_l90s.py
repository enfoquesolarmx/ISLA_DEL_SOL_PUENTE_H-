"""Modelo 3D aproximado del relé T90 / Rayex L90S para la huella Relay_SPDT_RAYEX-L90S (KiCad).
Medidas del contorno F.Fab de la huella; alto ~28 mm (aproximado). Coordenadas: mm, X a la derecha, Y hacia arriba
(la Y de la huella se invierte), Z hacia arriba desde la cara de componentes. Uso: python3 genera_rele_l90s.py"""
import cadquery as cq
H = 28.0
def caja(x0, x1, y0, y1, z0, z1):           # y0, y1 en coordenadas de la huella (Y hacia abajo)
    return cq.Workplane('XY').box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, -y1, z0))
cuerpo = caja(-3.96, 10.24, -10.7, 8.9, 0.5, H).union(caja(10.24, 27.84, -13.2, 14.2, 0.5, H))
cuerpo = cuerpo.edges('|Z').fillet(0.8).faces('>Z').edges().fillet(0.6)
marca = caja(1.0, 25.0, -7.0, 7.0, H - 0.05, H + 0.15)
pines = None
for x, y, w, t in ((15.2, -8.9, 2.0, 0.5), (25.34, 8.9, 2.0, 0.5), (17.74, 8.9, 2.0, 0.5), (2.54, -5.1, 0.8, 0.5), (2.54, 5.1, 0.8, 0.5)):
    p = caja(x - w / 2, x + w / 2, y - t / 2, y + t / 2, -3.5, 0.6)
    pines = p if pines is None else pines.union(p)
a = cq.Assembly(name='Relay_SPDT_RAYEX-L90S_aprox')
a.add(cuerpo, name='cuerpo', color=cq.Color(0.10, 0.20, 0.50))
a.add(marca, name='marca', color=cq.Color(0.85, 0.85, 0.85))
a.add(pines, name='pines', color=cq.Color(0.80, 0.80, 0.75))
a.save('Relay_SPDT_RAYEX-L90S_aprox.step', 'STEP')
print('ok')
