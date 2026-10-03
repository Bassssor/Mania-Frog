using System;
using System.Drawing;
using System.IO;
class VerifyDesk {
    static int Edge(Bitmap image,int x,bool bottom) {
        int result=-1;
        for(int y=400;y<image.Height;y++)if(image.GetPixel(x,y).A>=128) {
            if(!bottom)return y;result=y;
        }
        if(result<0)throw new Exception("Missing plane edge.");return result;
    }
    static int Main(string[] args) {
        try {
            string root=args[0];
            using(var table=new Bitmap(Path.Combine(root,"SFML","SFML","tabletop.png")))
            using(var old=new Bitmap(Path.Combine(root,"SFML","SFML","base.png")))
            using(var corner=new Bitmap(Path.Combine(root,"SFML","SFML","keyboard-corner.png")))
            using(var body=new Bitmap(Path.Combine(root,"dist","milk-frog-4k","verification","body-only.png")))
            using(var scene=new Bitmap(Path.Combine(root,"dist","milk-frog-4k","verification","state-0.png")))
            using(var keyboard=new Bitmap(Path.Combine(root,"dist","milk-frog-4k","verification","keyboard-only.png")))
            using(var topMask=new Bitmap(Path.Combine(root,"build","tabletop-top-mask.png")))
            using(var shadow=new Bitmap(Path.Combine(root,"dist","milk-frog-4k","verification","keyboard-shadow.png"))) {
                int left=880,top=880,right=0,bottom=0;
                for(int y=0;y<880;y++)for(int x=0;x<880;x++)if(table.GetPixel(x+120,y).A>=16) {
                    left=Math.Min(left,x);right=Math.Max(right,x);top=Math.Min(top,y);bottom=Math.Max(bottom,y);
                }
                if(left<15 || top<15 || right>864 || bottom>864)throw new Exception("Tabletop touches viewport edge.");
                double keyboardLong=(Edge(old,650,true)-Edge(old,550,true))/100.0;
                double deskLong=(Edge(table,700,true)-Edge(table,500,true))/200.0;
                double keyboardSide=(Edge(old,770,true)-Edge(old,750,true))/20.0;
                double deskSide=(Edge(table,240,false)-Edge(table,200,false))/40.0;
                if(Math.Abs(keyboardLong-deskLong)>.025 || Math.Abs(keyboardSide-deskSide)>.06)
                    throw new Exception("Desk and keyboard plane directions disagree.");
                int added=0;
                for(int x=698;x<=738;x++) {
                    bool seen=false,gap=false;
                    for(int y=625;y<657;y++) {
                        int a=body.GetPixel(x-120,y-1).A;
                        if(a>=128) {if(gap)throw new Exception("Gap between chassis and repaired corner: x="+x);seen=true;}
                        else if(seen)gap=true;
                        if(y>=648 && old.GetPixel(x,y).A==0 && a>=128)added++;
                    }
                }
                if(added<20)throw new Exception("Keyboard corner was not extended past the old crop.");
                for(int y=625;y<647;y++)for(int x=680;x<766;x++)
                    if(corner.GetPixel(x,y).A!=0)throw new Exception("Corner patch extends outside the missing crop.");
                int occluded=0;
                for(int x=815;x<865;x++)for(int y=525;y<563;y++) {
                    Color skin=old.GetPixel(x,y),desk=table.GetPixel(x,y);
                    if(skin.A<250 || desk.A<255 || skin.G-skin.B<40)continue;
                    Color result=scene.GetPixel(x-120,y-1);
                    if(result.A<250 || Math.Abs(result.R-desk.R)>1 || Math.Abs(result.G-desk.G)>1 || Math.Abs(result.B-desk.B)>1)
                        throw new Exception("Lower torso appears over tabletop: x="+x+",y="+y);
                    occluded++;
                }
                if(occluded<150)throw new Exception("Insufficient overlap between table edge and torso.");
                int keyLeft=880,keyRight=0,keyTop=880,keyBottom=0,clearanceChecks=0;
                for(int y=400;y<700;y++)for(int x=150;x<750;x++)if(keyboard.GetPixel(x,y).A>=128) {
                    keyLeft=Math.Min(keyLeft,x);keyRight=Math.Max(keyRight,x);keyTop=Math.Min(keyTop,y);keyBottom=Math.Max(keyBottom,y);
                    // Actual key silhouette must sit well inside the table,
                    // including the elevated caps and the repaired chassis.
                    if(x%3!=0 || y%3!=0)continue;
                    foreach(var offset in new Point[]{new Point(20,0),new Point(-20,0),new Point(0,20),new Point(0,-20),
                        new Point(14,14),new Point(-14,14),new Point(14,-14),new Point(-14,-14)}) {
                        if(topMask.GetPixel(x+120+offset.X,y+1+offset.Y).A<240)
                            throw new Exception("Keyboard is too close to a tabletop edge: x="+x+",y="+y);
                        clearanceChecks++;
                    }
                }
                double centerDx=Math.Abs((keyLeft+keyRight-left-right)/2.0),centerDy=Math.Abs((keyTop+keyBottom-top-bottom)/2.0);
                if(centerDx>16 || centerDy>16)throw new Exception("Keyboard is not centered on the tabletop.");
                int radiusChecks=0;
                foreach(int signX in new int[]{-1,1})foreach(int signZ in new int[]{-1,1})
                    foreach(int u in new int[]{4,8,12,16,24,34})foreach(int v in new int[]{4,8,12,16,24,34}) {
                        double distance=Math.Sqrt(Math.Pow(30-u,2)+Math.Pow(30-v,2));
                        if(u>=30 || v>=30 || Math.Abs(distance-30)<3)continue;
                        double px=signX*(325-u),pz=signZ*(125-v);
                        int x=(int)Math.Round(560+px-.75*pz-.5),y=(int)Math.Round(564+.18*px+.6375*pz-.5);
                        int a=topMask.GetPixel(x,y).A;
                        if(distance<30 && a<240 || distance>30 && a>16)
                            throw new Exception("Rendered corner disagrees with the common 30-unit radius.");
                        radiusChecks++;
                    }
                int shadowContacts=0,shadowCoverage=0;
                for(int x=350;x<710;x+=5) {
                    int lower=-1;
                    for(int y=520;y<670;y++)if(keyboard.GetPixel(x-120,y-1).A>=128)lower=y;
                    if(lower<0)throw new Exception("Missing actual chassis contour for shadow check.");
                    if(shadow.GetPixel(x,lower+1).A<14)throw new Exception("Contact shadow is detached from the chassis: x="+x);
                    if(shadow.GetPixel(x,lower+18).A>3)throw new Exception("Shadow extends too far beyond the chassis: x="+x);
                    shadowContacts++;
                }
                for(int y=495;y<715;y++)for(int x=300;x<820;x++)if(shadow.GetPixel(x,y).A>0) {
                    if(topMask.GetPixel(x,y).A<240)throw new Exception("Keyboard shadow spills outside the desktop.");
                    shadowCoverage++;
                }
                string report="PASS: full tabletop with transparent margins L="+left+",R="+(879-right)+",bottom="+(879-bottom)+"; "+added+" repaired-corner pixels; continuous chassis; "+occluded+" torso/table occlusion pixels; matching plane slopes long="+deskLong+"/"+keyboardLong+", side="+deskSide+"/"+keyboardSide+".\n"+
                    "PASS: keyboard center offset x="+centerDx+",y="+centerDy+"; "+clearanceChecks+" clearance samples at 20 pixels; "+radiusChecks+" rendered corner samples match the same 30-unit quarter circles.\n"+
                    "PASS: "+shadowContacts+" chassis-contact probes with near-edge density and distant fade; "+shadowCoverage+" shadow pixels contained on the desktop.";
                Console.WriteLine(report);File.WriteAllText(Path.Combine(root,"dist","milk-frog-4k","verification","desk-verification.txt"),report);
            }
            return 0;
        } catch(Exception e){Console.Error.WriteLine("FAIL: "+e.Message);return 1;}
    }
}
