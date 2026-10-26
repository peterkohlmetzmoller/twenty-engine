// one definition of the per-thread state used by the generated engine
#include "ksoloti_host.h"
namespace ms20 {
thread_local SysexSender g_sysexSender = nullptr;
thread_local void *g_sysexUser = nullptr;
thread_local uint32_t g_ks_rng = 0x12345678u;
}
