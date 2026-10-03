using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

// Packages the imagegen RGBA atlas into Mania-Cat's nine registered layers.
// It does not redraw the character. Disconnected alpha debris is discarded.
class PackAssets
{
    const int CanvasWidth = 1189, CanvasHeight = 669;
    const int CellSize = 627, HandSplitX = 285;
    static readonly PointF[] KeyboardOutline = {
        new PointF(46,493), new PointF(113,448), new PointF(179,456),
        new PointF(197,463), new PointF(269,476), new PointF(292,479),
        new PointF(369,493), new PointF(391,496), new PointF(470,508),
        new PointF(500,520), new PointF(505,590), new PointF(453,627),
        new PointF(46,553)
    };

    static readonly bool[] KeyboardArea = BuildKeyboardArea();
    static readonly bool[] KeyboardPixels = BuildKeyboardArea(0);

    static bool InKeyboardOutline(int x, int y)
    {
        bool inside = false;
        for (int i = 0, j = KeyboardOutline.Length - 1; i < KeyboardOutline.Length; j = i++)
        {
            PointF a = KeyboardOutline[i], b = KeyboardOutline[j];
            if ((a.Y > y + 0.5f) != (b.Y > y + 0.5f) &&
                x + 0.5f < (b.X - a.X) * (y + 0.5f - a.Y) / (b.Y - a.Y) + a.X) inside = !inside;
        }
        return inside;
    }

