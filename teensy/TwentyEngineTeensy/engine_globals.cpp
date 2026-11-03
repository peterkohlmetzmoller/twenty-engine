// one definition of the per-thread state used by the generated engine
#include "ksoloti_host.h"
namespace ms20 {
MS20_TLS SysexSender g_sysexSender = nullptr;
MS20_TLS void *g_sysexUser = nullptr;
MS20_TLS uint32_t g_ks_rng = 0x12345678u;
}
