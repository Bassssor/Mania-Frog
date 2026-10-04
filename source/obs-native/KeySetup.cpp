// Standalone OBS key configuration utility. No OBS, Python or SFML runtime needed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include "../shared/ObsKeyLabels.hpp"

namespace {
std::filesystem::path folder;
frog::ObsBindings edited=frog::DefaultObsBindings;
HWND window=nullptr,hint=nullptr,laughButton=nullptr,buttons[4]{};
frog::KeyChord editedLaugh;frog::ChordCapture laughCapture;bool laughCapturing=false;
HHOOK hook=nullptr;
int capturing=-1;
std::array<bool,256> swallowed{};
constexpr UINT Captured=WM_APP+10,LaughCaptured=WM_APP+11;
constexpr int SaveId=110,ResetId=111,LaughId=112,LaughResetId=113;
const wchar_t *positions[4]={L"左手外侧",L"左手内侧",L"右手内侧",L"右手外侧"};
std::wstring failure;
bool AtomicReplace(const std::filesystem::path& temporary,const std::filesystem::path& target) {
    for(int attempt=0;attempt<10;++attempt) {
        if(MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
        if(GetLastError()!=ERROR_SHARING_VIOLATION && GetLastError()!=ERROR_LOCK_VIOLATION)return false;
        Sleep(15);
    }
    return false;
}
bool Apply(const frog::ObsBindings& keys,const frog::KeyChord& laugh) {
    if(!frog::ValidObsBindings(keys)){failure=L"四个键位不能重复或相互重叠。";return false;}
    for(auto key:keys)for(unsigned i=0;i<laugh.count;++i)if(frog::ChordKey(key)==laugh.codes[i]) {
        failure=L"敲键与当前的重开大笑键重叠。请先修改大笑快捷键。";return false;
    }
    const auto preset=folder/L"milk-frog.json",base=folder/L"milk-frog-unlabeled.png";
    if(!std::filesystem::is_regular_file(preset) || !std::filesystem::is_regular_file(base)) {
        failure=L"找不到配套素材。请将本工具保留在完整的 obs-input-overlay 文件夹内。";return false;
    }
    auto rgba=frog::RenderObsKeyLabels(keys);if(rgba.empty()){failure=L"无法生成键帽文字。";return false;}
    const auto labelTemp=folder/L"key-labels.tmp.png",idleTemp=folder/L"milk-frog.tmp.png",configTemp=folder/L"keybindings.tmp.json";
    if(!frog::SaveObsLabels(labelTemp,rgba)){failure=L"无法写入素材，请检查文件夹权限。";return false;}
    {
        frog::GdiSession session;Gdiplus::Bitmap background(base.c_str()),labels(labelTemp.c_str());
        if(background.GetLastStatus()!=Gdiplus::Ok || labels.GetLastStatus()!=Gdiplus::Ok){failure=L"透明素材无法读取。";return false;}
        Gdiplus::Graphics painter(&background);painter.DrawImage(&labels,1,1,880,880);
        CLSID png{};CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
        if(background.Save(idleTemp.c_str(),&png,nullptr)!=Gdiplus::Ok){failure=L"无法保存输入叠加预览。";return false;}
    }
    {
        std::ofstream out(configTemp,std::ios::binary|std::ios::trunc);
        out<<"{\n  \"version\": 1,\n  \"keys\": [";
        for(unsigned i=0;i<4;++i){if(i)out<<", ";out<<keys[i];}
        out<<"],\n  \"revision\": "<<GetTickCount64()<<"\n}\n";
        out.flush();if(!out){failure=L"键位配置无法保存。";return false;}
    }
    if(!AtomicReplace(labelTemp,folder/L"key-labels.png") || !AtomicReplace(idleTemp,folder/L"milk-frog.png") ||
       !AtomicReplace(configTemp,folder/L"keybindings.json")) {
        failure=L"保存失败，请检查文件夹是否可写。原生 OBS 会保留最后一份有效键位。";return false;
    }
    return true;
}
std::wstring LaughText(const frog::KeyChord& chord) {
    std::wstring result;
    auto append=[&](const std::wstring& name){if(!result.empty())result+=L" + ";result+=name;};
    for(unsigned key:{unsigned(VK_CONTROL),unsigned(VK_SHIFT),unsigned(VK_MENU),unsigned(VK_LWIN)})
        if(std::find(chord.codes.begin(),chord.codes.begin()+chord.count,key)!=chord.codes.begin()+chord.count)
            append(key==VK_CONTROL?L"Ctrl":key==VK_SHIFT?L"Shift":key==VK_MENU?L"Alt":L"Win");
    for(unsigned i=0;i<chord.count;++i) {
        const auto key=chord.codes[i];
        if(key!=VK_CONTROL && key!=VK_SHIFT && key!=VK_MENU && key!=VK_LWIN)append(frog::KeyName(key));
    }
    return result;
}
bool SaveLaughShortcut() {
    const auto path=frog::LaughConfigPath();
    std::ifstream input(path,std::ios::binary);std::string line,content;
    while(std::getline(input,line))if(line.rfind("laugh_keys=",0)!=0)content+=line+'\n';
    input.close(); // Windows cannot atomically replace an open configuration file.
    content+="laugh_keys="+editedLaugh.text()+"\n";
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    if(error)return false;
    auto temporary=path;temporary+=L".obs-keys.tmp";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<content;output.flush();if(!output)return false;}
    return AtomicReplace(temporary,path);
}
void Refresh() {
    SetWindowTextW(laughButton,(L"重开大笑键："+LaughText(editedLaugh)).c_str());
    for(unsigned i=0;i<4;++i){auto text=std::wstring(positions[i])+L"："+frog::KeyName(edited[i]);SetWindowTextW(buttons[i],text.c_str());}
}
LRESULT CALLBACK Keyboard(int code,WPARAM message,LPARAM data) {
    if(code==HC_ACTION) {
        auto key=reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN,up=message==WM_KEYUP || message==WM_SYSKEYUP;
        if(key->vkCode<256 && (down||up)) {
            if(swallowed[key->vkCode] && !laughCapturing){if(up)swallowed[key->vkCode]=false;return 1;}
            if(laughCapturing && GetForegroundWindow()==window) {
                swallowed[key->vkCode]=down;
                if(down && key->vkCode==VK_ESCAPE){laughCapturing=false;PostMessageW(window,LaughCaptured,2,0);}
                else {
                    bool complete=laughCapture.key(key->vkCode,down);if(complete)laughCapturing=false;
                    PostMessageW(window,LaughCaptured,complete?1:0,0);
                }
                return 1;
            }
            if(capturing>=0 && down && GetForegroundWindow()==window) {
                const int slot=capturing;capturing=-1;swallowed[key->vkCode]=true;
                PostMessageW(window,Captured,key->vkCode,slot);return 1;
            }
        }
    }
    return CallNextHookEx(hook,code,message,data);
}
LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_CREATE: {
        auto instance=GetModuleHandleW(nullptr);
        auto control=[&](const wchar_t *type,const wchar_t *text,DWORD style,int x,int y,int width,int height,int id){
            auto child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;
        };
        control(L"STATIC",L"点击位置 → 按目标键 → 保存并应用",0,20,18,460,24,0);
        for(int i=0;i<4;++i)buttons[i]=control(L"BUTTON",L"",WS_TABSTOP,20+(i%2)*232,58+(i/2)*55,218,42,100+i);
        laughButton=control(L"BUTTON",L"",WS_TABSTOP,20,172,310,42,LaughId);
        control(L"BUTTON",L"恢复 F5",WS_TABSTOP,344,172,106,42,LaughResetId);
        hint=control(L"STATIC",L"保存后自动更新键帽、敲键绑定和大笑快捷键，无需重启 OBS。",0,20,238,460,65,0);
        control(L"BUTTON",L"恢复 DFJK",WS_TABSTOP,20,326,120,36,ResetId);
        control(L"BUTTON",L"保存并应用",WS_TABSTOP|BS_DEFPUSHBUTTON,218,326,126,36,SaveId);
        control(L"BUTTON",L"关闭",WS_TABSTOP,358,326,100,36,IDCANCEL);
        Refresh();return 0;
    }
    case Captured:
        if(w!=VK_ESCAPE && l>=0 && l<4)edited[size_t(l)]=unsigned(w);
        Refresh();SetWindowTextW(hint,w==VK_ESCAPE?L"已取消本次录入。":L"录入完成。点击“保存并应用”使 OBS 生效。");return 0;
    case LaughCaptured:
        if(w==1) {
            if(laughCapture.overflow){Refresh();SetWindowTextW(hint,L"组合最多支持 8 个按键，请重新录入。");return 0;}
            editedLaugh=laughCapture.chord;Refresh();SetWindowTextW(hint,L"大笑快捷键已录入，保存后生效。");
        }else if(w==2){Refresh();SetWindowTextW(hint,L"已取消大笑快捷键录入。");}
        else SetWindowTextW(laughButton,(L"录入中："+LaughText(laughCapture.chord)).c_str());
        return 0;
    case WM_COMMAND:
        if(LOWORD(w)>=100 && LOWORD(w)<104){laughCapturing=false;Refresh();capturing=int(LOWORD(w))-100;SetWindowTextW(buttons[capturing],L"请按目标键（Esc 取消）");SetWindowTextW(hint,L"支持字母、空格和左右 Ctrl / Shift / Alt 等功能键。");return 0;}
        if(LOWORD(w)==ResetId){capturing=-1;laughCapturing=false;edited=frog::DefaultObsBindings;Refresh();SetWindowTextW(hint,L"已恢复 DFJK，点击“保存并应用”保存。");return 0;}
        if(LOWORD(w)==LaughId){capturing=-1;Refresh();laughCapture.reset();laughCapturing=true;SetWindowTextW(laughButton,L"按下完整组合，再松开全部按键…");SetWindowTextW(hint,L"支持 F5、Ctrl+Q、Ctrl+Shift+Q 等组合；Esc 取消录入。");return 0;}
        if(LOWORD(w)==LaughResetId){capturing=-1;laughCapturing=false;editedLaugh=frog::KeyChord();Refresh();SetWindowTextW(hint,L"已恢复 F5，点击保存后生效。");return 0;}
        if(LOWORD(w)==SaveId){
            if(laughCapturing){SetWindowTextW(hint,L"请先松开全部按键完成组合录入，或按 Esc 取消。");return 0;}
            capturing=-1;Refresh();
            if(Apply(edited,editedLaugh)) {
                if(SaveLaughShortcut())SetWindowTextW(hint,L"已保存。OBS 自动更新中（约 0.3 秒）；无需保持本工具打开。");
                else MessageBoxW(hwnd,L"敲键已保存，但大笑快捷键写入失败。请检查应用数据目录权限。",L"保存失败",MB_ICONERROR);
            }else MessageBoxW(hwnd,failure.c_str(),L"无法应用键位",MB_OK|MB_ICONERROR);
            return 0;
        }
        if(LOWORD(w)==IDCANCEL){DestroyWindow(hwnd);return 0;}break;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
int Verify(const std::filesystem::path& out) {
    std::filesystem::create_directories(out);
    if(frog::ValidObsBindings({'A','A','S','D'}) || frog::ValidObsBindings({VK_CONTROL,VK_LCONTROL,'A','S'}))return 10;
    unsigned long long pixels=0;
    for(unsigned key=1;key<256;++key) {
        if(frog::KeyName(key).size()>3)return 11;
        auto data=frog::RenderObsKeyLabels({key,key,key,key});if(data.empty())return 12;
        std::array<unsigned,4> slotPixels{};
        for(unsigned y=0;y<880;++y)for(unsigned x=0;x<880;++x)if(data[(y*880+x)*4+3]) {
            if(!frog::InsideKeyLabel(x+.5f,y+.5f))return 13;
            ++pixels;for(unsigned i=0;i<4;++i)if(std::abs(float(x)-frog::ObsKeyX[i])<42 && std::abs(float(y)-frog::ObsKeyY[i])<28)++slotPixels[i];
        }
        if(std::any_of(slotPixels.begin(),slotPixels.end(),[](unsigned n){return n<4;}))return 14;
    }
    auto rgba=frog::RenderObsKeyLabels({VK_LCONTROL,VK_RCONTROL,VK_SPACE,VK_RETURN});
    if(!frog::SaveObsLabels(out/L"function-key-labels.png",rgba))return 15;
    std::array<bool,256> down{};down[VK_RCONTROL]=true;
    if(frog::ObsHeldMask({VK_LCONTROL,VK_RCONTROL,'A',VK_SPACE},down)!=2)return 16;
    down.fill(false);down[VK_RWIN]=true;
    if(!frog::ValidObsBindings({VK_LWIN,VK_RWIN,'A','S'}) || frog::ObsHeldMask({VK_LWIN,VK_RWIN,'A','S'},down)!=2)return 17;
    std::ofstream(out/L"report.json")<<"{\"passed\":true,\"label_checks\":1020,\"alpha_pixels_within_keycap_insets\":"<<pixels<<",\"left_right_modifiers_distinct\":true}\n";
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    std::array<wchar_t,32768> executable{};GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
    folder=std::filesystem::path(executable.data()).parent_path();
    for(int i=1;i+1<count;++i)if(std::wstring(args[i])==L"--folder")folder=std::filesystem::absolute(args[i+1]);
    try {
        if(count>=3 && std::wstring(args[1])==L"--verify"){int result=Verify(std::filesystem::absolute(args[2]));LocalFree(args);return result;}
        if(count>=6 && std::wstring(args[1])==L"--set") {
            frog::ObsBindings keys{};for(int i=0;i<4;++i)keys[i]=unsigned(std::stoul(args[i+2]));
            bool result=Apply(keys,frog::ReadLaughHotkey(frog::LaughConfigPath()));LocalFree(args);return result?0:20;
        }
        frog::ReadObsBindings(folder/L"keybindings.json",edited);
        editedLaugh=frog::ReadLaughHotkey(frog::LaughConfigPath());
        WNDCLASSW type{};type.lpfnWndProc=WindowProc;type.hInstance=instance;type.lpszClassName=L"MilkFrogObsKeySetup";
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
        RegisterClassW(&type);window=CreateWindowExW(0,type.lpszClassName,L"奶蛙 OBS 快捷改键",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,500,415,nullptr,nullptr,instance,nullptr);
        if(!window){LocalFree(args);return 1;}
        hook=SetWindowsHookExW(WH_KEYBOARD_LL,Keyboard,instance,0);
        if(!hook){DestroyWindow(window);LocalFree(args);return 2;}
        ShowWindow(window,show);MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0)if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
        UnhookWindowsHookEx(hook);LocalFree(args);return 0;
    }catch(...){LocalFree(args);MessageBoxW(nullptr,L"操作失败，请检查目录和键位参数。",L"奶蛙 OBS 改键",MB_ICONERROR);return 1;}
}
