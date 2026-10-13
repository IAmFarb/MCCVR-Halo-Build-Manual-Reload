# Per-weapon manual reload reference (all six titles), 2026-10-13

Design reference for the Titanfall-style manual reload: press Reload and the
part comes out, take a fresh one from the hip pouch, insert it where the old
one came out, then finish the weapon's own action (charging handle, slide,
pump, cover). The goal is that the player physically mimics each weapon's
real Halo reload.

This is a **gesture design table**, not native evidence. It never selects a
native address, offset, action number or animation. Halo still decides
whether a reload is possible and performs the ammo transfer.

## Sources and how much to trust them

1. **Official editing-kit node names (strongest).** `RELOAD-CATALOG-EVIDENCE-2026-09-16.json`
   lists every first-person weapon model's animated nodes, extracted from the
   official HCEEK/H2EK/H3EK/H3ODSTEK/HREK/H4EK tags. A node exists because the
   weapon's own animations move it, so names such as `magazine`, `ophandle`
   (operating/charging handle), `charging_handle`, `slide`, `lever` (slide
   release), `pump`, `bullet`, `tubes`, `push_bar`, `drum`, `battery`, `hatch`,
   `ammo_cap`, `chamber` and `cartridge` identify the moving reload parts.
   The catalogue's `receiver_anchor` is the kit's rest position of the
   detachable part, i.e. exactly where the new part must be inserted.
2. **Halopedia articles (supporting).** Used for loading order and lore rules
   (links at the end). Halopedia rarely describes animations frame by frame.
3. **Not documented anywhere I could reach:** the Needler family's insertion
   point and every Halo 4 Forerunner reload. These are marked *check in game*;
   the user's headset view of the native animation is the reference.

## Gesture vocabulary

| Step | Player action | Used for |
|---|---|---|
| Eject | Press Reload. The detachable part drops out and falls to the floor. | Every reloadable weapon with a detachable part |
| Grab | Support hand to the hip pouch, squeeze grip. | All reloads |
| Insert | Move the held part to the insertion point and release grip. | All reloads |
| Pull back | Support hand grips the handle/slide/pump and pulls toward you. | Charging handle, operating handle, slide, bolt, pump |
| Push | Support hand pushes the cover/bar closed, away from you. | Rocket launcher push bar, hatches/covers |
| Repeat | Grab and insert once per round. | Shotguns |
| None | No manual reload; native overheating/battery rules apply. | Plasma/energy/battery weapons, melee, objectives |

Lore rule worth matching (Halopedia, M6D and SRS99 articles): the pistol's
slide release and the CE sniper's charging handle are only needed after the
weapon ran dry. The mod cannot yet read the native loaded-round count, so the
finish step is required on every reload when enabled (Titanfall behaviour).
"Only when empty" needs a verified per-title ammo reader first.

## Halo: Combat Evolved

| Weapon | Kit nodes | Part out / in | Finish |
|---|---|---|---|
| Pistol (M6D) | slide, slide_release, chamber_button, magazine | Magazine, grip | Pull back slide (slide release) |
| Assault Rifle (MA5B) | magazine, ophandle | Magazine | Pull back operating handle |
| Sniper Rifle (S2 AM) | magazine, ophandle, bullet | 4-round magazine | Pull back charging handle (CE locks it back when empty) |
| Shotgun (M90) | bullet, magazine (loading gate), pump | Shells, one at a time | Pump (CE works the action after a fully empty reload) |
| Rocket Launcher (M41) | tubes, push_bar | Twin-tube unit | Push the bar closed |
| Needler | needles only | Crystal cluster, *check in game* | None |
| Flamethrower | ammo, cover | Fuel tank, *check in game* | Push cover closed |
| Fuel Rod Gun | (arms rig) | Rod clip, *check in game* | None |
| Plasma Rifle / Pistol | — | No reload (overheat) | — |

## Halo 2 (Classic and Anniversary)

