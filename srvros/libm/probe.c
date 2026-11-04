#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HOST_COMPARE
#define NAME(x) test_##x
#else
#define NAME(x) x
#endif
#define DECL(x) double NAME(x)(double)
#define DECLF(x) float NAME(x)(float)
DECL(sinh); DECL(cosh); DECL(tanh); DECL(asinh); DECL(acosh); DECL(atanh); DECL(cbrt); DECL(expm1); DECL(log1p);
DECLF(sinhf); DECLF(coshf); DECLF(asinhf); DECLF(acoshf); DECLF(atanhf); DECLF(cbrtf); DECLF(log1pf);
double NAME(hypot)(double,double);
static void check(int ok,const char* name) { if(!ok){fprintf(stderr,"libmprobe: failed %s\n",name);abort();} }
#ifdef HOST_COMPARE
static uint64_t bits(double v) { uint64_t b; memcpy(&b,&v,8); return b; }
static uint64_t order(double v) { uint64_t b=bits(v);return b>>63?~b:b|UINT64_C(0x8000000000000000); }
static uint32_t bitsf(float v) { uint32_t b; memcpy(&b,&v,4); return b; }
static uint32_t orderf(float v) { uint32_t b=bitsf(v);return b>>31?~b:b|UINT32_C(0x80000000); }
static void cmp(double x,double a,double b,const char *name) {
 if(isnan(b)){if(isnan(a))return;}else if(isinf(b)||b==0){if(bits(a)==bits(b))return;}else if(isfinite(a)){uint64_t i=order(a),j=order(b);if((i>j?i-j:j-i)<=8)return;}
 fprintf(stderr,"%s(%a): got %a expected %a\n",name,x,a,b);exit(1);
}
static void cmpf(float x,float a,float b,const char *name) {
 if(isnan(b)){if(isnan(a))return;}else if(isinf(b)||b==0){if(bitsf(a)==bitsf(b))return;}else if(isfinite(a)){uint32_t i=orderf(a),j=orderf(b);if((i>j?i-j:j-i)<=8)return;}
 fprintf(stderr,"%s(%a): got %a expected %a\n",name,(double)x,(double)a,(double)b);exit(1);
}
#define CMP(x,n) cmp((x),NAME(n)(x),n(x),#n)
#define CMPF(x,n) cmpf((x),NAME(n)(x),n(x),#n)
#endif
int main(void) {
#ifdef HOST_COMPARE
 const double v[]={-INFINITY,-1000,-710,-709,-30,-10,-2,-1,-0.999999999,-0.5,-0x1p-1074,-0.0,0.0,0x1p-1074,0x1p-1022,0x1p-52,0.1,0.5,0.999999999,1,1.000000001,2,10,30,709,710,1000,INFINITY,NAN};
 for(unsigned i=0;i<sizeof(v)/sizeof(*v);i++) { double x=v[i];float f=(float)x;
 CMP(x,sinh);CMP(x,cosh);CMP(x,tanh);CMP(x,asinh);CMP(x,acosh);CMP(x,atanh);CMP(x,cbrt);CMP(x,expm1);CMP(x,log1p);
 CMPF(f,sinhf);CMPF(f,coshf);CMPF(f,asinhf);CMPF(f,acoshf);CMPF(f,atanhf);CMPF(f,cbrtf);CMPF(f,log1pf);
 for(unsigned j=0;j<sizeof(v)/sizeof(*v);j++)cmp(x,NAME(hypot)(x,v[j]),hypot(x,v[j]),"hypot");
 }
 const double cuts[]={0.25541281188299536,0.5493061443340548,0.6931471805599453,1.0,2.0,10.0,18.714973875, 709.0,709.782712893384,710.4758600739439};
 for(unsigned i=0;i<sizeof(cuts)/sizeof(*cuts);i++) for(int j=-1;j<=1;j++) {
   double x=j<0?nextafter(cuts[i],-INFINITY):j>0?nextafter(cuts[i],INFINITY):cuts[i];
   for(int sign=0;sign<2;sign++){double y=sign?-x:x;CMP(y,sinh);CMP(y,cosh);CMP(y,tanh);CMP(y,asinh);CMP(y,acosh);CMP(y,atanh);CMP(y,cbrt);CMP(y,expm1);CMP(y,log1p);}
 }
 uint64_t state=UINT64_C(0x5d47eac113ab9ef3);
 for(int i=0;i<10000;i++){state=state*UINT64_C(6364136223846793005)+1;double x;memcpy(&x,&state,sizeof x);float f=(float)x;CMP(x,sinh);CMP(x,cosh);CMP(x,tanh);CMP(x,asinh);CMP(x,acosh);CMP(x,atanh);CMP(x,cbrt);CMP(x,expm1);CMP(x,log1p);CMPF(f,sinhf);CMPF(f,coshf);CMPF(f,asinhf);CMPF(f,acoshf);CMPF(f,atanhf);CMPF(f,cbrtf);CMPF(f,log1pf);}
 puts("libmprobe-host: ok (edge/domain/threshold and 10000 patterns <=8 ULP)");
#else
 volatile double tiny=0x1p-1074, z=-0.0, inf=INFINITY, nan=NAN;
 check(signbit(NAME(tanh)(z)) && signbit(NAME(expm1)(z)) && signbit(NAME(log1p)(z)),"signed zero");
 check(NAME(hypot)(tiny,tiny)>0 && NAME(asinh)(tiny)==tiny && NAME(cbrt)(tiny)>0,"subnormal");
 check(isnan(NAME(acosh)(0.5)) && isinf(NAME(atanh)(1)) && isnan(NAME(log1p)(-2)),"domain");
 check(NAME(tanh)(inf)==1 && isinf(NAME(sinh)(inf)) && isnan(NAME(cbrt)(nan)),"nonfinite");
 check(NAME(cosh)(1)>1.543 && NAME(cosh)(1)<1.544 && NAME(hypot)(3,4)==5,"finite");
 check(NAME(sinhf)(1)>1.175f && NAME(coshf)(1)>1.543f && NAME(asinhf)(1)>0.881f && NAME(acoshf)(2)>1.316f && NAME(atanhf)(0.5f)>0.549f && NAME(cbrtf)(8)==2 && NAME(log1pf)(1)>0.693f,"float");
 puts("libmprobe: ok");
#endif
 return 0;
}
