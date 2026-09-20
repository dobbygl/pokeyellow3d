#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace firstperson {
constexpr float Pi=3.14159265358979323846f;
constexpr uint64_t FrameCycles=70224,TurnCycles=2*FrameCycles;
inline uint8_t dpad(int facing) {
    switch(facing&12){case 4:return 0xfb;case 8:return 0xfd;case 12:return 0xfe;default:return 0xf7;}
}
inline int turn(int facing,int quarter_turns) {
    // Clockwise compass: north, east, south, west.
    constexpr int compass[]={4,12,0,8};int index=0;
    while(compass[index]!=(facing&12))++index;
    return compass[(index+quarter_turns+4)%4];
}
struct Controls {
    bool forward=false,pending=false,pulsing=false;
    int target=0,attempts=0;
    uint64_t pulse_started=0,rest_until=0,last_cycles=0;
    void reset() {*this={};}
    void request_turn(int facing,int amount,uint64_t cycles) {
        if(pending || cycles<rest_until)return;
        target=turn(facing,amount);pending=true;pulsing=false;attempts=0;
    }
    uint8_t update(bool allowed,int facing,bool idle,uint64_t cycles) {
        if(!allowed || cycles<last_cycles) {reset();last_cycles=cycles;return 0xff;}
        last_cycles=cycles;
        if(pending && facing==target) {
            pending=false;pulsing=false;rest_until=cycles+TurnCycles;return 0xff;
        }
        if(cycles<rest_until)return 0xff;
        if(pending) {
            if(pulsing) {
                if(cycles-pulse_started<TurnCycles)return dpad(target);
                pulsing=false;rest_until=cycles+TurnCycles;
                if(attempts>=3)pending=false;
                return 0xff;
            }
            // Let the engine finish a step and acknowledge neutral input before
            // emitting a turn. Never simulate a facing or coordinate change.
            if(!idle)return 0xff;
            pulse_started=cycles;pulsing=true;++attempts;return dpad(target);
        }
        return forward?dpad(facing):0xff;
    }
};
using Matrix=std::array<float,16>; // OpenGL column-major.
inline float facing_yaw(int facing) {
    switch(facing&12) {case 4:return 0;case 8:return -Pi/2;case 12:return Pi/2;default:return Pi;}
}
inline float angle_delta(float from,float to) {return std::remainder(to-from,2*Pi);}
inline Matrix orthographic(float x,float z,float yaw,float sx,float sy) {
    const float c=std::cos(yaw),s=std::sin(yaw),tilt=.78f,up=.6257795f;
    return {c*sx,-s*tilt*sy,-s*up/256,0,
            0,up*sy,-tilt/256,0,
            -s*sx,-c*tilt*sy,-c*up/256,0,
            (-c*x+s*z)*sx,(s*x+c*z)*tilt*sy+.025f,(s*x+c*z)*up/256,1};
}
struct Camera {
    float x=0,y=1.1f,z=0,yaw=0;
    bool ready=false;
    uint64_t last_cycles=0;
    void reset() {ready=false;}
    void update(float px,float pz,int facing,float dt,uint64_t cycles,float jump=0) {
        float target=facing_yaw(facing);
        if(!ready || cycles<last_cycles || std::abs(px-x)+std::abs(pz-z)>3)yaw=target;
        else yaw+=angle_delta(yaw,target)*(1-std::exp(-std::clamp(dt,0.f,.1f)/.15f));
        yaw=std::remainder(yaw,2*Pi);x=px;z=pz;y=1.1f+jump;
        ready=true;last_cycles=cycles;
    }
    Matrix perspective(float aspect) const {
        constexpr float near=.1f,far=96.f;
        float f=1/std::tan(70.f*Pi/360),c=std::cos(yaw),s=std::sin(yaw);
        float a=(far+near)/(near-far),b=2*far*near/(near-far);
        return {f*c/aspect,0,-a*s,s,
                0,f,0,0,
                f*s/aspect,0,a*c,-c,
                -f*(c*x+s*z)/aspect,-f*y,a*(s*x-c*z)+b,-s*x+c*z};
    }
};
} // namespace firstperson

// Implemented only in the generated SDL adapter; owns a separate input channel.
void pallet3d_set_input_mask(uint8_t mask,bool relative);
