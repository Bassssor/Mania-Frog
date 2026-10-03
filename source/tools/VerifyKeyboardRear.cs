using System;
using System.Drawing;
using System.IO;

class VerifyKeyboardRear {
    static int Main(string[] args) {
        try {
            string root=args[0],directory=Path.Combine(root,"dist","milk-frog-4k","verification");
            using(var source=new Bitmap(Path.Combine(root,"SFML","SFML","base.png")))
            using(var actual=new Bitmap(Path.Combine(directory,"keyboard-only.png"))) {
                int continuous=0;
                for(int x=430;x<=776;x+=2) {
                    int y=(int)Math.Round(68.0/379*x+398)+2;
                    Color c=actual.GetPixel(x-120,y-1);
                    if(c.A<245 || c.B<75 || c.R-c.G>16 || c.G-c.B>16)
                        throw new Exception("Missing or gold-contaminated rear deck: "+x+","+y);
                    continuous++;
                }
                int[,] spans={{401,458},{490,548},{584,643},{683,744}};
                string[] names={"K","J","F","D"};
                string report="PASS: "+continuous+" continuous rear-deck samples are opaque and ivory/neutral.\n";
                for(int key=0;key<4;key++) {
                    int unraised=0;
                    for(int x=spans[key,0]+6;x<=spans[key,1]-6;x++) {
                        int top=-1;
                        for(int y=460;y<560;y++) {
                            Color c=source.GetPixel(x,y);
                            if(c.A>=100 && c.B>=205 && c.R-c.G<=8 && c.G-c.B<=8){top=y;break;}
                        }
                        if(top<0)throw new Exception("Missing cap surface.");
                        // The housing belongs behind/between the elevated
                        // caps, without the old four decorative bars above.
                        Color above=actual.GetPixel(x-120,top-6);
                        if(above.A>0)throw new Exception("Raised rear bar above cap: "+names[key]+","+x);
                        unraised++;
                    }
                    report+="PASS: "+names[key]+" has no extra raised bar in "+unraised+" columns.\n";
                }
                int[,] bridges={{474,488},{565,505},{661,522},{778,542}};
                for(int i=0;i<4;i++) {
                    Color c=actual.GetPixel(bridges[i,0]-120,bridges[i,1]-1);
                    if(c.A<245 || c.R<185 || c.R-c.G>16 || c.G-c.B>16)
                        throw new Exception("Missing chassis bridge at a marked rear gap: "+i);
                }
                report+="PASS: all four marked rear chassis bridges are filled with opaque ivory material.\n";
                // Probe the entire reported triangular opening, rather than
                // the previous point outside it at (778,542).
                int sealedPixels=0;
                for(int y=540;y<=548;y++)for(int x=759;x<=764;x++) {
                    Color c=actual.GetPixel(x-120,y-1);
                    if(c.A<250 || c.R<205 || c.B<190)
                        throw new Exception("D right-rear corner still open: "+x+","+y);
                    sealedPixels++;
                }
                report+="PASS: "+sealedPixels+" D right-rear opening pixels are solid ivory, without a dark recess.\n";
                int[,] centers={{408,495},{498,510},{594,526},{694,543}};
                int checkedCaps=0;
                for(int key=0;key<4;key++)for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++) {
                    int x=centers[key,0]+dx,y=centers[key,1]+dy;
                    Color before=source.GetPixel(x,y),after=actual.GetPixel(x-120,y-1);
                    if(Math.Abs(before.R-after.R)>2 || Math.Abs(before.G-after.G)>2 || Math.Abs(before.B-after.B)>2)
                        throw new Exception("Keycap center changed: "+names[key]);
                    checkedCaps++;
                }
                report+="PASS: "+checkedCaps+" cap-center color samples preserved; no shifted key geometry.\n";
                Console.Write(report);File.WriteAllText(Path.Combine(directory,"keyboard-rear-verification.txt"),report);
            }
            return 0;
        }catch(Exception error){Console.Error.WriteLine("FAIL: "+error.Message);return 1;}
    }
}
