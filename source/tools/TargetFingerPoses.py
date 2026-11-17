"""Keep every fingertip round while independently posing its complete contour."""
import aggdraw
import numpy as np
from PIL import Image
from scipy.interpolate import RBFInterpolator
from scipy.ndimage import distance_transform_edt, map_coordinates


def outline(side, mask=15):
    a,b,c,d = [0 if mask&(1<<bit) else 36 for bit in range(4)]
    if side==0:
        start=(843,365)
        curves=[(933,385,997,474,999,555),
            # Split the original forearm curve at t=.8. The fixed segment
            # and the wrist's first control point keep the join tangent intact.
            (1003.8,621.4,961.88,664.12,900.376,701.592),
            (885,710.96,868.4,720-d*.32,851,729-d*.4),
            (847,749-d*.7,854,782-d,858,801-d),(863,821-d,855,830-d,840,829-d),
            (823,829-d,817,813-d,812,790-max(d,a)),
            (810,808-a,805,824-a,789,824-a),(771,825-a,766,811-a,763,778-max(a,b)),
            (760,798-b,754,816-b,736,816-b),(719,817-b,715,802-b,716,780-max(b,c)),
            (709,797-c,697,806-c,683,804-c),(666,802-c,672,777-c,685,756-c*.8),
            (703.9,736.4-c*.55,725.8625,714.2275-c*.2,749.1725,691.5834),
            (792.4625,649.53,840.4,605.85,882,574),(893,566,896,559,891,549),
            (872,510,856,469,846,436),(830,404,835,380,843,365)]
    else:
        start=(519,368)
        curves=[(550,374,570,391,559,421),(522,467,494,515,483,568),
            (475,597,481,624,502,658),(526,689-a*.3,538,714-a*.8,538,744-a),
            (541,765-a,535,776-a,519,775-a),(501,775-a,495,758-a,490,738-max(a,b)),
            (488,758-b,483,771-b,467,771-b),(450,771-b,446,753-b,440,737-max(b,c)),
            (438,753-c,431,763-c,417,762-c),(400,761-c,394,749-c,395,728-max(c,d)),
            (387,745-d,376,754-d,363,752-d),(345,750-d,354,714-d,366,689-d*.6),
            (375,670-d*.3,367,653,363,630),(355,613,357,600,361,588),
            (363,521,408,478,447,434),(471,408,495,386,519,368)]
    return start,curves


def draw_outline(size, side, mask=15):
    start,curves=outline(side,mask)
    image=Image.new('L',size);path=aggdraw.Path();path.moveto(*start)
    for curve in curves:path.curveto(*curve)
    path.close();painter=aggdraw.Draw(image);painter.path(path,aggdraw.Brush(255));painter.flush()
    return image


def contour_points(side, mask):
    start,curves=outline(side,mask);last=np.array(start,float);points=[]
    for curve in curves:
        c=np.array(curve,float).reshape(3,2)
        for t in np.linspace(0,1,9,endpoint=False):
            points.append((1-t)**3*last+3*(1-t)**2*t*c[0]+3*(1-t)*t*t*c[1]+t**3*c[2])
        last=c[2]
    return np.array(points)


class FingerPoses:
    def __init__(self, source, side, contacts):
        self.source=source;self.side=side;self.contacts=contacts
        # Include the fixed forearm/wrist join; no moving edge crosses the patch.
        self.box=(640,635,915,855) if side==0 else (335,585,550,800)
        rgba=np.array(source).astype(float)
        core=rgba[:,:,3]>250
        _,nearest=distance_transform_edt(~core,return_indices=True)
        # Extend character colours outside the cutout before texture warping.
        self.rgb=rgba[nearest[0],nearest[1],:3]
        self.points=contour_points(side,15)
        self.selection=self.points[:,1]>(670 if side==0 else 625)
        l,t,r,b=self.box
        self.anchors=np.array([(x,y) for y in (t,t+18,t+35) for x in range(l,r,30)],float)
        yy,xx=np.mgrid[t:b,l:r]
        self.queries=np.stack((xx.ravel(),yy.ravel()),axis=-1)

    def pose(self, mask):
        if mask==15:return self.source
        l,t,r,b=self.box
        source=np.vstack((self.anchors,self.points[self.selection]))
        destination=np.vstack((self.anchors,contour_points(self.side,mask)[self.selection]))
        # Pad centres and their lower arcs move together; neighbouring digits
        # never share a horizontal displacement band.
        for bit,(cx,tip) in enumerate(self.contacts):
            shift=0 if mask&(1<<bit) else 36
            pad=np.array([(cx-8,tip-17),(cx+8,tip-17),(cx,tip-8)],float)
            source=np.vstack((source,pad));destination=np.vstack((destination,pad-[0,shift]))
        warp=RBFInterpolator(destination,source,kernel='thin_plate_spline',smoothing=.1)
        coords=warp(self.queries).reshape((b-t,r-l,2))
        rgb=np.stack([map_coordinates(self.rgb[:,:,c],[coords[:,:,1],coords[:,:,0]],order=1,mode='nearest')
                      for c in range(3)],axis=-1)
        alpha=np.array(draw_outline(self.source.size,self.side,mask))[t:b,l:r]
        result=np.array(self.source)
        # All moving contour points are below the upper locked rows.
        fade=np.clip((np.arange(t,b)[:,None]-t-35)/25,0,1)[:,:,None]
        original=result[t:b,l:r,:3]
        result[t:b,l:r,:3]=np.uint8(np.clip(original*(1-fade)+rgb*fade,0,255))
        result[t:b,l:r,3]=np.uint8(result[t:b,l:r,3]*(1-fade[:,:,0])+alpha*fade[:,:,0])
        return Image.fromarray(result)
