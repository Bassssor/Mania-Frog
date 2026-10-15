// Re-encode the existing indexed WIC frames without changing size or timing.
// Windows' built-in JPEG XR codec supports lossless RGB and alpha; no runtime
// dependency or new frame-container format is required.
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("WIC failure: "+std::to_string(uint32_t(hr)));}
struct Frame {uint64_t offset=0;uint32_t size=0;};
ComPtr<IWICBitmapSource> source(IWICImagingFactory* factory,std::vector<BYTE>& bytes) {
    ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));
    check(stream->InitializeFromMemory(bytes.data(),DWORD(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));
    ComPtr<IWICFormatConverter> converted;check(factory->CreateFormatConverter(&converted));
    check(converted->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    // Materialize before the memory-backed stream's input bytes are reused.
    ComPtr<IWICBitmap> bitmap;check(factory->CreateBitmapFromSource(converted.Get(),WICBitmapCacheOnLoad,&bitmap));
    return bitmap;
}
std::vector<BYTE> encode(IWICImagingFactory* factory,IWICBitmapSource* bitmap,float quality) {
    ComPtr<IStream> stream;check(CreateStreamOnHGlobal(nullptr,TRUE,&stream));
    ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatWmp,nullptr,&encoder));
    check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> props;check(encoder->CreateNewFrame(&frame,&props));
    PROPBAG2 names[6]{};VARIANT values[6]{};
    const wchar_t* labels[]={L"Lossless",L"ImageQuality",L"InterleavedAlpha",L"AlphaQuality",L"HorizontalTileSlices",L"VerticalTileSlices"};
    for(int i=0;i<6;++i)names[i].pstrName=const_cast<wchar_t*>(labels[i]);
    values[0].vt=VT_BOOL;values[0].boolVal=quality==1?VARIANT_TRUE:VARIANT_FALSE;
    values[1].vt=VT_R4;values[1].fltVal=quality;
    values[2].vt=VT_BOOL;values[2].boolVal=VARIANT_FALSE;
    values[3].vt=VT_UI1;values[3].bVal=1; // Always lossless alpha, even in optional lossy RGB mode.
    values[4].vt=values[5].vt=VT_UI2;values[4].uiVal=values[5].uiVal=0;
    check(props->Write(6,names,values));check(frame->Initialize(props.Get()));
    UINT width=0,height=0;check(bitmap->GetSize(&width,&height));check(frame->SetSize(width,height));
    auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format));
    if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("Encoder changed pixel format");
    check(frame->WriteSource(bitmap,nullptr));check(frame->Commit());check(encoder->Commit());
    STATSTG stat{};check(stream->Stat(&stat,STATFLAG_NONAME));
    if(stat.cbSize.QuadPart>8*1024*1024)throw std::runtime_error("Unexpected frame size");
    std::vector<BYTE> bytes(size_t(stat.cbSize.QuadPart));LARGE_INTEGER zero{};check(stream->Seek(zero,STREAM_SEEK_SET,nullptr));
    ULONG read=0;check(stream->Read(bytes.data(),ULONG(bytes.size()),&read));if(read!=bytes.size())throw std::runtime_error("Incomplete encoded frame");
    return bytes;
}
int wmain(int argc,wchar_t** argv) {
    if(argc<3)return 2;
    try {
        check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
        ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        const float quality=argc>3?std::stof(argv[3]):1.f;
        if(!(quality>=.90f && quality<=1.f))throw std::runtime_error("Quality must be in [0.90, 1.0]");
        std::error_code pathError;
        if(std::filesystem::equivalent(std::filesystem::path(argv[1]),std::filesystem::path(argv[2]),pathError))
            throw std::runtime_error("Input and output must be different files");
        const bool sample=argc>4 && std::wstring(argv[4])==L"--sample";
        std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);
        std::array<char,24> header{};input.read(header.data(),header.size());
        if(!input || std::string(header.data(),8)!="MFLAUGH1")throw std::runtime_error("Invalid frame pack");
        std::array<uint32_t,4> shape{};memcpy(shape.data(),header.data()+8,16);
        if(shape!=std::array<uint32_t,4>{720,720,24,216})throw std::runtime_error("Unexpected frame dimensions or timing");
        std::vector<Frame> frames(216),encoded(216);
        for(auto& frame:frames){input.read(reinterpret_cast<char*>(&frame.offset),8);input.read(reinterpret_cast<char*>(&frame.size),4);}
        if(!input)throw std::runtime_error("Incomplete frame index");
        input.seekg(0,std::ios::end);const auto inputBytes=uint64_t(input.tellg());
        for(const auto& frame:frames)if(frame.offset<24+216*12 || !frame.size || frame.size>8*1024*1024 || frame.offset>inputBytes || frame.size>inputBytes-frame.offset)
            throw std::runtime_error("Frame index exceeds file bounds");
        std::ofstream output(std::filesystem::path(argv[2]),std::ios::binary|std::ios::trunc);
        output.write(header.data(),header.size());std::array<char,216*12> blank{};output.write(blank.data(),blank.size());
        uint64_t sourceBytes=0,outputBytes=0,colorSquared=0,visibleChannels=0;unsigned maxAlpha=0,maxRgb=0;std::vector<double> timings;
        std::vector<BYTE> before(720*720*4),after(before.size());
        for(unsigned index=0;index<216;++index) {
            if(sample && index!=0 && index!=54 && index!=108 && index!=162 && index!=215)continue;
            auto frame=frames[index];if(!frame.size || frame.size>8*1024*1024)throw std::runtime_error("Invalid input entry");
            std::vector<BYTE> bytes(frame.size);input.seekg(frame.offset);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
            if(!input)throw std::runtime_error("Input read failed");
            auto bitmap=source(factory.Get(),bytes);check(bitmap->CopyPixels(nullptr,720*4,UINT(before.size()),before.data()));
            auto compressed=encode(factory.Get(),bitmap.Get(),quality);
            const auto start=std::chrono::steady_clock::now();auto decoded=source(factory.Get(),compressed);
            check(decoded->CopyPixels(nullptr,720*4,UINT(after.size()),after.data()));
            timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            for(size_t i=0;i<before.size();i+=4) {
                maxAlpha=std::max(maxAlpha,unsigned(std::abs(int(before[i+3])-int(after[i+3]))));
                if(before[i+3])for(int c=0;c<3;++c){int error=(int(before[i+c])-int(after[i+c]))*before[i+3]/255;maxRgb=std::max(maxRgb,unsigned(std::abs(error)));colorSquared+=error*error;++visibleChannels;}
            }
            if(maxAlpha || (quality==1 && before!=after))throw std::runtime_error("Lossless roundtrip mismatch");
            encoded[index]={uint64_t(output.tellp()),uint32_t(compressed.size())};output.write(reinterpret_cast<char*>(compressed.data()),compressed.size());
            sourceBytes+=bytes.size();outputBytes+=compressed.size();
            if(sample || index%36==0)std::cout<<"frame="<<index<<" original="<<bytes.size()<<" encoded="<<compressed.size()<<'\n'<<std::flush;
        }
        output.seekp(24);for(const auto& frame:encoded){output.write(reinterpret_cast<const char*>(&frame.offset),8);output.write(reinterpret_cast<const char*>(&frame.size),4);}
        output.flush();if(!output)throw std::runtime_error("Output write failed");
        std::sort(timings.begin(),timings.end());
        const double mse=visibleChannels?double(colorSquared)/visibleChannels:0;
        std::cout<<"{\"quality\":"<<quality<<",\"source_bytes\":"<<sourceBytes<<",\"encoded_bytes\":"<<outputBytes
            <<",\"max_alpha_error\":"<<maxAlpha<<",\"max_premultiplied_RGB_error\":"<<maxRgb<<",\"MSE\":"<<mse
            <<",\"decode_p95_ms\":"<<timings[size_t(timings.size()*.95)]<<",\"lossless\":"<<(quality==1?"true":"false")<<"}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