    static bool[] BuildKeyboardArea(int dilation=12)
    {
        var area = new bool[CellSize * CellSize];
        // Include the union of the generated key positions in the extraction
        // mask. This removes displaced key edges and baked cyan light strips
        // from the hand sprites; the idle keyboard supplies the whole region.
        for (int y = 435; y < CellSize; y++)
            for (int x = 34; x < 520; x++)
                if (InKeyboardOutline(x,y))
                    for (int dy = -dilation; dy <= dilation; dy++)
                        for (int dx = -dilation; dx <= dilation; dx++)
                        {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && ny >= 0 && nx < CellSize && ny < CellSize) area[ny * CellSize + nx] = true;
                        }
        return area;
    }

    static bool InKeyboard(int x, int y) { return KeyboardArea[y * CellSize + x]; }

    static double EdgeAt(Bitmap image,int y,bool right)
    {
        int edge=-1;
        for(int x=70;x<590;x++) if(image.GetPixel(x,y).A>=128)
        {edge=x;if(!right)break;}
        if(edge<0) throw new Exception("Missing torso outline.");
        int adjacent=edge+(right?1:-1);
        double a=image.GetPixel(edge,y).A,b=image.GetPixel(adjacent,y).A;
        return edge+(right?1:-1)*(a-127.5)/Math.Max(1.0,a-b);
    }
    static double Hermite(double first,double last,double startSlope,double endSlope,double t,double length)
    {
        double t2=t*t,t3=t2*t;
        return (2*t3-3*t2+1)*first+(t3-2*t2+t)*length*startSlope+
            (-2*t3+3*t2)*last+(t3-t2)*length*endSlope;
    }
    static void FitTorsoContours(Bitmap image)
    {
        // A blur preserves a bad outline. Register the two shoulder donors
        // to one continuous curve instead, with explicit subpixel coverage.
        const int first=195,last=265;
        using(var source=(Bitmap)image.Clone())
        {
            double l0=EdgeAt(source,first,false),l1=EdgeAt(source,last,false);
            double r0=EdgeAt(source,first,true),r1=EdgeAt(source,last,true);
            double ls=(l0-EdgeAt(source,first-5,false))/5.0;
            double le=(EdgeAt(source,last+5,false)-l1)/5.0;
            double rs=(r0-EdgeAt(source,first-5,true))/5.0;
            double re=(EdgeAt(source,last+5,true)-r1)/5.0;
            for(int y=first;y<=last;y++)
            {
                double t=(y-first)/(double)(last-first);
                double left=Hermite(l0,l1,ls,le,t,last-first);
                double right=Hermite(r0,r1,rs,re,t,last-first);
                int oldLeft=(int)Math.Ceiling(EdgeAt(source,y,false));
                int oldRight=(int)Math.Floor(EdgeAt(source,y,true));
                Color lc=source.GetPixel(oldLeft+4,y),rc=source.GetPixel(oldRight-4,y);
                for(int x=70;x<590;x++)
                {
                    if(x>left+4 && x<right-4) continue;
                    double coverage=Math.Max(0.0,Math.Min(1.0,Math.Min(x-left+.5,right-x+.5)));
                    Color reference=x<(left+right)/2?lc:rc;
                    Color c=source.GetPixel(x,y);
                    if(c.A<180) c=reference;
                    image.SetPixel(x,y,coverage==0?Color.Transparent:
                        Color.FromArgb((int)Math.Round(reference.A*coverage),c.R,c.G,c.B));
                }
            }
            // Follow the exposed cream belly down to the keyboard. This
            // removes the protruding rectangular piece rather than softening it.
            double bellyStart=EdgeAt(source,310,false);
            int[] firstKeyRear=new int[170];
            // The coarse keyboard polygon extends into the belly where the
            // first cap meets it. Locate the actual pale cap in each column;
            // keep its one-pixel shaded rear edge, not the adjacent torso tab.
            for(int x=145;x<firstKeyRear.Length;x++)
            {
                firstKeyRear[x]=CellSize;
                for(int y=440;y<490;y++)
                {
                    Color c=source.GetPixel(x,y);
                    if(c.A>=128 && c.B>=205 && c.R-c.G<=8 && c.G-c.B<=8)
                    {firstKeyRear[x]=y-1;break;}
                }
                if(firstKeyRear[x]==CellSize) throw new Exception("Missing first-key rear surface.");
            }
            for(int y=310;y<=460;y++)
            {
                double left=y<355
                    ? Hermite(bellyStart,180.5,-.55,-.53,(y-310)/45.0,45)
                    : y<=440 ? Hermite(180.5,156.5,-.53,0,(y-355)/85.0,85)
                    : 156.5+.007*(y-440)*(y-440);
                // A small smooth overlap behind the raised far forearm closes
                // its remaining matte gap. Taper to the unchanged belly below.
                if(y<375)left-=3.5*Math.Pow(Math.Sin(Math.PI*(y-310)/65.0),2);
                Color reference=source.GetPixel((int)Math.Ceiling(left)+5,y);
                for(int x=70;x<left+8;x++)
                {
                    bool keyIntersection=x>=145 && x<firstKeyRear.Length && y>=440;
                    if(KeyboardPixels[y*CellSize+x] && (!keyIntersection || y>=firstKeyRear[x])) continue;
                    double coverage=Math.Max(0.0,Math.Min(1.0,x-left+.5));
                    Color c=source.GetPixel(x,y);
                    if(c.A<180)c=reference;
                    image.SetPixel(x,y,coverage==0?Color.Transparent:
                        Color.FromArgb((int)Math.Round(reference.A*coverage),c.R,c.G,c.B));
                }
            }
        }
    }

    static void SmoothEdgeBand(Bitmap image,int offsetX,int offsetY,int top,int bottom)
    {
        // Filter only the exterior contour around an atlas join. Colors are
        // averaged in premultiplied alpha so transparent pixels cannot create
        // dark/white fringes. The interior texture is left untouched.
        int[] weights={1,4,6,4,1};
        using(var source=(Bitmap)image.Clone())
        {
            for(int y=top;y<bottom;y++)
            {
                int left=CellSize,right=-1;
                for(int x=70;x<590;x++) if(source.GetPixel(offsetX+x,offsetY+y).A>64)
                {left=Math.Min(left,x);right=x;}
                if(right<left) continue;
                for(int x=Math.Max(70,left-3);x<=Math.Min(589,right+3);x++)
                {
                    if(x>left+3 && x<right-3) continue;
                    long alpha=0,red=0,green=0,blue=0;
                    for(int dy=-2;dy<=2;dy++) for(int dx=-2;dx<=2;dx++)
                    {
                        Color c=source.GetPixel(offsetX+x+dx,offsetY+y+dy);
                        int w=weights[dx+2]*weights[dy+2];
                        long a=(long)c.A*w;
                        alpha+=a;red+=c.R*a;green+=c.G*a;blue+=c.B*a;
                    }
                    int opacity=(int)((alpha+128)/256);
                    image.SetPixel(offsetX+x,offsetY+y,opacity<2?Color.Transparent:
                        Color.FromArgb(opacity,(int)(red/alpha),(int)(green/alpha),(int)(blue/alpha)));
                }
            }
        }
    }

    static void FitRaisedShoulder(Bitmap output,Bitmap body,Bitmap frame)
    {
        // The extracted shoulder and neck originally meet in two abrupt
        // steps. Extend only their missing attachment pixels to a continuous
        // outer curve; never crop the round hand to repair its yellow shoulder.
        const int first=225,last=248;
        double start=EdgeAt(body,first,false),end=EdgeAt(frame,last,false);
        double startSlope=(start-EdgeAt(body,first-5,false))/5.0;
        double endSlope=(EdgeAt(frame,last+5,false)-end)/5.0;
        for(int y=first;y<=last;y++)
        {
            double left=Hermite(start,end,startSlope,endSlope,(y-first)/(double)(last-first),last-first);
            int existing=-1;
            for(int x=70;x<HandSplitX;x++)
            {
                if(body.GetPixel(x,y).A>=128 || output.GetPixel(281+x,21+y).A>=128){existing=x;break;}
            }
            if(existing<0) continue;
            Color reference=output.GetPixel(281+existing+5,21+y);
            if(reference.A<180)reference=body.GetPixel(existing+5,y);
            int bodyEdge=(int)Math.Ceiling(EdgeAt(body,y,false));
            for(int x=(int)Math.Floor(left);x<Math.Min(HandSplitX,bodyEdge+2);x++)
            {
                if(body.GetPixel(x,y).A>=180)continue;
                Color c=output.GetPixel(281+x,21+y);
                if(c.A>=180)continue;
                double coverage=Math.Max(0.0,Math.Min(1.0,x-left+.5));
                Color original=frame.GetPixel(x,y);
                Color color=original.A>=180?original:reference;
                int alpha=(int)Math.Round(color.A*coverage);
                if(alpha>c.A)output.SetPixel(281+x,21+y,Color.FromArgb(alpha,color.R,color.G,color.B));
            }
        }
    }

    static void SmoothIdleInnerEdge(Bitmap output)
    {
        // Only the raised forearm's inner matte is filtered. Premultiplied
        // color keeps yellow highlights smooth without importing black from
        // transparent pixels or blurring the cream belly behind the arm.
        int[] weights={1,4,6,4,1};
        using(var original=(Bitmap)output.Clone())
        {
            for(int y=300;y<366;y++)
            {
                int right=-1;
                for(int x=120;x<HandSplitX;x++)if(original.GetPixel(281+x,21+y).A>=64)right=x;
                if(right<0)continue;
                double strength=Math.Min(1.0,Math.Min((y-300)/10.0,(366-y)/10.0));
                for(int x=right-3;x<=right+3;x++)
                {
                    double alpha=0,red=0,green=0,blue=0;
                    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)
                    {
                        Color c=original.GetPixel(281+x+dx,21+y+dy);
                        double a=c.A*weights[dx+2]*weights[dy+2]/256.0;
                        alpha+=a;red+=c.R*a;green+=c.G*a;blue+=c.B*a;
                    }
                    Color old=original.GetPixel(281+x,21+y);
                    alpha=old.A*(1-strength)+alpha*strength;
                    red=old.R*old.A*(1-strength)+red*strength;
                    green=old.G*old.A*(1-strength)+green*strength;
                    blue=old.B*old.A*(1-strength)+blue*strength;
                    output.SetPixel(281+x,21+y,alpha<2?Color.Transparent:
                        Color.FromArgb((int)Math.Round(alpha),(int)Math.Round(red/alpha),
                            (int)Math.Round(green/alpha),(int)Math.Round(blue/alpha)));
                }
            }
        }
    }

    static void SaveArmOnly(Bitmap frame,Bitmap body,Bitmap mask,int frameIndex,bool dfHand,string filename)
    {
        using(var output=new Bitmap(CanvasWidth,CanvasHeight,PixelFormat.Format32bppArgb))
        {
            for(int y=220;y<540;y++) for(int x=70;x<590;x++)
            {
                if(dfHand!=(x>=HandSplitX)) continue;
                if(InKeyboard(x,y) && !InContact(x,y,frameIndex)) continue;
                Color c=frame.GetPixel(x,y);
                if(c.A<8) continue;
                if(InKeyboard(x,y) && c.R>205 && c.G>190 && c.B>180) continue;
                Color label=mask.GetPixel(x,y);
                // Magenta torso/key pixels have no green. Use the generated
                // segmentation only as a matte; original RGB and registration
                // remain unchanged, including every fingertip gradient.
                if(label.A<8 || label.G<8) continue;
                Color background=body.GetPixel(x,y);
                // A moving shoulder must not protrude above the fixed neck
                // outline. Only this attachment area is clipped to the torso;
                // the elbow, forearm and fingers retain their full silhouette.
                bool neckAttachment=!dfHand && y<245;
                Color attachment=background;
                if(neckAttachment)
                {
                    // Ease out of the fixed neck over five rows before the
                    // highest forearm pixel. This preserves the round hand
                    // while avoiding a horizontal cutoff or narrow spike.
                    int extension=y<=240?0:4*(y-240)*(y-240);
                    attachment=body.GetPixel(Math.Min(CellSize-1,x+extension),y);
                    if(attachment.A<8) continue;
                }
                double coverage=Math.Min(1.0,label.G/(double)Math.Max(25,(int)c.G));
                if(coverage>.70) coverage=1.0;
                if(coverage<=0) continue;
                if(coverage<1.0 && background.A>200)
                {
                    Func<int,int,int> unmatte=(channel,bg)=>(int)Math.Round(Math.Max(0.0,
                        Math.Min(255.0,(channel-(1.0-coverage)*bg)/coverage)));
                    c=Color.FromArgb((int)Math.Round(c.A*coverage),unmatte(c.R,background.R),
                        unmatte(c.G,background.G),unmatte(c.B,background.B));
                }
                else if(coverage<1.0) c=Color.FromArgb((int)Math.Round(c.A*coverage),c.R,c.G,c.B);
                if(neckAttachment && attachment.A<255)
                    c=Color.FromArgb((int)Math.Round(c.A*attachment.A/255.0),c.R,c.G,c.B);
                output.SetPixel(281+x,21+y,c);
            }
            if(!dfHand)
            {
                FitRaisedShoulder(output,body,frame);
                SmoothEdgeBand(output,281,21,241,256);
                if(frameIndex==0) SmoothIdleInnerEdge(output);
            }
            output.Save(filename,ImageFormat.Png);
        }
    }
    static void SaveStaticBody(Bitmap body,Bitmap original,string filename)
    {
        // The fitted armless torso meets the approved arm contour directly.
        // Preserve the approved face. The torso and keyboard come from one
        // donor: swapping pixels at a polygon edge also swaps belly shading.
        for(int y=0;y<CellSize;y++) for(int x=0;x<CellSize;x++)
        {
            Color c=original.GetPixel(x,y);
            if(y<220)
            {
                Color donor=body.GetPixel(x,y);
                // Blend the lower-neck color before the atlas join, while
                // retaining the original outline. This removes the horizontal
                // lighting seam without copying idle-arm pixels into the body.
                if(y>=200 && c.A>128 && donor.A>128)
                {
                    double t=(y-200)/20.0;
                    c=Color.FromArgb(c.A,(int)Math.Round(c.R*(1-t)+donor.R*t),
                        (int)Math.Round(c.G*(1-t)+donor.G*t),(int)Math.Round(c.B*(1-t)+donor.B*t));
                }
                body.SetPixel(x,y,c);
            }
        }
        FitTorsoContours(body);
        using(var output=new Bitmap(CanvasWidth,CanvasHeight,PixelFormat.Format32bppArgb))
        {
            for(int y=0;y<CellSize;y++) for(int x=0;x<CellSize;x++)
                output.SetPixel(281+x,21+y,body.GetPixel(x,y));
            output.Save(filename,ImageFormat.Png);
        }
    }

    // Small contact footprints limit matting to the actual fingers. Neutral
    // keyboard shadows are also dark; darkness alone cannot separate them.
    static readonly PointF[][][] Contact = {
        new PointF[0][],
        new [] {
            new [] { new PointF(104,420),new PointF(161,412),new PointF(193,434),new PointF(207,463),new PointF(203,480),new PointF(178,481),new PointF(170,454),new PointF(166,471),new PointF(138,471),new PointF(132,460),new PointF(118,459),new PointF(107,445) },
            new [] { new PointF(416,435),new PointF(478,437),new PointF(489,457),new PointF(477,488),new PointF(463,515),new PointF(443,515),new PointF(438,496),new PointF(435,516),new PointF(413,516),new PointF(409,499),new PointF(414,472) }
        },
        new [] {
            new [] { new PointF(89,413),new PointF(162,412),new PointF(181,438),new PointF(184,457),new PointF(153,461),new PointF(149,450),new PointF(142,466),new PointF(120,466),new PointF(113,451),new PointF(95,451) },
            new [] { new PointF(368,439),new PointF(385,425),new PointF(457,425),new PointF(477,440),new PointF(467,467),new PointF(452,480),new PointF(435,482),new PointF(430,485),new PointF(408,486),new PointF(401,472),new PointF(386,494),new PointF(374,499),new PointF(359,496),new PointF(359,481) }
        },
        new [] {
            new [] { new PointF(102,415),new PointF(186,415),new PointF(212,443),new PointF(218,465),new PointF(210,481),new PointF(191,481),new PointF(182,452),new PointF(169,474),new PointF(141,474),new PointF(134,461),new PointF(115,465),new PointF(103,445) },
            new [] { new PointF(365,433),new PointF(398,421),new PointF(468,431),new PointF(480,453),new PointF(470,489),new PointF(461,514),new PointF(444,514),new PointF(435,492),new PointF(428,512),new PointF(404,512),new PointF(395,496),new PointF(388,471),new PointF(373,498),new PointF(354,503),new PointF(348,489),new PointF(346,477) }
        }
    };
    static bool InContact(int x,int y,int frame)
    {
        foreach (PointF[] polygon in Contact[frame])
        {
            bool inside=false;
            for (int i=0,j=polygon.Length-1;i<polygon.Length;j=i++)
            {
                PointF a=polygon[i],b=polygon[j];
                if ((a.Y>y+.5f)!=(b.Y>y+.5f) &&
                    x+.5f<(b.X-a.X)*(y+.5f-a.Y)/(b.Y-a.Y)+a.X) inside=!inside;
            }
            if (inside) return true;
        }
        return false;
    }

    static Bitmap ReadCell(Bitmap atlas, int index)
    {
        Bitmap image = atlas.Clone(new Rectangle(index % 2 * CellSize,
            index / 2 * CellSize, CellSize, CellSize), PixelFormat.Format32bppArgb);
        var data = image.LockBits(new Rectangle(0, 0, CellSize, CellSize),
            ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        byte[] pixels = new byte[data.Stride * CellSize];
        Marshal.Copy(data.Scan0, pixels, 0, pixels.Length);
        for (int y = 0; y < CellSize; y++)
            for (int x = 0; x < CellSize; x++)
            {
                int p = y * data.Stride + x * 4;
                int blue = pixels[p], green = pixels[p + 1], red = pixels[p + 2];
                if ((red > 180 && green < 80 && blue < 90) ||
                    (red > 180 && green < 130 && blue > 120) ||
                    (red > 220 && green > 240 && blue < 15)) pixels[p + 3] = 0;
            }
        int[] labels = new int[CellSize * CellSize], queue = new int[CellSize * CellSize];
        int component = 0, largest = 0, largestCount = 0;
        for (int start = 0; start < labels.Length; start++)
        {
            if (labels[start] != 0 || pixels[start / CellSize * data.Stride + start % CellSize * 4 + 3] < 32) continue;
            component++;
            int head = 0, count = 1;
            queue[0] = start; labels[start] = component;
            while (head < count)
            {
                int point = queue[head++], x = point % CellSize, y = point / CellSize;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= CellSize || ny >= CellSize) continue;
                        int next = ny * CellSize + nx;
                        if (labels[next] != 0 || pixels[ny * data.Stride + nx * 4 + 3] < 32) continue;
                        labels[next] = component; queue[count++] = next;
                    }
            }
            if (count > largestCount) { largestCount = count; largest = component; }
        }
        if (largestCount < CellSize * CellSize / 4 || largestCount > CellSize * CellSize * 9 / 10)
            throw new Exception("Incomplete sprite or opaque background in atlas.");
        int transparent = 0, partial = 0;
        for (int y = 0; y < CellSize; y++)
            for (int x = 0; x < CellSize; x++)
            {
                int n = y * CellSize + x, p = y * data.Stride + x * 4;
                bool keep = labels[n] == largest;
                if (!keep && pixels[p + 3] > 0 && pixels[p + 3] < 32)
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && ny >= 0 && nx < CellSize && ny < CellSize &&
                                labels[ny * CellSize + nx] == largest) keep = true;
                        }
                if (!keep) for (int channel = 0; channel < 4; channel++) pixels[p + channel] = 0;
                if (pixels[p + 3] == 0) transparent++;
                else if (pixels[p + 3] < 255) partial++;
            }
        Marshal.Copy(pixels, 0, data.Scan0, pixels.Length);
        image.UnlockBits(data);
        Console.WriteLine("Frame {0}: transparent={1}, antialiased={2}", index, transparent, partial);
        return image;
    }

    static int Main(string[] args)
    {
        if (args.Length != 4) { Console.Error.WriteLine("Usage: PackAssets poses.png fitted-body.png arm-mask.png output-directory"); return 1; }
        Directory.CreateDirectory(args[3]);
        using (var atlas = new Bitmap(args[0]))
        using (var bodyAtlas = new Bitmap(args[1]))
        using (var staticBody = ReadCell(bodyAtlas,0))
        using (var markerAtlas = new Bitmap(args[2]))
        {
            if (atlas.Width != CellSize * 2 || atlas.Height != CellSize * 2)
                throw new Exception("Expected registered 1254x1254 RGBA atlas.");
            Bitmap[] frames = new Bitmap[4];
            for (int i = 0; i < 4; i++) frames[i] = ReadCell(atlas, i);
            SaveStaticBody(staticBody,frames[0],Path.Combine(args[3],"base.png"));
            // With the approved camera, D/F are on screen-right and J/K on screen-left.
            string[] df = { "base_left.png", "base_1000.png", "base_0100.png", "base_1100.png" };
            string[] jk = { "base_right.png", "base_0010.png", "base_0001.png", "base_0011.png" };
            for (int i = 0; i < 4; i++)
            {
                using(var matte=markerAtlas.Clone(new Rectangle(i%2*CellSize,i/2*CellSize,CellSize,CellSize),PixelFormat.Format32bppArgb))
                {
                    SaveArmOnly(frames[i],staticBody,matte,i,true,Path.Combine(args[3],df[i]));
                    SaveArmOnly(frames[i],staticBody,matte,i,false,Path.Combine(args[3],jk[i]));
                }
            }
            using (var icon = new Bitmap(256, 256, PixelFormat.Format32bppArgb))
            using (var graphics = Graphics.FromImage(icon))
            using (var encoded = new MemoryStream())
            {
                graphics.DrawImage(frames[0], new Rectangle(0, 0, 256, 256), new Rectangle(200, 20, 300, 300), GraphicsUnit.Pixel);
                icon.Save(encoded, ImageFormat.Png);
                byte[] bytes = encoded.ToArray();
                using (var writer = new BinaryWriter(File.Create(Path.Combine(args[3], "milk-frog.ico"))))
                {
                    writer.Write((ushort)0); writer.Write((ushort)1); writer.Write((ushort)1);
                    writer.Write((byte)0); writer.Write((byte)0); writer.Write((byte)0); writer.Write((byte)0);
                    writer.Write((ushort)1); writer.Write((ushort)32); writer.Write((uint)bytes.Length); writer.Write((uint)22);
                    writer.Write(bytes);
                }
            }
            foreach (var frame in frames) frame.Dispose();
        }
        return 0;
    }
}
