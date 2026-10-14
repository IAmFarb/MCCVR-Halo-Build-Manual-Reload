"""Generate Halo 4 chest-grenade meshes and textures from H4EK tool.exe exports.

Input: the folder written by tools/h4ek/vrmod_export_grenades.ps1 (official
H4EK `export-tag-to-xml` render_model/material XML and `export-bitmap-tga`
textures). Output: src/common/weapon_grenade_models.generated.h (model
records) and src/common/weapon_grenade_meshes.generated.h (vertices in metres,
BC1 texture slices). Original tag files never enter the repository.

Usage: python tools/re/generate_grenade_models.py <vrmod_export folder>
"""
import hashlib
import json
import math
import re
import sys
from io import BytesIO
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from reload_tag_xml import read, block, value, values, descendant_blocks  # noqa: E402

SIZE = 256
# Native H4 grenade type order (H4EK GameGlobalsGrenadeList): frag, plasma, pulse.
GRENADES = [
    ("frag", "storm_frag_grenade"),
    ("plasma", "storm_plasma_grenade"),
    ("pulse", "storm_energy_drain_grenade"),
]


def floats(text):
    out = [float(x) for x in text.split(",")]
    assert all(math.isfinite(x) for x in out)
    return out


def number(v):
    assert math.isfinite(v)
    text = f"{v:.7g}"
    return text + ("f" if "." in text or "e" in text else ".0f")


def model_geometry(path):
    tree = read(path)
    compression = list(descendant_blocks(tree, "compression info")[0])[0]
    flags = int(value(compression, "compression flags", "0"))
    packed = floats(value(compression, "position bounds 0")) + floats(value(compression, "position bounds 1"))
    bounds = [packed[i:i + 2] for i in (0, 2, 4)]
    uv_bounds = [floats(value(compression, "texcoord bounds " + a)) for a in "01"]
    selected = set()
    for region in block(tree, "regions"):
        permutations = block(region, "permutations")
        if len(permutations):
            start = int(value(permutations[0], "mesh index"))
            selected.update(range(start, start + int(value(permutations[0], "mesh count"))))
    meshes = list(descendant_blocks(tree, "meshes")[0])
    raws = list(descendant_blocks(tree, "per mesh temporary")[0])
    shaders = [value(m, "render method", "").split(",")[0].strip() for m in block(tree, "materials")]
    triangles = []  # (positions[3], uvs[3], material)
    for index, raw in enumerate(raws):
        if index not in selected:
            continue
        header = meshes[index]
        points, uvs = [], []
        for vertex in block(raw, "raw vertices"):
            p = floats(value(vertex, "position"))
            if flags & 1:
                p = [lo + t * (hi - lo) for t, (lo, hi) in zip(p, bounds)]
            uv = floats(value(vertex, "texcoord"))
            if flags & 2:
                uv = [lo + t * (hi - lo) for t, (lo, hi) in zip(uv, uv_bounds)]
            points.append(p)
            uvs.append(uv)
        indices = [int(value(i, "word")) & 0xFFFF for i in block(raw, "raw indices")]
        strip = "strip" in value(header, "index buffer type", "")
        assert not strip, (path, "strip indices not expected for grenade meshes")
        for part in block(header, "parts"):
            start = int(value(part, "index start", "0"))
            count = int(value(part, "index count", "0"))
            material = int(value(part, "render method index", "0").rsplit(",", 1)[-1] or 0)
            for at in range(start, start + count - count % 3, 3):
                tri = indices[at:at + 3]
                if len(set(tri)) == 3:
                    triangles.append(([points[i] for i in tri], [uvs[i] for i in tri], material, tri))
    return dict(bounds=bounds, shaders=shaders, triangles=triangles,
                checksum=int(value(tree, "runtime import info checksum", "0")) & 0xFFFFFFFF)


def parameter(tree_text, name):
    at = tree_text.find('value="%s"' % name)
    if at < 0:
        return None
    segment = tree_text[at:at + 2500]
    bitmap = re.search(r'name="bitmap" value="([^"]*)"', segment)
    color = re.search(r'name="color" value="([^"]*)"', segment)
    return dict(bitmap=bitmap.group(1).split(",")[0] if bitmap else "",
                color=floats(color.group(1)) if color else None)


def material_texture(export, shader):
    name = shader.replace("/", "\\").split("\\")[-1]
    text = (export / (name + ".material.xml")).read_text(encoding="utf-8", errors="replace")
    color_map = parameter(text, "color_map")
    assert color_map and color_map["bitmap"], (shader, "no color_map")
    leaf = color_map["bitmap"].split("\\")[-1]
    image_path = export / f"{leaf}-{leaf}_00_00.tga"
    tint = [1.0, 1.0, 1.0]
    albedo = parameter(text, "albedo_tint")
    if albedo and albedo["color"]:
        tint = albedo["color"][1:]
    front = parameter(text, "front_color")
    illum = parameter(text, "si_color")
    return dict(material=name, image=image_path, tint=tint,
                front=front["color"][1:] if front and front["color"] else None,
                self_illum=illum["color"][1:] if illum and illum["color"] else None)


def slice_image(info, kind):
    image = Image.open(info["image"]).convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    tint = list(info["tint"])
    gain = 1.0
    if kind == "frag":
        gain = 2.2  # dark olive metal is lifted by specular in game; no specular here
    elif kind == "plasma":
        tint = [c * 1.3 for c in (info["front"] or tint)]
    elif kind == "plasma_glow":
        tint = [0.45, 0.70, 1.25]  # glowing core band
    elif kind == "pulse":
        gain = 2.0
    scale = [min(2.0, t * gain) for t in tint]
    channels = [c.point(lambda v, s=s: min(255, round(v * s))) for c, s in zip(image.split(), scale)]
    return Image.merge("RGB", channels)


