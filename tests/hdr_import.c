#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {size_t ref_count,len;char data[];} graphene_str_object;
typedef struct {graphene_str_object *object;} encore_str;
static encore_str encore_str_from_buffer(const void *data,size_t n){graphene_str_object *p=malloc(sizeof(*p)+n+1);p->len=n;p->ref_count=1;memcpy(p->data,data,n);p->data[n]=0;return(encore_str){p};}
static encore_str graphene_string(const char *s){return encore_str_from_buffer(s,strlen(s));}
bool encore_vk_buffer_write_f32s(size_t b,uint64_t o,const float *p,size_t n){(void)b;(void)o;(void)p;(void)n;return true;}
#include "../native/hdr_import.c"
int main(void){
    const char header[]="#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n";
    unsigned char source[256];size_t n=strlen(header);memcpy(source,header,n);
    unsigned char rle[]={2,2,0,8,136,128,136,64,136,32,136,131};memcpy(source+n,rle,sizeof rle);n+=sizeof rle;
    encore_str input=encore_str_from_buffer(source,n),out=graphene_hdr_decode(input);
    uint32_t w,h;if(!graphene_hdr_dimensions(out,&w,&h)||w!=8||h!=1)return 1;
    uint32_t bits=graphene_hdr_u32((unsigned char*)out.object->data+12);float red;memcpy(&red,&bits,4);
    if(red!=4.f || !graphene_hdr_upload(1,out))return 2;
    free(out.object);
    for(size_t i=0;i<n;++i){encore_str truncated=encore_str_from_buffer(source,i),bad=graphene_hdr_decode(truncated);if(bad.object->len)return 3;free(truncated.object);free(bad.object);}
    free(input.object);
    const char flat[]="#?RGBE\nFORMAT=32-bit_rle_rgbe\n\n+Y 1 -X 2\n\x80\x40\x20\x83\x40\x20\x10\x83";
    input=encore_str_from_buffer(flat,sizeof(flat)-1);out=graphene_hdr_decode(input);
    if(!graphene_hdr_dimensions(out,&w,&h)||w!=2)return 4;
    bits=graphene_hdr_u32((unsigned char*)out.object->data+12);memcpy(&red,&bits,4);if(red!=2.f)return 5;
    free(input.object);free(out.object);puts("HDR import: RLE, linear range, orientation, truncation passed");return 0;
}
