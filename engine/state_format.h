// state_format.h - the plugin's saved state (projects, presets), as far as the layout of its parameters goes.
// iPlug2 reads the parameters in order without a count. Since 1.1 the state starts with a header ('TEP2' and the
// number of parameters). A state saved before has none: 1.0.x saved 58 parameters (engine v50c), the 1.1 test
// builds 63 or 64. After the parameters comes the takes block (int32 length + bytes, nearly always 0), then in
// Twenty Engine UI the window's block ('TEU1'); VST3 adds a 4-byte bypass flag after the plugin's own data.
#pragma once
#include <cstdint>
#include <cstring>

namespace ms20 {

static const int32_t kStateMagicTEP2 = 0x32504554;   // 'TEP2'

inline int32_t StateI32(const uint8_t* d, int at) { int32_t v; memcpy(&v, d + at, 4); return v; }

// how many parameters a state stores: from the header, or (no header) from the layout. *paramsAt: where they start.
inline int StateParamCount(const uint8_t* d, int size, int startPos, int numParams, int* paramsAt)
{
  if (startPos + 8 <= size && StateI32(d, startPos) == kStateMagicTEP2)
  {
    const int32_t n = StateI32(d, startPos + 4);
    if (n > 0 && n <= 4096) { if (paramsAt) *paramsAt = startPos + 8; return n; }
  }
  if (paramsAt) *paramsAt = startPos;
  const int cands[] = { numParams, 64, 63, 58 };
  for (int np : cands)
  {
    const int p = startPos + 8 * np;
    if (p + 4 > size) continue;
    const int32_t n = StateI32(d, p);
    if (n < 0 || n > (1 << 24) || p + 4 + n > size) continue;
    const int end = p + 4 + n, rest = size - end;
    const int32_t next = rest >= 4 ? StateI32(d, end) : 0;
    if (rest == 0 || rest == 4 || next == 0x54455531) return np;   // nothing, VST3's bypass, or the window's block
  }
  return 58;
}

} // namespace ms20
