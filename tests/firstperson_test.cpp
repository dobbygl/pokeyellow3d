#include "firstperson.h"
#include <cstdio>
#include <cstdlib>
static void check(bool b,const char* why){if(!b){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static std::array<float,4> transform(const firstperson::Matrix& m,float x,float y,float z) {
    std::array<float,4> out{};
    for(int i=0;i<4;i++)out[i]=m[i]*x+m[4+i]*y+m[8+i]*z+m[12+i];
    return out;
}
int main() {
    using namespace firstperson;
    check(dpad(0)==0xf7&&dpad(4)==0xfb&&dpad(8)==0xfd&&dpad(12)==0xfe,"engine facing to active-low dpad");
    for(int facing:{0,4,8,12}) {
        check(turn(turn(facing,1),-1)==facing,"left and right are inverse");
        check(turn(turn(facing,2),2)==facing,"two half turns restore orientation");
        Controls c;c.request_turn(facing,1,0);int wanted=turn(facing,1);
        check(c.update(true,facing,false,0)==255,"wait for engine idle before turn");
        check(c.update(true,facing,true,1)==dpad(wanted),"start directional tap");
        c.request_turn(facing,-1,1);check(c.target==wanted,"rapid turn ignored while busy");
        check(c.update(true,facing,true,TurnCycles)==dpad(wanted),"tap remains within measured duration");
        check(c.update(true,facing,true,TurnCycles+1)==255,"tap released at two effective frames");
        check(c.update(true,wanted,true,TurnCycles+2)==255&&!c.pending,"engine orientation acknowledges turn");
        c.forward=true;
        check(c.update(true,wanted,true,3*TurnCycles)==dpad(wanted),"W follows real facing");
        check(c.update(false,wanted,true,4*TurnCycles)==255&&!c.forward,"dialogue or menu releases held input");
        c.forward=true;check(c.update(true,wanted,true,0)==255&&!c.forward,"savestate rewind releases input");
        Camera eye;eye.update(10,20,facing,1.f/60,0);
        auto p=transform(eye.perspective(1),10+5*std::sin(eye.yaw),1.1f,20-5*std::cos(eye.yaw));
        check(std::abs(p[0])<1e-4&&std::abs(p[1])<1e-4&&p[3]>0,"forward is centered and in front of perspective camera");
        eye.update(10,20,turn(facing,1),1.f/60,70224);
        float error=std::abs(angle_delta(eye.yaw,facing_yaw(turn(facing,1))));
        check(error>0&&error<Pi/2,"engine turn is smoothed");
        for(int i=2;i<90;i++)eye.update(10,20,turn(facing,1),1.f/60,i*70224);
        check(std::abs(angle_delta(eye.yaw,facing_yaw(turn(facing,1))))<.001,"turn converges");
        eye.reset();eye.update(10,20,facing,1.f/60,0);
        check(std::abs(angle_delta(eye.yaw,facing_yaw(facing)))<1e-6,"return from 2D snaps to engine facing");
    }
    check(std::abs(angle_delta(Pi-.01f,-Pi+.01f)-.02f)<1e-5,"shortest turn crosses angle wrap");
    for(float yaw:{-.75f,-.32f,0.f,.75f}) {
        float c=std::cos(yaw),s=std::sin(yaw);auto m=orthographic(-30,65,yaw,.1f,.2f);
        for(float x:{-50.f,0.f,60.f})for(float y:{0.f,1.1f,8.f})for(float z:{-70.f,0.f,90.f}) {
            auto p=transform(m,x,y,z);float dx=x+30,dz=z-65,along=dx*s+dz*c;
            check(std::abs(p[0]-(dx*c-dz*s)*.1f)<2e-6&&
                std::abs(p[1]-(y*.6257795f-along*.78f)*.2f-.025f)<4e-6&&
                std::abs(p[2]+(along*.6257795f+y*.78f)/256)<1e-6,"orthographic matrix matches original shader");
        }
    }
    std::puts("PASS: projections, orientation smoothing, relative controls, pulse duration and fallback neutrality");
}
