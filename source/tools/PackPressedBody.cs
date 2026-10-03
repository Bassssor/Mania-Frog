using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Register a newly generated, continuous laughing head/neck/body against the
// approved torso. No face patch, individual feature remapping or head warp.
class PackPressedBody {
    const int Width=1189,Height=669;
    static byte[] Read(Bitmap image) {
        var bits=image.LockBits(new Rectangle(0,0,image.Width,image.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        var pixels=new byte[image.Width*image.Height*4];
        for(int y=0;y<image.Height;y++)Marshal.Copy(IntPtr.Add(bits.Scan0,y*bits.Stride),pixels,y*image.Width*4,image.Width*4);
        image.UnlockBits(bits);return pixels;
    }
    static void Save(string path,byte[] pixels) {
        using(var image=new Bitmap(Width,Height,PixelFormat.Format32bppArgb)) {
            var bits=image.LockBits(new Rectangle(0,0,Width,Height),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
            for(int y=0;y<Height;y++)Marshal.Copy(pixels,y*Width*4,IntPtr.Add(bits.Scan0,y*bits.Stride),Width*4);
            image.UnlockBits(bits);image.Save(path,ImageFormat.Png);
        }
    }
    static double Smooth(double first,double last,double x) {
        double t=Math.Max(0,Math.Min(1,(x-first)/(last-first)));return t*t*(3-2*t);
    }
    static byte Byte(double x){return (byte)Math.Max(0,Math.Min(255,Math.Round(x)));}
    static double Sample(byte[] pixels,int width,int height,double x,double y,int channel) {
        int left=(int)Math.Floor(x),top=(int)Math.Floor(y);double dx=x-left,dy=y-top,value=0;
        for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
            int sx=left+i,sy=top+j;if(sx<0 || sy<0 || sx>=width || sy>=height)continue;
            value+=pixels[(sy*width+sx)*4+channel]*(i==0?1-dx:dx)*(j==0?1-dy:dy);
        }
        return value;
    }
    static int Main(string[] args) {
        using(var original=new Bitmap(args[0]))using(var generated=new Bitmap(args[1])) {
            if(original.Width!=Width || original.Height!=Height || generated.Width!=1254 || generated.Height!=1254)
                throw new Exception("Unexpected registered body dimensions.");
            byte[] idle=Read(original),donor=Read(generated),pressed=(byte[])idle.Clone();
            // The original lower torso and keyboard determine registration:
            // a native canvas resize, then translation (-1,-28), not eye/mouth
            // alignment. The resulting head-top displacement is four pixels.
            const double scale=1254.0/880,shiftX=-1,shiftY=-28;
            var connected=new bool[generated.Width*generated.Height];
            var queue=new Queue<int>();int seed=540*generated.Width+742;queue.Enqueue(seed);connected[seed]=true;
            while(queue.Count>0) {
                int index=queue.Dequeue(),x=index%generated.Width,y=index/generated.Width;
                foreach(int next in new int[]{x>0?index-1:-1,x+1<generated.Width?index+1:-1,y>0?index-generated.Width:-1,y+1<generated.Height?index+generated.Width:-1}) {
                    if(next<0 || connected[next] || donor[next*4+3]<5)continue;
                    connected[next]=true;queue.Enqueue(next);
                }
            }
            for(int n=0;n<connected.Length;n++) {
                if(!connected[n])donor[n*4+3]=0;
                for(int c=0;c<3;c++)donor[n*4+c]=Byte(donor[n*4+c]*donor[n*4+3]/255.0);
            }
            for(int y=0;y<291;y++)for(int x=0;x<Width;x++) {
                int n=(y*Width+x)*4;
                double nativeY=y-1.0,weight=1-Smooth(232,289,nativeY);
                double sx=((x-120.0-shiftX)+.5)*scale-.5,sy=((nativeY-shiftY)+.5)*scale-.5;
                double a=Sample(donor,generated.Width,generated.Height,sx,sy,3),oldAlpha=idle[n+3];
                // Complete regenerated head and neck above the shoulders;
                // original belly/torso/keyboard remain identical below them.
                double alpha=a*weight+oldAlpha*(1-weight);
                pressed[n+3]=Byte(alpha);
                for(int c=0;c<3;c++) {
                    double rgb=Sample(donor,generated.Width,generated.Height,sx,sy,c)*weight+idle[n+c]*oldAlpha/255.0*(1-weight);
                    pressed[n+c]=alpha>0?Byte(rgb*255/alpha):(byte)0;
                }
            }
            Directory.CreateDirectory(Path.GetDirectoryName(args[2]));Save(args[2],pressed);
            Console.WriteLine("Packed whole regenerated head/jaw/neck from the torso-anchored laugh; original torso unchanged below y=290.");
        }
        return 0;
    }
}
