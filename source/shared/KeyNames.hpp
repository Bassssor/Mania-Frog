#pragma once
#include <windows.h>
#include <string>
namespace frog {
inline std::wstring KeyName(int key) {
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

}
