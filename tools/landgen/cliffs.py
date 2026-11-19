#!/usr/bin/env python3
"""The numbers of the cliffs of a land: the slope of the faces, the size of the cliff bands and where they are.

measure.py adds these numbers to the numbers of each land. A cliff corner has a slope of 40 degrees or more. A cliff
band is a group of 20 or more cliff corners that touch.
"""
import numpy as np
from scipy import ndimage

CELL = 10.0
CLIFF = 40.0
# A band with fewer corners than this is not counted
SMALLEST_BAND = 20
# A gorge corner looks down its slope and finds a cliff that faces it within this many corners
GORGE_REACH = 8
# A band is on the coast when this part of its corners is within 60 m of the sea
COAST_PART = 0.3
COAST_REACH = 60.0


def gorge_mask(heights, land, cliff):
    """The cliff corners that face another cliff across a narrow valley: the corner looks down its slope, and within
    GORGE_REACH corners it finds a cliff corner whose slope faces back."""
    gz, gx = np.gradient(heights, CELL)
    length = np.hypot(gx, gz) + 1e-9
    ux, uz = gx / length, gz / length
    rows, cols = np.nonzero(cliff)
    found = np.zeros(len(rows), dtype=bool)
    open_ = np.ones(len(rows), dtype=bool)
    side_r, side_c = heights.shape
    for k in range(2, GORGE_REACH + 1):
        r = np.rint(rows - uz[rows, cols] * k).astype(int)
        c = np.rint(cols - ux[rows, cols] * k).astype(int)
        inside = (r >= 0) & (c >= 0) & (r < side_r) & (c < side_c)
        r = np.clip(r, 0, side_r - 1)
        c = np.clip(c, 0, side_c - 1)
        # The walk stops at the sea
        open_ &= inside & land[r, c]
        facing = (ux[r, c] * ux[rows, cols] + uz[r, c] * uz[rows, cols]) < -0.5
        found |= open_ & cliff[r, c] & facing
    out = np.zeros_like(cliff)
    out[rows, cols] = found
    return out


def cliff_measures(heights, land, slope, sea, high):
    """heights in metres, land a mask, slope in degrees, sea the distance to the sea in metres, high the height of a
    mountain band in metres (100 m of the lands of the story)."""
    cliff = land & (slope >= CLIFF)
    out = {}
    count = int(cliff.sum())
    if count == 0:
        return out
    km2 = land.sum() * CELL * CELL / 1e6
    faces = slope[cliff]
    out["cliff_face_median_deg"] = float(np.median(faces))
    out["cliff_face_p90_deg"] = float(np.percentile(faces, 90))
    out["cliff_over_60_pct"] = float((faces >= 60).mean() * 100)
    out["cliff_gorge_pct"] = float(gorge_mask(heights, land, cliff)[cliff].mean() * 100)
    labels, _ = ndimage.label(cliff)
    lengths, widths, rises = [], [], []
    shares = {"coast": 0, "mountain": 0, "terrace": 0}
    for i, box in enumerate(ndimage.find_objects(labels), start=1):
        if box is None:
            continue
        # The box grows by two corners for the ring round the band
        grown = tuple(slice(max(s.start - 2, 0), s.stop + 2) for s in box)
        mask = labels[grown] == i
        corners = int(mask.sum())
        if corners < SMALLEST_BAND:
            continue
        points = np.argwhere(mask).astype(float)
        points -= points.mean(axis=0)
        _values, vectors = np.linalg.eigh(np.cov(points.T))
        along = points @ vectors[:, -1]
        length = (along.max() - along.min() + 1) * CELL
        lengths.append(length)
        widths.append(corners * CELL * CELL / length)
        ring = ndimage.binary_dilation(mask, iterations=2) & land[grown]
        top = heights[grown][ring].max()
        rises.append(top - heights[grown][ring].min())
        if (sea[grown][mask] <= COAST_REACH).mean() > COAST_PART:
            shares["coast"] += corners
        elif top >= high:
            shares["mountain"] += corners
        else:
            shares["terrace"] += corners
    if lengths:
        out["cliff_bands_per_km2"] = len(lengths) / km2
        out["cliff_length_median_m"] = float(np.median(lengths))
        out["cliff_length_p90_m"] = float(np.percentile(lengths, 90))
        out["cliff_width_median_m"] = float(np.median(widths))
        out["cliff_rise_p90_m"] = float(np.percentile(rises, 90))
        total = sum(shares.values())
        for name, corners in shares.items():
            out[f"cliff_{name}_pct"] = corners * 100.0 / total
    return out
