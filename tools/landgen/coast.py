import os, sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import ndimage
from analyze import LANDS, Land, BLACK
print("| land | coast corners | beach coast % | gentle coast % | cliff coast % | median inland height at 50 m | cliff corners within 60 m of sea % of all cliff | flat land (<5 deg) above 20 m % | flat land below 5 m % | plateau tops (flat >=1 ha above 30 m) |")
print("|---|---|---|---|---|---|---|---|---|---|")
for name, lnd, *_ in LANDS:
    L = Land(f"{BLACK}/{lnd}")
    lm = L.land
    coast = lm & (L.dsea <= 10)
    # max height inside a 50 m disk
    fp = np.hypot(*np.mgrid[-5:6, -5:6]) <= 5
    hmax = ndimage.maximum_filter(np.where(lm, L.H, 0), footprint=fp)
    c = hmax[coast]
    beach = (c < 6).mean() * 100
    cliff = (c >= 20).mean() * 100
    gentle = 100 - beach - cliff
    cl = lm & (L.slope >= 40)
    near = (cl & (L.dsea <= 60)).sum() / max(cl.sum(), 1) * 100
    flat = lm & (L.slope < 5)
    fa = (flat & (L.H > 20)).sum() / lm.sum() * 100
    fl = (flat & (L.H <= 5)).sum() / lm.sum() * 100
    lab, n = ndimage.label(flat & (L.H > 30))
    sizes = np.bincount(lab.ravel())[1:]
    plateaus = (sizes >= 100).sum()
    hs = sorted(round(float(np.median(L.H[lab == i + 1]))) for i in np.where(sizes >= 100)[0])
    print(f"| {name} | {coast.sum()} | {beach:.0f} | {gentle:.0f} | {cliff:.0f} | {np.median(c):.0f} | {near:.0f} | {fa:.1f} | {fl:.1f} | {plateaus} at {hs} m |")
