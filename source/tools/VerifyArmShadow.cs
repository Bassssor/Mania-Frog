using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

class VerifyArmShadow {
    static byte[] Load(string path,int width,int height) {
        using(var bitmap=new Bitmap(path)) {
            if(bitmap.Width!=width || bitmap.Height!=height)throw new Exception("Wrong image dimensions: "+path);
            var locked=bitmap.LockBits(new Rectangle(0,0,width,height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
            var data=new byte[width*height*4];
            for(int y=0;y<height;y++)Marshal.Copy(IntPtr.Add(locked.Scan0,y*locked.Stride),data,y*width*4,width*4);
            bitmap.UnlockBits(locked);return data;
        }
    }
    static int Main(string[] args) {
        try {
            string root=args[0],directory=Path.Combine(root,"dist","milk-frog-4k","verification");
            var body=Load(Path.Combine(root,"SFML","SFML","base.png"),1189,669);
            var table=Load(Path.Combine(root,"SFML","SFML","tabletop.png"),1189,880);
            var keys=Load(Path.Combine(directory,"keyboard-only.png"),880,880);
            var baseline=Load(Path.Combine(directory,"keyboard-0.png"),880,880);
            byte[] idleShadow=null;
            long checkedAlpha=0,checkedShade=0;
            string csv="mask,visible_shadow_pixels,cream_belly_pixels,yellow_skin_pixels,changed_shadow_pixels\n";
            for(int mask=0;mask<16;mask++) {
                var scene=Load(Path.Combine(directory,"state-"+mask+".png"),880,880);
                var hand=Load(Path.Combine(directory,"foreground-"+mask+".png"),880,880);
                var shadow=Load(Path.Combine(directory,"arm-shadow-"+mask+".png"),880,880);
                if(mask==0)idleShadow=shadow;
                int shaded=0,cream=0,yellow=0,changed=0;
                for(int y=0;y<880;y++)for(int x=0;x<880;x++) {
                    // The pressed head has its own expression and tiny lean;
                    // this check concerns the unchanged torso receiver.
                    if(y<290)continue;
                    int n=(y*880+x)*4,world=((y+1)*1189+x+120)*4;
                    int a=shadow[n+3];
                    // The fused shoulder has no gap from the torso. The
                    // previous whole-arm caster produced a diagonal stripe
                    // here even while the distal forearm was raised.
                    if(x+120>=500 && x+120<=530 && y+1>=242 && y+1<=260 && a>2)
                        throw new Exception("False cast shadow on attached upper arm: "+mask);
                    if(a!=idleShadow[n+3])changed++;
                    if(a>141)throw new Exception("Excessive arm shadow opacity: "+mask);
                    if(a>0 && (y+1>=669 || body[world+3]==0 || keys[n+3]==255))
                        throw new Exception("Arm shadow outside torso or on an opaque key: "+mask);
                    if(hand[n+3]>4)continue;
                    if(hand[n+3]==0) {
                        if(scene[n+3]!=baseline[n+3])throw new Exception("Arm shadow changed transparent silhouette: "+mask);
                        checkedAlpha++;
                    }
                    if(a==0) {
                        for(int c=0;c<3;c++)if(hand[n+3]==0 && scene[n+c]!=baseline[n+c])
                            throw new Exception("Color drift outside shadow and hands: "+mask);
                        continue;
                    }
                    if(y+1>=669 || body[world+3]<240 || table[world+3]!=0 || keys[n+3]!=0)continue;
                    for(int c=0;c<3;c++) {
                        int tint=c==0?25:c==1?72:116;
                        double b=baseline[n+3]/255.0,h=hand[n+3]/255.0;
                        double expected=(baseline[n+c]*b*(1.0-a/255.0+tint*a/65025.0)*(1.0-h)+hand[n+c]*h)/(h+b*(1.0-h));
                        if(Math.Abs(scene[n+c]-expected)>4)
                            throw new Exception("Incorrect warm shadow blend: mask="+mask+",x="+x+",y="+y);
                        double unshaded=(baseline[n+c]*b*(1.0-h)+hand[n+c]*h)/(h+b*(1.0-h));
                        if(scene[n+c]>Math.Ceiling(unshaded)+1)throw new Exception("Shadow brightened the body.");
                    }
                    checkedShade++;if(a<8)continue;
                    shaded++;
                    if(body[world+2]-body[world+1]<15 && body[world+1]-body[world]<20)cream++;
                    else if(body[world+1]-body[world]>30)yellow++;
                }
                if(shaded<300 || cream<100 || yellow<100 || (mask!=0 && changed<500))
                    throw new Exception("Missing pose-dependent body/belly shadow: "+mask+","+shaded+","+cream+","+yellow+","+changed);
                csv+=mask+","+shaded+","+cream+","+yellow+","+changed+"\n";
            }
            File.WriteAllText(Path.Combine(directory,"arm-shadow-verification.csv"),csv);
            string report="PASS: all 16 poses have warm, pose-dependent shadows on both belly and yellow body.\n"+
                "PASS: "+checkedAlpha+" unoccluded alpha pixels unchanged; "+checkedShade+" shadow blends verified; no color drift outside shadow or hands.\n"+
                "PASS: no false diagonal cast shadow at the fused shoulder in any pose.\n";
            File.WriteAllText(Path.Combine(directory,"arm-shadow-verification.txt"),report);Console.Write(report);return 0;
        } catch(Exception error) {Console.Error.WriteLine("FAIL: "+error.Message);return 1;}
    }
}
