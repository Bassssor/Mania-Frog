#define NOMINMAX
#include <SFML/Graphics.hpp>
#include <Windows.h>
#include <Shellapi.h>
#include <ShlObj.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "ParticleEffects.hpp"
#include "DeskSurface.hpp"
#include "KeyboardShadow.hpp"
#include "ArmShadow.hpp"
#include "KeyboardRear.hpp"
#include "../../shared/LaughPlayback.hpp"
#include <mmsystem.h>

namespace {
constexpr unsigned Width = 1189, Height = 880, SpriteHeight = 669, SceneSize = 880;
constexpr float ViewX=120.f;
constexpr std::array<const char*, 9> Files = {
    "base.png", "base_1000.png", "base_1100.png", "base_0100.png",
    "base_0001.png", "base_0011.png", "base_0010.png", "base_left.png", "base_right.png"
};
constexpr std::array<int, 4> DefaultKeys = { 'D', 'F', 'J', 'K' };
std::array<int, 4> keys = DefaultKeys;
unsigned held = 0;
unsigned pendingBursts = 0;
frog::LaughPlayback laughPlayback;
frog::LaughFrames laughFrames;
bool laughReady=false;
uint64_t audioRevision=0;
void SyncLaughAudio() {
    if(!laughReady || audioRevision==laughPlayback.revision)return;
    audioRevision=laughPlayback.revision;
    PlaySoundW(nullptr,nullptr,0);
    if(laughPlayback.playing)PlaySoundW(L"laugh.wav",nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT);
}
bool redraw = true, topmost = true;
int size = 380;
constexpr float AnimationInterval=1.f/120.f;
constexpr UINT TrayMessage=WM_APP+1, CapturedKeyMessage=WM_APP+2;
constexpr UINT LaughChordMessage=WM_APP+3;
UINT taskbarCreated=0;
bool trayAdded=false;
HWND keyDialog=nullptr;
HWND laughDialog=nullptr,laughButton=nullptr,laughHint=nullptr;
frog::KeyChord editedLaughHotkey;
frog::ChordCapture laughCapture;
bool laughCapturing=false;
int captureSlot=-1;
std::array<int,4> editedKeys=DefaultKeys;
std::array<bool,256> capturedDown{};
std::array<HWND,4> keyButtons{};
HWND keyHint=nullptr;
std::filesystem::path configOverride;

class FrameWaiter {
    HANDLE timer=nullptr;
public:
    FrameWaiter() {
        timer=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
        if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
    }
    ~FrameWaiter(){if(timer)CloseHandle(timer);}
    void Wait(bool animated,bool invalidated,const sf::Clock& clock) {
        if(invalidated)return;
        if(!animated) {
            if(timer)CancelWaitableTimer(timer);
            MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            return;
        }
        const float remaining=AnimationInterval-clock.getElapsedTime().asSeconds();
        if(remaining<=0.f)return;
        LARGE_INTEGER due{};
        due.QuadPart=-std::max<LONGLONG>(1,static_cast<LONGLONG>(remaining*10000000.f));
        if(timer && SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))
            MsgWaitForMultipleObjectsEx(1,&timer,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        else MsgWaitForMultipleObjectsEx(0,nullptr,static_cast<DWORD>(std::ceil(remaining*1000.f)),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
};

int LeftFrame(unsigned mask) {
    constexpr int frames[] = { 7, 1, 3, 2 };
    return frames[mask & 3u];
}
int RightFrame(unsigned mask) {
    constexpr int frames[] = { 8, 6, 4, 5 };
    return frames[(mask >> 2) & 3u];
}
void UpdateKey(DWORD code, bool down) {
    const bool bound=std::find(keys.begin(),keys.end(),int(code))!=keys.end();
    const auto revision=laughPlayback.revision;
    if(laughReady)laughPlayback.key(code,down,bound,held!=0,frog::NowNs());
    if(laughPlayback.revision!=revision){redraw=true;SyncLaughAudio();}
    const unsigned old = held;
    for (unsigned i = 0; i < keys.size(); ++i) {
        if (code != static_cast<DWORD>(keys[i])) continue;
        if (down) held |= 1u << i;
        else held &= ~(1u << i);
    }
    redraw |= old != held;
    pendingBursts |= held & ~old;
}
// The hook and renderer share one message-loop thread; no rendering inside
// the hook and no detached thread modifying SFML textures.
LRESULT CALLBACK KeyboardHook(int code, WPARAM event, LPARAM value) {
    if (code == HC_ACTION) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(value);
        const bool down=event==WM_KEYDOWN || event==WM_SYSKEYDOWN;
        const bool up=event==WM_KEYUP || event==WM_SYSKEYUP;
        if(laughDialog && (down || up) && key->vkCode<256) {
            if(laughCapturing) {
                if(down && key->vkCode==VK_ESCAPE) {
                    laughCapturing=false;capturedDown[key->vkCode]=true;
                    PostMessageW(laughDialog,LaughChordMessage,2,0);
                } else {
                    capturedDown[key->vkCode]=down;
                    const bool complete=laughCapture.key(key->vkCode,down);
                    if(complete)laughCapturing=false;
                    PostMessageW(laughDialog,LaughChordMessage,complete?1:0,0);
                }
                return 1;
            }
            if(capturedDown[key->vkCode]){if(up)capturedDown[key->vkCode]=false;return 1;}
            return CallNextHookEx(nullptr,code,event,value);
        }
        if(key->vkCode<capturedDown.size() && capturedDown[key->vkCode]) {
            if(up)capturedDown[key->vkCode]=false;
            if(up && key->vkCode<256)laughPlayback.pressed[key->vkCode]=false;
            return 1;
        }
        if(keyDialog) {
            if(up && key->vkCode<256)laughPlayback.pressed[key->vkCode]=false;
            if(captureSlot>=0 && down && key->vkCode<256) {
                capturedDown[key->vkCode]=true;
                PostMessageW(keyDialog,CapturedKeyMessage,key->vkCode,captureSlot);
                captureSlot=-1;
                return 1;
            }
            return CallNextHookEx(nullptr,code,event,value);
        }
        if (event == WM_KEYDOWN || event == WM_SYSKEYDOWN) UpdateKey(key->vkCode, true);
        if (event == WM_KEYUP || event == WM_SYSKEYUP) UpdateKey(key->vkCode, false);
    }
    return CallNextHookEx(nullptr, code, event, value);
}
void SyncKeys() {
    held = 0;
    for (unsigned i = 0; i < keys.size(); ++i)
        if (GetAsyncKeyState(keys[i]) & 0x8000) held |= 1u << i;
    redraw = true;
}
std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}
std::filesystem::path ConfigPath() {
    if(!configOverride.empty())return configOverride;
    wchar_t local[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))
        return std::filesystem::path(local)/L"MilkFrog"/L"config.txt";
    return "config.txt";
}
bool SaveConfig(const std::array<int,4>& binding) {
    try {
        const auto path=ConfigPath();
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
        auto temporary=path;temporary+=L".tmp";
        {std::ofstream file(temporary,std::ios::trunc);
         for(unsigned i=0;i<binding.size();++i)file<<"key"<<i+1<<'='<<binding[i]<<'\n';
         file<<"laugh_keys="<<laughPlayback.hotkey.text()<<'\n';
         file.flush();if(!file)return false;}
        return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH)!=FALSE;
    } catch(...) {return false;}
}
void LoadConfig() {
    auto candidate = DefaultKeys;
    const auto path=ConfigPath();
    std::ifstream config(std::filesystem::is_regular_file(path)?path:std::filesystem::path("config.txt"));
    std::string line;
    while (std::getline(config, line)) {
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        const std::string name = Trim(line.substr(0, equal)), value = Trim(line.substr(equal + 1));
        if (name.size() != 4 || name.substr(0, 3) != "key" || name[3] < '1' || name[3] > '4' || value.empty()) continue;
        int key = -1;
        if (value.size() == 1 && (value[0] < '0' || value[0] > '9')) {
            const SHORT mapped = VkKeyScanA(value[0]);
            if (mapped != -1) key = mapped & 255;
        } else {
            try {
                size_t used = 0;
                key = std::stoi(value, &used, value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0 ? 16 : 10);
                if (used != value.size()) key = -1;
            } catch (...) { key = -1; }
        }
        if (key > 0 && key < 256) candidate[name[3] - '1'] = key;
    }
    bool distinct = true;
    for (unsigned i = 0; i < candidate.size(); ++i)
        for (unsigned j = i + 1; j < candidate.size(); ++j) distinct &= candidate[i] != candidate[j];
    keys = distinct ? candidate : DefaultKeys;
    laughPlayback.setHotkey(frog::ReadLaughHotkey(std::filesystem::is_regular_file(path)?path:std::filesystem::path("config.txt")));
    laughPlayback.syncPressed();
    pendingBursts=0;
    SyncKeys();
}
std::wstring KeyName(int key) {
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) return std::wstring(1, static_cast<wchar_t>(key));
    if (key == VK_OEM_COMMA) return L",";
    if (key == VK_OEM_PERIOD) return L".";
    if (key == VK_SPACE) return L"SPC";
    if (key >= VK_F1 && key <= VK_F24) return L"F" + std::to_wstring(key - VK_F1 + 1);
    if(key==VK_LEFT)return L"←";
    if(key==VK_RIGHT)return L"→";
    if(key==VK_UP)return L"↑";
    if(key==VK_DOWN)return L"↓";
    if(key==VK_RETURN)return L"ENT";
    if(key==VK_TAB)return L"TAB";
    if(key==VK_BACK)return L"BSP";
    if(key==VK_ESCAPE)return L"ESC";
    if(key==VK_LSHIFT)return L"LS";
    if(key==VK_RSHIFT)return L"RS";
    if(key==VK_SHIFT)return L"SHF";
    if(key==VK_LCONTROL)return L"LC";
    if(key==VK_RCONTROL)return L"RC";
    if(key==VK_CONTROL)return L"CTL";
    if(key==VK_LMENU)return L"LA";
    if(key==VK_RMENU)return L"RA";
    if(key==VK_MENU)return L"ALT";
    if(key==VK_CAPITAL)return L"CAP";
    if(key==VK_PRIOR)return L"PGU";
    if(key==VK_NEXT)return L"PGD";
    if(key==VK_HOME)return L"HOM";
    if(key==VK_END)return L"END";
    if(key==VK_INSERT)return L"INS";
    if(key==VK_DELETE)return L"DEL";
    if(key==VK_NUMLOCK)return L"NUM";
    if(key==VK_SCROLL)return L"SCR";
    if(key==VK_SNAPSHOT)return L"PRT";
    if(key==VK_PAUSE)return L"PAU";
    if(key==VK_LWIN)return L"LW";
    if(key==VK_RWIN)return L"RW";
    if(key==VK_APPS)return L"APP";
    if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9)return L"N"+std::to_wstring(key-VK_NUMPAD0);
    if(key==VK_MULTIPLY)return L"N*";
    if(key==VK_ADD)return L"N+";
    if(key==VK_SUBTRACT)return L"N-";
    if(key==VK_DECIMAL)return L"N.";
    if(key==VK_DIVIDE)return L"N/";
    if(key==VK_VOLUME_MUTE)return L"MUT";
    if(key==VK_VOLUME_DOWN)return L"V-";
    if(key==VK_VOLUME_UP)return L"V+";
    if(key==VK_MEDIA_NEXT_TRACK)return L"NXT";
    if(key==VK_MEDIA_PREV_TRACK)return L"PRV";
    if(key==VK_MEDIA_STOP)return L"STP";
    if(key==VK_MEDIA_PLAY_PAUSE)return L"PLY";
    if(key==VK_OEM_1)return L";";
    if(key==VK_OEM_PLUS)return L"=";
    if(key==VK_OEM_MINUS)return L"-";
    if(key==VK_OEM_2)return L"/";
    if(key==VK_OEM_3)return L"`";
    if(key==VK_OEM_4)return L"[";
    if(key==VK_OEM_5 || key==VK_OEM_102)return L"\\";
    if(key==VK_OEM_6)return L"]";
    if(key==VK_OEM_7)return L"'";
    // Unusual hardware keys also receive a deterministic three-character
    // label, instead of a potentially long, localized Windows key name.
    wchar_t label[4]{};swprintf_s(label,L"K%02X",key&255);return label;
}