def main():
    export = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out/vrmod_export"
    vertices, records, slices, evidence = [], [], [], []
    for kind, tag in GRENADES:
        path = export / (tag + ".render_model.xml")
        geometry = model_geometry(path)
        lows = [b[0] for b in geometry["bounds"]]
        highs = [b[1] for b in geometry["bounds"]]
        center = [(lo + hi) / 2 for lo, hi in zip(lows, highs)]
        material_slices = {}
        for index, shader in enumerate(geometry["shaders"]):
            info = material_texture(export, shader)
            role = kind
            if kind == "plasma" and "illum" in info["material"]:
                role = "plasma_glow"
            material_slices[index] = len(slices)
            slices.append(slice_image(info, role))
            evidence.append(dict(grenade=tag, material=info["material"], texture=info["image"].name,
                                 texture_sha256=hashlib.sha256(info["image"].read_bytes()).hexdigest(),
                                 slice=material_slices[index], role=role))
        # Smooth normals per authored vertex index (Halo axes: x fwd, y left, z up).
        def remap(p):
            q = [(p[a] - center[a]) * 3.048 for a in range(3)]
            return [-q[1], q[2], -q[0]]
        accum = {}
        faces = []
        for points, uvs, material, tri in geometry["triangles"]:
            ps = [remap(p) for p in points]
            u = [ps[1][a] - ps[0][a] for a in range(3)]
            v = [ps[2][a] - ps[0][a] for a in range(3)]
            n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
            length = math.sqrt(sum(x * x for x in n))
            if length < 1e-12:
                continue
            n = [x / length for x in n]
            for i in tri:
                a = accum.setdefault((material, i), [0.0, 0.0, 0.0])
                for k in range(3):
                    a[k] += n[k]
            faces.append((ps, uvs, material, tri, n))
        first = len(vertices)
        for ps, uvs, material, tri, face in faces:
            for p, uv, i in zip(ps, uvs, tri):
                n = accum[(material, i)]
                length = math.sqrt(sum(x * x for x in n))
                n = [x / length for x in n] if length > 1e-9 else face
                assert all(abs(x) < 0.5 for x in p), (tag, p)
                # The slice rides in u (+100 per slice); the shader splits it.
                vertices.append((p, n, (uv[0] + 100.0 * material_slices[material], uv[1])))
        count = len(vertices) - first
        assert count and count % 3 == 0
        lows_m = [min(v[0][a] for v in vertices[first:]) for a in range(3)]
        highs_m = [max(v[0][a] for v in vertices[first:]) for a in range(3)]
        records.append((kind, tag, geometry["checksum"], first, count, lows_m, highs_m))
    # BC1 slices stacked vertically: each 256x256 slice is contiguous.
    atlas = Image.new("RGB", (SIZE, SIZE * len(slices)))
    for n, image in enumerate(slices):
        atlas.paste(image, (0, n * SIZE))
    encoded = BytesIO()
    atlas.save(encoded, format="DDS", pixel_format="DXT1")
    dds = encoded.getvalue()
    assert dds[:4] == b"DDS " and dds[84:88] == b"DXT1"
    pixels = dds[128:]
    assert len(pixels) == SIZE * SIZE * len(slices) // 2
    lines = ["// Generated by tools/re/generate_grenade_models.py from H4EK exports.",
             "#pragma once", "namespace weapon_model {",
             "// Halo 4 grenade meshes in metres, native type order (frag, plasma, pulse).",
             f"inline constexpr unsigned kGrenadeModelCount={len(records)};",
             "inline constexpr Model kGrenadeModels[kGrenadeModelCount]{"]
    for kind, tag, checksum, first, count, low, high in records:
        lines.append('    {GameTitle::Halo4,0x%08Xull,"%s",false,1,%d,%d,{%s},{%s}},' % (
            checksum, tag, first, count, ",".join(map(number, low)), ",".join(map(number, high))))
    lines += ["};", "}", ""]
    (ROOT / "src/common/weapon_grenade_models.generated.h").write_text("\n".join(lines))
    lines = ["// Generated by tools/re/generate_grenade_models.py from H4EK exports.",
             "// Vertices in metres; u carries the texture slice (+100 per slice).",
             "#pragma once", "#include <cstdint>", "namespace weapon_model {",
             f"inline constexpr unsigned kGrenadeSurfaceSize={SIZE},kGrenadeSurfaceSlices={len(slices)};",
             "inline constexpr Vertex kGrenadeVertices[]{"]
    for p, n, uv in vertices:
        lines.append("    {{%s},{%s},{%s}}," % (",".join(map(number, p)), ",".join(map(number, n)),
                                                ",".join(map(number, uv))))
    lines += ["};", "inline constexpr uint8_t kGrenadeSurfaceBlocks[]{"]
    lines += ["    " + ",".join(str(b) for b in pixels[i:i + 32]) + "," for i in range(0, len(pixels), 32)]
    lines += ["};", "}", ""]
    (ROOT / "src/common/weapon_grenade_meshes.generated.h").write_text("\n".join(lines))
    (ROOT / "docs/GRENADE-MODEL-EVIDENCE-2026-10-13.json").write_text(json.dumps(dict(
        models=[dict(kind=k, tag=t, checksum=f"{c:08X}", first=f, vertices=n) for k, t, c, f, n, _, _ in records],
        slices=evidence, bc1_sha256=hashlib.sha256(pixels).hexdigest()), indent=2) + "\n")
    atlas.save(ROOT / "out/grenade-surfaces.png") if (ROOT / "out").exists() else None
    print(json.dumps(dict(models=len(records), vertices=len(vertices), slices=len(slices))))


if __name__ == "__main__":
    main()
