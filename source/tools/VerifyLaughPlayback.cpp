#include "../shared/LaughPlayback.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(int argc,char **argv) {
    try {
        frog::LaughPlayback p;uint64_t now=1000000000ull;
        p.key(VK_F5,true,false,false,now);require(p.frame(now)==0,"F5 starts first frame");
        require(p.frame(now+3000000000ull)==72,"Realtime frame timing");
        p.key(VK_F5,true,false,false,now+3000000000ull);
        require(p.frame(now+3000000000ull)==72,"Autorepeat must not restart");
        p.key(VK_F5,false,false,false,now+3100000000ull);
        p.key(VK_F5,true,false,false,now+3200000000ull);
        require(p.frame(now+3200000000ull)==0,"Repeated physical press restarts");
        p.key(VK_F5,false,false,false,now+3200000000ull);
        for(unsigned key:{unsigned('D'),unsigned('F'),unsigned('J'),unsigned('K'),unsigned(VK_SPACE),unsigned(VK_LCONTROL)}) {
            p.key(VK_F5,true,false,false,now);
            p.key(VK_F5,false,false,false,now);
            p.key(key,true,true,false,now+1);
            require(!p.playing,"Gameplay and rebound function keys cancel immediately");
            p.key(key,false,true,false,now+2);require(!p.playing,"Release must not resume");
        }
        p.key(VK_F5,true,false,true,now);require(!p.playing,"Gameplay held takes precedence");
        p.key(VK_F5,false,false,false,now);
        p.key(VK_F5,true,true,false,now);require(!p.playing,"F5 rebound as gameplay has precedence");
        p.key(VK_F5,false,false,false,now);
        p.key(VK_F5,true,false,false,now);
        require(p.frame(now+8990000000ull)==215,"Last frame");
        require(p.frame(now+9000000000ull)==-1 && !p.playing,"Finishes at 9 seconds without looping");
        frog::KeyChord chord;
        require(frog::KeyChord::parse("17+81",chord),"Parse Ctrl+Q");
        p.setHotkey(chord);
        p.key('Q',true,false,false,now);require(!p.playing,"Q alone must not trigger Ctrl+Q");p.key('Q',false,false,false,now);
        p.key(VK_LCONTROL,true,false,false,now);require(!p.playing,"Ctrl alone does not trigger");
        p.key('Q',true,false,false,now+1);require(p.playing && p.started==now+1,"Ctrl+Q starts");
        p.key('Q',true,false,false,now+2);require(p.started==now+1,"Combination autorepeat suppression");
        p.key('Q',false,false,false,now+3);p.key('Q',true,false,false,now+4);
        require(p.started==now+4,"Q may restart while Ctrl stays held");
        p.key('D',true,true,false,now+5);require(!p.playing,"Gameplay cancels a combination-triggered video");
        p.setHotkey(chord);p.key(VK_RCONTROL,true,false,false,now);p.key('Q',true,false,false,now+1);
        require(p.playing,"Right Ctrl also matches generic Ctrl");p.stop();p.setHotkey(chord);
        p.key(VK_LCONTROL,true,false,false,now);p.key(VK_LSHIFT,true,false,false,now);p.key('Q',true,false,false,now);
        require(!p.playing,"Extra modifiers do not match");
        require(frog::KeyChord::parse("17+16+81",chord),"Ctrl+Shift+Q parses");p.setHotkey(chord);
        p.key('Q',true,false,false,now);p.key(VK_RSHIFT,true,false,false,now);p.key(VK_RCONTROL,true,false,false,now);
        require(p.playing,"Three-key chord matches in either press order");
        require(!frog::KeyChord::parse("17+",chord) && !frog::KeyChord::parse("0",chord) && !frog::KeyChord::parse("999",chord),"Reject invalid config");
        frog::ChordCapture capture;capture.reset();
        capture.key(VK_LCONTROL,true);capture.key('Q',true);capture.key('Q',true);
        require(!capture.key('Q',false),"Capture waits for modifier release");
        require(capture.key(VK_LCONTROL,false) && capture.chord.text()=="17+81","Capture complete canonical combo");
        if(argc>=2) {
            frog::LaughFrames frames;require(frames.load(std::filesystem::u8path(argv[1])),"Frame pack load");
            std::vector<double> timings;uint64_t transparent=0,opaque=0;
            for(unsigned frame=0;frame<frames.count();++frame) {
                const auto start=frog::NowNs();require(frames.decode(int(frame)),"Every frame decodes");
                timings.push_back((frog::NowNs()-start)/1000000.0);
                for(size_t i=0;i<frames.rgba.size();i+=4) {
                    for(int c=0;c<3;++c)require(frames.rgba[i+c]<=frames.rgba[i+3],"Premultiplied alpha");
                    transparent+=frames.rgba[i+3]==0;opaque+=frames.rgba[i+3]==255;
                }
            }
            require(transparent>1000000 && opaque>1000000,"Real transparent channel and opaque artwork");
            std::sort(timings.begin(),timings.end());
            std::cout<<"216 frames; decode p95="<<timings[size_t(timings.size()*.95)]<<"ms, max="<<timings.back()<<"ms\n";
        }
        std::cout<<"PASS: physical F5/restart, repeat suppression, gameplay cancellation, rebound keys, completion and transparency.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
