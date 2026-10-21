/* Parser integration independent of Encore: run from the Graphene root.
 * clang -O1 -ffunction-sections -fdata-sections tests/native_model_import.c
 *   -Wl,--gc-sections -lm -o target/native-model-import-test */
#include "../runtime.c"
#include <assert.h>
void *encore_str_from_cstr(const char *value) {
    size_t n=strlen(value);graphene_str_object *s=malloc(sizeof(*s)+n+1);
    assert(s);s->ref_count=1;s->len=n;memcpy(s->data,value,n+1);return s;
}
int main(int argc,char **argv) {
    assert(argc>1);
    for(int i=1;i<argc;i++) {
        encore_str path=graphene_string(argv[i]);uintptr_t handle=graphene_model_open(path);
        graphene_source_model *m=(void*)handle;assert(m);
        if(m->error[0]){fprintf(stderr,"%s: %s\n",argv[i],m->error);graphene_model_close(handle);free(path.object);return 1;}
        assert(m->count>0 && m->count%3==0);
        float low[3]={INFINITY,INFINITY,INFINITY},high[3]={-INFINITY,-INFINITY,-INFINITY};
        for(size_t v=0;v<m->count;v++)for(size_t j=0;j<11;j++) {
            float value=graphene_model_vertex(handle,v*11+j);assert(isfinite(value));
            if(j<3){if(value<low[j])low[j]=value;if(value>high[j])high[j]=value;}
        }
        for(size_t v=0;v<m->count;v++) {
            float *n=m->vertices+v*11+3;
            assert(fabsf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]-1)<0.001f);
        }
        printf("{\"vertices\":%zu,\"triangles\":%zu,\"texture\":[%u,%u],\"bounds_min\":[%.9g,%.9g,%.9g],\"bounds_max\":[%.9g,%.9g,%.9g]}\n",m->count,m->count/3,m->width,m->height,low[0],low[1],low[2],high[0],high[1],high[2]);
        graphene_model_close(handle);free(path.object);
    }
    return 0;
}
