# Prueba: tira recta de 10 x 100 mm en 1 oz, corriente de punta a punta -> R = 10 cuadros * 0.4914 mohm
import numpy as np, scipy.sparse as sp, scipy.sparse.linalg as spl
h=0.4; W=10; L=100; Rsq=1.72e-5/0.035
nx=int(L/h)+1; ny=int(W/h)+1; N=nx*ny; I=np.arange(N).reshape(ny,nx); g=1/Rsq
r=[];c=[];v=[]
for dy,dx in ((0,1),(1,0)):
    i=I[:ny-dy,:nx-dx].ravel(); j=I[dy:,dx:].ravel(); r+=[i,j,i,j]; c+=[i,j,j,i]; v+=[np.full(i.size,g)]*2+[np.full(i.size,-g)]*2
A=sp.csr_matrix((np.concatenate(v),(np.concatenate(r),np.concatenate(c))),shape=(N,N))
src=I[:,0].ravel(); snk=I[:,-1].ravel(); b=np.zeros(N); b[src]=1/len(src)
fr=np.setdiff1d(np.arange(N),snk); x=np.zeros(N); x[fr]=spl.spsolve(A[fr][:,fr].tocsc(),b[fr])
print(f'R simulada {x[src].mean()*1e3:.3f} mOhm  vs teorica {Rsq*(L-h)/(W+h)*1e3:.3f} mOhm (geometria de celdas) / {Rsq*L/W*1e3:.3f} mOhm ideal')
