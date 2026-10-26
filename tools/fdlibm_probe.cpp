// Dumps fdlibm port outputs for differential testing.
// usage: fdlibm_probe <func> < inputs.txt   (one hex bit pattern per line; for 2-arg funcs two per line)
#include <cstdio>
#include <cstring>
#include "core/fdlibm.h"
#include "core/FloatBits.h"
using namespace py;
int main(int argc,char**argv){
  if(argc<2){std::fprintf(stderr,"usage: %s func\n",argv[0]);return 2;}
  const char*f=argv[1]; unsigned a,b; char line[128];
  while(std::fgets(line,sizeof line,stdin)){
    int k=std::sscanf(line,"%x %x",&a,&b); if(k<1)continue;
    float x=bitsf(a),y=k>1?bitsf(b):0.f,r=0;
    if(!std::strcmp(f,"sinf"))r=fdlibm::sinf(x); else if(!std::strcmp(f,"cosf"))r=fdlibm::cosf(x);
    else if(!std::strcmp(f,"tanf"))r=fdlibm::tanf(x); else if(!std::strcmp(f,"asinf"))r=fdlibm::asinf(x);
    else if(!std::strcmp(f,"acosf"))r=fdlibm::acosf(x); else if(!std::strcmp(f,"atanf"))r=fdlibm::atanf(x);
    else if(!std::strcmp(f,"atan2f"))r=fdlibm::atan2f(x,y); else if(!std::strcmp(f,"logf"))r=fdlibm::logf(x);
    else if(!std::strcmp(f,"expf"))r=fdlibm::expf(x); else if(!std::strcmp(f,"powf"))r=fdlibm::powf(x,y);
    else if(!std::strcmp(f,"sqrtf"))r=fdlibm::sqrtf(x); else if(!std::strcmp(f,"fmodf"))r=fdlibm::fmodf(x,y);
    else {std::fprintf(stderr,"unknown func\n");return 2;}
    std::printf("%08x\n",fbits(r));
  }
}