| Weapon | Kit nodes | Part out / in | Finish |
|---|---|---|---|
| Magnum (M6C) | slide, barrel, bullet, magazine | Magazine, grip | Pull back slide |
| SMG (M7) | charging_handle, magazine | Magazine | Pull back charging handle |
| Battle Rifle (BR55) | magazine, ophandle | Magazine, behind the grip | Pull back operating handle |
| Sniper Rifle (S2 AM) | magazine, ophandle, bullet | Magazine | None (Halo 2 config needs no extra step) |
| Shotgun (M90) | bullet, magazine, pump | Shells, one at a time | None (Halo 2 does not work the action) |
| Rocket Launcher | tubes, push_bar | Twin-tube unit | Push the bar closed |
| Covenant Carbine | battery, hood | Cell pops out; insert new cell | None |
| Brute Shot | ammo01-04, ammo_feeder, ammo_cap | Belt of 4 grenades, in through the top | None |
| Fuel Rod Gun | ammo_top, ammo_bottom, cowling | Clip of 5 rods via the top port | None |
| Needler | needles | Crystal cluster, *check in game* | None |
| Beam Rifle | ammo | No reload (battery) | — |
| Plasma Rifle, Brute Plasma Rifle, Plasma Pistol, Sentinel Beam, Energy Sword | — | No reload | — |

## Halo 3 and Halo 3: ODST

| Weapon | Kit nodes | Part out / in | Finish |
|---|---|---|---|
| Assault Rifle (MA5C) | magazine, ophandle | Magazine | Pull back operating handle |
| Battle Rifle (BR55HB) | magazine, ophandle | Magazine, behind the grip | Pull back operating handle |
| SMG / silenced SMG (ODST) | charging_handle, magazine | Magazine | Pull back charging handle |
| Magnum (H3) / Automag (ODST) | slide, lever, magazine | Magazine, grip | Pull back slide (lever = slide release) |
| Sniper Rifle | magazine, ophandle, handle, bullet | Magazine | Pull back operating handle |
| Shotgun (M90A) | bullet, button, magazine (gate), pump | Shells, one at a time | Pump |
| Rocket Launcher | tubes, push_bar | Twin-tube unit | Push the bar closed |
| Covenant Carbine | battery, hood | Cell drops to the floor (Halopedia: it lies briefly, then vanishes) | None |
| Spiker | ammo_drum, slide | Drum, underneath between the blades and trigger | Pull back slide |
| Mauler | drum, piston | Drum | None |
| Brute Shot | ammo01-04, ammo_feeder, ammo_cap | Belt of 6 grenades, in through the top | None |
| Fuel Rod Gun | ammo_top, ammo_bottom | Clip of 5 rods via the top port | None |
| Needler | needles | Crystal cluster, *check in game* | None |
| Spartan Laser, Beam Rifle, Plasma Rifle/Pistol, Sentinel Beam, Hammer, Sword | — | No reload | — |

## Halo: Reach

| Weapon | Kit nodes | Part out / in | Finish |
|---|---|---|---|
| Magnum (M6G) | slide, lever, magazine | Magazine, grip | Pull back slide |
| Assault Rifle (MA37) | magazine, ophandle | Magazine | Pull back operating handle |
| DMR (M392) | magazine, ophandle | Magazine | Pull back operating handle |
| Sniper Rifle | magazine, ophandle, handle, bullet | Magazine | Pull back operating handle |
| Shotgun (M45) | bullet, button, magazine, pump | Shells, one at a time | Pump |
| Grenade Launcher (M319) | chamber, handle, grenade | One grenade into the breech (break action) | Push the breech closed |
| Rocket Launcher | tubes, push_bar | Twin-tube unit | Push the bar closed |
| Spiker | ammo_drum, slide | Drum, underneath | Pull back slide (Reach adds a bolt) |
| Needle Rifle | needles, scope | Top module flips open; canister out/in at the top | None |
| Needler | needles | Crystal cluster, *check in game* | None |
| Concussion Rifle | hatch, drum, drum flaps | Open hatch, insert drum | Push hatch closed |
| Fuel Rod Gun | ammo_top, ammo_bottom | Canister via the top port | None |
| Plasma Pistol/Rifle/Repeater, Plasma Launcher, Focus Rifle, Spartan Laser, Target Locator | — | No reload (overheat/battery), *confirm Plasma Launcher in game* | — |

## Halo 4

