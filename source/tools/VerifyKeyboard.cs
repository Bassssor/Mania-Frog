using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

class VerifyKeyboard
{
    const int Size = 760;
    static byte[] Load(string directory, string name)
    {
        using (var bitmap = new Bitmap(Path.Combine(directory,name)))
        {
            if (bitmap.Width != 880 || bitmap.Height != 880) throw new Exception("Wrong rendered dimensions.");
            var data = bitmap.LockBits(new Rectangle(0,0,880,880), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var raw = new byte[data.Stride * 880];
            Marshal.Copy(data.Scan0,raw,0,raw.Length);
            // Preserve the original body/key regression coordinates while
            // runtime expands leftward to contain the larger desk. Full-scene
            // alpha and desk margins are checked separately at all sizes.
            var pixels = new byte[Size * Size * 4];
            for(int y=0;y<Size;y++)Array.Copy(raw,y*data.Stride+141*4,pixels,y*Size*4,(880-141)*4);
            bitmap.UnlockBits(data);
            return pixels;
        }
    }
    static int Main(string[] args)
    {
        try { return Verify(args); }
        catch(Exception error) { Console.Error.WriteLine("FAIL: "+error.Message); return 1; }
    }
    static int Verify(string[] args)
    {
        if (args.Length != 1) return 1;
        string directory = args[0];
        byte[] idle = Load(directory,"keyboard-0.png");
        byte[] idleScene = Load(directory,"state-0.png");
        byte[] bodyOnly = Load(directory,"body-only.png");
        // Above the first key, this pixel used to contain a rectangular torso
        // remnant copied by the broad keyboard-extraction mask.
        if(bodyOnly[(460*Size+173)*4+3]!=0)
            throw new Exception("Torso fragment above the first key remains.");
        // The previous contour test ended above the real keyboard join.
        // These background pixels sit between the belly and the rear of K;
        // the keyboard guard previously retained an opaque torso shelf here.
        for(int y=473;y<476;y++) for(int x=174;x<178;x++)
            if(bodyOnly[(y*Size+x)*4+3]!=0)
                throw new Exception("Belly tab retained by first-key mask: x="+x+",y="+y);
        int previousLeft=-1,previousRight=-1;
        for(int y=235;y<255;y++)
        {
            int left=-1,right=-1;
            for(int x=220;x<620;x++) if(idle[(y*Size+x)*4+3]>=128)
            { if(left<0) left=x;right=x; }
            if(left<0 || (previousLeft>=0 && (Math.Abs(left-previousLeft)>2 || Math.Abs(right-previousRight)>2)))
                throw new Exception("Abrupt contour step at the neck/torso join: row="+y);
            previousLeft=left;previousRight=right;
        }
        previousLeft=-1;
        for(int y=425;y<465;y++)
        {
            int left=-1;
            for(int x=140;x<230;x++) if(bodyOnly[(y*Size+x)*4+3]>=128){left=x;break;}
            if(left<0 || (previousLeft>=0 && Math.Abs(left-previousLeft)>1))
                throw new Exception("Protruding shelf on the belly above the keyboard: row="+y);
            previousLeft=left;
        }
        previousLeft=-1;
        for(int y=365;y<410;y++)
        {
            int left=-1;
            for(int x=140;x<240;x++) if(idle[(y*Size+x)*4+3]>=128){left=x;break;}
            if(left<0 || (previousLeft>=0 && Math.Abs(left-previousLeft)>1))
                throw new Exception("Belly clipping notch below the raised forearm: row="+y);
            previousLeft=left;
        }
        long checkedGeometry = 0, checkedVisibleKeyboard = 0;
        for (int mask = 0; mask < 16; mask++)
        {
            byte[] keyboard = Load(directory,"keyboard-" + mask + ".png");
            byte[] scene = Load(directory,"state-" + mask + ".png");
            byte[] foreground = Load(directory,"foreground-" + mask + ".png");
            byte[] armShadow = Load(directory,"arm-shadow-" + mask + ".png");
            // Check the actual combined attachment, not an armless body's
            // outline: a raised upper arm may extend beyond that body.
            // Old extraction produced jumps of 13 and 26 pixels here.
            previousLeft=-1;
            for(int y=235;y<273;y++)
            {
                int left=-1;
                for(int x=100;x<320;x++) if(scene[(y*Size+x)*4+3]>=128){left=x;break;}
                if(left<0 || (previousLeft>=0 && (left-previousLeft>1 || previousLeft-left>5)))
                    throw new Exception("Abrupt shoulder ledge at attachment: row="+y+",mask="+mask);
                previousLeft=left;
            }
            // With J/K idle, the exposed left silhouette below that forearm
            // must be cream belly, not the old thin yellow protruding rim.
            if((mask&12)==0)
                for(int y=380;y<440;y++)
                {
                    for(int x=140;x<225;x++)
                    {
                        int n=(y*Size+x)*4;
                        if(scene[n+3]<180) continue;
                        if(scene[n]<105 || scene[n+2]-scene[n+1]>40 || scene[n+1]-scene[n]>50)
                            throw new Exception("Yellow rim between idle right arm and belly: row="+y+",mask="+mask);
                        break;
                    }
                }
            for(int y=548;y<Size;y++) for(int x=50;x<540;x++)
                if(foreground[(y*Size+x)*4+3]!=0)
                    throw new Exception("Keyboard/base pixels remain in arm overlay: mask="+mask);
            for(int y=345;y<465;y++) for(int x=240;x<350;x++)
            {
                int n=(y*Size+x)*4;
                if(foreground[n+3]!=0) {
                    // F/J now reach further inward. Permit their yellow skin
                    // and dark hand to occlude the belly, but never a cream
                    // torso fragment baked into the arm overlay.
                    bool hand=foreground[n+2]<205 && foreground[n+1]<190 && foreground[n]<180;
                    bool arm=foreground[n+2]-foreground[n+1]>=15 && foreground[n+1]-foreground[n]>=30;
                    if(foreground[n+3]>100 && !hand && !arm)
                        throw new Exception("Cream torso pixels in inward-reaching arm overlay: mask="+mask);
                    continue;
                }
                for(int channel=0;channel<4;channel++) if(armShadow[n+3]==0 && scene[n+channel]!=idle[n+channel])
                    throw new Exception("Central belly color changed outside arm shadow: mask="+mask);
            }
            for(int y=510;y<558;y++) for(int x=535;x<602;x++)
            {
                int n=(y*Size+x)*4;
                if(foreground[n+3]!=0) throw new Exception("Arm overlay contains lower-right torso pixels.");
                for(int channel=0;channel<4;channel++) if(scene[n+channel]!=idle[n+channel])
                    throw new Exception("Lower-right torso edge changed: mask="+mask);
            }
            for (int side=0;side<2;side++)
            {
                bool down=side==0 ? (mask&3)!=0 : (mask&12)!=0;
                if (!down) continue;
                int left=side==0?340:65,right=side==0?595:255;
                int armChanges=0,oldCount=0,newCount=0;
                long oldY=0,newY=0;
                for (int y=250;y<535;y++) for (int x=left;x<right;x++)
                {
                    int n=(y*Size+x)*4;
                    bool oldYellow=idleScene[n+3]>180 && idleScene[n+2]>180 &&
                        idleScene[n+2]-idleScene[n+1]>25 && idleScene[n+1]-idleScene[n]>40;
                    bool newYellow=scene[n+3]>180 && scene[n+2]>180 &&
                        scene[n+2]-scene[n+1]>25 && scene[n+1]-scene[n]>40;
                    int difference=Math.Max(Math.Abs(scene[n]-idleScene[n]),
                        Math.Max(Math.Abs(scene[n+1]-idleScene[n+1]),Math.Abs(scene[n+2]-idleScene[n+2])));
                    if ((oldYellow||newYellow) && difference>45) armChanges++;
                    bool oldFinger=idleScene[n+3]>180 && idleScene[n+2]<140 && idleScene[n+1]<140 && idleScene[n]>45;
                    bool newFinger=scene[n+3]>180 && scene[n+2]<140 && scene[n+1]<140 && scene[n]>45;
                    if(oldFinger){oldCount++;oldY+=y;}
                    if(newFinger){newCount++;newY+=y;}
                }
                if(oldCount<100 || newCount<100 || armChanges<1200 ||
                    (double)newY/newCount-(double)oldY/oldCount<70)
                    throw new Exception("Whole forearm movement too small: mask="+mask+",side="+side+
                        ",armPixels="+armChanges+",old="+oldCount+",new="+newCount+
                        ",drop="+((double)newY/Math.Max(1,newCount)-(double)oldY/Math.Max(1,oldCount)));
            }
            for (int pixel = 0; pixel < Size * Size; pixel++)
            {
                int offset = pixel * 4;
                if (scene[offset+3]>64 && scene[offset]-scene[offset+2]>35 &&
                    scene[offset+1]-scene[offset+2]>35)
                    throw new Exception("Baked cyan strip remains in unlit scene: mask="+mask+",pixel="+pixel);
                for (int channel = 0; channel < 4; channel++)
                    if (keyboard[offset + channel] != idle[offset + channel])
                        throw new Exception("Unlit keyboard changed: mask=" + mask + ",pixel=" + pixel);
                checkedGeometry++;
                // Fingers may cover a key. Every remaining visible keyboard
                // pixel must equal the fixed keyboard, with effects disabled.
                // Only the upper head intentionally changes for a held key.
                if (pixel/Size<290 || idle[offset + 3] == 0 || foreground[offset + 3] != 0) continue;
                for (int channel = 0; channel < 4; channel++)
                    if(armShadow[offset+3]==0 || channel==3)
                    if (scene[offset + channel] != keyboard[offset + channel])
                        throw new Exception("Uncovered static body/keyboard changed: mask=" + mask + ",pixel=" + pixel);
                if(armShadow[offset+3]==0)checkedVisibleKeyboard++;
            }
        }
        string report = "PASS: fixed keyboard geometry for 16 states; " + checkedGeometry +
            " RGBA pixel comparisons; " + checkedVisibleKeyboard + " unchanged unoccluded body/keyboard pixels.\n" +
            "PASS: active forearms change at least 1200 yellow-arm pixels and fingertips descend at least 70 native pixels.\n";
        report += "PASS: belly RGBA remains fixed outside pose-dependent arm shadows; lower-right torso contour identical across all 16 combinations.\n";
        report += "PASS: continuous shoulder attachment without abrupt ledges; exposed left belly edge has no yellow rim in all J/K-idle combinations.\n";
        report += "PASS: smooth neck/torso contour; no torso fragments including the first-key rear intersection.\n";
        for (int mask=0;mask<16;mask++)
        {
            byte[] effect = Load(directory,"particles-"+mask+".png");
            int nonzero=0;
            for (int pixel=0;pixel<Size*Size;pixel++)
            {
                int offset=pixel*4, x=pixel%Size, y=pixel/Size;
                if (effect[offset+3]==0) continue;
                nonzero++;
                if (effect[offset+3]>64 && (effect[offset+2]+3<effect[offset+1] ||
                    effect[offset+1]-effect[offset]<15)) throw new Exception("Particle color is not warm gold.");
                if (y<200 || y>620 || x<40 || x>550) throw new Exception("Particles escaped fingertip-to-neck fountain vicinity.");
            }
            if ((mask==0 && nonzero!=0) || (mask!=0 && nonzero<100)) throw new Exception("Missing or unexpected particles.");
        }
        Console.Write(report);
        File.WriteAllText(Path.Combine(directory,"keyboard-verification.txt"),report);
        return 0;
    }
}