const std::array<sf::Vector2f,4> KeyCenters={sf::Vector2f(694.f,543.f),sf::Vector2f(594.f,526.f),
    sf::Vector2f(498.f,510.f),sf::Vector2f(408.f,495.f)};

class Artwork {
    std::array<sf::Texture, 9> textures;
    sf::Sprite body, left, right;
    sf::Texture tableTexture, cornerTexture, keyboardTexture;
    sf::Texture keyboardRearTexture;
    sf::Texture pressedBodyTexture;
    sf::Texture shadowTexture;
    std::array<sf::Texture,16> armShadows;
    bool hasTable=false,hasCorner=false,hasPressedFace=false;
    sf::Font font;
    sf::RenderTexture composite;
    std::array<sf::Texture,2> reachTextures;
    std::array<sf::VertexArray,2> reaches;
    bool BuildReach(unsigned slot,unsigned frame,sf::Vector2f shift) {
        const auto image=textures[frame].copyToImage();
        const auto* source=image.getPixelsPtr();
        std::vector<sf::Uint8> premultiplied(source,source+Width*SpriteHeight*4);
        for(unsigned pixel=0;pixel<Width*SpriteHeight;++pixel) {
            const unsigned offset=pixel*4,alpha=premultiplied[offset+3];
            for(unsigned channel=0;channel<3;++channel)
                premultiplied[offset+channel]=static_cast<sf::Uint8>((premultiplied[offset+channel]*alpha+127)/255);
        }
        if(!reachTextures[slot].create(Width,SpriteHeight))return false;
        reachTextures[slot].update(premultiplied.data());reachTextures[slot].setSmooth(true);
        reaches[slot].clear();
        reaches[slot].setPrimitiveType(sf::Triangles);
        auto vertex=[shift](float x,float y) {
            const float t=std::clamp((y-326.f)/135.f,0.f,1.f);
            const float weight=t*t*(3.f-2.f*t);
            return sf::Vertex(sf::Vector2f(x,y)+shift*weight,sf::Color::White,sf::Vector2f(x,y));
        };
        for(unsigned row=0;row<SpriteHeight;row+=4) {
            const float first=static_cast<float>(row),last=static_cast<float>(std::min(row+4,SpriteHeight));
            const std::array<sf::Vertex,4> quad={{vertex(0,first),vertex(static_cast<float>(Width),first),
                vertex(static_cast<float>(Width),last),vertex(0,last)}};
            for(unsigned corner : {0u,1u,2u,0u,2u,3u})reaches[slot].append(quad[corner]);
        }
        return true;
    }
    void DrawHands(sf::RenderTarget& target,unsigned mask) {
        const sf::BlendMode blend(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,
            sf::BlendMode::Add,sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
        if(LeftFrame(mask)==3)target.draw(reaches[0],sf::RenderStates(blend,sf::Transform::Identity,&reachTextures[0],nullptr));
        else target.draw(left);
        if(RightFrame(mask)==6)target.draw(reaches[1],sf::RenderStates(blend,sf::Transform::Identity,&reachTextures[1],nullptr));
        else target.draw(right);
    }
    void DrawArmShadow(sf::RenderTarget& target,unsigned mask,bool isolated=false) {
        sf::Sprite shadow(armShadows[mask]);
        shadow.setPosition(static_cast<float>(ArmShadow::Left),static_cast<float>(ArmShadow::Top));
        // Tint existing receiver RGB only: no new alpha at body cut edges.
        const sf::BlendMode multiply(sf::BlendMode::DstColor,sf::BlendMode::OneMinusSrcAlpha,
            sf::BlendMode::Add,sf::BlendMode::Zero,sf::BlendMode::One,sf::BlendMode::Add);
        const sf::BlendMode premultiplied(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,
            sf::BlendMode::Add,sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
        target.draw(shadow,sf::RenderStates(isolated?premultiplied:multiply));
    }
    bool BuildArmShadows(const sf::Image& keyboard) {
        sf::RenderTexture pose;
        if(!pose.create(Width,SpriteHeight))return false;
        const auto receiver=textures[0].copyToImage();
        for(unsigned mask=0;mask<armShadows.size();++mask) {
            DrawForeground(pose,mask);pose.display();
            if(!armShadows[mask].loadFromImage(ArmShadow::Build(pose.getTexture().copyToImage(),receiver,keyboard)))return false;
            armShadows[mask].setSmooth(true);
        }
        return true;
    }
public:
    ParticleEffects effects;
    void DrawArmShadowOnly(sf::RenderTarget& target,unsigned mask) {
        target.clear(sf::Color::Transparent);DrawArmShadow(target,mask,true);
    }
    bool SaveKeyboard(sf::RenderTarget& target) {
        target.clear(sf::Color::Transparent);
        if(hasTable)target.draw(sf::Sprite(keyboardRearTexture));
        if(hasCorner)target.draw(sf::Sprite(cornerTexture));
        if(!hasTable)return false;
        target.draw(sf::Sprite(keyboardTexture));return true;
    }
    bool SaveShadow(const std::filesystem::path& path) {
        return hasTable && shadowTexture.copyToImage().saveToFile(path.string());
    }
    bool Load(const std::filesystem::path& assetDirectory = {}) {
        for (unsigned i = 0; i < textures.size(); ++i) {
            if (!textures[i].loadFromFile((assetDirectory / Files[i]).string()) || textures[i].getSize() != sf::Vector2u(Width, SpriteHeight)) {
                const auto message = L"角色素材缺失或尺寸错误：\n" + std::filesystem::path(Files[i]).wstring();
                MessageBoxW(nullptr, message.c_str(), L"奶蛙桌宠", MB_ICONERROR);
                return false;
            }
            textures[i].setSmooth(true);
        }
        body.setTexture(textures[0]);
        const auto pressedPath=assetDirectory / "body-pressed.png";
        hasPressedFace=std::filesystem::exists(pressedPath);
        if(hasPressedFace && (!pressedBodyTexture.loadFromFile(pressedPath.string()) ||
            pressedBodyTexture.getSize()!=sf::Vector2u(Width,SpriteHeight)))return false;
        pressedBodyTexture.setSmooth(true);
        const auto tablePath=assetDirectory / "tabletop.png";
        const auto cornerPath=assetDirectory / "keyboard-corner.png";
        hasTable=std::filesystem::exists(tablePath);
        hasCorner=std::filesystem::exists(cornerPath);
        if(hasTable && (!tableTexture.loadFromFile(tablePath.string()) || tableTexture.getSize()!=sf::Vector2u(Width,Height)))return false;
        if(hasCorner && (!cornerTexture.loadFromFile(cornerPath.string()) || cornerTexture.getSize()!=sf::Vector2u(Width,SpriteHeight)))return false;
        tableTexture.setSmooth(true);cornerTexture.setSmooth(true);
        if(hasTable) {
            // The tabletop hides the lower torso. Restore the existing
            // keyboard above it without redrawing or moving any keycap.
            const auto original=textures[0].copyToImage();
            const auto rear=KeyboardRear::Build(original);
            if(!keyboardRearTexture.loadFromImage(rear))return false;
            keyboardRearTexture.setSmooth(true);
            sf::Image keyboard;keyboard.create(Width,SpriteHeight,sf::Color::Transparent);
            for(unsigned x=320;x<800;++x) {
                unsigned first=SpriteHeight;
                if(x<437)first=455;
                else {
                    for(unsigned y=460;y<590;++y) {
                        const auto c=original.getPixel(x,y);
                        if(c.a>=100 && c.b>=205 && c.r-c.g<=8 && c.g-c.b<=8){first=y;break;}
                    }
                    if(first==SpriteHeight)for(unsigned y=520;y<640;++y) {
                        const auto c=original.getPixel(x,y);
                        if(c.a>=100 && c.b>=140 && c.r-c.g<=15 && c.g-c.b<=15){first=y;break;}
                    }
                }
                for(unsigned y=first>0?first-1:first;y<SpriteHeight;++y) {
                    auto c=original.getPixel(x,y);
                    if(rear.getPixel(x,y).a && c.r<215) {
                        const float closure=KeyboardRear::CornerClosureCoverage(static_cast<int>(x),static_cast<int>(y));
                        c.a=static_cast<sf::Uint8>(std::lround(c.a*(1.f-closure)));
                        if(!c.a)continue;
                    }
                    // Warm torso debris in the old rear recess must reveal
                    // the continuous deck, rather than overwrite it again.
                    if(rear.getPixel(x,y).a && c.r>=170 && c.r-c.g>=8 && c.g-c.b>=15)continue;
                    if(rear.getPixel(x,y).a && c.r<170 && c.r-c.g>16 && c.g-c.b>16) {
                        const auto gray=static_cast<sf::Uint8>(std::lround(c.r*.21f+c.g*.72f+c.b*.07f));
                        c=sf::Color(gray+5,gray+2,gray-3,c.a);
                    }
                    if(x>=437 && y+1==first)c.a=static_cast<sf::Uint8>(c.a*.3f);
                    keyboard.setPixel(x,y,c);
                }
            }
            if(!keyboardTexture.loadFromImage(keyboard))return false;
            keyboardTexture.setSmooth(true);
            sf::Image corner;corner.create(Width,SpriteHeight,sf::Color::Transparent);
            if(hasCorner)corner=cornerTexture.copyToImage();
            if(!shadowTexture.loadFromImage(KeyboardShadow::Build(keyboard,corner,Width,Height)))return false;
            shadowTexture.setSmooth(true);
        }
        // Fixed shoulder, progressively displaced forearm, and a rigid hand
        // at the keycap. Premultiplied sampling prevents a dark warped edge.
        if(!BuildReach(0,3,{-57.f,11.f}) || !BuildReach(1,6,{29.f,13.f}))return false;
        sf::Image shadowExclusion;shadowExclusion.create(Width,SpriteHeight,sf::Color::Transparent);
        if(hasTable) {
            shadowExclusion=keyboardTexture.copyToImage();
            const auto rear=keyboardRearTexture.copyToImage();
            for(unsigned y=466;y<564;++y)for(unsigned x=382;x<791;++x) {
                const unsigned keyAlpha=shadowExclusion.getPixel(x,y).a,rearAlpha=rear.getPixel(x,y).a;
                const auto alpha=static_cast<sf::Uint8>(255-((255-keyAlpha)*(255-rearAlpha)+127)/255);
                shadowExclusion.setPixel(x,y,sf::Color(0,0,0,alpha));
            }
        }
        if(!BuildArmShadows(shadowExclusion))return false;
        if (!composite.create(Width,Height)) return false;
        composite.setSmooth(true);
        std::array<wchar_t, MAX_PATH> windows{};
        GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
        const auto directory = std::filesystem::path(windows.data()) / "Fonts";
        return effects.Load() && (font.loadFromFile((directory / "segoeuib.ttf").string()) ||
            font.loadFromFile((directory / "arialbd.ttf").string()));
    }
    sf::Text KeyLabel(int key,unsigned slot) const {
        sf::Text label(sf::String(KeyName(key)),font,44);
        label.setFillColor(sf::Color(45,51,49));
        const auto bounds=label.getLocalBounds();
        label.setOrigin(bounds.left+bounds.width/2.f,bounds.top+bounds.height/2.f);
        label.setPosition(KeyCenters[slot]);label.setRotation(191.f);
        // Fit both dimensions inside the inset of the existing keycap plane.
        // Default D/F/J/K keep their original size and registration.
        label.setScale(bounds.width>52.f?52.f/bounds.width:1.f,
            bounds.height>40.f?26.f/bounds.height:0.65f);
        return label;
    }
    void DrawLayers(sf::RenderTarget& target, unsigned mask, bool includeHands = true, bool includeEffects = true, bool includeTable = true, bool drawHandLayer = true) {
        // Expression follows the held-key mask, never the lingering sparks.
        // Body-only exports keep the original face for geometry regressions.
        body.setTexture(hasPressedFace && includeHands && mask!=0 ? pressedBodyTexture : textures[0]);
        left.setTexture(textures[LeftFrame(mask)]);
        right.setTexture(textures[RightFrame(mask)]);
        target.clear(sf::Color::Transparent);
        if(includeTable && hasTable) {
            target.draw(body);
            if(includeHands)DrawArmShadow(target,mask);
            target.draw(sf::Sprite(tableTexture));
            target.draw(sf::Sprite(shadowTexture));
            target.draw(sf::Sprite(keyboardRearTexture));
        }
        if(hasCorner)target.draw(sf::Sprite(cornerTexture));
        if(includeTable && hasTable)target.draw(sf::Sprite(keyboardTexture));
        else {target.draw(body);if(includeHands)DrawArmShadow(target,mask);}
        // Key legends face the character. D/F are screen-right and J/K
        // screen-left. Positions are the visible top-surface centers, before
        // hand occlusion; legends belong to the key, not on top of a finger.
        for (unsigned i = 0; i < keys.size(); ++i) {
            target.draw(KeyLabel(keys[i],i));
        }
        // Both keycaps and base are a single stationary image. Bloom lies
        // over the key legends and behind hands; front particles draw last.
        if (includeEffects) { effects.DrawSurface(target); effects.DrawParticles(target,-1); }
        if (includeHands && drawHandLayer) DrawHands(target,mask);
        if (includeEffects) effects.DrawParticles(target,1);
    }
    void Draw(sf::RenderTarget& target, unsigned mask, bool includeHands = true, bool includeEffects = true, bool includeTable = true, bool drawHandLayer = true) {
        // Composite registered layers at their native pixel centers first.
        // Filtering complementary transparent cut edges separately causes
        // alpha holes and dark seams at every non-native display scale.
        DrawLayers(composite,mask,includeHands,includeEffects,includeTable,drawHandLayer); composite.display();
        target.clear(sf::Color::Transparent);
        const sf::BlendMode premultiplied(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,
            sf::BlendMode::Add,sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
        target.draw(sf::Sprite(composite.getTexture()),sf::RenderStates(premultiplied));
    }
    void DrawForeground(sf::RenderTarget& target, unsigned mask) {
        left.setTexture(textures[LeftFrame(mask)]);
        right.setTexture(textures[RightFrame(mask)]);
        target.clear(sf::Color::Transparent);
        DrawHands(target,mask);
    }
};

// A transparent SFML render target contains premultiplied RGB, accepted by
// UpdateLayeredWindow. PNG export instead needs straight RGB.
sf::Image StraightImage(const sf::Texture& texture) {
    auto image = texture.copyToImage();
    for (unsigned y = 0; y < image.getSize().y; ++y)
        for (unsigned x = 0; x < image.getSize().x; ++x) {
            auto pixel = image.getPixel(x, y);
            if (pixel.a > 0 && pixel.a < 255) {
                pixel.r = static_cast<sf::Uint8>(std::min(255u, pixel.r * 255u / pixel.a));
                pixel.g = static_cast<sf::Uint8>(std::min(255u, pixel.g * 255u / pixel.a));
                pixel.b = static_cast<sf::Uint8>(std::min(255u, pixel.b * 255u / pixel.a));
                image.setPixel(x, y, pixel);
            }
        }
    return image;
}

class Presenter {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    void* pixels = nullptr;
    int allocated = 0;
    sf::RenderTexture target;
    sf::Texture video;
public:
    bool Export(const std::filesystem::path& path){return StraightImage(target.getTexture()).saveToFile(path.string());}
    ~Presenter() {
        if (previous) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
    bool Paint(HWND window, Artwork& artwork) {
        if (allocated != size) {
            if (previous) { SelectObject(dc, previous); previous = nullptr; }
            if (bitmap) { DeleteObject(bitmap); bitmap = nullptr; }
            if (!dc) dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = size; info.bmiHeader.biHeight = -size;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            if (!bitmap || !target.create(static_cast<unsigned>(size), static_cast<unsigned>(size))) return false;
            previous = SelectObject(dc, bitmap);
            allocated = size;
            target.setView(sf::View(sf::FloatRect(ViewX, 1.f, static_cast<float>(SceneSize), static_cast<float>(SceneSize))));
        }
        int frame=laughReady?laughPlayback.frame(frog::NowNs()):-1;
        if(frame>=0 && laughFrames.decode(frame)) {
            if(video.getSize().x!=laughFrames.width && !video.create(laughFrames.width,laughFrames.height))return false;
            video.setSmooth(true);video.update(laughFrames.rgba.data());
            sf::Sprite sprite(video);sprite.setPosition(ViewX+frog::LaughDisplayX,1.f);
            sprite.setScale(float(frog::LaughDisplaySize)/laughFrames.width,float(frog::LaughDisplaySize)/laughFrames.height);
            target.clear(sf::Color::Transparent);
            const sf::BlendMode premultiplied(sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,
                sf::BlendMode::Add,sf::BlendMode::One,sf::BlendMode::OneMinusSrcAlpha,sf::BlendMode::Add);
            target.draw(sprite,sf::RenderStates(premultiplied));
        } else {
            if(frame>=0){laughPlayback.stop();SyncLaughAudio();}
            artwork.Draw(target, held);
        }
        target.display();
        const auto image = target.getTexture().copyToImage();
        const auto* rgba = image.getPixelsPtr();
        auto* bgra = static_cast<unsigned char*>(pixels);
        for (int i = 0; i < size * size; ++i) {
            bgra[i * 4] = rgba[i * 4 + 2]; bgra[i * 4 + 1] = rgba[i * 4 + 1];
            bgra[i * 4 + 2] = rgba[i * 4]; bgra[i * 4 + 3] = rgba[i * 4 + 3];
        }
        POINT source{}; SIZE dimensions{ size, size };
        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        return UpdateLayeredWindow(window, nullptr, nullptr, &dimensions, dc, &source, 0, &blend, ULW_ALPHA) != FALSE;
    }
};

int BenchmarkParticles(Artwork& artwork,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::ofstream report(directory/"render-benchmark.csv");
    report << "size,frames,mean_ms,p50_ms,p95_ms,max_ms,paced_fps,paced_p95_frame_ms\n";
    for(int dimension : {380,600,800}) {
        size=dimension;
        HWND window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
            L"STATIC",L"MilkFrog hidden render benchmark",WS_POPUP,0,0,size,size,
            nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window) return 2;
        Presenter presenter;
        artwork.effects.Reset(); held=15;
        std::vector<float> samples;
        for(unsigned frame=0;frame<150;++frame) {
            artwork.effects.Advance(1.f/120.f,held);
            sf::Clock timer;
            if(!presenter.Paint(window,artwork)){DestroyWindow(window);return 3;}
            if(frame>=30)samples.push_back(timer.getElapsedTime().asMicroseconds()/1000.f);
        }
        float total=0.f;
        for(float value:samples)total+=value;
        std::sort(samples.begin(),samples.end());
        FrameWaiter waiter;
        sf::Clock frameClock,pacingClock;
        float lastStart=0.f,firstStart=0.f;
        std::vector<float> intervals;
        for(unsigned frame=0;frame<=120;++frame) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            const float start=pacingClock.getElapsedTime().asSeconds();
            if(frame==0)firstStart=start;
            else intervals.push_back((start-lastStart)*1000.f);
            lastStart=start;
            artwork.effects.Advance(frameClock.restart().asSeconds(),held);
            if(!presenter.Paint(window,artwork)){DestroyWindow(window);return 3;}
            if(frame<120)waiter.Wait(true,false,frameClock);
        }
        const float pacedFps=120.f/(lastStart-firstStart);
        std::sort(intervals.begin(),intervals.end());
        DestroyWindow(window);
        report << dimension << ',' << samples.size() << ',' << total/samples.size() << ','
            << samples[samples.size()/2] << ',' << samples[samples.size()*95/100] << ',' << samples.back() << ','
            << pacedFps << ',' << intervals[intervals.size()*95/100] << '\n';
    }
    artwork.effects.Reset(); held=0;
    return 0;
}

void Resize(HWND window, int delta) {
    size = std::clamp(size + delta, 220, 800);
    SetWindowPos(window, nullptr, 0, 0, size, size, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    redraw = true;
}
bool AddTray(HWND window) {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
    data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=TrayMessage;
    data.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101));
    wcscpy_s(data.szTip,L"奶蛙桌宠 — 右键放大、缩小或修改键位");
    trayAdded=Shell_NotifyIconW(NIM_ADD,&data)!=FALSE;
    if(trayAdded){data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);}
    return trayAdded;
}
void RemoveTray(HWND window) {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
    if(trayAdded)Shell_NotifyIconW(NIM_DELETE,&data);
    trayAdded=false;
}
void RefreshKeyButtons() {
    constexpr const wchar_t* positions[]={L"左手外侧",L"左手内侧",L"右手内侧",L"右手外侧"};
    for(unsigned i=0;i<4;++i) {
        const std::wstring text=std::wstring(positions[i])+L"："+KeyName(editedKeys[i]);
        SetWindowTextW(keyButtons[i],text.c_str());
    }
}
LRESULT CALLBACK KeyDialogProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch(message) {
    case WM_CREATE: {
        const HINSTANCE instance=GetModuleHandleW(nullptr);
        auto control=[&](const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id) {
            HWND child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            return child;
        };
        control(L"STATIC",L"点击要更换的键位，再按一下目标键。四个键位不能重复。",0,18,16,450,26,0);
        for(unsigned i=0;i<4;++i)keyButtons[i]=control(L"BUTTON",L"",WS_TABSTOP,18+static_cast<int>(i%2)*230,
            52+static_cast<int>(i/2)*46,218,36,201+i);
        keyHint=control(L"STATIC",L"保存后立即更新动作与键帽字母；关闭重开仍会保留。",0,18,151,450,38,0);
        control(L"BUTTON",L"恢复 D / F / J / K",WS_TABSTOP,18,198,168,34,210);
        control(L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,274,198,90,34,IDOK);
        control(L"BUTTON",L"取消",WS_TABSTOP,376,198,90,34,IDCANCEL);
        RefreshKeyButtons();return 0;
    }
    case CapturedKeyMessage:
        if(wparam!=VK_ESCAPE && lparam>=0 && lparam<4)editedKeys[static_cast<unsigned>(lparam)]=static_cast<int>(wparam);
        RefreshKeyButtons();
        SetWindowTextW(keyHint,wparam==VK_ESCAPE?L"已取消本次录入。":L"键位已录入。点击保存，立即更新动作与键帽字母。");
        return 0;
    case WM_COMMAND: {
        const unsigned command=LOWORD(wparam);
        if(command>=201 && command<=204) {
            captureSlot=static_cast<int>(command-201);
            SetWindowTextW(keyHint,L"请按一下目标键（Esc 取消本次录入）…");return 0;
        }
        if(command==210) {captureSlot=-1;editedKeys=DefaultKeys;RefreshKeyButtons();return 0;}
        if(command==IDOK) {
            captureSlot=-1;
            for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)if(editedKeys[i]==editedKeys[j]) {
                SetWindowTextW(keyHint,L"有重复键位，请为四个位置设置不同的键。");return 0;
            }
            if(!SaveConfig(editedKeys)) {
                SetWindowTextW(keyHint,L"配置保存失败，请检查用户数据目录是否可写。");return 0;
            }
            keys=editedKeys;held=0;pendingBursts=0;redraw=true;
            DestroyWindow(window);return 0;
        }
        if(command==IDCANCEL){DestroyWindow(window);return 0;}
        break;
    }
    case WM_KEYDOWN:
        if(wparam==VK_ESCAPE){DestroyWindow(window);return 0;}
        break;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:
        captureSlot=-1;keyDialog=nullptr;held=0;pendingBursts=0;redraw=true;
        laughPlayback.syncPressed();return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
