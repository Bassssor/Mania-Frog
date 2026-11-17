// Runs against the real window procedure, renderer and layered-window alpha.
int VerifyExit(HWND window,Artwork& artwork,const std::filesystem::path& directory,bool menu) {
    std::filesystem::create_directories(directory);
    size=880;
    Presenter presenter;
    keys=frog::DefaultLaneKeys(frog::LaneCount);
    held=0;
    if(!presenter.Paint(window,artwork) || !presenter.Export(directory/"idle.png"))return 91;
    if(laughReady){
        frog::KeyChord trigger;trigger.count=0;trigger.add(VK_F5);
        laughPlayback.setHotkey(trigger);
        UpdateKey(VK_F5,true);UpdateKey(VK_F5,false);
        if(!laughPlayback.playing)return 90;
    }
    held=(1u<<frog::LaneCount)-1;pendingBursts=held;
    SendMessageW(window,menu?WM_COMMAND:WM_CLOSE,menu?Exit:0,0);
    if(!IsWindow(window) || !exitAnimation.active || held || pendingBursts || laughPlayback.playing)return 92;
    const auto started=exitAnimation.started;
    SendMessageW(window,WM_CLOSE,0,0);
    if(exitAnimation.started!=started)return 93;
    for(int key:keys)UpdateKey(key,true);
    UpdateKey(VK_F5,true);
    SyncKeys();
    if(held || pendingBursts || laughPlayback.playing)return 94;
    SendMessageW(window,TrayMessage,0,WM_LBUTTONDBLCLK);
    SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
    SendMessageW(window,WM_LBUTTONDOWN,0,0);
    if(keyDialog || laughDialog || size!=880)return 100;
    unsigned previous=255;
    std::ofstream report(directory/"opacity.csv");report<<"milliseconds,opacity\n";
    for(unsigned ms:{0u,300u,600u,700u,800u,900u,1000u,1100u,1200u,1300u,1400u}) {
        const auto now=started+ms*1000000ull;
        const auto opacity=exitAnimation.Opacity(now);
        if(opacity>previous || (ms<=600 && opacity!=255) || (ms==1400 && opacity!=0))return 95;
        if(!presenter.Paint(window,artwork,now) || !presenter.Export(directory/("exit-"+std::to_string(ms)+".png")))return 96;
        if(!IsWindow(window))return 97;
        report<<ms<<','<<unsigned(opacity)<<'\n';previous=opacity;
    }
    if(FinishExit(window,started+1399000000ull) || !IsWindow(window))return 98;
    if(!FinishExit(window,started+1400000000ull) || IsWindow(window) || trayAdded)return 99;
    std::ofstream(directory/"report.txt")<<"PASS: deferred close, repeated-close idempotence, input and laughter stopped, resize/drag/dialogs blocked, 600 ms expression hold, 800 ms monotonic fade, transparent final frame, window/tray cleanup.\n";
    return 0;
}
