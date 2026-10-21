/* Static source import only. Included by runtime.c so the editor and Encore
 * standalone tests use exactly the same pinned parsers. No intermediate files. */
#include <math.h>
#include <limits.h>
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 8192
#define STBI_WINDOWS_UTF8
#include "third_party/stb/stb_image.h"
#include "third_party/ufbx/ufbx.c"

#define GRAPHENE_IMPORT_VERTICES 1000000
typedef struct {
    float *vertices;
    size_t count, capacity;
    uint32_t *pixels;
    uint32_t width, height;
    bool data_samples; /* normal/data maps bypass the color transfer function */
    char error[256], texture[4096];
} graphene_source_model;

static bool model_error(graphene_source_model *m, const char *message) {
    if (!m->error[0]) snprintf(m->error, sizeof(m->error), "%s", message);
    return false;
}
static bool model_reserve(graphene_source_model *m, size_t additional) {
    if (additional > GRAPHENE_IMPORT_VERTICES - m->count)
        return model_error(m, "Source mesh exceeds the one-million-corner import limit");
    size_t needed = m->count + additional;
    if (needed <= m->capacity) return true;
    float *p = realloc(m->vertices, needed * 11 * sizeof(float));
    if (!p) return model_error(m, "Unable to allocate source geometry");
    m->vertices = p; m->capacity = needed; return true;
}
static bool model_vertex(graphene_source_model *m, const float p[3], const float n[3], const float c[3], const float uv[2]) {
    float v[11] = {p[0],p[1],p[2],n[0],n[1],n[2],c[0],c[1],c[2],uv[0],uv[1]};
    for (size_t i=0; i<11; ++i) if (!isfinite(v[i])) return model_error(m, "Non-finite source vertex");
    memcpy(m->vertices + m->count++ * 11, v, sizeof(v)); return true;
}
static void model_normalize(float v[3]) {
    double length = sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);
    if (length > 1e-20) for (int i=0;i<3;i++) v[i]=(float)(v[i]/length);
    else { v[0]=0;v[1]=1;v[2]=0; }
}
static void model_face_normals(graphene_source_model *m, size_t first) {
    float *a=m->vertices+first*11, *b=a+11, *c=b+11;
    float u[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]}, v[3]={c[0]-a[0],c[1]-a[1],c[2]-a[2]};
    float n[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
    model_normalize(n); for (int corner=0;corner<3;corner++) memcpy(a+corner*11+3,n,sizeof(n));
}
static bool model_image(graphene_source_model *m, unsigned char *rgba, int width, int height, const char *identity) {
    if (!rgba || width<=0 || height<=0 || width>8192 || height>8192) {
        stbi_image_free(rgba); return model_error(m, "Unable to decode base-color PNG/JPEG (maximum 8192 per axis)");
    }
    if (m->pixels) {
        stbi_image_free(rgba);
        return strcmp(m->texture,identity)==0 || model_error(m, "Multiple base-color textures require separate mesh assets");
    }
    size_t count=(size_t)width*height;
    m->pixels=malloc(count*sizeof(uint32_t));
    if (!m->pixels) { stbi_image_free(rgba); return model_error(m,"Unable to allocate texture"); }
    /* Imported mesh render paths sample linear UNORM, including legacy assets. */
    unsigned char linear[256];
    for (int i=0;i<256;i++) { double s=i/255.0;linear[i]=m->data_samples?(unsigned char)i:(unsigned char)lround(255.0*(s<=.04045?s/12.92:pow((s+.055)/1.055,2.4))); }
    for (size_t i=0;i<count;i++) m->pixels[i]=(uint32_t)linear[rgba[i*4]]|((uint32_t)linear[rgba[i*4+1]]<<8)|((uint32_t)linear[rgba[i*4+2]]<<16)|((uint32_t)rgba[i*4+3]<<24);
    stbi_image_free(rgba);m->width=(uint32_t)width;m->height=(uint32_t)height;
    snprintf(m->texture,sizeof(m->texture),"%s",identity);return true;
}
static bool model_readable(const char *path) { FILE *f=fopen(path,"rb");if(!f)return false;fclose(f);return true; }
static const char *model_leaf(const char *p) { const char *leaf=p;for(;*p;p++)if(*p=='/'||*p=='\\')leaf=p+1;return leaf; }
static bool model_texture_path(char out[4096], const char *source, const char *reference) {
    if (!reference || !*reference) return false;
    const char *leaf=model_leaf(source);size_t parent=(size_t)(leaf-source);
    if (parent+strlen(reference)<4096) {
        memcpy(out,source,parent);strcpy(out+parent,reference);
        for(char *p=out;*p;p++)if(*p=='\\')*p='/';
        if(model_readable(out))return true;
    }
    if(strlen(reference)<4096 && model_readable(reference)){strcpy(out,reference);return true;}
    reference=model_leaf(reference);
    if(parent+strlen(reference)>=4096)return false;
    memcpy(out,source,parent);strcpy(out+parent,reference);return model_readable(out);
}
static bool model_image_file(graphene_source_model *m,const char *path) {
    if(m->pixels && strcmp(path,m->texture)==0)return true;
    int w=0,h=0,n=0;unsigned char *rgba=stbi_load(path,&w,&h,&n,4);return model_image(m,rgba,w,h,path);
}
static bool model_fallback_image(graphene_source_model *m,const char *source) {
    if(m->pixels)return true;
    char path[4096];size_t length=strlen(source);if(length>=sizeof(path))return false;
    strcpy(path,source);char *dot=strrchr(path,'.');if(!dot)return true;
    const char *suffixes[]={".png",".jpg","_BaseColor.png","_BaseColor.jpg","_COL_4K_METALNESS.jpg"};
    size_t base=(size_t)(dot-path);
    for(size_t i=0;i<sizeof(suffixes)/sizeof(*suffixes);i++) {
        if(base+strlen(suffixes[i])>=sizeof(path))continue;
        strcpy(path+base,suffixes[i]);if(model_readable(path))return model_image_file(m,path);
    }
    return true;
}
static bool model_ufbx_image(graphene_source_model *m,ufbx_texture *texture,const char *source) {
    if(!texture)return true;
    if(texture->type!=UFBX_TEXTURE_FILE) return model_error(m,"Layered or procedural base-color textures are unsupported");
    char path[4096];
    if(model_texture_path(path,source,texture->relative_filename.data)||model_texture_path(path,source,texture->filename.data))return model_image_file(m,path);
    if(texture->content.size && texture->content.size<=INT_MAX) {
        int w=0,h=0,n=0;unsigned char *rgba=stbi_load_from_memory(texture->content.data,(int)texture->content.size,&w,&h,&n,4);
        return model_image(m,rgba,w,h,texture->filename.data);
    }
    /* Exported authoring paths often refer to another machine. Try the model's
       conventional sibling image before failing an explicit missing map. */
    if(!model_fallback_image(m,source))return false;
    return m->pixels || model_error(m,"Referenced base-color texture was not found");
}
static bool model_ufbx(graphene_source_model *m,const char *path) {
    ufbx_load_opts opts={0};opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;
    opts.generate_missing_normals=true;opts.ignore_animation=true;opts.load_external_files=true;opts.ignore_missing_external_files=true;
    opts.temp_allocator.memory_limit=512*1024*1024;opts.result_allocator.memory_limit=512*1024*1024;
    ufbx_error error;ufbx_scene *scene=ufbx_load_file(path,&opts,&error);
    if(!scene)return model_error(m,error.description.data);
    bool ok=true;
    for(size_t ni=0;ni<scene->nodes.count && ok;ni++) {
        ufbx_node *node=scene->nodes.data[ni];ufbx_mesh *mesh=node->mesh;if(!mesh)continue;
        if(mesh->num_triangles>GRAPHENE_IMPORT_VERTICES/3 || !model_reserve(m,mesh->num_triangles*3)){ok=false;break;}
        size_t capacity=mesh->max_face_triangles*3;uint32_t *indices=calloc(capacity?capacity:1,sizeof(uint32_t));
        if(!indices){ok=model_error(m,"Unable to allocate triangulation buffer");break;}
        ufbx_matrix normals=ufbx_matrix_for_normals(&node->geometry_to_world);bool flip=ufbx_matrix_determinant(&node->geometry_to_world)<0;
        for(size_t fi=0;fi<mesh->faces.count && ok;fi++) {
            ufbx_face face=mesh->faces.data[fi];if(face.num_indices<3)continue;
            ufbx_material *material=NULL;float color[3]={1,1,1};
            if(fi<mesh->face_material.count){size_t mi=mesh->face_material.data[fi];if(mi<node->materials.count)material=node->materials.data[mi];}
            if(material){
                ufbx_material_map map=material->pbr.base_color.has_value||material->pbr.base_color.texture?material->pbr.base_color:material->fbx.diffuse_color;
                if(map.has_value){color[0]=(float)map.value_vec3.x;color[1]=(float)map.value_vec3.y;color[2]=(float)map.value_vec3.z;}
                if(!model_ufbx_image(m,map.texture,path)){ok=false;break;}
            }
            uint32_t count=ufbx_triangulate_face(indices,capacity,mesh,face);if(!count){ok=model_error(m,"Unable to triangulate source face");break;}
            for(size_t ti=0;ti<count && ok;ti++)for(size_t corner=0;corner<3;corner++) {
                size_t order=flip&&corner?3-corner:corner;uint32_t index=indices[ti*3+order];
                ufbx_vec3 p=ufbx_transform_position(&node->geometry_to_world,ufbx_get_vertex_vec3(&mesh->vertex_position,index));
                ufbx_vec3 normal=ufbx_transform_direction(&normals,ufbx_get_vertex_vec3(&mesh->vertex_normal,index));
                ufbx_vec2 uv={0};if(mesh->vertex_uv.exists)uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,index);
                float position[3]={(float)p.x,(float)p.y,(float)p.z},n[3]={(float)normal.x,(float)normal.y,(float)normal.z},c[3]={color[0],color[1],color[2]},tex[2]={(float)uv.x,1-(float)uv.y};model_normalize(n);
                if(mesh->vertex_color.exists){ufbx_vec4 vc=ufbx_get_vertex_vec4(&mesh->vertex_color,index);c[0]*=(float)vc.x;c[1]*=(float)vc.y;c[2]*=(float)vc.z;}
                if(!model_vertex(m,position,n,c,tex)){ok=false;break;}
            }
        }
        free(indices);
    }
    ufbx_free_scene(scene);return ok && model_fallback_image(m,path);
}
static bool model_gltf_image(graphene_source_model *m,const cgltf_image *image,const char *source) {
    if(!image)return true;
    if(image->uri && strncmp(image->uri,"data:",5)!=0) {
        char reference[4096],path[4096];if(strlen(image->uri)>=sizeof(reference))return model_error(m,"Texture URI is too long");
        strcpy(reference,image->uri);cgltf_decode_uri(reference);
        return model_texture_path(path,source,reference)?model_image_file(m,path):model_error(m,"Referenced glTF base-color image was not found");
    }
    const unsigned char *bytes=NULL;size_t size=0;void *allocated=NULL;
    if(image->buffer_view){bytes=cgltf_buffer_view_data(image->buffer_view);size=image->buffer_view->size;}
    else if(image->uri){
        const char *comma=strchr(image->uri,',');if(!comma || comma-image->uri<7 || strncmp(comma-7,";base64",7))return model_error(m,"Expected a base64 image URI");
        size_t length=strlen(comma+1);if(length>90000000 || length%4)return model_error(m,"Invalid embedded image size");
        size=length/4*3;if(length&&comma[length]=='=')size--;if(length>1&&comma[length-1]=='=')size--;
        cgltf_options opts={0};if(cgltf_load_buffer_base64(&opts,size,comma+1,&allocated)!=cgltf_result_success)return model_error(m,"Invalid embedded image");bytes=allocated;
    }
    if(!bytes || !size || size>INT_MAX){free(allocated);return model_error(m,"Invalid embedded image buffer");}
    char identity[64];snprintf(identity,sizeof(identity),"embedded:%p",(const void*)image);
    if(m->pixels && strcmp(identity,m->texture)==0){free(allocated);return true;}
    int w=0,h=0,n=0;unsigned char *rgba=stbi_load_from_memory(bytes,(int)size,&w,&h,&n,4);free(allocated);
    return model_image(m,rgba,w,h,identity);
}
static float model_matrix_determinant(const float a[16]) {
    return a[0]*(a[5]*a[10]-a[9]*a[6])-a[4]*(a[1]*a[10]-a[9]*a[2])+a[8]*(a[1]*a[6]-a[5]*a[2]);
}
static void model_transform(const float a[16],float p[3],float n[3],float det) {
    float q[3];for(int i=0;i<3;i++)q[i]=a[i]*p[0]+a[4+i]*p[1]+a[8+i]*p[2]+a[12+i];memcpy(p,q,sizeof(q));
    q[0]=((a[5]*a[10]-a[9]*a[6])*n[0]+(a[9]*a[2]-a[1]*a[10])*n[1]+(a[1]*a[6]-a[5]*a[2])*n[2])/det;
    q[1]=((a[8]*a[6]-a[4]*a[10])*n[0]+(a[0]*a[10]-a[8]*a[2])*n[1]+(a[4]*a[2]-a[0]*a[6])*n[2])/det;
    q[2]=((a[4]*a[9]-a[8]*a[5])*n[0]+(a[8]*a[1]-a[0]*a[9])*n[1]+(a[0]*a[5]-a[4]*a[1])*n[2])/det;
    memcpy(n,q,sizeof(q));model_normalize(n);
}
static bool model_gltf_node(graphene_source_model *m,const cgltf_node *node,const char *path,unsigned depth) {
    if(depth>256)return model_error(m,"glTF node hierarchy is too deep");
    if(node->skin)return model_error(m,"Skinned glTF meshes require a skeletal mesh importer");
    if(node->mesh){
        float matrix[16];cgltf_node_transform_world(node,matrix);float det=model_matrix_determinant(matrix);
        if(!isfinite(det)||fabsf(det)<1e-20f)return model_error(m,"Singular glTF node transform");
        for(size_t pi=0;pi<node->mesh->primitives_count;pi++){
            const cgltf_primitive *prim=&node->mesh->primitives[pi];
            if(prim->has_draco_mesh_compression || prim->targets_count)return model_error(m,"Compressed or morph-target glTF meshes are unsupported");
            if(prim->type!=cgltf_primitive_type_triangles && prim->type!=cgltf_primitive_type_triangle_strip && prim->type!=cgltf_primitive_type_triangle_fan)return model_error(m,"Expected triangle mesh primitives");
            const cgltf_accessor *positions=cgltf_find_accessor(prim,cgltf_attribute_type_position,0),*normals=cgltf_find_accessor(prim,cgltf_attribute_type_normal,0),*uvs=cgltf_find_accessor(prim,cgltf_attribute_type_texcoord,0),*colors=cgltf_find_accessor(prim,cgltf_attribute_type_color,0);
            if(!positions || positions->type!=cgltf_type_vec3 || !positions->count)return model_error(m,"Invalid POSITION accessor");
            size_t count=prim->indices?prim->indices->count:positions->count;
            if(count<3 || (prim->type==cgltf_primitive_type_triangles && count%3))return model_error(m,"Invalid triangle index count");
            size_t triangles=prim->type==cgltf_primitive_type_triangles?count/3:count-2;
            if(triangles>GRAPHENE_IMPORT_VERTICES/3 || !model_reserve(m,triangles*3))return false;
            float tint[3]={1,1,1};
            if(prim->material){
                const cgltf_pbr_metallic_roughness *pbr=&prim->material->pbr_metallic_roughness;
                if(prim->material->has_pbr_metallic_roughness){memcpy(tint,pbr->base_color_factor,sizeof(tint));
                    if(pbr->base_color_texture.texture){if(pbr->base_color_texture.texcoord!=0 || pbr->base_color_texture.has_transform)return model_error(m,"Only TEXCOORD_0 without texture transforms is supported");
                        if(!model_gltf_image(m,pbr->base_color_texture.texture->image,path))return false;}}
            }
            for(size_t triangle=0;triangle<triangles;triangle++){
                size_t first=m->count;
                for(size_t corner=0;corner<3;corner++){
                    size_t c=det<0 && corner?3-corner:corner;
                    size_t offset=prim->type==cgltf_primitive_type_triangles?triangle*3+c:prim->type==cgltf_primitive_type_triangle_fan?(c?triangle+c:0):triangle+((triangle&1)&&c<2?1-c:c);
                    size_t index=prim->indices?cgltf_accessor_read_index(prim->indices,offset):offset;
                    float p[3],n[3]={0,1,0},uv[2]={0},color[4]={1,1,1,1};
                    if(index>=positions->count || !cgltf_accessor_read_float(positions,index,p,3) ||
                       (normals && (index>=normals->count||normals->type!=cgltf_type_vec3||!cgltf_accessor_read_float(normals,index,n,3))) ||
                       (uvs && (index>=uvs->count||uvs->type!=cgltf_type_vec2||!cgltf_accessor_read_float(uvs,index,uv,2))) ||
                       (colors && (index>=colors->count||!cgltf_accessor_read_float(colors,index,color,4))))return model_error(m,"Invalid vertex accessor");
                    model_transform(matrix,p,n,det);for(int j=0;j<3;j++)color[j]*=tint[j];
                    if(!model_vertex(m,p,n,color,uv))return false;
                }
                if(!normals)model_face_normals(m,first);
            }
        }
    }
    for(size_t i=0;i<node->children_count;i++)if(!model_gltf_node(m,node->children[i],path,depth+1))return false;
    return true;
}
static bool model_gltf(graphene_source_model *m,const char *path) {
    cgltf_options opts={0};cgltf_data *data=NULL;cgltf_result result=cgltf_parse_file(&opts,path,&data);
    if(result!=cgltf_result_success)return model_error(m,"Unable to parse glTF/GLB");
    bool ok=false;
    size_t buffer_bytes=0;
    for(size_t i=0;i<data->buffers_count;i++){
        if(data->buffers[i].size>536870912-buffer_bytes){model_error(m,"glTF buffers exceed 512 MiB");goto done;}
        buffer_bytes+=data->buffers[i].size;
    }
    if(cgltf_load_buffers(&opts,data,path)!=cgltf_result_success){model_error(m,"Unable to load glTF buffers");goto done;}
    /* Explicit nonzero counts precede the upstream validator's count-1 bounds
       arithmetic; validated accessors are required before any scalar reads. */
    for(size_t i=0;i<data->buffer_views_count;i++){
        cgltf_buffer_view *v=&data->buffer_views[i];
        if(!v->buffer || v->offset>v->buffer->size || v->size>v->buffer->size-v->offset || v->has_meshopt_compression){model_error(m,"Invalid or compressed glTF buffer view");goto done;}
    }
    for(size_t i=0;i<data->accessors_count;i++){
        cgltf_accessor *a=&data->accessors[i];size_t size=cgltf_calc_size(a->type,a->component_type);
        if(!a->count || a->count>GRAPHENE_IMPORT_VERTICES || !size || a->stride<size || a->stride>256){model_error(m,"Invalid glTF accessor count or stride");goto done;}
        if(a->buffer_view && (a->offset>a->buffer_view->size || size>a->buffer_view->size-a->offset || (a->count-1)>(a->buffer_view->size-a->offset-size)/a->stride)){model_error(m,"glTF accessor exceeds its buffer view");goto done;}
        if(a->is_sparse){
            cgltf_accessor_sparse *s=&a->sparse;size_t index_size=cgltf_component_size(s->indices_component_type);
            if(!index_size || !s->count || s->count>a->count || !s->indices_buffer_view || !s->values_buffer_view ||
               s->indices_byte_offset>s->indices_buffer_view->size || s->count>(s->indices_buffer_view->size-s->indices_byte_offset)/index_size ||
               s->values_byte_offset>s->values_buffer_view->size || s->count>(s->values_buffer_view->size-s->values_byte_offset)/size){model_error(m,"Invalid sparse glTF accessor");goto done;}
        }
    }
    if(cgltf_validate(data)!=cgltf_result_success){model_error(m,"Invalid glTF buffer/accessor layout");goto done;}
    if(data->extensions_required_count){model_error(m,"Required glTF extensions are unsupported");goto done;}
    const cgltf_scene *scene=data->scene?data->scene:data->scenes_count?&data->scenes[0]:NULL;
    ok=true;
    if(scene){for(size_t i=0;i<scene->nodes_count && ok;i++)ok=model_gltf_node(m,scene->nodes[i],path,0);}
    else for(size_t i=0;i<data->nodes_count && ok;i++)if(!data->nodes[i].parent)ok=model_gltf_node(m,&data->nodes[i],path,0);
    if(ok)ok=model_fallback_image(m,path);
done:cgltf_free(data);return ok;
}
uintptr_t graphene_model_open(encore_str input) {
    graphene_source_model *m=calloc(1,sizeof(*m));if(!m)return 0;m->width=m->height=1;
    char *path=graphene_c_string(input);if(!path){model_error(m,"Unable to allocate model path");return (uintptr_t)m;}
    FILE *file=fopen(path,"rb");
    if(!file){model_error(m,"Unable to open model source");free(path);return (uintptr_t)m;}
    bool bounded=fseek(file,0,SEEK_END)==0;long size=bounded?ftell(file):-1;fclose(file);
    if(size<0 || size>536870912){model_error(m,"Model source exceeds 512 MiB");free(path);return (uintptr_t)m;}
    const char *extension=strrchr(path,'.');char lower_extension[8]={0};bool ok=false;
    if(extension && strlen(extension)<sizeof(lower_extension))for(size_t i=0;extension[i];i++)lower_extension[i]=extension[i]>='A'&&extension[i]<='Z'?extension[i]+32:extension[i];
    if(!strcmp(lower_extension,".glb")||!strcmp(lower_extension,".gltf"))ok=model_gltf(m,path);
    else ok=model_ufbx(m,path);
    if(!ok && !m->error[0])model_error(m,"Source import failed");
    if(!m->count)model_error(m,"Source contains no triangle meshes");free(path);return (uintptr_t)m;
}
encore_str graphene_model_error(uintptr_t handle) {graphene_source_model *m=(void*)handle;return graphene_string(m?m->error:"Unable to allocate importer");}
size_t graphene_model_count(uintptr_t handle) {graphene_source_model *m=(void*)handle;return m&&!m->error[0]?m->count:0;}
float graphene_model_vertex(uintptr_t handle,size_t index) {graphene_source_model *m=(void*)handle;return m&&index<m->count*11?m->vertices[index]:0;}
uint32_t graphene_model_width(uintptr_t handle) {graphene_source_model *m=(void*)handle;return m?m->width:0;}
uint32_t graphene_model_height(uintptr_t handle) {graphene_source_model *m=(void*)handle;return m?m->height:0;}
uint32_t graphene_model_pixel(uintptr_t handle,size_t index) {graphene_source_model *m=(void*)handle;return m&&m->pixels&&index<(size_t)m->width*m->height?m->pixels[index]:UINT32_MAX;}
void graphene_model_close(uintptr_t handle) {graphene_source_model *m=(void*)handle;if(m){free(m->vertices);free(m->pixels);free(m);}}
