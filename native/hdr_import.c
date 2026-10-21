/* Radiance RGBE import. Original Graphene implementation; linear RGBA32F output.
 * Workers return immutable byte strings; Vulkan resource ownership stays on its owner. */
#include <math.h>
static uint32_t graphene_hdr_u32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void graphene_hdr_put32(unsigned char *p,uint32_t v) {
    p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
static bool graphene_hdr_dimensions(encore_str data,uint32_t *width,uint32_t *height) {
    if(!data.object || data.object->len<12 || memcmp(data.object->data,"GHF1",4))return false;
    const unsigned char *p=(const unsigned char*)data.object->data;
    *width=graphene_hdr_u32(p+4);*height=graphene_hdr_u32(p+8);
    return *width && *height && *width<=8192 && *height<=8192 &&
        (uint64_t)*width * *height <= 16777215 && data.object->len==12+(uint64_t)*width * *height*16;
}
encore_str graphene_hdr_decode(encore_str source) {
    if(!source.object || source.object->len<20 || source.object->len>268435456)return graphene_string("");
    const unsigned char *p=(const unsigned char*)source.object->data,*end=p+source.object->len;
    if(memcmp(p,"#?RADIANCE",10) && memcmp(p,"#?RGBE",6))return graphene_string("");
    bool format=false,blank=false;
    while(p<end) {
        const unsigned char *line=p;while(p<end && *p!='\n')++p;
        size_t n=(size_t)(p-line);if(p<end)++p;
        if(n && line[n-1]=='\r')--n;
        if(n==22 && !memcmp(line,"FORMAT=32-bit_rle_rgbe",22))format=true;
        if(n==0){blank=true;break;}
    }
    if(!blank || !format || p>=end)return graphene_string("");
    const unsigned char *line=p;while(p<end && *p!='\n')++p;
    if(p==end || (size_t)(p-line)>100)return graphene_string("");
    char resolution[101];memcpy(resolution,line,(size_t)(p-line));resolution[p-line]=0;++p;
    char ys=0,xs=0,tail=0;unsigned h=0,w=0;
    if(sscanf(resolution," %cY %u %cX %u %c",&ys,&h,&xs,&w,&tail)!=4 ||
        (ys!='-'&&ys!='+') || (xs!='-'&&xs!='+') || !w || !h || w>8192 || h>8192 || (uint64_t)w*h>16777215)return graphene_string("");
    size_t size=12+(size_t)w*h*16;
    unsigned char *bytes=malloc(size),*scan=malloc((size_t)w*4);
    if(!bytes || !scan){free(bytes);free(scan);return graphene_string("");}
    memcpy(bytes,"GHF1",4);graphene_hdr_put32(bytes+4,w);graphene_hdr_put32(bytes+8,h);
    for(unsigned y=0;y<h;++y) {
        if((size_t)(end-p)<4)goto invalid;
        if(w>=8 && w<=32767 && p[0]==2 && p[1]==2 && !(p[2]&128)) {
            if((((unsigned)p[2]<<8)|p[3])!=w)goto invalid;
            p+=4;
            for(unsigned c=0;c<4;++c) {
                unsigned x=0;
                while(x<w) {
                    if(p>=end)goto invalid;
                    unsigned code=*p++;
                    if(code>128) {
                        unsigned count=code-128;if(count>w-x || p>=end)goto invalid;
                        unsigned char v=*p++;for(unsigned i=0;i<count;++i)scan[(x++)*4+c]=v;
                    } else {
                        unsigned count=code;if(!count || count>w-x || (size_t)(end-p)<count)goto invalid;
                        for(unsigned i=0;i<count;++i)scan[(x++)*4+c]=*p++;
                    }
                }
            }
        } else {
            /* Non-RLE RGBE streams. Old 1,1,1 repeat packets are rejected explicitly. */
            if((size_t)(end-p)<(size_t)w*4)goto invalid;
            memcpy(scan,p,(size_t)w*4);p+=(size_t)w*4;
            for(unsigned x=0;x<w;++x)if(scan[x*4]==1 && scan[x*4+1]==1 && scan[x*4+2]==1)goto invalid;
        }
        for(unsigned x=0;x<w;++x) {
            unsigned dy=ys=='-'?y:h-1-y,dx=xs=='+'?x:w-1-x;
            unsigned char *rgbe=scan+x*4;float scale=rgbe[3]?ldexpf(1.f,(int)rgbe[3]-136):0.f;
            float pixel[4]={rgbe[0]*scale,rgbe[1]*scale,rgbe[2]*scale,1.f};
            for(unsigned c=0;c<4;++c){uint32_t bits;memcpy(&bits,pixel+c,4);graphene_hdr_put32(bytes+12+((size_t)dy*w+dx)*16+c*4,bits);}
        }
    }
    {encore_str result=encore_str_from_buffer(bytes,size);free(bytes);free(scan);return result;}
invalid:free(bytes);free(scan);return graphene_string("");
}
uint32_t graphene_hdr_dimension(encore_str data,uint32_t axis) {
    uint32_t w,h;if(!graphene_hdr_dimensions(data,&w,&h))return 0;return axis?h:w;
}
extern bool encore_vk_buffer_write_f32s(size_t,uint64_t,const float*,size_t);
bool graphene_hdr_upload(size_t buffer,encore_str data) {
    uint32_t w,h;if(!graphene_hdr_dimensions(data,&w,&h))return false;
    /* memcpy handles the byte-string's potentially unaligned storage. */
    size_t count=(size_t)w*h*4;float *samples=malloc(count*4);if(!samples)return false;
    const unsigned char *p=(const unsigned char*)data.object->data+12;
    for(size_t i=0;i<count;++i){uint32_t bits=graphene_hdr_u32(p+i*4);memcpy(samples+i,&bits,4);if(!isfinite(samples[i]) || samples[i]<0){free(samples);return false;}}
    bool result=encore_vk_buffer_write_f32s(buffer,0,samples,count);free(samples);return result;
}
