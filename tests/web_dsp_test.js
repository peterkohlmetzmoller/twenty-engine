// web_dsp_test.js - the web edition's sound module (web/dsp.cpp -> dsp.wasm) runs the engine: a key sounds, a
// knob changes the sound, a sound saved on the keys lands in the memory file and comes back in a new page.
//   node tests/web_dsp_test.js <dsp.wasm>
const fs = require('fs');
let fails = 0; const check = (c, m) => { console.log(`  [${c ? 'PASS' : 'FAIL'}] ${m}`); if (!c) fails++; };
const mod = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const make = () => { const i = new WebAssembly.Instance(mod, { wasi_snapshot_preview1: { proc_exit: (c) => { throw new Error('exit ' + c); } } }); i.exports._initialize(); return i.exports; };
const rms = (ex, blocks) => { let s = 0, n = 0; for (let b = 0; b < blocks; b++) { ex.te_process(128); const L = new Float32Array(ex.memory.buffer, ex.te_left(), 128); for (const v of L) { s += v * v; n++; } } return Math.sqrt(s / n); };
console.log('The engine in WebAssembly');
const ex = make(); ex.te_init(44100);
check(ex.te_nparams() === 64, `64 parameters (${ex.te_nparams()})`);
const quiet = rms(ex, 200);
ex.te_midi(0x90, 57, 100); const on = rms(ex, 400);
console.log(`    level: ${quiet.toFixed(5)} silent, ${on.toFixed(4)} with a key held (44.1 kHz)`);
check(quiet < 1e-4 && on > 0.01, 'a key sounds');
const LPF = 29; ex.te_set(LPF, 2 << 21); const dark = rms(ex, 400); ex.te_set(LPF, 60 << 21); const bright = rms(ex, 400);
console.log(`    LPF cutoff down ${dark.toFixed(4)}, up ${bright.toFixed(4)}`);
check(bright > dark * 1.3, 'a knob (LPF CUTOFF) changes the sound');
ex.te_midi(0x80, 57, 0); rms(ex, 400);
let leds = 0; while (ex.te_led_pop() > 0) leds++;
check(leds > 0, `LED messages for the MS-20ic (${leds})`);
console.log('Saving a sound on the keys');
ex.te_midi(0xD0, 127, 0); rms(ex, 4); ex.te_midi(0x90, 48 + 7, 100);                 // switch held, key 8 held...
let saved = 0; for (let b = 0; b < 1200 && !saved; b++) { rms(ex, 1); saved = ex.te_mem_poll(); }   // ...2 s: saved
ex.te_midi(0x80, 48 + 7, 0); ex.te_midi(0xD0, 0, 0); rms(ex, 10);
check(saved > 6000, `the memory file is written (${saved} bytes)`);
const file = new Uint8Array(ex.memory.buffer, ex.te_mem_ptr(), saved).slice();
check(String.fromCharCode(...file.slice(0, 4)) === '12SM' || String.fromCharCode(...file.slice(0, 4)).includes('MS'), 'the MS20MEM.BIN format (MS21)');
const ex2 = make(); ex2.te_init(48000);
new Uint8Array(ex2.memory.buffer, ex2.te_mem_ptr(), saved).set(file);
check(ex2.te_mem_load(saved) === 1, 'a new page loads it');
ex2.te_set(LPF, 30 << 21); rms(ex2, 10);
ex2.te_midi(0xD0, 127, 0); rms(ex2, 4); ex2.te_midi(0x90, 48 + 7, 100); rms(ex2, 4); ex2.te_midi(0x80, 48 + 7, 0); rms(ex2, 4); ex2.te_midi(0xD0, 0, 0); rms(ex2, 20);
console.log(`    LPF cutoff after recalling sound 8: ${(ex2.te_get(LPF) / 2097152).toFixed(1)} (saved at 60.0)`);
check(Math.abs(ex2.te_get(LPF) / 2097152 - 60) < 0.6 && ex2.te_mem_slot() === 7, 'sound 8 comes back with its knobs');
console.log(fails ? `WEB DSP CHECKS FAILED (${fails})` : 'ALL WEB DSP CHECKS PASSED (0 failed)');
process.exit(fails ? 1 : 0);
