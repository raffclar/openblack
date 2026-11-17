import os, sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from analyze import LANDS, Land, BLACK
print("| land | flat land (<5 deg) share | main flat levels m (share of flat land) | cliff height: median rise across slope>40 runs m | slope p50 / p75 / p90 deg |")
print("|---|---|---|---|---|")
from scipy import ndimage
for name, lnd, *_ in LANDS:
    L = Land(f"{BLACK}/{lnd}"); lm=L.land
    flat = lm & (L.slope<5) & (L.H>3)
    h = L.H[flat]
    hist,edges=np.histogram(h,bins=np.arange(0,174,4))
    top=np.argsort(hist)[::-1][:5]
    lv=", ".join(f"{edges[i]:.0f}-{edges[i]+4:.0f} ({hist[i]*100/len(h):.0f}%)" for i in sorted(top))
    lab,n=ndimage.label(lm&(L.slope>=40))
    rises=[]
    for i in range(1,n+1):
        m=lab==i
        if m.sum()<20: continue
        d=ndimage.binary_dilation(m,iterations=2)&lm
        rises.append(L.H[d].max()-L.H[d].min())
    s=L.slope[lm]
    print(f"| {name} | {flat.sum()*100/lm.sum():.0f}% | {lv} | {np.median(rises):.0f} (p90 {np.percentile(rises,90):.0f}) | {np.percentile(s,50):.0f} / {np.percentile(s,75):.0f} / {np.percentile(s,90):.0f} |")
