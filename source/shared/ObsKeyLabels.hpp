#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <cmath>
#include "KeyNames.hpp"
#include "ObsKeybindings.hpp"

namespace frog {
struct GdiSession {
    ULONG_PTR token=0;
    GdiSession(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}
    ~GdiSession(){if(token)Gdiplus::GdiplusShutdown(token);}
};
constexpr float ObsKeyX[4]={574,474,378,288},ObsKeyY[4]={542,525,509,494};
inline bool InsideKeyLabel(float x,float y) {
    for(unsigned i=0;i<4;++i) {
        float dx=x-ObsKeyX[i],dy=y-ObsKeyY[i];
        float v=(dy-.18f*dx)/1.1206f,u=dx+.67f*v;
        if(std::abs(u)<=34 && std::abs(v)<=18)return true;
    }
    return false;
}
// Produce one transparent legend layer, independent of hands, torso and desk.
// Four-times rasterization and the keycap inset keep small legends smooth.
inline std::vector<unsigned char> RenderObsKeyLabels(const ObsBindings& keys) {
    using namespace Gdiplus;
    GdiSession session;if(!session.token)return {};
    Bitmap layer(880,880,PixelFormat32bppARGB);Graphics output(&layer);
    output.Clear(Color(0,0,0,0));output.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    output.SetPixelOffsetMode(PixelOffsetModeHalf);
    FontFamily family(L"Segoe UI");FontFamily fallback(L"Arial");
    Font font(family.GetLastStatus()==Ok?&family:&fallback,176,FontStyleBold,UnitPixel);
    SolidBrush ink(Color(255,45,51,49));StringFormat format(StringFormat::GenericTypographic());
    format.SetFormatFlags(format.GetFormatFlags()|StringFormatFlagsNoClip);
    for(unsigned slot=0;slot<4;++slot) {
        Bitmap glyph(800,320,PixelFormat32bppARGB);Graphics painter(&glyph);
        painter.Clear(Color(0,0,0,0));painter.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        auto name=KeyName(int(keys[slot]));painter.DrawString(name.c_str(),int(name.size()),&font,PointF(30,20),&format,&ink);
        int left=800,right=-1,top=320,bottom=-1;
        painter.Flush(FlushIntentionSync);
        BitmapData glyphData{};Rect glyphRect(0,0,800,320);
        if(glyph.LockBits(&glyphRect,ImageLockModeRead,PixelFormat32bppARGB,&glyphData)!=Ok)return {};
        for(int y=0;y<320;++y)for(int x=0;x<800;++x){
            const auto *pixel=static_cast<unsigned char*>(glyphData.Scan0)+y*glyphData.Stride+x*4;
            if(pixel[3]){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
        }
        glyph.UnlockBits(&glyphData);
        if(right<left)return {};
        const float width=float(right-left+1)/4,height=float(bottom-top+1)/4;
        const float w=std::min(52.f,width),h=height>40?26:height*.65f;
        auto state=output.Save();output.TranslateTransform(ObsKeyX[slot],ObsKeyY[slot]);output.RotateTransform(191);
        output.DrawImage(&glyph,RectF(-w/2,-h/2,w,h),float(left),float(top),float(right-left+1),float(bottom-top+1),UnitPixel);
        output.Restore(state);
    }
    // OBS expects premultiplied RGBA, whereas GDI+ exposes premultiplied BGRA.
    std::vector<unsigned char> result(880*880*4);BitmapData data{};Rect rect(0,0,880,880);
    if(layer.LockBits(&rect,ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Ok)return {};
    for(int y=0;y<880;++y)for(int x=0;x<880;++x) {
        const auto *src=static_cast<unsigned char*>(data.Scan0)+y*data.Stride+x*4;
        auto *dst=result.data()+(y*880+x)*4;
        if(InsideKeyLabel(x+.5f,y+.5f)){dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=src[3];}
    }
    layer.UnlockBits(&data);return result;
}
inline bool SaveObsLabels(const std::filesystem::path& path,const std::vector<unsigned char>& rgba) {
    using namespace Gdiplus;
    if(rgba.size()!=880*880*4)return false;
    GdiSession session;if(!session.token)return false;
    std::vector<unsigned char> bgra(rgba.size());
    for(size_t i=0;i<rgba.size();i+=4){bgra[i]=rgba[i+2];bgra[i+1]=rgba[i+1];bgra[i+2]=rgba[i];bgra[i+3]=rgba[i+3];}
    Bitmap image(880,880,880*4,PixelFormat32bppPARGB,bgra.data());
    CLSID png{};if(CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png)!=S_OK)return false;
    return image.Save(path.c_str(),&png,nullptr)==Ok;
}
}
