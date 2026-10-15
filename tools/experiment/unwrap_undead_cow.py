"""EXPERIMENT (not for merge). Unwraps the undead cow glb by body region: each triangle goes to the region of its main bone, each region is
unrolled round its own axis (the outward faces in one island, the inward faces in another), the islands are packed
with even texel density, mirror pairs side by side. Writes the glb copy the game reads, the UV layout and the
placeholder texture. Usage: unwrap_regions.py <in.glb> <out_dir>"""
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

SIZE = 1024
PADDING = 12  # pixels between islands
src, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
out_dir.mkdir(parents=True, exist_ok=True)
data = src.read_bytes()
json_len = struct.unpack_from("<I", data, 12)[0]
doc = json.loads(data[20:20 + json_len])
bin_start = 20 + json_len + 8
bin_len = struct.unpack_from("<I", data, 20 + json_len)[0]
binary = bytearray(data[bin_start:bin_start + bin_len])
COMPONENT = {5126: "f4", 5123: "u2", 5121: "u1", 5125: "u4"}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def read(index):
    a = doc["accessors"][index]
    view = doc["bufferViews"][a["bufferView"]]
    offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    n = WIDTH[a["type"]]
    return np.frombuffer(binary, dtype="<" + COMPONENT[a["componentType"]], count=a["count"] * n,
                         offset=offset).reshape(a["count"], n).copy()


def append(array, component_type, type_name, target=None, bounds=False):
    while len(binary) % 4:
        binary.append(0)
    raw = np.ascontiguousarray(array).tobytes()
    view = {"buffer": 0, "byteOffset": len(binary), "byteLength": len(raw)}
    if target is not None:
        view["target"] = target
    binary.extend(raw)
    doc["bufferViews"].append(view)
    accessor = {"bufferView": len(doc["bufferViews"]) - 1, "componentType": component_type,
                "count": int(array.shape[0]), "type": type_name}
    if bounds:
        accessor["min"] = [float(v) for v in array.min(0)]
        accessor["max"] = [float(v) for v in array.max(0)]
    doc["accessors"].append(accessor)
    return len(doc["accessors"]) - 1


# The regions, by the bone names of the skin (bone numbers as the l3dtool export names them)
REGIONS = {
    "pelvis": [0, 1, 2, 3, 6, 45, 46, 47],
    "right leg": [4, 42, 43, 44], "left leg": [5, 39, 40, 41],
    "ribs and spine": [7, 8, 9],
    "right upper arm": [10, 32], "left upper arm": [11, 25],
    "right forearm": [33, 34, 35, 36, 37, 38], "left forearm": [26, 27, 28, 29, 30, 31],
    "neck": [12], "skull": [13, 14, 15, 16, 17, 18, 19, 20, 21], "jaw": [22, 23, 24],
}
# Mirror pairs go side by side; the order is the order of the layout
ORDER = ["ribs and spine", "pelvis", "skull", "jaw", "neck", "right upper arm", "left upper arm",
         "right forearm", "left forearm", "right leg", "left leg"]
# Regions unrolled round the up axis; the others round their own long axis
CAP_COSINE = 0.7
MIN_ISLAND = 0.08
# Only these regions get islands for their ends; the others keep them in their main island
CAPPED = {"ribs and spine", "pelvis", "skull", "jaw"}
# A layer of fewer triangles goes back into the first layer, its small overlap allowed
MIN_LAYER = 60
UPRIGHT = {"ribs and spine", "pelvis", "skull", "jaw", "neck"}

mesh = doc["meshes"][0]
prim = mesh["primitives"][0]
positions = read(prim["attributes"]["POSITION"]).astype(np.float64)
normals = read(prim["attributes"]["NORMAL"]).astype(np.float64)
weights = read(prim["attributes"]["WEIGHTS_0"])
joints = read(prim["attributes"]["JOINTS_0"])
triangles = read(prim["indices"]).reshape(-1, 3).astype(np.int64)
skin_joints = doc["skins"][0]["joints"]
bone_of_joint = [int(doc["nodes"][n]["name"].split("_")[1]) for n in skin_joints]
region_of_bone = {b: r for r, bones in REGIONS.items() for b in bones}
main_bone = np.array([bone_of_joint[joints[v][np.argmax(weights[v])]] for v in range(len(positions))])

# Each triangle's region: the region most of its corners' main bones are in
tri_region = []
for t in triangles:
    names = [region_of_bone[main_bone[v]] for v in t]
    tri_region.append(max(set(names), key=names.count))
tri_region = np.array(tri_region)


