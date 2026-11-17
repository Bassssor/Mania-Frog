// Integration checks for real hook handling, dialog capture, persisted profiles,
// artwork contacts, independent hand combinations, labels and particle release.
int VerifyLaneModes(Artwork& artwork,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    configOverride=directory/"profiles.txt";
    sf::RenderTexture target;if(!target.create(Width,SpriteHeight))return 101;
    sf::RenderTexture scene;if(!scene.create(SceneSize,SceneSize))return 101;
    scene.setView(sf::View(sf::FloatRect(ViewX,1.f,float(SceneSize),float(SceneSize))));
    unsigned combinations=0,contacts=0,releasedContacts=0,labelCases=0;
    std::ofstream report(directory/"checks.txt");
    for(unsigned count=4;count<=frog::MaxLanes;++count) {
        frog::LaneCount=count;DefaultKeys=frog::DefaultLaneKeys(count);keys=DefaultKeys;
        held=0;pendingBursts=0;artwork.effects.Reset();
        const auto pictures=directory/std::to_string(count);
        std::filesystem::create_directories(pictures);
        std::vector<uint64_t> hashes;
        for(unsigned mask=0;mask<(1u<<count);++mask) {
            held=0;pendingBursts=0;
            for(unsigned i=0;i<count;++i)if(mask&(1u<<i)) {
                KBDLLHOOKSTRUCT event{};event.vkCode=keys[i];
                KeyboardHook(HC_ACTION,WM_KEYDOWN,reinterpret_cast<LPARAM>(&event));
            }
            if(held!=mask || pendingBursts!=mask)return 102;
            pendingBursts=0;
            for(unsigned i=0;i<count;++i)if(mask&(1u<<i))UpdateKey(keys[i],true);
            if(pendingBursts)return 103;
            artwork.DrawForeground(target,mask);target.display();
            const auto image=target.getTexture().copyToImage();
            uint64_t hash=1469598103934665603ull;
            const auto *rgba=image.getPixelsPtr();
            for(unsigned i=0;i<Width*SpriteHeight*4;++i){hash^=rgba[i];hash*=1099511628211ull;}
            if(std::find(hashes.begin(),hashes.end(),hash)!=hashes.end())return 104;
            hashes.push_back(hash);
            for(unsigned i=0;i<count;++i) {
                const auto p=ParticleEffects::Anchor(mask,i);unsigned dark=0;
                for(int y=int(p.y)-12;y<=int(p.y)+6;++y)for(int x=int(p.x)-12;x<=int(p.x)+12;++x) {
                    const auto c=image.getPixel(x,y);
                    if(c.a>100 && c.r<170 && c.g<160 && c.b<150)++dark;
                }
                if(mask&(1u<<i)) {
                    if(dark<8){report<<"missing_contact "<<count<<' '<<mask<<' '<<i<<' '<<dark<<'\n';return 105;}
                    ++contacts;
                } else {
                    if(dark>8){report<<"released_contact "<<count<<' '<<mask<<' '<<i<<' '<<dark<<'\n';return 119;}
                    ++releasedContacts;
                }
            }
            artwork.effects.Reset();artwork.effects.Advance(1.f/120.f,mask);
            for(unsigned i=0;i<count;++i)if(mask&(1u<<i))if(artwork.effects.Count(i)<ParticleEffects::BurstCount)return 106;
            for(unsigned i=0;i<count;++i)UpdateKey(keys[i],false);
            if(held)return 107;
            for(unsigned i=0;i<52;++i)artwork.effects.Advance(1.f/120.f,0);
            if(artwork.effects.Active())return 108;
            if(mask==0 || (mask&(mask-1))==0 || mask==((1u<<count)-1)) {
                artwork.Draw(scene,mask,true,false);scene.display();
                if(!StraightImage(scene.getTexture()).saveToFile((pictures/("state-"+std::to_string(mask)+".png")).string()))return 120;
            }
            ++combinations;
        }
        for(int code=1;code<256;++code) {
            target.clear(sf::Color::Transparent);
            for(unsigned slot=0;slot<count;++slot)artwork.DrawKeyLabel(target,code,slot);
            target.display();const auto image=target.getTexture().copyToImage();
            const auto *pixels=image.getPixelsPtr();
            for(unsigned y=0;y<SpriteHeight;++y)for(unsigned x=0;x<Width;++x)if(pixels[(y*Width+x)*4+3]) {
                bool inside=false;
                for(unsigned slot=0;slot<count;++slot) {
                    inside|=frog::InsideLaneLabel(x+.5f,y+.5f,slot);
                }
                if(!inside){report<<"label_outside "<<count<<' '<<code<<' '<<x<<' '<<y<<'\n';return 109;}
            }
            labelCases+=count;
        }
        EditKeys(nullptr,false);if(!keyDialog)return 110;
        for(unsigned i=0;i<count;++i) {
            SendMessageW(keyDialog,WM_COMMAND,201+i,0);
            KBDLLHOOKSTRUCT event{};event.vkCode='A'+i;
            KeyboardHook(HC_ACTION,WM_KEYDOWN,reinterpret_cast<LPARAM>(&event));
            MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
            KeyboardHook(HC_ACTION,WM_KEYUP,reinterpret_cast<LPARAM>(&event));
            if(editedKeys[i]!=int('A'+i))return 111;
        }
        SendMessageW(keyDialog,WM_COMMAND,IDOK,0);if(keyDialog)return 112;
        LoadConfig();if(frog::LaneCount!=count || keys.size()!=count)return 113;
        for(unsigned i=0;i<count;++i)if(keys[i]!=int('A'+i))return 114;
        EditKeys(nullptr,false);
        SendMessageW(keyDialog,CapturedKeyMessage,'A',count-1);
        SendMessageW(keyDialog,WM_COMMAND,IDOK,0);if(!keyDialog)return 115;
        SendMessageW(keyDialog,WM_COMMAND,IDCANCEL,0);
        if(keys.back()!=int('A'+count-1))return 116;
        const auto saved=keys;
        EditKeys(nullptr,false);SendMessageW(keyDialog,WM_COMMAND,210,0);SendMessageW(keyDialog,WM_COMMAND,IDCANCEL,0);
        if(keys!=saved)return 117;
        report<<count<<"K: "<<(1u<<count)<<" distinct combinations, contacts, release, key capture, save/reload, duplicate rejection and cancel passed.\n";
    }
    // Saving one profile must preserve all other modes, including the last lane.
    for(unsigned count=4;count<=frog::MaxLanes;++count)for(unsigned i=0;i<count;++i)
        if(profiles[count-4][i]!=int('A'+i))return 118;
    std::ofstream(directory/"report.json")<<"{\"passed\":true,\"combinations\":"<<combinations
        <<",\"finger_contacts\":"<<contacts<<",\"released_contacts\":"<<releasedContacts<<",\"label_checks\":"<<labelCases
        <<",\"per_mode_profiles\":true,\"capture_save_cancel_duplicates\":true,\"independent_release\":true}\n";
    configOverride.clear();LoadConfig();return 0;
}
