#import <CoreImage/CIContext.h>
#import <CoreImage/CIImage.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CIColor.h>
#import <CoreImage/CIVector.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSNumber.h>

#import "CIImagePrivate.h"
#include <stdlib.h>
#include <math.h>

@implementation CIContext

-initWithCGContext:(CGContextRef)cgContext options:(NSDictionary *)options {
   _cgContext=CGContextRetain(cgContext);
   return self;
}

-(void)dealloc {
   CGContextRelease(_cgContext);
   [super dealloc];
}

+(CIContext *)contextWithCGContext:(CGContextRef)cgContext options:(NSDictionary *)options {
   return [[[self alloc] initWithCGContext:cgContext options:options] autorelease];
}

-(void)drawImage:(CIImage *)image atPoint:(CGPoint)atPoint fromRect:(CGRect)fromRect {
   CGRect inRect={{atPoint.x,atPoint.y},{fromRect.size.width,fromRect.size.height}};
   
   [self drawImage:image inRect:inRect fromRect:fromRect];
}

/* Minkar: an image is made into pixels (its own, or what its filter makes
   of its inputs) and drawn; image coordinates have their origin at the
   bottom left, as a context's. */
-(void)drawImage:(CIImage *)image inRect:(CGRect)inRect fromRect:(CGRect)fromRect {
   CGImageRef pixels=[self createCGImage:image fromRect:fromRect];

   if(pixels==NULL)
    return;
   CGContextDrawImage(_cgContext,inRect,pixels);
   CGImageRelease(pixels);
}

static CGContextRef createBitmap(size_t width,size_t height) {
   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGContextRef    bitmap=CGBitmapContextCreate(NULL,width,height,8,width*4,space,kCGImageAlphaPremultipliedLast);

   CGColorSpaceRelease(space);
   return bitmap;
}

typedef struct {
 CGFloat _C0[4];
 CGFloat _C1[4];
} CIGradientColors;

/* Core Image works in linear light: colors are mixed there and given back
   in sRGB, as on the reference. */
static CGFloat linearFromSRGB(CGFloat v) {
   return (v<=0.04045)?v/12.92:pow((v+0.055)/1.055,2.4);
}

static CGFloat sRGBFromLinear(CGFloat v) {
   if(v<=0)
    return 0;
   if(v>=1)
    return 1;
   return (v<=0.0031308)?v*12.92:1.055*pow(v,1/2.4)-0.055;
}

static void evaluate(void *info,const CGFloat *in,CGFloat *output) {
   CGFloat           x=in[0];
   CIGradientColors *colors=info;
   int               i;

   for(i=0;i<3;i++)
    output[i]=sRGBFromLinear(linearFromSRGB(colors->_C0[i])+x*(linearFromSRGB(colors->_C1[i])-linearFromSRGB(colors->_C0[i])));
   output[3]=colors->_C0[3]+x*(colors->_C1[3]-colors->_C0[3]);
}

static void colorComponents(CIColor *color,CGFloat *rgba) {
   rgba[0]=[color red];
   rgba[1]=[color green];
   rgba[2]=[color blue];
   rgba[3]=[color alpha];
}

/* The gradient over the rectangle, extended both ways. */
static void drawLinearGradient(CIFilter *filter,CGContextRef bitmap,CGRect rect) {
   CIVector           *start=[filter valueForKey:@"inputPoint0"];
   CIVector           *end=[filter valueForKey:@"inputPoint1"];
   CGFloat             domain[2]={0,1};
   CGFloat             range[8]={0,1,0,1,0,1,0,1};
   CGFunctionCallbacks callbacks={0,evaluate,NULL};
   CIGradientColors    colors;

   colorComponents([filter valueForKey:@"inputColor0"],colors._C0);
   colorComponents([filter valueForKey:@"inputColor1"],colors._C1);

   CGFunctionRef   function=CGFunctionCreate(&colors,1,domain,4,range,&callbacks);
   CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
   CGShadingRef    shading=CGShadingCreateAxial(space,CGPointMake([start X],[start Y]),CGPointMake([end X],[end Y]),function,YES,YES);

   CGContextTranslateCTM(bitmap,-rect.origin.x,-rect.origin.y);
   CGContextDrawShading(bitmap,shading);
   CGShadingRelease(shading);
   CGColorSpaceRelease(space);
   CGFunctionRelease(function);
}

