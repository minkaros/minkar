#import <QuartzCore/CATransform3D.h>
#include <math.h>

const CATransform3D CATransform3DIdentity = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

CATransform3D CATransform3DMakeTranslation(CGFloat tx, CGFloat ty, CGFloat tz) {
   CATransform3D t=CATransform3DIdentity;

   t.m41=tx;
   t.m42=ty;
   t.m43=tz;
   return t;
}

CATransform3D CATransform3DMakeScale(CGFloat sx, CGFloat sy, CGFloat sz) {
   CATransform3D t=CATransform3DIdentity;

   t.m11=sx;
   t.m22=sy;
   t.m33=sz;
   return t;
}

/* Rotation by angle about the unit axis (x, y, z), for row vectors (a
   point times the matrix), counterclockwise looking down the axis. */
CATransform3D CATransform3DMakeRotation(CGFloat angle, CGFloat x, CGFloat y, CGFloat z) {
   CGFloat length=sqrt(x*x+y*y+z*z);

   if(length==0)
    return CATransform3DIdentity;
   x/=length;
   y/=length;
   z/=length;

   CGFloat c=cos(angle),s=sin(angle),t=1-c;
   CATransform3D r=CATransform3DIdentity;

   r.m11=t*x*x+c;   r.m12=t*x*y+s*z; r.m13=t*x*z-s*y;
   r.m21=t*x*y-s*z; r.m22=t*y*y+c;   r.m23=t*y*z+s*x;
   r.m31=t*x*z+s*y; r.m32=t*y*z-s*x; r.m33=t*z*z+c;
   return r;
}

CATransform3D CATransform3DConcat(CATransform3D a, CATransform3D b) {
   const CGFloat *m=&a.m11,*n=&b.m11;
   CATransform3D r;
   CGFloat *o=&r.m11;

   for(int i=0;i<4;i++)
    for(int j=0;j<4;j++)
     o[i*4+j]=m[i*4+0]*n[0*4+j]+m[i*4+1]*n[1*4+j]+m[i*4+2]*n[2*4+j]+m[i*4+3]*n[3*4+j];
   return r;
}