void EditKeys(HWND owner,bool show=true) {
    laughPlayback.stop();SyncLaughAudio();
    if(laughDialog)DestroyWindow(laughDialog);
    if(keyDialog){ShowWindow(keyDialog,SW_RESTORE);SetForegroundWindow(keyDialog);return;}
    editedKeys=keys;captureSlot=-1;held=0;pendingBursts=0;redraw=true;
    WNDCLASSW type{};type.lpfnWndProc=KeyDialogProc;type.hInstance=GetModuleHandleW(nullptr);
    type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    type.hIcon=LoadIconW(type.hInstance,MAKEINTRESOURCEW(101));type.lpszClassName=L"MilkFrogKeySettings";
    RegisterClassW(&type);
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT bounds{0,0,484,252};AdjustWindowRectEx(&bounds,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME);
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    keyDialog=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"奶蛙桌宠 · 修改键位",
        WS_CAPTION|WS_SYSMENU,(monitor.rcWork.left+monitor.rcWork.right-width)/2,
        (monitor.rcWork.top+monitor.rcWork.bottom-height)/2,width,height,owner,nullptr,type.hInstance,nullptr);
    if(keyDialog && show){ShowWindow(keyDialog,SW_SHOW);SetForegroundWindow(keyDialog);}
}
std::wstring LaughHotkeyName(const frog::KeyChord& chord) {
    std::wstring result;
    auto append=[&](const std::wstring& label){if(!result.empty())result+=L" + ";result+=label;};
    for(unsigned modifier:{unsigned(VK_CONTROL),unsigned(VK_MENU),unsigned(VK_SHIFT),unsigned(VK_LWIN)})
        if(std::find(chord.codes.begin(),chord.codes.begin()+chord.count,modifier)!=chord.codes.begin()+chord.count)
            append(modifier==VK_CONTROL?L"Ctrl":modifier==VK_MENU?L"Alt":modifier==VK_SHIFT?L"Shift":L"Win");
    for(unsigned i=0;i<chord.count;++i) {
        const auto code=chord.codes[i];
        if(code!=VK_CONTROL && code!=VK_MENU && code!=VK_SHIFT && code!=VK_LWIN)append(KeyName(int(code)));
    }
    return result.empty()?L"尚未录入":result;
}
void RefreshLaughButton(){SetWindowTextW(laughButton,LaughHotkeyName(editedLaughHotkey).c_str());}
LRESULT CALLBACK LaughDialogProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch(message) {
    case WM_CREATE: {
        const auto instance=GetModuleHandleW(nullptr);
        auto control=[&](const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id) {
            HWND child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window,
                reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;
        };
        control(L"STATIC",L"点击下方按钮，按下完整组合键，再松开全部按键。",0,18,16,454,28,0);
        laughButton=control(L"BUTTON",L"",WS_TABSTOP,18,52,454,42,301);
        laughHint=control(L"STATIC",L"支持 Ctrl + Q、Ctrl + Shift + Q 等组合；保存后同步到 OBS。",0,18,110,454,48,0);
        control(L"BUTTON",L"恢复 F5",WS_TABSTOP,18,171,120,34,310);
        control(L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,282,171,90,34,IDOK);
        control(L"BUTTON",L"取消",WS_TABSTOP,384,171,90,34,IDCANCEL);
        RefreshLaughButton();return 0;
    }
    case LaughChordMessage:
        if(wparam==2) {
            RefreshLaughButton();SetWindowTextW(laughHint,L"已取消本次录入，原设置未改变。");return 0;
        }
        if(wparam==1) {
            if(laughCapture.overflow){RefreshLaughButton();SetWindowTextW(laughHint,L"组合最多支持 8 个按键，请重新录入。");return 0;}
            editedLaughHotkey=laughCapture.chord;RefreshLaughButton();
            SetWindowTextW(laughHint,L"组合键已录入。点击保存，桌宠与 OBS 会同步使用。");
        } else SetWindowTextW(laughButton,LaughHotkeyName(laughCapture.chord).c_str());
        return 0;
    case WM_COMMAND:
        if(LOWORD(wparam)==301) {
            laughCapture.reset();laughCapturing=true;SetWindowTextW(laughButton,L"等待录入…");
            SetWindowTextW(laughHint,L"请按下组合键并松开全部按键；Esc 取消本次录入。");return 0;
        }
        if(LOWORD(wparam)==310){laughCapturing=false;editedLaughHotkey=frog::KeyChord();RefreshLaughButton();return 0;}
        if(LOWORD(wparam)==IDCANCEL){DestroyWindow(window);return 0;}
        if(LOWORD(wparam)==IDOK) {
            if(laughCapturing){SetWindowTextW(laughHint,L"请先完成录入并松开全部按键。");return 0;}
            for(unsigned i=0;i<editedLaughHotkey.count;++i)for(int key:keys)
                if(editedLaughHotkey.codes[i]==frog::ChordKey(unsigned(key))) {
                    SetWindowTextW(laughHint,L"与敲键键位冲突，请选择另一组按键。");return 0;
                }
            const auto previous=laughPlayback.hotkey;
            laughPlayback.setHotkey(editedLaughHotkey);
            if(!SaveConfig(keys)) {
                laughPlayback.setHotkey(previous);SetWindowTextW(laughHint,L"配置保存失败，请重试。");return 0;
            }
            DestroyWindow(window);return 0;
        }
        break;
    case WM_KEYDOWN:if(wparam==VK_ESCAPE){DestroyWindow(window);return 0;}break;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:
        laughCapturing=false;laughDialog=nullptr;laughPlayback.syncPressed();SyncKeys();return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
void EditLaughKey(HWND owner,bool show=true) {
    laughPlayback.stop();SyncLaughAudio();
    if(keyDialog)DestroyWindow(keyDialog);
    if(laughDialog){ShowWindow(laughDialog,SW_RESTORE);SetForegroundWindow(laughDialog);return;}
    editedLaughHotkey=laughPlayback.hotkey;laughCapturing=false;held=0;pendingBursts=0;redraw=true;
    WNDCLASSW type{};type.lpfnWndProc=LaughDialogProc;type.hInstance=GetModuleHandleW(nullptr);
    type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    type.hIcon=LoadIconW(type.hInstance,MAKEINTRESOURCEW(101));type.lpszClassName=L"MilkFrogLaughHotkey";RegisterClassW(&type);
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);
    RECT bounds{0,0,490,225};AdjustWindowRectEx(&bounds,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME);
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    laughDialog=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"重开大笑键",WS_CAPTION|WS_SYSMENU,
        (monitor.rcWork.left+monitor.rcWork.right-width)/2,(monitor.rcWork.top+monitor.rcWork.bottom-height)/2,
        width,height,owner,nullptr,type.hInstance,nullptr);
    if(laughDialog && show){ShowWindow(laughDialog,SW_SHOW);SetForegroundWindow(laughDialog);}
}
enum Command { Bigger = 101, Smaller, Top, Edit, Reload, Exit, EditLaugh };
void Menu(HWND window) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, Bigger, L"放大");
    AppendMenuW(menu, MF_STRING, Smaller, L"缩小");
    AppendMenuW(menu, MF_STRING | (topmost ? MF_CHECKED : 0), Top, L"置顶");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, Edit, L"修改键位…");
    const std::wstring laughMenu=L"重开大笑键（"+LaughHotkeyName(laughPlayback.hotkey)+L"）…";
    AppendMenuW(menu, MF_STRING, EditLaugh, laughMenu.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, Exit, L"退出");
    POINT point{}; GetCursorPos(&point); SetForegroundWindow(window);
    const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command == Bigger) Resize(window, 40);
    else if (command == Smaller) Resize(window, -40);
    else if (command == Top) {
        topmost = !topmost;
        SetWindowPos(window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else if (command == Edit) EditKeys(window);
    else if (command == EditLaugh) EditLaughKey(window);
    else if (command == Exit) DestroyWindow(window);
    if(!keyDialog && !laughDialog)SyncKeys();
    PostMessageW(window, WM_NULL, 0, 0);
}
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if(taskbarCreated && message==taskbarCreated){trayAdded=false;AddTray(window);return 0;}
    switch (message) {
    case TrayMessage:
        if(LOWORD(lparam)==WM_CONTEXTMENU || LOWORD(lparam)==WM_RBUTTONUP)Menu(window);
        else if(LOWORD(lparam)==WM_LBUTTONDBLCLK)EditKeys(window);
        return 0;
    case WM_LBUTTONDOWN:
        ReleaseCapture(); SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0); return 0;
    case WM_RBUTTONUP: Menu(window); return 0;
    case WM_MOUSEWHEEL: Resize(window, GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 20 : -20); return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_SIZE: redraw = true; return 0;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        if(keyDialog)DestroyWindow(keyDialog);
        if(laughDialog)DestroyWindow(laughDialog);
        laughPlayback.stop();SyncLaughAudio();
        RemoveTray(window);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int VerifyLabels(Artwork& artwork,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    if(KeyName(VK_LCONTROL)!=L"LC" || KeyName(VK_RCONTROL)!=L"RC" || KeyName(VK_SPACE)!=L"SPC" ||
       KeyName(VK_LSHIFT)!=L"LS" || KeyName(VK_RSHIFT)!=L"RS" || KeyName(VK_LMENU)!=L"LA" || KeyName(VK_RMENU)!=L"RA")return 31;
    const auto savedKeys=keys;
    keys={VK_LCONTROL,VK_RCONTROL,VK_SPACE,VK_RETURN};held=0;pendingBursts=0;
    for(unsigned mask=0;mask<16;++mask) {
        held=0;
        for(unsigned i=0;i<4;++i)if(mask&(1u<<i)) {
            KBDLLHOOKSTRUCT key{};key.vkCode=keys[i];
            KeyboardHook(HC_ACTION,WM_KEYDOWN,reinterpret_cast<LPARAM>(&key));
        }
        if(held!=mask)return 32;
        for(unsigned i=0;i<4;++i) {
            KBDLLHOOKSTRUCT key{};key.vkCode=keys[i];
            KeyboardHook(HC_ACTION,WM_KEYUP,reinterpret_cast<LPARAM>(&key));
        }
        if(held)return 33;
    }
    sf::RenderTexture target;
    if(!target.create(SceneSize,SceneSize))return 34;
    target.setView(sf::View(sf::FloatRect(ViewX,1.f,float(SceneSize),float(SceneSize))));
    artwork.Draw(target,0,true,false);target.display();
    if(!StraightImage(target.getTexture()).saveToFile((directory/"function-keys-preview.png").string()))return 35;
    unsigned long long checkedPixels=0;
    for(int code=1;code<256;++code) {
        const auto name=KeyName(code);
        if(name.empty() || name.size()>3)return 36;
        target.clear(sf::Color::Transparent);
        for(unsigned slot=0;slot<4;++slot)target.draw(artwork.KeyLabel(code,slot));
        target.display();const auto image=target.getTexture().copyToImage();
        const auto* pixels=image.getPixelsPtr();
        for(unsigned y=0;y<SceneSize;++y)for(unsigned x=0;x<SceneSize;++x) {
            if(!pixels[(y*SceneSize+x)*4+3])continue;
            bool inside=false;
            for(const auto& center:KeyCenters) {
                const float dx=float(x)+0.5f+ViewX-center.x,dy=float(y)+1.5f-center.y;
                // An inset parallelogram within the original keycap's top.
                const float v=(dy-0.18f*dx)/1.1206f,u=dx+0.67f*v;
                if(std::abs(u)<=34.f && std::abs(v)<=18.f){inside=true;break;}
            }
            if(!inside) {
                std::ofstream(directory/"failure.txt")<<"key="<<code<<", pixel="<<x<<','<<y;
                return 37;
            }
            ++checkedPixels;
        }
    }
    keys=savedKeys;held=0;pendingBursts=0;
    std::ofstream report(directory/"report.json");
    report<<"{\n  \"passed\": true,\n  \"virtualKeys\": 255,\n  \"keycapPositions\": 4,\n"
        <<"  \"renderedKeycapChecks\": 1020,\n  \"maximumLabelCharacters\": 3,\n"
        <<"  \"nonzeroAlphaPixelsInsideKeycapInsets\": "<<checkedPixels<<",\n"
        <<"  \"leftRightModifiersDistinct\": true,\n  \"modifierAndSpaceCombinations\": 16\n}\n";
    return 0;
}

int VerifyControls(HWND window,Artwork& artwork,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    configOverride=directory/"test-config.txt";
    if(!SaveConfig(DefaultKeys))return 10;
    LoadConfig();
    if(!trayAdded || keys!=DefaultKeys)return 11;
    // Exercise actual dialog commands, keyboard-hook capture, and save/reload.
    EditKeys(window,false);
    if(!keyDialog)return 12;
    SendMessageW(keyDialog,WM_COMMAND,201,0);
    KBDLLHOOKSTRUCT capture{};capture.vkCode='A';
    if(KeyboardHook(HC_ACTION,WM_KEYDOWN,reinterpret_cast<LPARAM>(&capture))!=1 || held!=0)return 13;
    MSG message{};
    while(PeekMessageW(&message,keyDialog,CapturedKeyMessage,CapturedKeyMessage,PM_REMOVE))DispatchMessageW(&message);
    KeyboardHook(HC_ACTION,WM_KEYUP,reinterpret_cast<LPARAM>(&capture));
    if(editedKeys[0]!='A' || captureSlot!=-1)return 14;
    editedKeys[1]='A';
    SendMessageW(keyDialog,WM_COMMAND,IDOK,0);
    if(!keyDialog || keys!=DefaultKeys)return 15; // Duplicate save must stay open.
    SendMessageW(keyDialog,WM_COMMAND,IDCANCEL,0);
    LoadConfig();if(keys!=DefaultKeys || keyDialog)return 16;
    const std::array<int,4> alternate={'A','S','L',VK_OEM_1};
    sf::RenderTexture target;
    if(!target.create(SceneSize,SceneSize))return 17;
    target.setView(sf::View(sf::FloatRect(ViewX,1.f,float(SceneSize),float(SceneSize))));
    artwork.Draw(target,0,true,false);target.display();
    const auto before=StraightImage(target.getTexture());
    before.saveToFile((directory/"default-keys.png").string());
    EditKeys(window,false);editedKeys=alternate;
    SendMessageW(keyDialog,WM_COMMAND,IDOK,0);
    if(keyDialog || keys!=alternate)return 18;
    keys=DefaultKeys;LoadConfig();if(keys!=alternate)return 19;
    for(unsigned mask=0;mask<16;++mask) {
        held=0;pendingBursts=0;
        for(unsigned i=0;i<4;++i)if(mask&(1u<<i))UpdateKey(alternate[i],true);
        if(held!=mask)return 20;
        UpdateKey('D',true);UpdateKey('F',true);UpdateKey('J',true);UpdateKey('K',true);
        if(held!=mask)return 21;
        for(int key:alternate)UpdateKey(key,false);
        if(held!=0)return 22;
    }
    artwork.Draw(target,0,true,false);target.display();
    const auto after=StraightImage(target.getTexture());
    after.saveToFile((directory/"remapped-keys.png").string());
    constexpr int centerX[]={574,474,378,288},centerY[]={542,525,509,494};
    std::array<unsigned,4> changed{};
    for(unsigned y=0;y<SceneSize;++y)for(unsigned x=0;x<SceneSize;++x) {
        const auto a=before.getPixel(x,y),b=after.getPixel(x,y);
        if(a.a!=b.a)return 23;
        if(a==b)continue;
        bool onKey=false;
        for(unsigned i=0;i<4;++i)if(std::abs(int(x)-centerX[i])<=44 && std::abs(int(y)-centerY[i])<=30) {
            ++changed[i];onKey=true;break;
        }
        if(!onKey)return 24;
    }
    for(unsigned count:changed)if(count<30)return 25;
    const int originalSize=size;Resize(window,40);if(size!=originalSize+40)return 26;
    Resize(window,-40);if(size!=originalSize)return 27;
    RemoveTray(window);if(trayAdded)return 28;
    SendMessageW(window,taskbarCreated,0,0);if(!trayAdded)return 29;
    EditKeys(window,false);SendMessageW(keyDialog,WM_COMMAND,210,0);
    SendMessageW(keyDialog,WM_COMMAND,IDOK,0);LoadConfig();if(keys!=DefaultKeys)return 30;
    std::ofstream report(directory/"report.json");
    report<<"{\n  \"passed\": true,\n  \"trayAddedAndRecreated\": true,\n  \"resizeVerified\": true,\n"
        <<"  \"dialogCaptureSaveCancelAndDuplicateValidation\": true,\n  \"configurationReloadVerified\": true,\n"
        <<"  \"remappedCombinations\": 16,\n  \"unchangedAlphaAndNonLegendPixels\": true,\n"
        <<"  \"changedLegendPixels\": ["<<changed[0]<<','<<changed[1]<<','<<changed[2]<<','<<changed[3]<<"]\n}\n";
    configOverride.clear();LoadConfig();return 0;
}

int Verify(Artwork& artwork, const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    sf::RenderTexture target;
    if (!target.create(SceneSize, SceneSize)) return 2;
    target.setView(sf::View(sf::FloatRect(ViewX, 1.f, static_cast<float>(SceneSize), static_cast<float>(SceneSize))));
    if(!artwork.SaveKeyboard(target))return 20;target.display();
    if(!StraightImage(target.getTexture()).saveToFile((directory/"keyboard-only.png").string()))return 21;
    if(!artwork.SaveShadow(directory/"keyboard-shadow.png"))return 22;
    artwork.Draw(target,0,false,false,false);target.display();
    if(!StraightImage(target.getTexture()).saveToFile((directory/"body-only.png").string()))return 19;
    std::ofstream report(directory / "states.csv");
    report << "mask,D,F,J,K,df_layer,jk_layer,zero_alpha,partial_alpha\n";
    for (unsigned mask = 0; mask < 16; ++mask) {
        held = 0;
        for (unsigned i = 0; i < 4; ++i) if (mask & (1u << i)) {
            KBDLLHOOKSTRUCT key{}; key.vkCode = static_cast<DWORD>(keys[i]);
            KeyboardHook(HC_ACTION, WM_KEYDOWN, reinterpret_cast<LPARAM>(&key));
            KeyboardHook(HC_ACTION, WM_KEYDOWN, reinterpret_cast<LPARAM>(&key));
        }
        if (held != mask) return 3;
        unsigned remaining = mask;
        for (unsigned i = 0; i < 4; ++i) {
            KBDLLHOOKSTRUCT key{}; key.vkCode = static_cast<DWORD>(keys[i]);
            KeyboardHook(HC_ACTION, WM_KEYUP, reinterpret_cast<LPARAM>(&key));
            remaining &= ~(1u << i);
            if (held != remaining) return 4;
        }
        artwork.Draw(target, mask, true, false); target.display();
        const auto image = StraightImage(target.getTexture());
        unsigned transparent = 0, partial = 0;
        for (unsigned y = 0; y < image.getSize().y; ++y)
            for (unsigned x = 0; x < image.getSize().x; ++x) {
                const auto alpha = image.getPixel(x, y).a;
                transparent += alpha == 0; partial += alpha > 0 && alpha < 255;
            }
        if (transparent < 100000 || partial == 0 || image.getPixel(0, 0).a != 0) return 5;
        if (!image.saveToFile((directory / ("state-" + std::to_string(mask) + ".png")).string())) return 6;
        // OBS composites the native background, particles and hands before
        // resizing once. Keep the pose's arm shadow in the background.
        artwork.Draw(target, mask, true, false, true, false); target.display();
        if (!StraightImage(target.getTexture()).saveToFile(
            (directory / ("background-" + std::to_string(mask) + ".png")).string())) return 24;
        artwork.DrawArmShadowOnly(target,mask);target.display();
        if(!StraightImage(target.getTexture()).saveToFile(
            (directory/("arm-shadow-"+std::to_string(mask)+".png")).string()))return 23;
        artwork.Draw(target, mask, false, false); target.display();
        if (!StraightImage(target.getTexture()).saveToFile(
            (directory / ("keyboard-" + std::to_string(mask) + ".png")).string())) return 7;
        artwork.DrawForeground(target, mask); target.display();
        const auto contactImage=StraightImage(target.getTexture());
        for(unsigned key=0;key<4;++key) if(mask&(1u<<key)) {
            const auto anchor=ParticleEffects::Anchor(mask,key)-sf::Vector2f(ViewX,1);
            unsigned fingerPixels=0;
            for(int dy=-8;dy<=0;++dy)for(int dx=-8;dx<=8;++dx) {
                const auto pixel=contactImage.getPixel(static_cast<unsigned>(anchor.x+dx),static_cast<unsigned>(anchor.y+dy));
                if(pixel.a>100 && pixel.r<170 && pixel.g<160 && pixel.b<150)++fingerPixels;
            }
            if(fingerPixels<8)return 16;
        }
        if (!StraightImage(target.getTexture()).saveToFile(
            (directory / ("foreground-" + std::to_string(mask) + ".png")).string())) return 8;
        artwork.effects.Reset();
        artwork.effects.Advance(0.f,mask);
        for (unsigned i=0; i<4; ++i)
            if (artwork.effects.Count(i) != ((mask & (1u<<i)) ? ParticleEffects::BurstCount : 0u)) return 9;
        // OS key repeat does not create another strike burst.
        artwork.effects.Advance(0.f,mask);
        if (artwork.effects.Count() != ParticleEffects::BurstCount * ((mask&1u)+((mask>>1)&1u)+((mask>>2)&1u)+((mask>>3)&1u))) return 10;
        for (unsigned frame=0;frame<4;++frame) artwork.effects.Advance(.03f,mask);
        artwork.Draw(target,mask); target.display();
        if (!StraightImage(target.getTexture()).saveToFile(
            (directory / ("effect-state-" + std::to_string(mask) + ".png")).string())) return 11;
        target.clear(sf::Color::Transparent);
        artwork.effects.DrawSurface(target); artwork.effects.DrawParticles(target); target.display();
        if (!StraightImage(target.getTexture()).saveToFile(
            (directory / ("particles-" + std::to_string(mask) + ".png")).string())) return 12;
        for (unsigned frame=0;frame<25;++frame) artwork.effects.Advance(1.f/60.f,0);
        if (artwork.effects.Active() || artwork.effects.Count() != 0) return 13;
        report << mask;
        for (unsigned i = 0; i < 4; ++i) report << ',' << ((mask >> i) & 1u);
        report << ',' << Files[LeftFrame(mask)] << ',' << Files[RightFrame(mask)] << ',' << transparent << ',' << partial << '\n';
    }
    artwork.effects.Reset();
    artwork.effects.Advance(0.f,0,3); // Fast down/up between rendered frames.
    if (artwork.effects.Count(0)!=ParticleEffects::BurstCount || artwork.effects.Count(1)!=ParticleEffects::BurstCount) return 14;
    artwork.effects.Reset();
    for (unsigned frame=0;frame<600;++frame) {
        artwork.effects.Advance(1.f/60.f,15);
        if (artwork.effects.Count()>ParticleEffects::MaxParticles) return 15;
    }
    // Releasing one key clears its old fountain promptly while the other
    // three keep emitting. Rendering must also be empty after all release.
    for(unsigned frame=0;frame<25;++frame)artwork.effects.Advance(1.f/60.f,14);
    if(artwork.effects.Count(0)!=0 || !artwork.effects.Count(1) ||
        !artwork.effects.Count(2) || !artwork.effects.Count(3))return 24;
    for(unsigned frame=0;frame<25;++frame)artwork.effects.Advance(1.f/60.f,0);
    target.clear(sf::Color::Transparent);
    artwork.effects.DrawSurface(target);artwork.effects.DrawParticles(target);target.display();
    const auto clearEffects=target.getTexture().copyToImage();
    for(unsigned y=0;y<SceneSize;++y)for(unsigned x=0;x<SceneSize;++x)
        if(clearEffects.getPixel(x,y).a)return 25;
    std::ofstream heightReport(directory/"fountain-height.csv");
    heightReport << "mask,highest_visible_y\n";
    for(unsigned mask=1;mask<16;++mask) {
        artwork.effects.Reset();artwork.effects.Advance(0.f,mask);
        unsigned highest=SceneSize;
        for(unsigned frame=1;frame<=60;++frame) {
            artwork.effects.Advance(1.f/60.f,mask);
            if(frame!=48 && frame!=60)continue;
            target.clear(sf::Color::Transparent);
            artwork.effects.DrawParticles(target);target.display();
            const auto image=target.getTexture().copyToImage();
            unsigned top=SceneSize;
            for(unsigned y=0;y<SceneSize && top==SceneSize;++y)
                for(unsigned x=0;x<SceneSize;++x)if(image.getPixel(x,y).a>=96){top=y;break;}
            highest=std::min(highest,top);
        }
        if(highest<200 || highest>285)return 17;
        heightReport << mask << ',' << highest << '\n';
        artwork.Draw(target,mask);target.display();
        if(!StraightImage(target.getTexture()).saveToFile(
            (directory/("fountain-height-"+std::to_string(mask)+".png")).string()))return 18;
    }
    artwork.effects.Reset(); held = pendingBursts = 0;
    std::ofstream(directory / "particle-verification.txt") <<
        "PASS: 16 combinations, per-key strike bursts, no repeat bursts, fast taps, release decay, bounded continuous emission.\n"
        "PASS: released-key particles fade within 0.42 seconds; other held-key fountains remain active; no residual visible effects after all release.\n"
        "PASS: all 15 active combinations reach the neck-height band (visible spark y=200..285).\n";
    return 0;
}
int PreviewEffects(Artwork& artwork, const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    sf::RenderTexture target;
    if (!target.create(SceneSize,SceneSize)) return 2;
    target.setView(sf::View(sf::FloatRect(ViewX,1.f,static_cast<float>(SceneSize),static_cast<float>(SceneSize))));
    artwork.effects.Reset();
    for (unsigned frame=0;frame<432;++frame) {
        unsigned mask=0;
        if (frame>=24 && frame<50) mask=1;
        if (frame>=72 && frame<98) mask=2;
        if (frame>=120 && frame<146) mask=4;
        if (frame>=168 && frame<194) mask=8;
        if (frame>=216 && frame<252) mask=15;
        artwork.effects.Advance(1.f/60.f,mask);
        artwork.Draw(target,mask); target.display();
        const std::string number=std::to_string(1000+frame).substr(1);
        if (!StraightImage(target.getTexture()).saveToFile((directory / ("frame-"+number+".png")).string())) return 3;
    }
    return 0;
}
int VerifyScales(Artwork& artwork,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::array<std::vector<unsigned char>,16> solid;
    sf::RenderTexture native;
    if (!native.create(SceneSize,SceneSize)) return 2;
    native.setView(sf::View(sf::FloatRect(ViewX,1.f,static_cast<float>(SceneSize),static_cast<float>(SceneSize))));
    for (unsigned mask=0;mask<16;++mask) {
        artwork.Draw(native,mask,true,false); native.display();
        const auto reference=native.getTexture().copyToImage();
        const auto* rgba=reference.getPixelsPtr();
        solid[mask].resize(SceneSize*SceneSize);
        // Only pixels surrounded by solid artwork are checked. Natural
        // antialiased silhouettes and spaces between fingers stay transparent.
        for (unsigned y=2;y<SceneSize-2;++y) for (unsigned x=2;x<SceneSize-2;++x) {
            bool inside=true;
            for (int dy=-2;dy<=2 && inside;++dy) for (int dx=-2;dx<=2;++dx)
                if (rgba[((y+dy)*SceneSize+x+dx)*4+3]<250) {inside=false;break;}
            solid[mask][y*SceneSize+x]=inside;
        }
    }
    std::ofstream report(directory/"scale-verification.csv");
    report << "size,mask,solid_pixels_checked,minimum_alpha\n";
    unsigned long long checked=0;
    unsigned legacyHoles=0;
    Artwork legacy;
    const bool hasLegacy=std::filesystem::exists("legacy-seam-fixture/base.png");
    std::vector<unsigned char> legacySolid(SceneSize*SceneSize);
    if(hasLegacy) {
        if(!legacy.Load("legacy-seam-fixture")) return 8;
        legacy.Draw(native,0,true,false);native.display();
        const auto reference=native.getTexture().copyToImage();
        for(unsigned y=2;y<SceneSize-2;++y) for(unsigned x=2;x<SceneSize-2;++x) {
            bool inside=true;
            for(int dy=-2;dy<=2 && inside;++dy) for(int dx=-2;dx<=2;++dx)
                if(reference.getPixel(x+dx,y+dy).a<250){inside=false;break;}
            legacySolid[y*SceneSize+x]=inside;
        }
    }
    for (unsigned dimension=220;dimension<=800;dimension+=20) {
        sf::RenderTexture target;
        if (!target.create(dimension,dimension)) return 3;
        target.setView(sf::View(sf::FloatRect(ViewX,1.f,static_cast<float>(SceneSize),static_cast<float>(SceneSize))));
        for (unsigned mask=0;mask<16;++mask) {
            artwork.Draw(target,mask,true,false); target.display();
            const auto image=target.getTexture().copyToImage();
            const auto* rgba=image.getPixelsPtr();
            unsigned tested=0,minimum=255;
            for (unsigned y=0;y<dimension;++y) for (unsigned x=0;x<dimension;++x) {
                const unsigned sx=std::min(SceneSize-1u,static_cast<unsigned>((x+.5f)*static_cast<float>(SceneSize)/dimension));
                const unsigned sy=std::min(SceneSize-1u,static_cast<unsigned>((y+.5f)*static_cast<float>(SceneSize)/dimension));
                if (!solid[mask][sy*SceneSize+sx]) continue;
                const unsigned alpha=rgba[(y*dimension+x)*4+3];
                if (alpha<248) {
                    report << "FAIL," << dimension << ',' << mask << ',' << x << ',' << y << ',' << alpha << '\n';
                    return 4;
                }
                minimum=std::min(minimum,alpha); tested++;
            }
            if (image.getPixel(0,0).a || image.getPixel(dimension-1,0).a ||
                image.getPixel(0,dimension-1).a || image.getPixel(dimension-1,dimension-1).a) return 5;
            checked+=tested; report<<dimension<<','<<mask<<','<<tested<<','<<minimum<<'\n';
            if (mask==0 && !StraightImage(target.getTexture()).saveToFile(
                (directory/("scale-"+std::to_string(dimension)+".png")).string())) return 6;
            if (dimension==380 && mask==0 && hasLegacy) {
                legacy.DrawLayers(target,0,true,false); target.display();
                const auto legacyImage=target.getTexture().copyToImage();
                for (unsigned y=0;y<dimension;++y) for (unsigned x=0;x<dimension;++x) {
                    const unsigned sx=static_cast<unsigned>((x+.5f)*static_cast<float>(SceneSize)/dimension);
                    const unsigned sy=static_cast<unsigned>((y+.5f)*static_cast<float>(SceneSize)/dimension);
                    if (legacySolid[sy*SceneSize+sx] && legacyImage.getPixel(x,y).a<248) legacyHoles++;
                }
                StraightImage(target.getTexture()).saveToFile((directory/"before-scaling-fix-380.png").string());
            }
        }
    }
    // Proves the test detects the reported failure in the old drawing order.
    if (hasLegacy && legacyHoles==0) return 7;
    std::ofstream summary(directory/"scale-verification.txt");
    summary << "PASS: 30 sizes (220..800, step 20), 16 states each; "
        << checked << " interior alpha checks; zero seams; transparent corners.\n";
    if(hasLegacy) summary << "Old separate-layer scaling reproduces " << legacyHoles << " alpha holes at size 380.\n";
    return 0;
}
int VerifyVideo(HWND window,Artwork& artwork,const std::filesystem::path& directory) {
    if(!laughReady)return 41;
    laughPlayback.setHotkey(frog::KeyChord());
    std::filesystem::create_directories(directory);
    Presenter presenter;
    held=0;pendingBursts=0;artwork.effects.Reset();
    if(!presenter.Paint(window,artwork) || !presenter.Export(directory/"desktop-idle.png"))return 42;
    UpdateKey(VK_F5,true);UpdateKey(VK_F5,false);
    if(!laughPlayback.playing || !presenter.Paint(window,artwork) || !presenter.Export(directory/"desktop-video.png"))return 43;
    const auto start=laughPlayback.started;UpdateKey(VK_F5,true);
    if(laughPlayback.started==start)return 44;
    const auto restarted=laughPlayback.started;UpdateKey(VK_F5,true);
    if(laughPlayback.started!=restarted)return 45;
    UpdateKey(VK_F5,false);
    for(const auto bindings:{DefaultKeys,std::array<int,4>{'A','S',VK_LCONTROL,VK_SPACE}}) {
        keys=bindings;
        for(int code:bindings) {
            UpdateKey(VK_F5,true);UpdateKey(VK_F5,false);
            UpdateKey(code,true);
            if(laughPlayback.playing || held==0)return 46;
            if(!presenter.Paint(window,artwork))return 47;
            UpdateKey(code,false);if(held || laughPlayback.playing)return 48;
        }
    }
    keys=DefaultKeys;pendingBursts=0;artwork.effects.Reset();
    UpdateKey(VK_F5,true);UpdateKey(VK_F5,false);
    laughPlayback.started=frog::NowNs()-9000000000ull;
    if(!presenter.Paint(window,artwork) || laughPlayback.playing || !presenter.Export(directory/"desktop-restored.png"))return 49;
    SyncLaughAudio();
    sf::Image before,after;
    if(!before.loadFromFile((directory/"desktop-idle.png").string()) || !after.loadFromFile((directory/"desktop-restored.png").string()))return 50;
    if(memcmp(before.getPixelsPtr(),after.getPixelsPtr(),size*size*4)!=0)return 51;
    std::ofstream(directory/"desktop-verification.txt")<<"PASS: F5 single playback/restart/repeat suppression, all DFJK and rebound A/S/LC/Space cancellations, automatic end and exact idle restoration.\n";
    return 0;
}
int VerifyLaughHotkey(HWND window,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);configOverride=directory/"test-config.txt";
    keys=DefaultKeys;laughPlayback.setHotkey(frog::KeyChord());
    if(!SaveConfig(keys))return 61;
    EditLaughKey(window,false);
    if(!laughDialog)return 62;
    wchar_t title[64]{};GetWindowTextW(laughDialog,title,64);
    if(std::wstring(title)!=L"重开大笑键")return 63;
    SendMessageW(laughDialog,WM_COMMAND,301,0);
    auto capture=[&](unsigned code,bool down) {
        KBDLLHOOKSTRUCT key{};key.vkCode=code;
        const auto result=KeyboardHook(HC_ACTION,down?WM_KEYDOWN:WM_KEYUP,reinterpret_cast<LPARAM>(&key));
        MSG message{};
        while(PeekMessageW(&message,nullptr,LaughChordMessage,LaughChordMessage,PM_REMOVE))DispatchMessageW(&message);
        return result;
    };
    if(capture(VK_RCONTROL,true)!=1 || capture('Q',true)!=1 || capture('Q',true)!=1)return 64;
    capture('Q',false);if(!laughCapturing)return 65;
    capture(VK_RCONTROL,false);
    if(laughCapturing || editedLaughHotkey.text()!="17+81")return 66;
    SendMessageW(laughDialog,WM_COMMAND,IDOK,0);
    if(laughDialog)return 67;
    LoadConfig();if(laughPlayback.hotkey.text()!="17+81")return 68;
    UpdateKey('Q',true);if(laughPlayback.playing)return 69;UpdateKey('Q',false);
    UpdateKey(VK_LCONTROL,true);UpdateKey('Q',true);
    if(!laughPlayback.playing)return 70;
    const auto started=laughPlayback.started;UpdateKey('Q',true);
    if(laughPlayback.started!=started)return 71;
    UpdateKey('Q',false);UpdateKey('Q',true);if(laughPlayback.started==started)return 72;
    UpdateKey('D',true);if(laughPlayback.playing)return 73;
    for(unsigned key:{unsigned('Q'),unsigned('D'),unsigned(VK_LCONTROL)})UpdateKey(key,false);
    EditLaughKey(window,false);SendMessageW(laughDialog,WM_COMMAND,310,0);
    SendMessageW(laughDialog,WM_COMMAND,IDCANCEL,0);LoadConfig();
    if(laughPlayback.hotkey.text()!="17+81")return 74;
    EditLaughKey(window,false);editedLaughHotkey.count=0;editedLaughHotkey.add('D');
    SendMessageW(laughDialog,WM_COMMAND,IDOK,0);if(!laughDialog)return 75;
    SendMessageW(laughDialog,WM_COMMAND,301,0);capture(VK_ESCAPE,true);capture(VK_ESCAPE,false);
    if(laughCapturing || !laughDialog)return 76;
    SendMessageW(laughDialog,WM_COMMAND,310,0);SendMessageW(laughDialog,WM_COMMAND,IDOK,0);
    LoadConfig();if(laughPlayback.hotkey.text()!="116")return 77;
    EditLaughKey(window,false);SendMessageW(laughDialog,WM_COMMAND,301,0);
    capture(VK_LCONTROL,true);capture(VK_RSHIFT,true);capture('Q',true);
    capture('Q',false);capture(VK_LCONTROL,false);capture(VK_RSHIFT,false);
    SendMessageW(laughDialog,WM_COMMAND,IDOK,0);LoadConfig();
    if(laughPlayback.hotkey.text()!="16+17+81")return 78;
    const std::array<int,4> alternate={'A','S','J','K'};
    if(!SaveConfig(alternate))return 79;
    LoadConfig();if(keys!=alternate || laughPlayback.hotkey.text()!="16+17+81")return 80;
    laughPlayback.setHotkey(frog::KeyChord());UpdateKey(VK_F5,true);
    EditKeys(window,false);
    KBDLLHOOKSTRUCT released{};released.vkCode=VK_F5;
    KeyboardHook(HC_ACTION,WM_KEYUP,reinterpret_cast<LPARAM>(&released));
    SendMessageW(keyDialog,WM_COMMAND,IDCANCEL,0);
    UpdateKey(VK_F5,true);if(!laughPlayback.playing)return 81;
    UpdateKey(VK_F5,false);laughPlayback.stop();SyncLaughAudio();
    std::ofstream(directory/"report.txt")<<"PASS: actual settings dialog and keyboard-hook capture, Ctrl+Q and Ctrl+Shift+Q, right Ctrl, autorepeat suppression, restart, gameplay cancellation, persistence, cancel, Esc, restore F5, conflicting keys and gameplay-settings preservation.\n";
    configOverride.clear();LoadConfig();return 0;
}
} // namespace

