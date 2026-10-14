# Reach and Halo 4 direct action bridge default off, 2026-10-13

## Evidence (player headset logs, Steam edition, SteamVR/Oculus Link)

- Reach and Halo 4 campaign loads crashed less than one second after the
  level started. In both, `Native VR action bridge <title>: verified hooks
  installed` was logged 0.4-0.8 s before a native first-chance access
  violation inside the title module with no HaloMCCVR frame on the stack:
  Reach `haloreach.dll+65BA9` read of `0x4`; Halo 4 `halo4.dll+ABB47` write
  of `0x0`. The game stopped presenting and MCC raised its fatal error.
- Both faulting stacks sit just after each title's matched native
  player-control builder range (Reach `5DF80..60F12`, H4 `A2670..A55EE`).
- Turning **Automatic VR action routing** off removed the crash.
- An unmodified Alpha 0.7.0 source build (control, same compiler) crashed the
  same way, so the step-by-step reload/grenade changes are not involved.
- The player's "official" comparison DLL was Alpha 0.5.1 (`f53f0bd`), which
  predates the 0.7.0 bridges. Official 0.7.0 itself was not tested.
- The compiled Reach hooks forward the converter's six arguments and the
  reader's two correctly (disassembly checked), so this is not a calling
  convention miscompile. The root cause inside the titles is not established.

## Change

`vr_native_action_bridge_<title>` (F1 > VR Mappings, "Direct action bridge for
this game (experimental)") gates installation and use of each title's direct
bridge, and retires installed hooks when switched off. Defaults: H3, ODST, CE,
H2 on (0.7.0 behaviour); Reach and Halo 4 off. With a bridge off, Automatic VR
action routing still reads the game's own layout and routes through the
controller transport, as before 0.7.0; Reload and Use may share a button.

Code is disabled, not deleted. Headset confirmation in Reach and Halo 4 with
routing on is required; a Halo 3 regression with its bridge on is still owed.
