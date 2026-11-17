import json, numpy as np
from collections import defaultdict, Counter
from scipy.spatial import cKDTree, ConvexHull
objs=json.load(open('objects.json')); summ=json.load(open('summary.json'))
print("| land | trees | trees per km2 of land | rocks+boulders per km2 | dense groups (15 m link, 21+ trees) | trees per ha inside them p10/med/p90 | main kinds of the densest group | edge share (trees with under 3 neighbours in 10 m) |")
print("|---|---|---|---|---|---|---|---|")
for land in summ:
    t=[o for o in objs if o['land']==land and o['cls']=='tree']
    r=[o for o in objs if o['land']==land and o['cls']=='static' and ('rock' in o['kind'].lower() or 'Boulder' in o['kind'] or o['kind']=='Rock')]
    p=np.array([o['pos'] for o in t]); tr=cKDTree(p)
    par=list(range(len(p)))
    def f(i):
        while par[i]!=i: par[i]=par[par[i]]; i=par[i]
        return i
    for i,j in tr.query_pairs(15): par[f(i)]=f(j)
    g=defaultdict(list)
    for i in range(len(p)): g[f(i)].append(i)
    dens=[];best=None
    for idx in g.values():
        if len(idx)<21: continue
        try: a=ConvexHull(p[idx]).volume/1e4
        except Exception: continue
        d=len(idx)/max(a,0.01); dens.append(d)
        if best is None or d>best[0]: best=(d,Counter(t[i]['kind'] for i in idx).most_common(3))
    nb=np.array([len(x)-1 for x in tr.query_ball_point(p,10)])
    a=summ[land]['area_km2']
    print(f"| {land} | {len(t)} | {len(t)/a:.0f} | {len(r)/a:.0f} | {len(dens)} | {"/".join(f"{np.percentile(dens,v):.0f}" for v in (10,50,90)) if dens else "-"} | {best[1] if best else ''} | {(nb<3).mean()*100:.0f}% |")
