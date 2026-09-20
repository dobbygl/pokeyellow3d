#include "presentation_blend.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* message) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main() {
    presentation::Blend fade;
    check(!fade.begin(true,false,0,false)&&!fade.running,"first presentation needs no invented predecessor");
    check(fade.begin(false,false,20,true)&&fade.running&&fade.progress()==0,"outgoing starts at the completed source");
    for(uint32_t now=40;now<=200;now+=20) {
        check(!fade.begin(false,false,now,true),"stable target does not replace the source");
        check(fade.running&&fade.elapsed==float(now-20),"200 ms linear wall clock");
    }
    fade.begin(false,false,220,true);check(!fade.running&&fade.progress()==1,"outgoing endpoint after 200 ms");
    check(fade.begin(true,false,240,true),"incoming uses a fresh source");
    fade.begin(true,false,260,true);float previous=fade.elapsed;
    fade.begin(false,true,5000,true);check(fade.running&&fade.target&&fade.elapsed==previous,"pause preserves direction and elapsed time");
    fade.begin(true,false,5016,true);check(fade.elapsed==previous,"resume never counts time spent paused");
    check(fade.begin(false,false,5032,true)&&fade.elapsed==0,"reversal uses current mixed presentation as its source");
    fade.begin(false,false,10000,true);check(fade.elapsed==25&&fade.running,"a driver stall cannot skip the whole fade");
    presentation::Blend wrap;
    wrap.begin(false,false,0xffffffe0u,false);wrap.begin(true,false,0xfffffff0u,true);
    wrap.begin(true,false,0,true);check(wrap.elapsed==16,"unsigned SDL clock wrap");
    presentation::Blend missing;
    missing.begin(false,false,0,false);missing.begin(true,false,10,false);
    check(missing.target&&!missing.running,"unavailable history keeps a direct valid presentation");
    std::puts("PASS: endpoints, pause/resume, reversal, clock wrap and stalled frames");
}