/* One pass of a box blur along rows (or columns) of linear-light values:
   each pixel the mean of the 2r+1 around it, those outside the image
   clear. */
static void boxBlurPass(const float *src,float *dst,size_t width,size_t height,int radius,BOOL vertical) {
   size_t lines=vertical?width:height,length=vertical?height:width;
   size_t step=vertical?width*4:4,lineStep=vertical?4:width*4;
   int    window=2*radius+1;

   for(size_t line=0;line<lines;line++){
    const float *in=src+line*lineStep;
    float       *out=dst+line*lineStep;
    float        sum[4]={0,0,0,0};

    for(int i=-radius;i<=radius;i++)
     if(i>=0 && (size_t)i<length)
      for(int c=0;c<4;c++)
       sum[c]+=in[i*step+c];
    for(size_t i=0;i<length;i++){
     for(int c=0;c<4;c++)
      out[i*step+c]=sum[c]/window;
     long leaving=(long)i-radius,entering=(long)i+radius+1;

     if(leaving>=0)
      for(int c=0;c<4;c++)
       sum[c]-=in[leaving*step+c];
     if(entering<(long)length)
      for(int c=0;c<4;c++)
       sum[c]+=in[entering*step+c];
    }
   }
}

-(CGImageRef)_createPixels:(CIImage *)image rect:(CGRect)rect {
   size_t       width=(size_t)ceil(rect.size.width),height=(size_t)ceil(rect.size.height);
   CIFilter    *filter=[image filter];
   NSString    *name=[filter name];
   CGContextRef bitmap;

   if(width==0 || height==0)
    return NULL;
   if((bitmap=createBitmap(width,height))==NULL)
    return NULL;

   if(filter==nil){
    CGImageRef own=[image CGImage];

    if(own!=NULL)
     CGContextDrawImage(bitmap,CGRectMake(-rect.origin.x,-rect.origin.y,CGImageGetWidth(own),CGImageGetHeight(own)),own);
   }
   else if([name isEqualToString:@"CILinearGradient"])
    drawLinearGradient(filter,bitmap,rect);
   else if([name isEqualToString:@"CIBoxBlur"]){
    int        radius=CIBoxBlurHalfWidth([[filter valueForKey:@"inputRadius"] doubleValue]);
    CGImageRef input=[self _createPixels:[filter valueForKey:@"inputImage"] rect:rect];

    if(input!=NULL){
     CGContextDrawImage(bitmap,CGRectMake(0,0,width,height),input);
     CGImageRelease(input);
     if(radius>0){
      uint8_t *pixels=CGBitmapContextGetData(bitmap);
      size_t   count=width*height*4;
      float   *values=malloc(count*sizeof(float)),*buffer=malloc(count*sizeof(float));
      float    toLinear[256];

      for(int v=0;v<256;v++)
       toLinear[v]=(float)linearFromSRGB(v/255.0);
      for(size_t i=0;i<count;i++)
       values[i]=((i&3)==3)?pixels[i]/255.0f:toLinear[pixels[i]];
      boxBlurPass(values,buffer,width,height,radius,NO);
      boxBlurPass(buffer,values,width,height,radius,YES);
      for(size_t i=0;i<count;i++){
       CGFloat v=((i&3)==3)?values[i]:sRGBFromLinear(values[i]);

       pixels[i]=(uint8_t)lround(fmin(fmax(v,0),1)*255);
      }
      free(values);
      free(buffer);
     }
    }
   }

   CGImageRef result=CGBitmapContextCreateImage(bitmap);
   CGContextRelease(bitmap);
   return result;
}

-(CGImageRef)createCGImage:(CIImage *)image fromRect:(CGRect)fromRect {
   if(image==nil || CGRectIsEmpty(fromRect) || CGRectIsInfinite(fromRect))
    return NULL;
   return [self _createPixels:image rect:fromRect];
}

@end
