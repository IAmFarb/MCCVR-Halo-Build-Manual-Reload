# First-run feature setup, right-chest grenade, blocker logging, 2026-10-13

## Evidence (player headset test, Halo 4, Steam, SteamVR/Oculus Link)

- Player report with video: neither the hip magazine nor the chest grenade
  appeared or responded; empty magazines reloaded automatically.
- The player's earlier logs from this branch show no `Manual reload`,
  `Native reload policy`, `Step-by-step reload:` or `Chest grenades:` lines:
  Manual Reload was off in their existing `halomccvr.cfg`, so the step-by-step
  reload, its disable-auto/skip-animation policy and (default off) chest
  grenades never ran. Halo 4's binding reader verified, so routing would have
  had the Reload and Grenade transports.
- The player asked for the grenade on the right chest and for no automatic
  reload: an empty gun must wait for a hip magazine.

## Changes

1. `Config_ApplyFeatureSetup()` (called by the DLL right after the config
   loads): a config without `fork_feature_setup >= 1` gets Manual Reload,
   Step-by-step reload, Disable automatic reload and Chest grenades switched
   on once, then saved with `fork_feature_setup = 1`. Afterwards the F1
   switches are the player's own; nothing is re-enabled. `ConfigLoad` itself
   and its defaults are unchanged, so legacy-config tests keep their meaning.
2. Chest grenade sits `weapon_grenade_chest_side_m` (default 0.10 m, range
   -0.30..0.30) toward the weapon hand: the right chest for right-handed play,
   mirrored for left-handed. F1 slider "Grenade side (m, + = weapon-hand side)".
3. Log lines `Step-by-step reload:` and `Chest grenades:` gain
   `blockedBy=[...]`, naming what kept the gesture from taking input on the
   latest frame (grips inactive, not in gameplay, no held weapon model seen,
   Reload not on a face button, layout not read, menu/pause, dual wield...).
   Printed on change, at most every 5 s.

No hook, signature, native read or write changed. Disable-auto still depends on
each title's verified `Native reload policy` install line.

## Local verification

`tests/grenade_gesture_tests.cpp`: 44 checks, 0 failures (new: right chest,
left-handed mirror, body-direction yaw, centre/clamp/invalid offset, empty left
chest, model drawn on the right chest). Realistic reload: 277 checks, 0
failures. `config.cpp`, `dllmain.cpp`, `menu.cpp`, `vr.cpp` pass the
llvm-mingw Windows-header syntax check. MSVC Release build and full CTest run
on the GitHub Windows runner.

## Headset test

Halo 4 (or any title), Steam or Store:
1. Shoot a magazine empty: the gun should stay empty, no automatic reload.
2. Tap Reload: the magazine drops; take one from the left hip with the left
   grip, push it in where the old one came out, pull the charging handle.
3. Look down: a small green grenade sits on the right side of the chest.
   Grab it (either hand), switch type with A/B or X/Y, throw it.
4. Send `HaloMCCVR.log`; the `blockedBy=` field explains any refusal.

## Follow-up: free throw (same day)

Player request: after taking the grenade, throw it wherever the arm throws,
not where the gun points; X/Y switch type in the left hand, A/B in the right
(already implemented: each fresh press on the holding controller is one native
Switch Grenade press, so Halo cycles only through types the player carries).

- `grenade_gesture::State` records the unit release velocity of a throw as
  `aimDirection` and reports `aimActive` from release until
  `kThrowAimHoldMs` (800 ms) after the native throw press, which now waits
  `kThrowAimLeadMs` (150 ms) after release so the aim can turn first. Pause,
  dual wield, tracking gaps or switching it off end both the throw and the aim.
- vr.cpp publishes the direction (yaw/pitch bits, space epoch, renewed every
  input frame, expiring 100 ms after the last renewal);
  `VR_GetGrenadeThrowAim` is lock-free and allocation-free.
- `Game_ComputeAimStick` (the shared closed-loop aim for Halo 3, ODST, Reach
  and Halo 4) substitutes that direction, from the head, for the hand ray
  while it is active. It emits the same right-stick deflection the loop
  always does; no hook, signature, native read or write is added. The visible
  gun and reticle keep following the hand.
- Config `weapon_grenade_free_throw` (default on), F1 "Free throw".
  Halo still decides throw speed and grenade physics. Titles without the
  closed loop (Halo 2 on foot, possibly CE) keep the gun's aim.

Grenade suite: 69 checks, 0 failures. game.cpp, vr.cpp, menu.cpp, config.cpp
pass the llvm-mingw Windows-header syntax check.

## Follow-up: scene-matched accessory lighting (same day)

Player report: the hip magazine (Halo 4 `storm_assault_rifle`, the kit-authored
part) "wasn't lit correctly and didn't match the environmental lighting". The
accessory pass draws after the game picture with a fixed key light, so it stays
bright in dark rooms.

`WeaponAccessoryRenderer::DrawMany` gains `SceneLighting{enabled,capture}`.
With capture (eye 0, whose command list executes first), it copies a 256x256
region of the finished eye picture centred on each item into a private
256x256x6 array, generates mips, and the pixel shader reads the 1x1 average:
display-space luminance / 0.40 sets exposure (0.06..1.3) with a 35 % tint
toward the local colour; sRGB/float targets are converted both ways. The
other eye reuses that sample so both eyes match. Unsupported formats, MSAA or
odd views keep the old fixed lighting; native state is restored by the
command list as before. Config `weapon_accessory_scene_lighting` (default on),
F1 "Magazines match the scene's lighting". The renderer suite adds a WARP
check that the same part over a dark picture renders much darker than over a
bright one, and that the scene binding does not leak to the native context.

## Follow-up: gesture melee on, quiet during reload strokes and throws

Player request: shaking either arm at an enemy should melee. Alpha 0.7.0
already has "Gesture melee (experimental)" (F1 > Body & Hands): a hand speed
crossing `physical_melee_swing_speed` (default 5 m/s) presses the game's own
melee binding through the verified binding reader (Halo 3, ODST, Reach, Halo 4,
Halo 2 when armed). Setup version 2 switches it on once.

Because a grenade throw or a charging-handle pull is also a fast hand motion,
vr.cpp now publishes `VR_MeleeGestureQuiet(leftController)`: both controllers
stay quiet while a step-by-step reload is out of Idle (+400 ms), and the
grenade's controller while holding (+300 ms) and for 600 ms after a throw or
put-back. `Game_GestureMeleeInput` latches a quiet hand, so it must settle
below the threshold before it can strike. Contact ("true physical") melee is
unchanged and still takes priority when selected.