| Weapon | Kit nodes | Part out / in | Finish |
|---|---|---|---|
| Assault Rifle (MA5D) | magazine, ophandle, eject_panel | Magazine | Pull back operating handle |
| Battle Rifle (BR85) | magazine, ophandle | Magazine | Pull back operating handle |
| DMR (M395) | magazine, ophandle | Magazine | Pull back operating handle |
| Magnum (M6H) | slide, lever, magazine | Magazine, grip | Pull back slide |
| Sniper Rifle (S2 AM) | magazine, ophandle, handle, bullet | Magazine | Pull back operating handle |
| SAW (M739) | magazine | Box magazine | None |
| Shotgun (M45D) | bullet, magazine, pump | Shells, one at a time | Pump |
| Railgun (ARC-920) | cover_01/02, cartridge, bullet | One slug into the barrel/breech | Push cover closed |
| Sticky Detonator | grenade, sight | One grenade, break-open | Push closed |
| Rocket Launcher | tubes, push_bar | Twin-tube unit | Push the bar closed |
| Covenant Carbine | ammo, ammo_cover | Cover opens; cell out/in | None |
| Concussion Rifle | hatch, drum, flaps | Open hatch, insert drum | Push hatch closed |
| Fuel Rod Cannon | ammo_top, ammo_bottom | Canister via the top port | None |
| Needler | needles | Crystal cluster, *check in game* | None |
| Suppressor (`forerunner_smg`) | shells, chamber, cartridge, power cores | Hard-light cartridge, *check in game* | None |
| LightRifle (`forerunner_rifle`) | cartridge, ammo | Hard-light cartridge, *check in game* | None |
| Scattershot (`spread_gun`) | cartridge, floating segments | Hard-light cartridge, *check in game* | None |
| Boltshot (likely `stasis_pistol`) | cartridge, capsules, plates | Hard-light cartridge, *check in game* | None |
| Binary Rifle (`forerunner_sniper_rifle`) | spinners, flaps | *check in game* | None |
| Incineration Cannon | cylinders, flaps | *check in game* | None |
| Beam Rifle | flaps, vents | *check in game whether it reloads or only overheats* | — |
| Plasma Pistol, Storm Rifle (likely `assault_carbine`), Spartan Laser, Target Designator, Sword, Hammer | — | No reload | — |

Tag-name mappings marked "likely" are inferred from node names and Halopedia's
cut-weapon article; the runtime selects by catalogued model identity, so a
wrong guess only changes which gesture recipe a weapon gets.

## What this table can and cannot drive

- Insertion point: catalogued `receiver_anchor` when the kit has a
  detachable part; otherwise a stated controller-local default per recipe
  (top port, rear tube end, breech, shell port).
- Falling part: only catalogued detachable meshes (magazines, cells, drums).
  Shells, belts, tube pods and cartridges use the existing generic or
  Promethean token in the hand and do not leave a floor copy.
- The native gun keeps drawing its own magazine; hiding a native node needs
  separate per-title evidence. Use "Skip native reload animation" so Halo does
  not replay a second reload after the player's own.
- Shotguns: each inserted shell is counted for the player, but the native
  reload, once requested, still loads until full. True one-shell-per-insert
  needs evidence about each engine's reload continuation.

## References

- Halopedia: [M6D pistol](https://www.halopedia.org/M6D_Personal_Defense_Weapon_System),
  [SRS99C-S2 AM sniper rifle](https://www.halopedia.org/SRS99C-S2_AM_sniper_rifle),
  [M90 shotgun](https://www.halopedia.org/M90_shotgun),
  [M41 SPNKr](https://www.halopedia.org/M41_SPNKr),
  [Brute Shot](https://www.halopedia.org/Brute_Shot),
  [Fuel rod gun](https://www.halopedia.org/Type-33_Light_Anti-Armor_Weapon),
  [Covenant carbine](https://www.halopedia.org/Type-51_carbine),
  [Spiker](https://www.halopedia.org/Type-25_carbine),
  [Mauler](https://www.halopedia.org/Type-52_pistol),
  [Needle rifle](https://www.halopedia.org/Gadulo-pattern_needle_rifle),
  [M319 grenade launcher](https://www.halopedia.org/M319_Individual_Grenade_Launcher),
  [ARC-920 railgun](https://www.halopedia.org/Asymmetric_Recoilless_Carbine-920),
  [Beam rifle](https://www.halopedia.org/Type-50_Sniper_Rifle_System),
  [Cut Halo 4 weapons](https://www.halopedia.org/Cut_Halo_4_weapons).
- Titanfall 2 VR reference behaviour (eject, grab, insert, cock; shotgun
  shells individually): [Aftermath](https://aftermath.site/titanfall-2-vr-mod-circuitlord/),
  [Road to VR](https://www.roadtovr.com/titanfall-2-vr-mod-circuitlord/).
- Kit node evidence: `docs/RELOAD-CATALOG-EVIDENCE-2026-09-16.json`.
