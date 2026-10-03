using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Register generated supporting art; original body, keycaps and hands remain
// in their existing textures. The corner donor only sits behind the old cut.
class ImportDeskAssets {
    static Bitmap Clean(string path) {
        using(var source=new Bitmap(path)) {
            var image=new Bitmap(source.Width,source.Height,PixelFormat.Format32bppArgb);
            using(var g=Graphics.FromImage(image)) { g.CompositingMode=CompositingMode.SourceCopy;g.DrawImageUnscaled(source,0,0); }
            int width=image.Width,height=image.Height,count=width*height;
            var data=image.LockBits(new Rectangle(0,0,width,height),ImageLockMode.ReadWrite,PixelFormat.Format32bppArgb);
            byte[] pixels=new byte[count*4];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);
            bool[] visited=new bool[count];int[] queue=new int[count];int[] largest=new int[0];
            for(int start=0;start<count;start++) {
                if(visited[start] || pixels[start*4+3]<32)continue;
                int head=0,tail=1;queue[0]=start;visited[start]=true;
                while(head<tail) {
                    int n=queue[head++],x=n%width,y=n/width;
                    foreach(int next in new int[]{x>0?n-1:-1,x+1<width?n+1:-1,y>0?n-width:-1,y+1<height?n+width:-1})
                        if(next>=0 && !visited[next] && pixels[next*4+3]>=32){visited[next]=true;queue[tail++]=next;}
                }
                if(tail>largest.Length){largest=new int[tail];Array.Copy(queue,largest,tail);}
            }
            bool[] keep=new bool[count];
            foreach(int n in largest) {
                int x=n%width,y=n/width;
                for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++) {
                    int nx=x+dx,ny=y+dy;if(nx>=0 && nx<width && ny>=0 && ny<height)keep[ny*width+nx]=true;
                }
            }
            for(int n=0;n<count;n++) {
                if(!keep[n])for(int c=0;c<4;c++)pixels[n*4+c]=0;
                else if(pixels[n*4+3]>=245)pixels[n*4+3]=255;
            }
            Marshal.Copy(pixels,0,data.Scan0,pixels.Length);image.UnlockBits(data);return image;
        }
    }
    static Rectangle Bounds(Bitmap image) {
        int left=image.Width,top=image.Height,right=0,bottom=0;
        for(int y=0;y<image.Height;y++)for(int x=0;x<image.Width;x++)if(image.GetPixel(x,y).A>=128) {
            left=Math.Min(left,x);right=Math.Max(right,x);top=Math.Min(top,y);bottom=Math.Max(bottom,y);
        }
        return Rectangle.FromLTRB(Math.Max(0,left-3),Math.Max(0,top-3),Math.Min(image.Width,right+4),Math.Min(image.Height,bottom+4));
    }
    static void Configure(Graphics g) {
        g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.CompositingQuality=CompositingQuality.HighQuality;
    }
    static int Main(string[] args) {
        string repository=args[0],art=Path.Combine(repository,"assets","milk-frog");
        using(var source=Clean(Path.Combine(art,"keyboard-corner-source.png")))
        using(var registered=new Bitmap(1189,669,PixelFormat.Format32bppPArgb))
        using(var output=new Bitmap(1189,669,PixelFormat.Format32bppArgb)) {
            Rectangle bounds=Bounds(source);
            int bottom=bounds.Bottom-4;long sum=0;int samples=0;
            for(int y=bottom-2;y<=bottom;y++)for(int x=bounds.Left;x<bounds.Right;x++)if(source.GetPixel(x,y).A>=128){sum+=x;samples++;}
            float scale=460f/(bounds.Width-6),tipX=sum/(float)samples;
            float left=725f-tipX*scale,top=653f-bottom*scale;
            using(var g=Graphics.FromImage(registered)) { Configure(g);g.DrawImage(source,new RectangleF(left,top,source.Width*scale,source.Height*scale)); }
            // Keep only the missing clipped triangle. The donor's whole
            // chassis has slightly different proportions; side-face pixels
            // outside the two existing lower edges would become extra tabs.
            for(int y=647;y<657;y++)for(int x=690;x<742;x++) {
                Color c=registered.GetPixel(x,y);
                double boundary=Math.Min(.2*x+507,1267.5-.85*x);
                double coverage=Math.Max(0,Math.Min(1,boundary-y+.5));
                if(c.A>0 && coverage>0)output.SetPixel(x,y,Color.FromArgb((int)Math.Round(c.A*coverage),c.R,c.G,c.B));
            }
            output.Save(Path.Combine(art,"sprites","keyboard-corner.png"),ImageFormat.Png);
            Console.WriteLine("Registered generated keyboard corner: scale="+scale+", tip=(725,653)");
        }
        foreach(string name in new string[]{"keyboard-corner.png"})
            File.Copy(Path.Combine(art,"sprites",name),Path.Combine(repository,"SFML","SFML",name),true);
        return 0;
    }
}
