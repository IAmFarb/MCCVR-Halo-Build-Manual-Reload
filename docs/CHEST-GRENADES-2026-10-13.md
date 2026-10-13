# Chest grenades candidate, 2026-10-13

Player request: a spot on the chest to grab grenades, switch grenade type with
A/B (right hand) or X/Y (left hand) while holding one, and throw it physically
like the Titanfall 2 VR mod. All six titles. Not headset tested; do not
advance `CURRENT-STATE.md`.

## Player behaviour

Enable **Chest grenades (grab, switch, throw)** in F1 > Reload. Off by default.

- A small dark-green grenade marker floats at the middle of the chest
  (12 cm forward, 0.32 m below the head; follows body direction).
- Squeeze grip there with **either hand** to take a grenade. The off hand is
  checked first. It vibrates.
- While holding it, **A or B** (grenade in the right hand) or **X or Y** (left
  hand) presses Halo's **Switch Grenade** once per press. Those buttons do not
  jump/reload/etc. while the grenade is held, and stay hidden until released.
- **Throw:** move the hand forward and let go of grip. If the hand is moving at
  least 1.5 m/s (adjustable) in a forward direction, Halo's **Throw Grenade**
  is pressed. Letting go slowly or backwards puts the grenade back.
- The trigger of the hand holding the grenade is withheld while holding.
- Normal grenade buttons keep working. Dual wielding, vehicles, menus, pause,
  cutscenes and lost tracking keep stock controls and cancel a held grenade.

## What Halo still decides

Grenade count, the selected type, the throw animation, and the trajectory:
the grenade flies along the game's aim (your weapon direction), not along the
arm's path. Throw strength is not passed to the game. Changing that would need
per-title projectile evidence; none exists yet. The marker does not show which
type is selected; Halo's HUD does.

## Settings (`halomccvr.cfg`)

| Key | Default | Meaning |
|---|---|---|
| `weapon_grenades` | 0 | Enable chest grenades |
| `weapon_grenade_chest_down_m` | 0.32 | Chest centre below the head |
| `weapon_grenade_zone_radius_m` | 0.14 | Grab size |
| `weapon_grenade_throw_speed_mps` | 1.5 | Hand speed at release that counts as a throw |
| `weapon_grenade_button_<title>` | 6 (LT) | Legacy mapping only: MCC Throw Grenade button |
| `weapon_grenade_switch_button_<title>` | 2 (LB) | Legacy mapping only: MCC Switch Grenade button |

With VR Mappings on (the default), throw and switch use each title's verified
native action transport, like the reload and holster gestures. The legacy
button keys only matter with `vr_action_mapping = 0` and should match the MCC
controller layout (0=X, 1=RB, 2=LB, 3=B, 4=Y, 5=A, 6=LT, 7=RT).

## Implementation

`src/common/grenade_gesture_logic.h` (pure, allocation-free): chest zone,
fresh-grip grab per hand, grip ownership through release, per-controller face
button edges queued as separate 100 ms native presses with 70 ms gaps (a throw
waits for queued switches), release velocity from the last ~80 ms of hand
positions, held-button draining, and the marker/hand presentation.

`vr.cpp`: runs after the holster/reload gestures and only takes grips they did
not claim that frame. A held grenade owns its controller's face buttons, so the
step-by-step reload ignores a Reload button on that controller. Pulses use the
existing `weaponButtons` transport and freshness checks (now also admitted when
only chest grenades are on). The renderer draws up to six items per eye. New
5 x 6 x 5 cm grenade token (token vertices 72..107).

## Headset test steps (plain language)

1. F1 > Reload: turn on Chest grenades.
2. Look down: a small green block should sit in front of your chest.
3. Put your left hand on it and squeeze grip. It should be in your hand.
4. Press X: the grenade type on Halo's HUD should change. Press Y: it changes
   again. You should not reload or switch weapons.
5. Throw forward and let go: Halo should throw the selected grenade.
6. Take another, then let go slowly at your chest: nothing should be thrown.
7. Repeat with the right hand using A and B; you should not jump.
8. Try in each game you play. Report if the chest spot feels too high/low, if
   throws are missed or too easy, and which games ignore the switch.
9. Send `HaloMCCVR.log` lines starting `Chest grenades:`.

## Local verification

`tests/grenade_gesture_tests.cpp` (CTest `halomccvr_grenade_gesture_tests`):
36 checks, 0 failures with clang on Linux, covering zone, fresh grip, busy
grips, both hands and left-handed mode, button ownership/draining, press
queueing and order, throw/put-back/backwards, cancellation and presentation.
Touched DLL sources pass the llvm-mingw Windows-header syntax/semantic check.
MSVC Release build, full CTest and Reach gate must still run on Windows.