def unroll(name, tris):
    """The UVs (in world units) of each corner of the region's triangles, in islands: [(label, tris, corner_uv)]"""
    corners = positions[tris]  # (n, 3, 3)
    points = corners.reshape(-1, 3)
    centre = points.mean(0)
    if name in UPRIGHT:
        axis = np.array([0.0, 1.0, 0.0])
    else:
        _, _, vt = np.linalg.svd(points - centre, full_matrices=False)
        axis = vt[0] * (1 if vt[0][1] >= 0 else -1)
    ref = np.cross(axis, [0.0, 0.0, 1.0])
    if np.linalg.norm(ref) < 1e-3:
        ref = np.cross(axis, [1.0, 0.0, 0.0])
    ref /= np.linalg.norm(ref)
    side = np.cross(axis, ref)
    rel = points - centre
    height = rel @ axis
    radial = rel - np.outer(height, axis)
    angle = np.arctan2(radial @ side, radial @ ref)
    radius = np.median(np.linalg.norm(radial, axis=1))
    # The seam goes into the widest gap of the angles, behind or between the parts
    sorted_angles = np.sort(angle)
    gaps = np.diff(np.concatenate([sorted_angles, [sorted_angles[0] + 2 * np.pi]]))
    i = int(np.argmax(gaps))
    seam = sorted_angles[i] + gaps[i] / 2
    angle = (angle - seam) % (2 * np.pi)
    angle = angle.reshape(-1, 3)
    height = height.reshape(-1, 3)
    # A triangle across the seam keeps its corners together
    for k in range(len(angle)):
        if angle[k].max() - angle[k].min() > np.pi:
            angle[k][angle[k] < np.pi] += 2 * np.pi
    uv = np.stack([angle * radius, height], axis=-1)  # (n, 3, 2)
    # Faces turned towards the axis (the inside of the ribs) go to an island of their own
    face_normal = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
    tri_radial = corners.mean(1) - centre
    tri_radial -= np.outer(tri_radial @ axis, axis)
    inward = np.einsum("ij,ij->i", face_normal, tri_radial) < 0
    # Faces that look along the axis (the ends of the bones, the tops of the ribs) are projected along it instead
    unit_normal = face_normal / np.maximum(np.linalg.norm(face_normal, axis=1, keepdims=True), 1e-12)
    along = unit_normal @ axis
    plane = np.stack([(corners - centre) @ ref, (corners - centre) @ side], axis=-1)
    groups = [
        (name, (np.abs(along) < CAP_COSINE) & ~inward, uv),
        (name + " (inside)", (np.abs(along) < CAP_COSINE) & inward, uv * [-1, 1]),
    ]
    if name in CAPPED:
        groups[0] = (name, (np.abs(along) < CAP_COSINE) & ~inward, uv)
        groups += [(name + " (top)", along >= CAP_COSINE, plane), (name + " (bottom)", along <= -CAP_COSINE, plane * [-1, 1])]
    else:
        groups[0] = (name, ~inward, uv)
        groups[1] = (name + " (inside)", inward, uv * [-1, 1])
    islands = []
    rest = []
    for label, mask, values in groups:
        if mask.sum() >= MIN_ISLAND * len(tris):
            islands.append((label, tris[mask], values[mask]))
        elif mask.any():
            rest.append(mask)
    # Small groups join the main island
    if rest and islands:
        mask = np.any(rest, axis=0)
        label, main_tris, main_uv = islands[0]
        islands[0] = (label, np.concatenate([main_tris, tris[mask]]), np.concatenate([main_uv, uv[mask]]))
    return islands


islands = []
for name in ORDER:
    tris = triangles[tri_region == name]
    if len(tris):
        islands.extend(unroll(name, tris))

_, welded_ids = np.unique(np.round(positions, 4), axis=0, return_inverse=True)
welded_ids = welded_ids.reshape(-1)
LAYER_SCALE = 8.0


def raster(uv, low, shape):
    mask = Image.new("1", shape, 0)
    draw = ImageDraw.Draw(mask)
    for tri in uv:
        centre = tri.mean(0)
        draw.polygon([tuple(p) for p in (centre + (tri - centre) * 0.85 - low) * LAYER_SCALE], fill=1)
    return np.array(mask, dtype=bool)


