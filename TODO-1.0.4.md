# 1.0.4 plan

## Power-on and switch (from Alberto, Win10 / Live 12)
- Merge the nrpn branch (reorder CC 99/98/6 per block, power-on recognition) once Alberto confirms the knobs take over after volume off/on. Remove the "Last burst" readout before merging.
- CC 7 = 0 counts as power off (the host may drop the NRPN power-off).
- Reversed switch on Mac too (Windows has it in 1.0.3).
- Manual: a recalled sound may need a second key press the first time (first-press limitation).

## MG on the patch panel (Instagram user, Nov 2026)
Report: MG/T.EXT alone did not seem to move the filter; patched, the MG is always a triangle whatever the WAVEFORM knob says.

Findings (engine test, tests not committed):
- Internal MG -> LPF/HPF works without cables. The iC's VCF MG/T.EXT knobs send CC 85 (LPF) and CC 30 (HPF), confirmed in the real iC dump.
- MG TRI OUT and MG SQ OUT are fixed triangle/square (O_MG_Morphing_LFO: outlet_tri = sw, outlet_sq = sq). WAVEFORM only shapes the internal MG.
- TOTAL replaces the MG on all three MG/T.EXT knobs, so MG TRI OUT -> TOTAL gives a triangle whatever WAVEFORM says.
- Likely causes of "no modulation": a cable left in TOTAL, MG FREQUENCY near 0, or pickup after a memory recall (stored 0, knob turned down does nothing until it meets).

Plan:
- MG TRI OUT follows the WAVEFORM knob (same wave as the internal MG), like the original MS-20 and Korg's plugin. MG SQ OUT stays square (S&H clock).
- Update the manual line "MG TRI OUT and MG SQ OUT on the patch panel always give triangle and square".
- Debug with the user: Mac or Windows, iC or host knobs, anything in TOTAL, MG FREQUENCY position.

## Teensy: key click setting and extra switches (ohm, Nov 2026) - done on main
- KEY_CLICK (0 off, 6 standard, 64 max, same scale as the plugin) and SWITCH_NEXT / PREV / PANEL / MONO pins in the SETTINGS block. Default = unchanged behaviour. Compiles in CI; messages checked on the desktop engine.
- Guide: manual/teensy_guide.py. Rebuild the shipped .hex with the next release.
