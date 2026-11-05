#!/usr/bin/env python3
"""Sensibilidad de la auditoría térmica: corre los casos optimista, base y pesimista. Uso: python3 sensibilidad.py TARJETA.kicad_pcb  (MIT)"""
import sys, math, json, copy
sys.path.insert(0,'.')
import termica_componentes as TC, auditoria_cobre as AC
from auditoria_cobre import Tarjeta
T = Tarjeta(sys.argv[1], h=0.4)
base = (copy.deepcopy(TC.OP), copy.deepcopy(TC.MOS), copy.deepcopy(TC.CAP)); casos0 = copy.deepcopy(AC.ESCENARIO['casos'])
def run(op={}, mos={}, cap={}, tcu=0.035):
    TC.OP.clear(); TC.OP.update(base[0]); TC.OP.update(op)
    TC.MOS.clear(); TC.MOS.update(base[1]); TC.MOS.update(mos)
    TC.CAP.clear(); TC.CAP.update(base[2]); TC.CAP.update(cap)
    T.tcu = tcu; T.Rsq = 1.72e-5 / tcu
    c = TC.corrientes(); Pm = TC.perdidas_mosfet(c)
    Icap, Jhf = TC.rizo_capacitores(T, c); Pcap = {k: v**2*TC.CAP['ESR'] for k, v in Icap.items()}
    Ibat = math.sqrt(c['Idc']**2 + (0.9*c['I120'])**2)
    AC.ESCENARIO['casos'] = [x for x in casos0 if x[0] in ('VS1','VS2')] + [
        ('48V',('Q1','2'),[('J5',None),('J6',None)],Ibat,0.5,''),('48V',('Q3','2'),[('J5',None),('J6',None)],Ibat,0.5,''),
        ('GND',('Q2','3'),[('R16','2'),('R28','2')],Ibat,0.5,''),('GND',('Q4','3'),[('R16','2'),('R28','2')],Ibat,0.5,''),
        ('GND_IN',[('R16','1'),('R28','1')],[('J1',None),('J2',None)],Ibat,1.0,'')]
    Jrms,_,f = AC.correr(T, None)
    J2 = {ly: Jrms[ly]**2 + Jhf['48V'][ly] + Jhf['GND'][ly] for ly in Jrms}
    q = {ly: J2[ly]*T.Rsq for ly in J2}
    Tm, Tn = TC.termico_total(T, q, Pm, Pcap)
    Pc = sum(float(q[ly].sum()) for ly in q)*T.h**2
    return dict(Ptot=sum(v['total'] for v in Pm.values())+Pc+sum(Pcap.values()), PmosA=Pm['Q1']['total'], Pcu=Pc,
                Tj=Tn['Q1 unión'], Tcu=max(v.max() for v in Tm.values()), Tcap=max(Tn[k+' Rubycon'] for k in TC.RUBYCON), Icap=max(Icap.values()))
r = {'base': run(),
     'optimista': run(op=dict(h_conv=14e-6), mos=dict(Rds25=1.4e-3, k_T=1.5, tr=100e-9, tf=60e-9, Qrr=100e-9, Vf=0.8, R_dis=4.5), cap=dict(ESR=0.024), tcu=0.038),
     'pesimista': run(op=dict(h_conv=7e-6), mos=dict(Rds25=1.7e-3, k_T=1.75, tr=250e-9, tf=150e-9, Qrr=250e-9, Vf=1.0, R_dis=6.5, R_patas=14.0, R_pines_dis=15.0), cap=dict(ESR=0.036, beta=1.8e-3), tcu=0.031)}
for k,v in r.items(): print(k, {a: round(b,2) for a,b in v.items()})
