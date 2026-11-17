"""Shapes of vanilla groups of special things: rings, lines, clumps. Read only."""
import json
import math
from collections import Counter, defaultdict

import numpy as np
from scipy.spatial import cKDTree

objs = json.load(open("objects.json"))
KINDS = {"Aztec Statue Feature", "Pilar2 Lime", "Tibetan Large Pillar Feature", "Spikey Pilar Lime",
         "Spikey Pilar Volcanic", "MagicMushroom", "Toadstool", "Champi", "RockX", "Rock", "WeepingStoneReward",
         "Crater", "Prison Pillar 1", "Prison Pillar 2", "Prison Pillar 3", "Copse", "CopseA", "Cypress", "CypressA",
         "Boulder1Lime", "Hedge", "HedgeA", "Mist", "Flatrock", "Longrock"}


def shape(p):
    c = p.mean(0)
    r = np.hypot(*(p - c).T)
    ang = np.sort(np.arctan2(*(p - c).T[::-1]))
    gaps = np.diff(np.concatenate([ang, [ang[0] + 2 * math.pi]])) if len(p) > 2 else np.array([0])
    # line: ratio of the two principal axes
    if len(p) > 2:
        ev = np.sort(np.linalg.eigvalsh(np.cov((p - c).T)))
        elong = math.sqrt(ev[1] / max(ev[0], 1e-6))
    else:
        elong = float("nan")
    return c, r, gaps, elong


out = []
for land in sorted({o["land"] for o in objs}):
    lo = [o for o in objs if o["land"] == land]
    allp = np.array([o["pos"] for o in lo])
    t_all = cKDTree(allp)
    for kind in sorted(KINDS):
        v = [o for o in lo if o["kind"] == kind or (kind.startswith("Boulder1") and o["kind"] == kind)]
        if len(v) < 3:
            continue
        p = np.array([o["pos"] for o in v])
        t = cKDTree(p)
        parent = list(range(len(p)))

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        for i, j in t.query_pairs(40):
            parent[find(i)] = find(j)
        g = defaultdict(list)
        for i in range(len(p)):
            g[find(i)].append(i)
        for idx in g.values():
            if len(idx) < 3:
                continue
            q = p[idx]
            c, r, gaps, elong = shape(q)
            hs = [v[i]["h"] for i in idx]
            sc = [v[i].get("scale", 1) for i in idx]
            centre = Counter(lo[k]["kind"] for k in t_all.query_ball_point(c, 8) if lo[k]["kind"] != kind)
            ringy = r.mean() > 3 and r.std() / r.mean() < 0.25 and gaps.max() < 2.2
            out.append(f"| {land} | {kind} | {len(idx)} | {c[0]:.0f},{c[1]:.0f} | {r.mean():.1f} | "
                       f"{r.std() / max(r.mean(), 1e-6):.2f} | {math.degrees(gaps.max()):.0f} | {elong:.1f} | "
                       f"{min(hs):.0f}-{max(hs):.0f} | {min(sc):.2f}-{max(sc):.2f} | {'ring' if ringy else ('line' if elong > 4 else 'clump')} | "
                       f"{', '.join(f'{k} {n}' for k, n in centre.most_common(3))} |")
print("| land | kind | n | centre | mean radius m | radius spread | largest angle gap deg | elongation | h m | scale | "
      "shape | at the centre (8 m) |")
print("|---|---|---|---|---|---|---|---|---|---|---|---|")
print("\n".join(out))
