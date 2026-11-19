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

## Régis feature list (Teensy user, Nov 2026) - promised as an update
- Function switch: a separate button does all the momentary-switch shortcuts; the iC switch then only acts as the original (patch bay SWITCH). Engine change, all editions (option).
- Buttons: ESP layer toggle, mono/poly (as the KBD CV -> VCO1+2 CV cable), tap tempo (as SWITCH -> ESP SIGNAL IN), retrigger (MS20_Voices multitrig, exists, default off).
- Wheel: faster smoothing (mws += 0.004 per tick at 3 kHz = about 80 ms); setting to turn wheel vibrato off. Ask Régis what his new wheel pot sends at rest.
- Delay: wet level capped at about 0.62 (bw = mx^2 * 0.62 in BBD_Delay); make it louder or a setting.
- Sounds remember their cables (used until a cable is touched)? Design decision.
- Make VCO1+2 CV IN a real pitch input for both VCOs, and INITIAL GAIN active (VCA open).
- Régis follow-up: also a PANEL button (has 5 momentary buttons, may add more) -> one function list, any function on any pin. Wheel calibrated to 0 at rest; wants wheel vibrato OFF (uses wheel as patch source). Cables in presets: yes please, plus a "cables only" combo that reads the current cables into the recalled sound. He'll make a YouTube video of the update (@studi_ohm).
