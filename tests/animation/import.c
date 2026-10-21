/* clang -O1 -g -ffunction-sections -fdata-sections tests/animation/import.c
 * -Wl,--gc-sections -lm -o target/animation/import-test */
#include "../../runtime.c"
#include <assert.h>
void *encore_str_from_cstr(const char *value){size_t n=strlen(value);graphene_str_object *s=malloc(sizeof(*s)+n+1);assert(s);s->ref_count=1;s->len=n;memcpy(s->data,value,n+1);return s;}
static graphene_animation_source *load(const char *path,bool valid){
    encore_str input=graphene_string(path);uintptr_t h=graphene_animation_open(input);free(input.object);graphene_animation_source *a=(void*)h;assert(a);
    if(valid&&a->error[0])fprintf(stderr,"%s: %s\n",path,a->error);
    assert((a->error[0]==0)==valid);return a;
}
int main(int argc,char **argv){
    assert(argc>=2);
    for(int argument=1;argument<argc;argument++){
        const char *path=argv[argument];bool valid=strstr(path,"bad-")==NULL;
        graphene_animation_source *a=load(path,valid);
        if(valid){
            assert(a->bone_count&&a->vertex_count&&a->index_count%3==0&&a->clip_count);
            for(size_t b=0;b<a->bone_count;b++){assert(a->bones[b].parent<(int32_t)b);for(int j=0;j<10;j++)assert(isfinite(a->bones[b].trs[j]));}
            for(size_t v=0;v<a->vertex_count;v++){float sum=0;assert(a->vertices[v].count&&a->vertices[v].count<=8);for(size_t j=0;j<a->vertices[v].count;j++){assert(a->vertices[v].joints[j]<a->bone_count);sum+=a->vertices[v].weights[j];}assert(fabsf(sum-1)<1e-5f);}
            for(size_t i=0;i<a->index_count;i++)assert(a->indices[i]<a->vertex_count);
            if(strstr(path,"rig.glb")){
                assert(a->bone_count==3&&a->morph_count==1&&a->clip_count==1&&a->track_count==3);
                assert(a->morphs[0].count==1&&a->morphs[0].deltas[0].vertex==2&&a->morphs[0].deltas[0].position[2]==2);
                assert(a->tracks[0].keys[1].value[0]==4&&a->tracks[1].interpolation==2&&a->tracks[2].path==3);
                assert(a->vertices[2].count==2&&a->vertices[2].weights[0]==.5f);
            }
            printf("%s: %zu bones, %zu vertices, %zu clips, %zu morphs\n",path,a->bone_count,a->vertex_count,a->clip_count,a->morph_count);
        }else printf("Rejected %s: %s\n",path,a->error);
        graphene_animation_close((uintptr_t)a);
    }
    return 0;
}
