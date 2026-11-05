import numpy as np
P=1000; Vdc=48; Vp=30.5; fsw=24e3; f=60
m=Vp*np.sqrt(2)/Vdc; Io=P/Vp
N=400*2000  # 2000 puntos por periodo de portadora
t=np.arange(N)/(fsw*2000)  # un ciclo de 60 Hz (400 portadoras)
th=2*np.pi*f*t; io=np.sqrt(2)*Io*np.sin(th)
car=np.abs(((t*fsw)%1)*2-1)  # triangular 0..1 sincronizada
ref=m*np.abs(np.sin(th))
pos=np.sin(th)>=0
dA=np.where(pos,ref,1-ref)        # ciclo de QAH
sA=(dA>car).astype(float)          # 1 = QAH encendido
sB=np.where(pos,0.0,1.0)           # pierna B a 60 Hz
vab=(sA-sB)*Vdc
ibus=(sA-sB)*io                    # corriente tomada del bus
X=np.fft.rfft(ibus)/N; fr=np.fft.rfftfreq(N,t[1]-t[0])
dc=abs(X[0]); amp=2*abs(X)
i120=amp[np.argmin(abs(fr-120))]/np.sqrt(2)
rms=np.sqrt(np.mean(ibus**2)); hf=np.sqrt(np.sum((amp[fr>1000]/np.sqrt(2))**2))
v1=2*abs(np.fft.rfft(vab)/N)[np.argmin(abs(fr-60))]/np.sqrt(2)
print(f'm={m:.3f} Io={Io:.2f} | simulado: Ibus_rms={rms:.2f} DC={dc:.2f} 120Hz={i120:.2f} HF={hf:.2f} | V_ab fund={v1:.2f} V rms')
print(f'formulas : Ibus_rms={np.sqrt(8*m*Io**2/(3*np.pi)):.2f} DC={m*np.sqrt(2)*Io/2:.2f} 120Hz={m*np.sqrt(2)*Io/2/np.sqrt(2):.2f}')
