// A single-file desktop entry point. The final native renderer and
// its exact runtime dependencies are embedded as PE resources, never as a ZIP.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include "payload_manifest.h"

namespace {
std::wstring failureDetails;
bool quietVerification=false;
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h):value(h){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
bool sameFile(const std::filesystem::path& path,const unsigned char *bytes,DWORD length) {
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size{};if(!GetFileSizeEx(file.value,&size)||size.QuadPart!=length)return false;
    std::array<unsigned char,65536> buffer{};DWORD offset=0;
    while(offset<length) {
        DWORD count=std::min<DWORD>(static_cast<DWORD>(buffer.size()),length-offset),read=0;
        if(!ReadFile(file.value,buffer.data(),count,&read,nullptr)||read!=count||memcmp(buffer.data(),bytes+offset,count))return false;
        offset+=count;
    }
    return true;
}
bool extract(HINSTANCE module,const PayloadFile& item,const std::filesystem::path& folder) {
    failureDetails=L"资源文件："+std::wstring(item.name)+L"\n目录："+folder.wstring();
    HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(item.resource),RT_RCDATA);
    if(!resource){failureDetails+=L"\nFindResource";return false;}
    auto bytes=static_cast<const unsigned char*>(LockResource(LoadResource(module,resource)));
    DWORD length=SizeofResource(module,resource);if(!bytes||!length){failureDetails+=L"\nLoadResource";return false;}
    auto destination=folder/item.name;
    // Editable key configuration remains editable between ordinary launches.
    if(item.editable && std::filesystem::is_regular_file(destination))return true;
    if(sameFile(destination,bytes,length))return true;
    auto temporary=destination;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
    bool written=false;
    {
        Handle file(CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(file.value!=INVALID_HANDLE_VALUE) {
            DWORD count=0;written=WriteFile(file.value,bytes,length,&count,nullptr)&&count==length&&FlushFileBuffers(file.value);
        }
    }
    if(!written || !MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH|MOVEFILE_COPY_ALLOWED)) {
        DWORD error=GetLastError();failureDetails+=L"\nWrite/Move: "+std::to_wstring(error);
        DeleteFileW(temporary.c_str());return false;
    }
    return true;
}
int fail(const wchar_t *message) {
    std::wstring text=std::wstring(message)+L"\n"+failureDetails;
    if(quietVerification) {
        std::array<wchar_t,32768> executable{};GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        int length=WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,nullptr,0,nullptr,nullptr);
        std::string bytes(length,'\0');WideCharToMultiByte(CP_UTF8,0,text.c_str(),-1,bytes.data(),length,nullptr,nullptr);
        std::ofstream(std::filesystem::path(executable.data()).parent_path()/"launch-error.txt")<<bytes;
    } else MessageBoxW(nullptr,text.c_str(),L"奶蛙桌宠",MB_OK|MB_ICONERROR);
    return 1;
}
}

int WINAPI wWinMain(HINSTANCE module,HINSTANCE,wchar_t *arguments,int) {
    quietVerification=arguments && (std::wstring(arguments).find(L"--smoke-test")!=std::wstring::npos || std::wstring(arguments).find(L"--verify-")!=std::wstring::npos);
    try {
        PWSTR local=nullptr;
        if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local)))return fail(L"无法访问应用程序数据目录。");
        std::filesystem::path folder=std::filesystem::path(local)/L"MilkFrog"/L"Runtime"/PayloadVersion;
        CoTaskMemFree(local);
        std::filesystem::create_directories(folder);
        std::wstring mutexName=L"Local\\MilkFrogRuntime-"+std::wstring(PayloadVersion);
        Handle mutex(CreateMutexW(nullptr,FALSE,mutexName.c_str()));
        if(!mutex.value)return fail(L"无法准备奶蛙运行环境。");
        DWORD wait=WaitForSingleObject(mutex.value,30000);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED)return fail(L"另一个奶蛙实例正在准备运行环境，请稍后再试。");
        bool ok=true;for(const auto& item:PayloadFiles)if(!extract(module,item,folder)){ok=false;break;}
        ReleaseMutex(mutex.value);
        if(!ok)return fail(L"无法准备奶蛙运行素材。");
        auto executable=folder/L"MilkFrog.exe";
        std::wstring command=L"\""+executable.wstring()+L"\"";
        if(arguments && *arguments){command+=L" ";command+=arguments;}
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,folder.c_str(),&startup,&process))
            return fail(L"无法启动奶蛙桌宠。");
        Handle child(process.hProcess),thread(process.hThread);
        WaitForSingleObject(child.value,INFINITE);DWORD result=1;GetExitCodeProcess(child.value,&result);
        return static_cast<int>(result);
    } catch(...) {return fail(L"准备奶蛙运行环境时发生错误。");}
}
