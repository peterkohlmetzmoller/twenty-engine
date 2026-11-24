// worklet.js - Twenty Engine in the browser, the sound: runs the engine (dsp.wasm) in the browser's audio thread.
// The page sends MIDI and the panel's knob changes; this sends back the engine's values (the MS-20ic's knobs move
// the panel), the LEDs (for the iC and the panel) and the memory file after a sound was saved on the keys.
class TwentyEngineProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions;
    const exit = (c) => { throw new Error('engine stopped (' + c + ')'); };
    this.ex = new WebAssembly.Instance(o.module, { wasi_snapshot_preview1: { proc_exit: exit } }).exports;
    if (this.ex._initialize) this.ex._initialize();
    this.ex.te_init(sampleRate);
    this.np = this.ex.te_nparams();
    if (o.mem && o.mem.length) {                            // the sounds saved in this browser
      new Uint8Array(this.ex.memory.buffer, this.ex.te_mem_ptr(), o.mem.length).set(o.mem);
      this.ex.te_mem_load(o.mem.length);
    }
    this.sent = new Int32Array(this.np).fill(0x7fffffff);
    this.tick = 0;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.t === 'midi') this.ex.te_midi(m.s, m.d1, m.d2);
      else if (m.t === 'set') this.ex.te_set(m.i, m.v);
      else if (m.t === 'lights') this.ex.te_light_show();
    };
  }
  process(inputs, outputs) {
    const out = outputs[0], n = out[0].length, ex = this.ex;
    ex.te_process(n);
    out[0].set(new Float32Array(ex.memory.buffer, ex.te_left(), n));
    if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.te_right(), n));
    // LEDs: every message (the iC needs each one)
    let len;
    while ((len = ex.te_led_pop()) > 0) this.port.postMessage({ t: 'led', b: new Uint8Array(ex.memory.buffer, ex.te_led_buf(), len).slice() });
    // about 30 times a second: the engine's values that changed, a saved memory file
    if (++this.tick >= 12) {
      this.tick = 0;
      const ch = [];
      for (let i = 0; i < this.np; i++) { const v = ex.te_get(i); if (v !== this.sent[i]) { this.sent[i] = v; ch.push(i, v); } }
      if (ch.length) this.port.postMessage({ t: 'values', v: ch, slot: ex.te_mem_slot() });
      const saved = ex.te_mem_poll();
      if (saved > 0) this.port.postMessage({ t: 'mem', b: new Uint8Array(ex.memory.buffer, ex.te_mem_ptr(), saved).slice() });
    }
    return true;
  }
}
registerProcessor('twenty-engine', TwentyEngineProcessor);