def layered(label, tris, uv):
    """The island's connected pieces, kept where they are, in as few layers as needed for no two pieces to overlap"""
    keys = {}
    local = np.zeros(tris.shape, dtype=np.int64)
    for t in range(len(tris)):
        for k in range(3):
            local[t][k] = keys.setdefault((int(welded_ids[tris[t][k]]), round(float(uv[t][k][0]), 2),
                                           round(float(uv[t][k][1]), 2)), len(keys))
    parent = list(range(len(keys)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b, c in local:
        parent[find(a)] = find(b)
        parent[find(b)] = find(c)
    pieces = {}
    for t in range(len(tris)):
        pieces.setdefault(find(local[t][0]), []).append(t)
    pieces = sorted(pieces.values(), key=len, reverse=True)
    low = uv.reshape(-1, 2).min(0)
    shape = tuple(int(v) + 2 for v in (uv.reshape(-1, 2).max(0) - low) * LAYER_SCALE)
    layers = []
    for members in pieces:
        mask = raster(uv[members], low, shape)
        for layer in layers:
            if not (layer["mask"] & mask).any():
                layer["mask"] |= mask
                layer["members"] += members
                break
        else:
            layers.append({"mask": mask, "members": list(members)})
    for layer in layers[1:]:
        if len(layer["members"]) < MIN_LAYER:
            layers[0]["members"] += layer["members"]
    layers = [layers[0]] + [layer for layer in layers[1:] if len(layer["members"]) >= MIN_LAYER]
    out = []
    for i, layer in enumerate(layers):
        members = np.array(layer["members"])
        out.append((label if i == 0 else f"{label} {i + 1}", tris[members], uv[members]))
    return out


islands = [piece for island in islands for piece in layered(*island)]
print("islands after layers", len(islands))


# Pack: each island at the same scale with a row above it for its name. A left island sits beside its right island.
# Blocks go in rows, the tallest first.
LABEL = 18


def mirror_key(label):
    return label.replace("right ", "").replace("left ", "")


def pack(scale):
    blocks = {}
    for index, (label, tris, uv) in enumerate(islands):
        blocks.setdefault(mirror_key(label), []).append(index)
    sizes = {}
    for index, (label, tris, uv) in enumerate(islands):
        flat = uv.reshape(-1, 2)
        sizes[index] = (flat.max(0) - flat.min(0)) * scale + [0, LABEL]
    order = sorted(blocks.values(), key=lambda members: -max(sizes[i][1] for i in members))
    x = y = PADDING
    row_height = 0
    placed = [None] * len(islands)
    for members in order:
        width = sum(sizes[i][0] for i in members) + PADDING * (len(members) - 1)
        if x + width + PADDING > SIZE:
            x = PADDING
            y += row_height + PADDING
            row_height = 0
        for i in members:
            label, tris, uv = islands[i]
            low = uv.reshape(-1, 2).min(0)
            placed[i] = (label, tris, (uv - low) * scale + [x, y + LABEL])
            x += sizes[i][0] + PADDING
            row_height = max(row_height, sizes[i][1])
    return placed, y + row_height + PADDING


scale = 40.0
while True:
    placed, height = pack(scale)
    if height <= SIZE:
        break
    scale *= 0.98
connected = 0
for label, tris, uv in placed:
    keys = {}
    parent = []

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    local = []
    for t, tri in zip(tris, uv):
        ids = []
        for k in range(3):
            key = (int(welded_ids[t[k]]), round(float(tri[k][0]), 1), round(float(tri[k][1]), 1))
            if key not in keys:
                keys[key] = len(parent)
                parent.append(len(parent))
            ids.append(keys[key])
        parent[find(ids[0])] = find(ids[1])
        parent[find(ids[1])] = find(ids[2])
        local.append(ids[0])
    connected += len({find(i) for i in local})
print("texels per unit", round(scale, 2), "islands", len(placed), "connected pieces", connected)

# Overlap: how much of the area more than one triangle covers, each triangle shrunk a little so that neighbours
# do not touch, at 4 times the size
FINE = 4


def coverage(uv_sets):
    cover = np.zeros((SIZE * FINE, SIZE * FINE), dtype=np.int16)
    for uv in uv_sets:
        for tri in uv:
            centre = tri.mean(0)
            shrunk = (centre + (tri - centre) * 0.85) * FINE
            mask = Image.new("L", (SIZE * FINE, SIZE * FINE), 0)
            ImageDraw.Draw(mask).polygon([tuple(p) for p in shrunk], fill=1)
            cover += np.asarray(mask, dtype=np.int16)
    return (cover > 1).sum() / max(1, (cover > 0).sum())


if "--overlap" in sys.argv:
    for label, tris, uv in placed:
        print("  ", label, len(tris), "tris overlap", f"{100 * coverage([uv]):.1f}%")
print("overlap of the layout", f"{100 * coverage([uv for _, _, uv in placed]):.1f}%")
# Stretch: the spread of each triangle's UV area against its area in the model
ratios = []
for label, tris, uv in placed:
    for t, tri in zip(tris, uv):
        a3 = np.linalg.norm(np.cross(positions[t[1]] - positions[t[0]], positions[t[2]] - positions[t[0]])) / 2
        a2 = abs(np.cross(tri[1] - tri[0], tri[2] - tri[0])) / 2
        if a3 > 1e-6:
            ratios.append(a2 / (a3 * scale * scale))
ratios = np.array(ratios)
print("area ratio median", round(float(np.median(ratios)), 2), "10-90%", np.round(np.percentile(ratios, [10, 90]), 2))

# One vertex for each original vertex in each island corner position
pairs = {}
vmapping = []
uv_list = []
new_triangles = []
for island, (label, tris, uv) in enumerate(placed):
    for t, tri_uv in zip(tris, uv):
        corner_indices = []
        for k in range(3):
            key = (int(t[k]), island, round(float(tri_uv[k][0]), 3), round(float(tri_uv[k][1]), 3))
            if key not in pairs:
                pairs[key] = len(vmapping)
                vmapping.append(int(t[k]))
                uv_list.append(tri_uv[k] / SIZE)
            corner_indices.append(pairs[key])
        new_triangles.append(corner_indices)
vmapping = np.array(vmapping)
uvs = np.array(uv_list, dtype=np.float32)
new_triangles = np.array(new_triangles, dtype=np.uint16)
print("vertices", len(positions), "->", len(vmapping))

new_attributes = {}
for name, index in prim["attributes"].items():
    if name == "TEXCOORD_0":
        continue
    a = doc["accessors"][index]
    new_attributes[name] = append(read(index)[vmapping], a["componentType"], a["type"], 34962,
                                  bounds=(name == "POSITION"))
new_attributes["TEXCOORD_0"] = append(uvs, 5126, "VEC2", 34962)
prim["attributes"] = new_attributes
prim["indices"] = append(new_triangles.reshape(-1), 5123, "SCALAR", 34963)
prim.pop("targets", None)
mesh.pop("weights", None)
mesh.pop("extras", None)
for material in doc.get("materials", []):
    texture = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if texture is not None:
        texture["texCoord"] = 0
doc["buffers"][0]["byteLength"] = len(binary)
doc["asset"]["generator"] = "openblack experiment region unwrap of " + doc["asset"].get("generator", "")
json_bytes = json.dumps(doc, separators=(",", ":")).encode()
json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
while len(binary) % 4:
    binary.append(0)
total = 12 + 8 + len(json_bytes) + 8 + len(binary)
out = struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
out += struct.pack("<II", len(binary), 0x004E4942) + bytes(binary)
(out_dir / "undead_cow.glb").write_bytes(out)

# The layout: a faint grid, each triangle's edges thin, the island borders (seams) red, each island named
layout = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(layout)
for g in range(0, SIZE + 1, 64):
    draw.line([(g, 0), (g, SIZE)], fill=(128, 128, 128, 60), width=1)
    draw.line([(0, g), (SIZE, g)], fill=(128, 128, 128, 60), width=1)
font = ImageFont.load_default(size=14)
for label, tris, uv in placed:
    edges = {}
    for t, tri in zip(tris, uv):
        draw.polygon([tuple(p) for p in tri], fill=(255, 255, 255, 40))
        for a, b in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted(((int(welded_ids[t[a]]), tuple(np.round(tri[a], 1))),
                               (int(welded_ids[t[b]]), tuple(np.round(tri[b], 1))))))
            edges.setdefault(key, []).append((tri[a], tri[b]))
    for key, uses in edges.items():
        if len(uses) > 1:
            draw.line([tuple(uses[0][0]), tuple(uses[0][1])], fill=(40, 40, 40, 255), width=1)
    for key, uses in edges.items():
        if len(uses) == 1:
            draw.line([tuple(uses[0][0]), tuple(uses[0][1])], fill=(220, 30, 30, 255), width=2)
    low = uv.reshape(-1, 2).min(0)
    short = (label.replace("ribs and spine", "ribs").replace("right ", "R ").replace("left ", "L ")
             .replace("upper arm", "arm").replace(" (inside)", " in").replace(" (top)", " top").replace(" (bottom)", " btm"))
    draw.text((low[0], low[1] - 3), short, fill=(20, 60, 200, 255), font=font, anchor="ld")
layout.save(out_dir / "undead_cow_uv_layout.png")

cell = SIZE // 16
yy, xx = np.mgrid[0:SIZE, 0:SIZE]
dark = ((yy // cell) + (xx // cell)) % 2 == 0
checker = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
checker[..., :3] = np.where(dark[..., None], 110, 170)
checker[..., 3] = 255
Image.fromarray(checker, "RGBA").save(out_dir / "undead_cow_texture.png")
print("wrote", out_dir)
