# Halo 4 real chest-grenade models, 2026-10-13

Player request: replace the mod-authored grenade block with the game's own
grenade, and show the type the player is carrying.

## Source and reproduction

1. Install Halo 4 Mod Tools (H4EK) from Steam and run `H4EK (Extract).bat`.
2. Copy `tools/h4ek/VRMod Export Grenades.bat` and
   `tools/h4ek/vrmod_export_grenades.ps1` into the H4EK folder and run the
   .bat. It calls the official `tool.exe export-tag-to-xml` for the frag
   (`storm_frag_grenade`), plasma (`storm_plasma_grenade`), pulse
   (`storm_energy_drain_grenade`, which uses the disruption grenade's
   `storm_disruption_grenade_blinn` material) render models, materials,
   projectiles and equipment, and `export-bitmap-tga` for their bitmaps, into
   `H4EK\vrmod_export`. It only reads tags.
3. `python tools/re/generate_grenade_models.py <vrmod_export>` writes
   `src/common/weapon_grenade_models.generated.h` (records, native type order
   frag 0, plasma 1, pulse 2 per H4EK `GameGlobalsGrenadeList`) and
   `src/common/weapon_grenade_meshes.generated.h` (vertices in metres, BC1
   texture slices), plus `docs/GRENADE-MODEL-EVIDENCE-2026-10-13.json` with
   texture hashes. Original tags/bitmaps are not committed.

Geometry: first permutation of each region, compressed positions and texcoords
expanded with the tag's own bounds, centred, x3.048 m/wu and remapped to the
accessory frame exactly like the reload magazines; smooth normals per authored
vertex. Sizes: frag 0.19 x 0.26 m, plasma 0.18 m, pulse 0.18 m (Halo scale).

Texture: each material's `color_map` (authored UVs repeat, so wrap sampling),
resampled to a 256x256 BC1 slice. Lighting there is specular-driven, so the
bake applies `albedo_tint`, lifts the dark frag metal x2.2 and the pulse x2,
tints the plasma body with its `front_color` and its illum band blue. The
texture slice rides in u (+100 per slice); the shader splits it.

## Runtime

`weapon_model::GrenadeModel(title,type)`: Halo 4 shows the mesh for the native
type (unknown = frag); other titles keep the token until their kits are
exported. The accessory renderer has a third vertex buffer, a BC1 texture
array (t2) and a wrap sampler (s1); `objectP.w` = 1 marks grenade meshes. Scene
lighting applies to them like the magazines.

## Carried type (pending evidence)

The native selected grenade type and counts are not yet read. Diagnostic:
on each chest-grenade switch and throw in Halo 4, the engine-thread contact
update snapshots the local player's unit datum (0xC00 bytes, SEH-guarded, read
only) before and after, and the poll logs it as `Halo 4 grenade diagnostic`
plus `H4G` hex lines. Offsets that track switching and counts will come from
those logs; until then the frag is shown.
