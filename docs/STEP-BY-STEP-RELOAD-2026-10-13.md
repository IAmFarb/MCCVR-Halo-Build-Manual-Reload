# Step-by-step (Titanfall-style) reload candidate, 2026-10-13

Requested by a player: make reloading work like the Titanfall 2 VR mod, with
the reserve at the hip, for every reloadable weapon in every title this build
supports (CE, H2, H3, ODST, Reach, H4). Per-weapon behaviour comes from
`MANUAL-RELOAD-WEAPON-REFERENCE-2026-10-13.md`. Not headset tested; do not
advance `CURRENT-STATE.md`.

## Player behaviour

Enable **Manual Reload**, then **Step-by-step reload (eject, hip, insert, rack)**
in the Reload section of F1. Both default off.

1. **Tap Reload.** The magazine (or cell/drum) drops out of the gun and falls
   to the floor, then disappears after a few seconds. The trigger does nothing
   until the reload is finished (optional).
2. **Take a new one from your hip.** Support hand to the support-side hip, then
   squeeze grip. The kit-derived part (or a token) is in your hand.
3. **Push it in where the old one came out.** No release needed; it seats with
   a click (vibration). Let go of it anywhere else and it falls; take another.
4. **Finish the gun's action** with the support hand: pull back the charging
   handle (rifles, SMGs, sniper, Spiker), pull back the slide (pistols), pump
   back and forward (shotguns), or push the bar/cover closed (rocket launcher,
   railgun, sticky detonator, grenade launcher, concussion rifle). Only then is
   Halo's normal reload requested, with its own animation skipped.
- **Shotguns:** take and insert shells one at a time, then pump. Halo 2's
  shotgun and sniper have no finishing step (Halopedia).
- **Hold Reload** (longer than 0.35 s) for Halo's own Reload/Use/pick up. A
  long hold also cancels a step-by-step reload in progress.
- **Plasma, battery and melee weapons:** unchanged; Reload passes straight to Halo.
- Dual wielding, vehicles, menus, pause, cutscenes, death and lost tracking
  keep stock controls and cancel a reload in progress.

## Settings (`halomccvr.cfg`)

| Key | Default | Meaning |
|---|---|---|
| `manual_reload_realistic` | 0 | Step-by-step reload (requires `manual_reload`) |
| `manual_reload_finish_step` | 1 | Require handle/slide/pump/cover step |
| `manual_reload_block_fire` | 1 | Trigger withheld while the part is out |
| `weapon_finish_radius_m` | 0.12 | Grab size of the handle/pump zone |
| `weapon_finish_travel_m` | 0.06 | Pull/push distance along the gun |
| `weapon_drop_floor_m` | 1.70 | Assumed floor distance below the head |
| `weapon_drop_linger_s` | 4.0 | Seconds a dropped part stays visible |

Existing pouch location/offsets, grab radius and insertion radius are reused.
While on, the native policy also requests disabled automatic reload and the
full reload-animation skip (same hooks and fallbacks as before).

## Implementation

- `src/common/realistic_reload_logic.h`: pure, allocation-free state machine
  (Idle, Ejected, Holding, Inserted, Finishing), per-weapon recipe table keyed
  by catalogued model name, falling-part pool and presentations.
- `vr.cpp`: runs beside the accepted gesture state (which keeps holsters; its
  instant pouch reload and needle shake are off while this is on). The raw VR
  control bound to Reload (semantic mapping or legacy letter) is withheld during
  a tap and passed through after the hold threshold. The Fire control is
  withheld while the part is out. The completed reload uses the same native
  button pulse transport and freshness checks as the accepted gesture.
- Reload bound to a bumper or trigger cannot be withheld without also taking
  grip/aim/melee away; the log reports `UNSUPPORTED` and stock input remains.
- `WeaponAccessoryRenderer::DrawMany`: hip/held part plus up to three parts on
  the floor in one command list and one self-depth clear. `Draw` delegates to it.
- New 2 x 2 x 7 cm red shell token (token vertices 36..71).
- Native ammo, inventory, reserve and timing remain Halo's. Recipes are gesture
  design data, never native bindings; no new hook or address.

## Known limits

- The native gun still draws its own magazine; only the VR copy drops out.
- Dropped parts live in tracking space with an assumed floor height; they are
  not occluded by world geometry and stay with the play space when moving.
- The reserve part is shown even with no reserve ammo; Halo then refuses the
  reload as before. Ejecting a full magazine and reinserting is a no-op.
- Shotgun shells are counted for the player only; Halo's reload, once
  requested, still loads until full.
- Handle/port positions are controller-relative defaults, not authored per gun.
- Needler and Halo 4 Forerunner insertion points are unconfirmed.

## Headset test steps (plain language)

1. F1 > Reload: turn on Manual Reload and Step-by-step reload.
2. Halo 3, Assault Rifle: tap Reload. A magazine should fall from the gun to the
   floor and vanish after about 4 seconds. Pull the trigger: nothing fires.
3. Reach to your hip with your other hand and squeeze grip. A magazine should be
   in your hand. Push it to the bottom of the gun. You should feel a click.
4. Put your other hand on top of the gun and squeeze grip, then pull back about
   a hand's width. The rifle should reload and fire again.
5. Shotgun: tap Reload, load two or three shells from the hip one at a time,
   then grip under the barrel and pump back and forward.
6. Rocket launcher: tap Reload, take the tube pod from the hip, put it at the
   back of the launcher, then push forward at the back to close it.
7. Hold Reload near a dropped weapon: it should pick up normally.
8. Plasma rifle: Reload should behave exactly as before.
9. Repeat a rifle, pistol and shotgun in CE, H2, ODST, Reach and H4. Note any
   weapon where the hip part, insertion spot or handle spot feels wrong, and
   what the game's own reload animation does for the Needler and Forerunner guns.
10. Send `HaloMCCVR.log` (lines starting `Step-by-step reload:`), with title,
    weapon, Steam or Store, headset and runtime.

## Local verification

`tests/realistic_reload_tests.cpp` (new CTest `halomccvr_realistic_reload_tests`)
covers recipes for every catalogued model, tap/hold, eject, grab, seat, drop and
regrab, pull/pump/push strokes in the gun's frame (body motion rejected), Halo 2
exceptions, plasma passthrough, cancellation, unsupported sources, dual wield,
fire blocking, falling parts and presentations. Run with clang on Linux: 277
checks, 0 failures; the existing weapon-interaction suite still passes. All
touched DLL sources and the affected test suites pass a Windows-header
syntax/semantic check with llvm-mingw clang. The MSVC Release build, full CTest
and Reach gate still need to run on Windows via `tools/package-candidate.ps1`.