int main(int argc, char** argv) {
    std::array<wchar_t, 32768> executable{};
    if (!GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()))) return 1;
    std::filesystem::current_path(std::filesystem::path(executable.data()).parent_path());
    if(argc>=3 && std::string(argv[1])=="--export-tabletop") {
        sf::RenderTexture surface;
        if(!DeskSurface::Render(surface,Width,Height))return 2;
        if(!StraightImage(surface.getTexture()).saveToFile(argv[2]))return 3;
        if(argc>=4) {
            if(!DeskSurface::Render(surface,Width,Height,true))return 2;
            if(!StraightImage(surface.getTexture()).saveToFile(argv[3]))return 3;
        }
        return 0;
    }
    LoadConfig();
    laughReady=laughFrames.load("laugh-frames.mfa") && std::filesystem::is_regular_file("laugh.wav");
    laughPlayback.syncPressed();
    // OBS exports use their established DFJK mapping independently of the
    // desktop user's saved bindings. Verification never rewrites that file.
    if(argc>=4 && std::string(argv[3])=="--default-keys"){keys=DefaultKeys;held=0;pendingBursts=0;}
    Artwork artwork;
    if (!artwork.Load()) return 1;
    if (argc >= 3 && std::string(argv[1]) == "--verify-labels") return VerifyLabels(artwork,argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "--verify-assets") return Verify(artwork, argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "--preview-effects") return PreviewEffects(artwork, argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "--verify-scales") return VerifyScales(artwork, argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "--benchmark-particles") return BenchmarkParticles(artwork, argv[2]);
    const bool smoke = argc >= 2 && std::string(argv[1]) == "--smoke-test";
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.lpfnWndProc = WindowProc; type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    type.lpszClassName = L"MilkFrogTransparentFresh";
    if (!RegisterClassW(&type)) return 1;
    RECT desktop{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    int windowX=desktop.right-size-30,windowY=desktop.bottom-size-30;
    if (argc==5 && std::string(argv[1])=="--window") {
        try {windowX=std::stoi(argv[2]);windowY=std::stoi(argv[3]);size=std::clamp(std::stoi(argv[4]),220,800);}
        catch (...) {return 3;}
    }
    // Restore in this process's logical coordinates, keeping the full pet on
    // the nearest monitor after DPI, display-layout or saved-position changes.
    RECT proposed{windowX,windowY,windowX+size,windowY+size};
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromRect(&proposed,MONITOR_DEFAULTTONEAREST),&monitor)) {
        const RECT& work=monitor.rcWork;
        windowX=static_cast<int>(std::clamp(static_cast<LONG>(windowX),work.left,std::max(work.left,work.right-size)));
        windowY=static_cast<int>(std::clamp(static_cast<LONG>(windowY),work.top,std::max(work.top,work.bottom-size)));
    }
    HWND window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        type.lpszClassName, L"奶蛙桌宠", WS_POPUP,
        windowX,windowY,
        size, size, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(!AddTray(window)) {
        DestroyWindow(window);MessageBoxW(nullptr,L"无法添加系统托盘图标，请重试。",L"奶蛙桌宠",MB_ICONERROR);return 1;
    }
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, instance, 0);
    if (!hook) { DestroyWindow(window); MessageBoxW(nullptr, L"无法安装键盘监听。", L"奶蛙桌宠", MB_ICONERROR); return 1; }
    if(argc>=3 && std::string(argv[1])=="--verify-controls") {
        const int result=VerifyControls(window,artwork,std::filesystem::absolute(argv[2]));
        UnhookWindowsHookEx(hook);DestroyWindow(window);return result;
    }
    if(argc>=3 && std::string(argv[1])=="--verify-laugh") {
        const int result=VerifyVideo(window,artwork,std::filesystem::absolute(argv[2]));
        UnhookWindowsHookEx(hook);DestroyWindow(window);return result;
    }
    if(argc>=3 && std::string(argv[1])=="--verify-laugh-hotkey") {
        const int result=VerifyLaughHotkey(window,std::filesystem::absolute(argv[2]));
        UnhookWindowsHookEx(hook);DestroyWindow(window);return result;
    }
    Presenter presenter;
    if (!presenter.Paint(window, artwork)) { UnhookWindowsHookEx(hook); DestroyWindow(window); return 2; }
    redraw = false;
    if (!smoke) SetWindowPos(window,HWND_TOPMOST,0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    int result = 0;
    sf::Clock clock;
    sf::Clock frameClock;
    FrameWaiter frameWaiter;
    unsigned smokeMask = 0;
    bool running = true;
    int displayedVideoFrame=-1;
    while (running) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { running = false; break; }
            const HWND dialog=keyDialog?keyDialog:laughDialog;
            if(!dialog || !IsDialogMessageW(dialog,&message)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
        if (!running) break;
        const int videoFrame=laughReady?laughPlayback.frame(frog::NowNs()):-1;
        if(videoFrame!=displayedVideoFrame){displayedVideoFrame=videoFrame;redraw=true;}
        SyncLaughAudio();
        if (smoke) {
            held = smokeMask++ & 15u; redraw = true;
            if (smokeMask == 17) Resize(window, 40);
            if (smokeMask == 33) Resize(window, -40);
        }
        const float dt = frameClock.getElapsedTime().asSeconds();
        if (dt >= AnimationInterval || redraw) {
            frameClock.restart();
            const bool wasActive = artwork.effects.Active();
            artwork.effects.Advance(wasActive?dt:0.f,held,pendingBursts); pendingBursts = 0;
            redraw |= wasActive || artwork.effects.Active();
        }
        if (redraw) {
            if (!presenter.Paint(window, artwork)) { result = 2; break; }
            redraw = false;
        }
        if (smoke && clock.getElapsedTime().asSeconds() >= 1.f) DestroyWindow(window);
        frameWaiter.Wait(artwork.effects.Active() || laughPlayback.playing || smoke,redraw,frameClock);
    }
    UnhookWindowsHookEx(hook);
    if (IsWindow(window)) DestroyWindow(window);
    return result;
}
