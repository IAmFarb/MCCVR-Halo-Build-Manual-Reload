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
