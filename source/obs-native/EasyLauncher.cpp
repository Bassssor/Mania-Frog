// Keep the double-click entry point free of console windows.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    auto script=std::filesystem::path(path).parent_path()/L"obs-input-overlay"/L"InstallObsEasy.ps1";
    std::wstring args=L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \""+script.wstring()+L"\"";
    SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpFile=L"powershell.exe";info.lpParameters=args.c_str();info.nShow=SW_HIDE;
    if(!ShellExecuteExW(&info)){MessageBoxW(nullptr,L"无法启动安装工具。请保留完整 obs-input-overlay 文件夹。",L"奶蛙 OBS 安装",MB_OK|MB_ICONERROR);return 1;}
    CloseHandle(info.hProcess);return 0;
}
