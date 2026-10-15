"""Unwraps the undead cow glb (which has no UVs), writes a copy with TEXCOORD_0, a UV layout PNG and a checker
placeholder texture. Usage: unwrap_glb.py <in.glb> <out_dir>"""
import json
import struct
import sys
from pathlib import Path

import numpy as np
import xatlas
from PIL import Image, ImageDraw

SIZE = 1024
src, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
out_dir.mkdir(parents=True, exist_ok=True)
data = src.read_bytes()
json_len = struct.unpack_from("<I", data, 12)[0]
doc = json.loads(data[20:20 + json_len])
bin_start = 20 + json_len + 8
bin_len = struct.unpack_from("<I", data, 20 + json_len)[0]
binary = bytearray(data[bin_start:bin_start + bin_len])

COMPONENT = {5126: ("f4", 4), 5123: ("u2", 2), 5121: ("u1", 1), 5125: ("u4", 4)}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def read(index):
    a = doc["accessors"][index]
    view = doc["bufferViews"][a["bufferView"]]
    kind, _ = COMPONENT[a["componentType"]]
    offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    n = WIDTH[a["type"]]
    return np.frombuffer(binary, dtype="<" + kind, count=a["count"] * n, offset=offset).reshape(a["count"], n)


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


mesh = doc["meshes"][0]
prim = mesh["primitives"][0]
positions = read(prim["attributes"]["POSITION"]).astype(np.float32)
normals = read(prim["attributes"]["NORMAL"]).astype(np.float32)
indices = read(prim["indices"]).reshape(-1, 3).astype(np.uint32)

# The mesh's vertices are split at hard normals; weld them by position so that the islands follow the surface
_, welded = np.unique(np.round(positions, 4), axis=0, return_inverse=True)
welded = welded.reshape(-1)
welded_positions = np.zeros((welded.max() + 1, 3), dtype=np.float32)
welded_positions[welded] = positions
atlas = xatlas.Atlas()
atlas.add_mesh(welded_positions, welded[indices].astype(np.uint32))
pack = xatlas.PackOptions()
pack.resolution = SIZE
pack.padding = 4
pack.bilinear = True
chart = xatlas.ChartOptions()
chart.max_cost = 16.0
chart.max_iterations = 4
atlas.generate(chart_options=chart, pack_options=pack)
_, atlas_indices, atlas_uvs = atlas[0]
assert len(atlas_indices) == len(indices)
# Each corner is the original vertex with the UV of its island: one vertex per pair
pairs = {}
vmapping = []
uv_list = []
new_indices = np.zeros_like(indices)
for t in range(len(indices)):
    for k in range(3):
        key = (int(indices[t][k]), int(atlas_indices[t][k]))
        if key not in pairs:
            pairs[key] = len(vmapping)
            vmapping.append(key[0])
            uv_list.append(atlas_uvs[key[1]])
        new_indices[t][k] = pairs[key]
vmapping = np.array(vmapping)
uvs = np.array(uv_list, dtype=np.float32)
print("vertices", len(positions), "->", len(vmapping), "triangles", len(new_indices), "charts", atlas.chart_count)

# Every attribute follows the vertex it was split from; the morph targets are all zero and are left out
new_attributes = {}
for name, index in prim["attributes"].items():
    values = read(index)[vmapping]
    a = doc["accessors"][index]
    target = 34962
    new_attributes[name] = append(values, a["componentType"], a["type"], target, bounds=(name == "POSITION"))
new_attributes["TEXCOORD_0"] = append(uvs.astype(np.float32), 5126, "VEC2", 34962)
prim["attributes"] = new_attributes
prim["indices"] = append(new_indices.astype(np.uint16).reshape(-1), 5123, "SCALAR", 34963)
prim.pop("targets", None)
mesh.pop("weights", None)
mesh.pop("extras", None)
for material in doc.get("materials", []):
    texture = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if texture is not None:
        texture["texCoord"] = 0
doc["buffers"][0]["byteLength"] = len(binary)
doc.setdefault("asset", {})["generator"] = "openblack experiment unwrap (xatlas) of " + doc["asset"].get("generator", "")

json_bytes = json.dumps(doc, separators=(",", ":")).encode()
json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
while len(binary) % 4:
    binary.append(0)
total = 12 + 8 + len(json_bytes) + 8 + len(binary)
out = struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
out += struct.pack("<II", len(binary), 0x004E4942) + bytes(binary)
(out_dir / "undead_cow.glb").write_bytes(out)

# The UV layout: every triangle's edges thin, the edges on the border of each island (the seams) thick
layout = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(layout)
pixels = uvs * SIZE
edges = {}
for tri in new_indices:
    for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
        key = (min(a, b), max(a, b))
        edges[key] = edges.get(key, 0) + 1
for tri in new_indices:
    draw.polygon([tuple(pixels[i]) for i in tri], fill=(255, 255, 255, 40))
for (a, b), count in edges.items():
    if count > 1:
        draw.line([tuple(pixels[a]), tuple(pixels[b])], fill=(40, 40, 40, 255), width=1)
for (a, b), count in edges.items():
    if count == 1:
        draw.line([tuple(pixels[a]), tuple(pixels[b])], fill=(220, 30, 30, 255), width=2)
layout.save(out_dir / "undead_cow_uv_layout.png")

# The placeholder: a plain grey checker the game draws until a painted texture replaces it
cell = SIZE // 16
checker = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
yy, xx = np.mgrid[0:SIZE, 0:SIZE]
dark = ((yy // cell) + (xx // cell)) % 2 == 0
checker[..., :3] = np.where(dark[..., None], 110, 170)
checker[..., 3] = 255
Image.fromarray(checker, "RGBA").save(out_dir / "undead_cow_texture.png")
print("wrote", out_dir)
