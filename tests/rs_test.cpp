#define MS20HOST_TESTING
#include "../engine/ms20_host.h"
#include <vector>
#include <stdio.h>
namespace ms20 { thread_local SysexSender g_sysexSender = 0; thread_local void *g_sysexUser = 0; thread_local uint32_t g_ks_rng = 1; }
using namespace ms20;
static double amp(const std::vector<float>&x,double f,double sr){double c=0,s=0;int N=x.size();for(int i=0;i<N;i++){double w=0.5-0.5*cos(2*M_PI*i/(N-1));c+=w*x[i]*cos(2*M_PI*f*i/sr);s+=w*x[i]*sin(2*M_PI*f*i/sr);}return 2*sqrt(c*c+s*s)/(N*0.5);}
int main(){ int fails=0;
 for(double sr:{44100.0,96000.0}){ MS20Host h; h.reset(sr);
  printf("48 kHz -> %.0f Hz:\n",sr);
  for(double f:{1000.0,10000.0,15000.0,19000.0,20000.0,23000.0}){
   int N=48000; std::vector<float> in(N); for(int i=0;i<N;i++) in[i]=0.5f*sinf(2*M_PI*f*i/48000.0);
   std::vector<float> out(N*2); int no=h.resampleTest(in.data(),N,out.data(),(int)(N*sr/48000)-64); out.resize(no);
   std::vector<float> seg(out.begin()+1000,out.end()-1000);
   double fo=f; bool alias=(f>sr/2); if(alias) fo=sr-f;
   double a=amp(seg,fo,sr)/0.5; double resid=0; // everything except the tone
   printf("  %5.0f Hz tone: %s %6.2f dB\n",f,alias?"alias at":"level",20*log10(a+1e-12));
   if(!alias&&f<=19000&&fabs(20*log10(a))>0.1) fails++;
   if(alias&&20*log10(a)>-60) fails++;
  }}
 printf(fails?"SOME RESAMPLER CHECKS FAILED\n":"RESAMPLER OK: flat to 19 kHz (within 0.1 dB), aliases below -60 dB\n"); return fails;}
