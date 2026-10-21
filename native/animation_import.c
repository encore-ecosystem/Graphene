/* Offline skeletal import. Included after model_import.c to share the pinned
 * cgltf/ufbx readers. Runtime animation evaluation remains entirely in Encore. */
#define GRAPHENE_ANIM_BUDGET ((size_t)512*1024*1024)
#define GRAPHENE_ANIM_MAX_BONES 4096
#define GRAPHENE_ANIM_MAX_VERTICES 1000000
#define GRAPHENE_ANIM_MAX_KEYS 1000000

typedef struct { char name[256]; int32_t parent; float trs[10], inverse_bind[16]; } graphene_anim_bone;
typedef struct { float data[12], weights[8]; uint32_t joints[8], count; } graphene_anim_vertex;
typedef struct { char name[256], material[256]; uint32_t first, count, material_index; } graphene_anim_section;
typedef struct { uint32_t vertex; float position[3], normal[3], tangent[3]; } graphene_anim_delta;
typedef struct {char name[256];uint32_t source_index;float base_color[4],metallic,roughness,normal_scale,alpha_cutoff;uint32_t alpha_mode;bool unlit,double_sided;graphene_source_model base_image,normal_image;} graphene_anim_material;
typedef struct { char name[256]; float weight; size_t count; graphene_anim_delta *deltas; } graphene_anim_morph;
typedef struct { float time, value[4], incoming[4], outgoing[4]; } graphene_anim_key;
typedef struct { uint32_t target, path, interpolation; size_t count; graphene_anim_key *keys; } graphene_anim_track;
typedef struct { char name[256]; float duration; size_t first, count; } graphene_anim_clip;
typedef struct {
    char error[256]; size_t allocated; float discarded_weight;
    graphene_anim_bone *bones; size_t bone_count;
    graphene_anim_vertex *vertices; size_t vertex_count;
    uint32_t *indices; size_t index_count;
    graphene_anim_section *sections; size_t section_count;
    graphene_anim_material *materials; size_t material_count;
    graphene_anim_morph *morphs; size_t morph_count;
    graphene_anim_clip *clips; size_t clip_count;
    graphene_anim_track *tracks; size_t track_count;
} graphene_animation_source;
static bool anim_fail(graphene_animation_source *a,const char *message) {
    if(!a->error[0])snprintf(a->error,sizeof(a->error),"%s",message);return false;
}
static void *anim_allocate(graphene_animation_source *a,size_t count,size_t stride) {
    if(!count)return NULL;
    if(stride>GRAPHENE_ANIM_BUDGET/count || count*stride>GRAPHENE_ANIM_BUDGET-a->allocated){anim_fail(a,"Animation import exceeds 512 MiB allocation budget");return NULL;}
    void *p=calloc(count,stride);if(!p){anim_fail(a,"Unable to allocate animation data");return NULL;}
    a->allocated+=count*stride;return p;
}
static void anim_identity(float m[16]) {memset(m,0,sizeof(float)*16);m[0]=m[5]=m[10]=m[15]=1;}
static void anim_name(char out[256],const char *name,const char *fallback,size_t index) {
    if(name&&*name)snprintf(out,256,"%s",name);else snprintf(out,256,"%s%zu",fallback,index);
}
static float *anim_unpack(graphene_animation_source *a,const cgltf_accessor *accessor,size_t components) {
    if(!accessor||!accessor->count||accessor->count>GRAPHENE_ANIM_MAX_KEYS||cgltf_num_components(accessor->type)!=components){anim_fail(a,"Invalid animation accessor type or count");return NULL;}
    float *data=anim_allocate(a,accessor->count,components*sizeof(float));if(!data)return NULL;
    size_t count=accessor->count*components;
    if(cgltf_accessor_unpack_floats(accessor,data,count)!=count){free(data);anim_fail(a,"Unable to unpack animation accessor");return NULL;}
    for(size_t i=0;i<count;i++)if(!isfinite(data[i])){free(data);anim_fail(a,"Non-finite animation accessor");return NULL;}
    return data;
}
/* Check range arithmetic before upstream cgltf_validate's count-1 operations.
 * Sparse data is unpacked for every attribute, including JOINTS and indices. */
static bool anim_validate_gltf(graphene_animation_source *a,cgltf_data *d) {
    if(d->extensions_required_count)return anim_fail(a,"Required glTF extensions are unsupported by skeletal import");
    for(size_t i=0;i<d->buffer_views_count;i++){
        cgltf_buffer_view *v=&d->buffer_views[i];
        if(!v->buffer||v->offset>v->buffer->size||v->size>v->buffer->size-v->offset||v->has_meshopt_compression)return anim_fail(a,"Invalid or compressed animation buffer view");
    }
    for(size_t i=0;i<d->accessors_count;i++){
        cgltf_accessor *v=&d->accessors[i];size_t size=cgltf_calc_size(v->type,v->component_type);
        if(!v->count||v->count>GRAPHENE_ANIM_MAX_KEYS||!size||v->stride<size||v->stride>256)return anim_fail(a,"Invalid animation accessor layout");
        if(v->buffer_view&&(v->offset>v->buffer_view->size||size>v->buffer_view->size-v->offset||(v->count-1)>(v->buffer_view->size-v->offset-size)/v->stride))return anim_fail(a,"Animation accessor exceeds buffer view");
        if(!v->buffer_view&&!v->is_sparse)return anim_fail(a,"Missing animation accessor data");
        if(v->is_sparse){cgltf_accessor_sparse *s=&v->sparse;size_t is=cgltf_component_size(s->indices_component_type);
            if((s->indices_component_type!=cgltf_component_type_r_8u&&s->indices_component_type!=cgltf_component_type_r_16u&&s->indices_component_type!=cgltf_component_type_r_32u)||!is||!s->count||s->count>v->count||!s->indices_buffer_view||!s->values_buffer_view||s->indices_byte_offset>s->indices_buffer_view->size||s->count>(s->indices_buffer_view->size-s->indices_byte_offset)/is||s->values_byte_offset>s->values_buffer_view->size||s->count>(s->values_buffer_view->size-s->values_byte_offset)/size)return anim_fail(a,"Invalid sparse animation accessor");
            const uint8_t *raw=cgltf_buffer_view_data(s->indices_buffer_view);if(!raw)return anim_fail(a,"Missing sparse index data");raw+=s->indices_byte_offset;
            size_t previous=0;
            for(size_t j=0;j<s->count;j++){size_t ix=raw[j*is];if(is>1)ix|=(size_t)raw[j*is+1]<<8;if(is>2)ix|=(size_t)raw[j*is+2]<<16|(size_t)raw[j*is+3]<<24;if(ix>=v->count||(j&&ix<=previous))return anim_fail(a,"Sparse accessor indices must be increasing and in range");previous=ix;}
        }
    }
    return cgltf_validate(d)==cgltf_result_success||anim_fail(a,"Invalid glTF animation structure");
}
static bool anim_node_trs(graphene_animation_source *a,cgltf_node *node,float out[10]) {
    if(!node->has_matrix){
        memcpy(out,node->translation,sizeof(float)*3);memcpy(out+3,node->rotation,sizeof(float)*4);memcpy(out+7,node->scale,sizeof(float)*3);
    }else{
        const float *m=node->matrix;
        if(fabsf(m[3])+fabsf(m[7])+fabsf(m[11])+fabsf(m[15]-1)>1e-5f)return anim_fail(a,"Non-affine skeleton node matrix");
        float x[3]={m[0],m[1],m[2]},y[3]={m[4],m[5],m[6]},z[3]={m[8],m[9],m[10]};
        float sx=sqrtf(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]),sy=sqrtf(y[0]*y[0]+y[1]*y[1]+y[2]*y[2]),sz=sqrtf(z[0]*z[0]+z[1]*z[1]+z[2]*z[2]);
        if(sx<1e-6f||sy<1e-6f||sz<1e-6f)return anim_fail(a,"Singular skeleton scale");
        float determinant=x[0]*(y[1]*z[2]-y[2]*z[1])-y[0]*(x[1]*z[2]-x[2]*z[1])+z[0]*(x[1]*y[2]-x[2]*y[1]);
        if(determinant<0)sx=-sx;
        for(int j=0;j<3;j++){x[j]/=sx;y[j]/=sy;z[j]/=sz;}
        if(fabsf(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])+fabsf(x[0]*z[0]+x[1]*z[1]+x[2]*z[2])+fabsf(y[0]*z[0]+y[1]*z[1]+y[2]*z[2])>1e-4f)return anim_fail(a,"Skeleton rest matrices with shear cannot be represented as local TRS");
        float trace=x[0]+y[1]+z[2],q[4];
        if(trace>0){float s=sqrtf(trace+1)*2;q[3]=s*.25f;q[0]=(y[2]-z[1])/s;q[1]=(z[0]-x[2])/s;q[2]=(x[1]-y[0])/s;}
        else if(x[0]>y[1]&&x[0]>z[2]){float s=sqrtf(1+x[0]-y[1]-z[2])*2;q[3]=(y[2]-z[1])/s;q[0]=s*.25f;q[1]=(y[0]+x[1])/s;q[2]=(z[0]+x[2])/s;}
        else if(y[1]>z[2]){float s=sqrtf(1+y[1]-x[0]-z[2])*2;q[3]=(z[0]-x[2])/s;q[0]=(y[0]+x[1])/s;q[1]=s*.25f;q[2]=(z[1]+y[2])/s;}
        else{float s=sqrtf(1+z[2]-x[0]-y[1])*2;q[3]=(x[1]-y[0])/s;q[0]=(z[0]+x[2])/s;q[1]=(z[1]+y[2])/s;q[2]=s*.25f;}
        memcpy(out,m+12,sizeof(float)*3);memcpy(out+3,q,sizeof(q));out[7]=sx;out[8]=sy;out[9]=sz;
    }
    for(int i=0;i<10;i++)if(!isfinite(out[i]))return anim_fail(a,"Non-finite skeleton transform");
    float length=0;for(int i=3;i<7;i++)length+=out[i]*out[i];
    if(length<1e-12f||fabsf(length-1)>1e-3f)return anim_fail(a,"Non-unit skeleton quaternion");
    for(int i=3;i<7;i++)out[i]/=sqrtf(length);
    for(int i=7;i<10;i++)if(fabsf(out[i])<1e-6f)return anim_fail(a,"Singular skeleton scale");
    return true;
}
static bool anim_gltf_skeleton(graphene_animation_source *a,cgltf_data *d,uint32_t **mapping) {
    if(!d->nodes_count||d->nodes_count>GRAPHENE_ANIM_MAX_BONES)return anim_fail(a,"Skeleton must contain 1..4096 nodes");
    a->bone_count=d->nodes_count;a->bones=anim_allocate(a,a->bone_count,sizeof(*a->bones));
    uint32_t *map=anim_allocate(a,a->bone_count,sizeof(*map));if(!a->bones||!map){free(map);return false;}
    for(size_t i=0;i<a->bone_count;i++)map[i]=UINT32_MAX;
    size_t count=0;
    while(count<a->bone_count){size_t previous=count;
        for(size_t i=0;i<d->nodes_count;i++)if(map[i]==UINT32_MAX){cgltf_node *node=&d->nodes[i];
            size_t parent=node->parent?(size_t)(node->parent-d->nodes):0;
            if(node->parent&&map[parent]==UINT32_MAX)continue;
            graphene_anim_bone *bone=&a->bones[count];bone->parent=node->parent?(int32_t)map[parent]:-1;
            anim_name(bone->name,node->name,"Bone",i);
            bool duplicate=false;for(size_t j=0;j<d->nodes_count;j++)if(j!=i&&node->name&&d->nodes[j].name&&!strcmp(node->name,d->nodes[j].name))duplicate=true;
            if(duplicate)snprintf(bone->name,sizeof(bone->name),"%.220s#%zu",node->name,i);
            anim_identity(bone->inverse_bind);
            if(!anim_node_trs(a,node,bone->trs)){free(map);return false;}
            map[i]=(uint32_t)count++;
        }
        if(previous==count){free(map);return anim_fail(a,"Cyclic skeleton hierarchy");}
    }
    *mapping=map;return true;
}
static cgltf_accessor *anim_attribute(cgltf_attribute *attributes,size_t count,cgltf_attribute_type type,int index) {
    for(size_t i=0;i<count;i++)if(attributes[i].type==type&&attributes[i].index==index)return attributes[i].data;return NULL;
}
static bool anim_gltf_material(graphene_animation_source *a,cgltf_data *data,const cgltf_material *material,const char *path,uint32_t *out){
    uint32_t source_index=material?(uint32_t)(material-data->materials):UINT32_MAX;
    for(size_t i=0;i<a->material_count;i++)if(a->materials[i].source_index==source_index){*out=(uint32_t)i;return true;}
    graphene_anim_material *m=&a->materials[a->material_count];m->source_index=source_index;
    anim_name(m->name,material?material->name:NULL,"Material",material?source_index:0);
    if(material&&material->name)for(size_t i=0;i<data->materials_count;i++)if(i!=source_index&&data->materials[i].name&&!strcmp(material->name,data->materials[i].name)){snprintf(m->name,sizeof(m->name),"%.220s#%u",material->name,source_index);break;}
    for(int j=0;j<4;j++)m->base_color[j]=material?material->pbr_metallic_roughness.base_color_factor[j]:1;
    m->metallic=material?material->pbr_metallic_roughness.metallic_factor:0;m->roughness=material?material->pbr_metallic_roughness.roughness_factor:.5f;m->normal_scale=material?material->normal_texture.scale:1;
    m->alpha_mode=material?(uint32_t)material->alpha_mode:0;m->alpha_cutoff=material?material->alpha_cutoff:.5f;m->unlit=material&&material->unlit;m->double_sided=material&&material->double_sided;m->normal_image.data_samples=true;m->base_image.data_samples=true;
    const cgltf_texture_view *maps[2]={material?&material->pbr_metallic_roughness.base_color_texture:NULL,material?&material->normal_texture:NULL};
    graphene_source_model *images[2]={&m->base_image,&m->normal_image};
    // Include the current entry in cleanup even when decoding a map fails.
    *out=(uint32_t)a->material_count++;
    for(int j=0;j<2;j++)if(maps[j]&&maps[j]->texture){
        if(maps[j]->texcoord!=0||maps[j]->has_transform)return anim_fail(a,"Skeletal texture transforms and non-primary UV sets need explicit material authoring");
        if(!model_gltf_image(images[j],maps[j]->texture->image,path))return anim_fail(a,images[j]->error);
        size_t bytes=(size_t)images[j]->width*images[j]->height*4;
        if(bytes>GRAPHENE_ANIM_BUDGET-a->allocated)return anim_fail(a,"Animation material images exceed import budget");a->allocated+=bytes;
    }
    for(int j=0;j<4;j++)if(!isfinite(m->base_color[j])||m->base_color[j]<0||m->base_color[j]>1)return anim_fail(a,"Invalid skeletal base color factor");
    if(!isfinite(m->metallic)||m->metallic<0||m->metallic>1||!isfinite(m->roughness)||m->roughness<0||m->roughness>1||!isfinite(m->normal_scale)||!isfinite(m->alpha_cutoff)||m->alpha_cutoff<0||m->alpha_mode>2)return anim_fail(a,"Invalid skeletal material factor");
    return true;
}
static bool anim_gltf_mesh(graphene_animation_source *a,cgltf_data *d,const uint32_t *map,cgltf_node **mesh_node,const char *path) {
    cgltf_node *node=NULL;
    for(size_t i=0;i<d->nodes_count;i++)if(d->nodes[i].skin&&d->nodes[i].mesh){if(node)return anim_fail(a,"Import one skinned mesh node per asset; split multiple characters into separate sources");node=&d->nodes[i];}
    if(!node)return anim_fail(a,"Source contains no skinned mesh");
    cgltf_skin *skin=node->skin;cgltf_mesh *mesh=node->mesh;
    if(!skin->joints_count||skin->joints_count>GRAPHENE_ANIM_MAX_BONES)return anim_fail(a,"Invalid skin joint count");
    float *bind=NULL;
    if(skin->inverse_bind_matrices){if(skin->inverse_bind_matrices->count!=skin->joints_count)return anim_fail(a,"Inverse bind matrix count mismatch");bind=anim_unpack(a,skin->inverse_bind_matrices,16);if(!bind)return false;}
    for(size_t i=0;i<skin->joints_count;i++){
        if(bind){float *matrix=bind+i*16;if(fabsf(matrix[3])+fabsf(matrix[7])+fabsf(matrix[11])+fabsf(matrix[15]-1)>1e-5f){free(bind);return anim_fail(a,"Non-affine inverse bind matrix");}memcpy(a->bones[map[skin->joints[i]-d->nodes]].inverse_bind,matrix,sizeof(float)*16);}
        for(size_t j=0;j<i;j++)if(skin->joints[j]==skin->joints[i]){free(bind);return anim_fail(a,"Duplicate skin joint");}
    }
    free(bind);
    if(!mesh->primitives_count||mesh->primitives_count>4096)return anim_fail(a,"Invalid skeletal material section count");
    size_t nv=0,ni=0,nm=mesh->primitives[0].targets_count;
    if(nm>256)return anim_fail(a,"More than 256 morph targets");
    for(size_t i=0;i<mesh->primitives_count;i++){
        cgltf_primitive *p=&mesh->primitives[i];cgltf_accessor *positions=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_position,0);
        if(p->type!=cgltf_primitive_type_triangles||p->has_draco_mesh_compression||!positions||p->targets_count!=nm)return anim_fail(a,"Skeletal primitives require triangles, positions and matching morph layouts");
        size_t ic=p->indices?p->indices->count:positions->count;
        if(!ic||ic%3||nv>GRAPHENE_ANIM_MAX_VERTICES-positions->count||ic>3000000-ni)return anim_fail(a,"Skeletal mesh exceeds vertex/index limit");
        nv+=positions->count;ni+=ic;
    }
    a->vertex_count=nv;a->index_count=ni;a->section_count=mesh->primitives_count;a->morph_count=nm;
    a->vertices=anim_allocate(a,nv,sizeof(*a->vertices));a->indices=anim_allocate(a,ni,sizeof(*a->indices));a->sections=anim_allocate(a,a->section_count,sizeof(*a->sections));a->materials=anim_allocate(a,a->section_count,sizeof(*a->materials));
    if(nm)a->morphs=anim_allocate(a,nm,sizeof(*a->morphs));
    if(!a->vertices||!a->indices||!a->sections||!a->materials||(nm&&!a->morphs))return false;
    for(size_t i=0;i<nm;i++){
        graphene_anim_morph *m=&a->morphs[i];anim_name(m->name,i<mesh->target_names_count?mesh->target_names[i]:NULL,"Morph",i);
        m->weight=i<node->weights_count?node->weights[i]:i<mesh->weights_count?mesh->weights[i]:0;
        if(!isfinite(m->weight))return anim_fail(a,"Non-finite morph default weight");
        for(size_t j=0;j<i;j++)if(!strcmp(m->name,a->morphs[j].name))return anim_fail(a,"Duplicate morph name");
        m->deltas=anim_allocate(a,nv,sizeof(*m->deltas));if(!m->deltas)return false;
    }
    size_t vo=0,io=0;
    for(size_t s=0;s<mesh->primitives_count;s++){
        cgltf_primitive *p=&mesh->primitives[s];cgltf_accessor *pa=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_position,0),*na=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_normal,0),*ta=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_tangent,0),*ua=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_texcoord,0);
        size_t count=pa->count;float *positions=anim_unpack(a,pa,3),*normals=na?anim_unpack(a,na,3):NULL,*tangents=ta?anim_unpack(a,ta,4):NULL,*uvs=ua?anim_unpack(a,ua,2):NULL;
        float *joints[8]={0},*weights[8]={0},*indices=NULL;bool ok=false;
        if(!positions||(na&&(!normals||na->count!=count))||(ta&&(!tangents||ta->count!=count))||(ua&&(!uvs||ua->count!=count))){anim_fail(a,"Mismatched skeletal vertex attributes");goto primitive_done;}
        for(int set=0;set<8;set++){
            cgltf_accessor *ja=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_joints,set),*wa=anim_attribute(p->attributes,p->attributes_count,cgltf_attribute_type_weights,set);
            if((ja==NULL)!=(wa==NULL)||(!ja&&!set)){anim_fail(a,"Missing or mismatched JOINTS/WEIGHTS attributes");goto primitive_done;}
            if(ja){
                if(ja->count!=count||wa->count!=count||ja->normalized||(ja->component_type!=cgltf_component_type_r_8u&&ja->component_type!=cgltf_component_type_r_16u)){anim_fail(a,"Invalid JOINTS/WEIGHTS encoding");goto primitive_done;}
                joints[set]=anim_unpack(a,ja,4);weights[set]=anim_unpack(a,wa,4);if(!joints[set]||!weights[set])goto primitive_done;
            }
        }
        for(size_t k=0;k<p->attributes_count;k++)if((p->attributes[k].type==cgltf_attribute_type_joints||p->attributes[k].type==cgltf_attribute_type_weights)&&p->attributes[k].index>7){anim_fail(a,"More than 32 source skin influences are unsupported");goto primitive_done;}
        for(size_t v=0;v<count;v++){
            graphene_anim_vertex *out=&a->vertices[vo+v];memcpy(out->data,positions+v*3,sizeof(float)*3);
            if(normals){memcpy(out->data+3,normals+v*3,sizeof(float)*3);model_normalize(out->data+3);}
            if(tangents)memcpy(out->data+6,tangents+v*4,sizeof(float)*4);else{out->data[6]=1;out->data[9]=1;}
            if(uvs)memcpy(out->data+10,uvs+v*2,sizeof(float)*2);
            float sum=0,candidate_weights[32]={0};uint32_t candidate_joints[32]={0};size_t candidate_count=0;
            for(int set=0;set<8;set++)if(joints[set])for(int k=0;k<4;k++){
                float w=weights[set][v*4+k],jf=joints[set][v*4+k];
                if(w<0||w>1||jf<0||jf>=(float)skin->joints_count||jf!=(uint32_t)jf){anim_fail(a,"Skin joint/weight out of range");goto primitive_done;}
                if(w==0)continue;
                uint32_t joint=map[skin->joints[(uint32_t)jf]-d->nodes];size_t ix=0;
                while(ix<candidate_count&&candidate_joints[ix]!=joint)ix++;
                if(ix==candidate_count){candidate_joints[ix]=joint;candidate_count++;}candidate_weights[ix]+=w;sum+=w;
            }
            if(sum<1e-8f){anim_fail(a,"Skin vertex has no positive influence");goto primitive_done;}
            for(size_t k=0;k<candidate_count;k++)for(size_t j=k+1;j<candidate_count;j++)if(candidate_weights[j]>candidate_weights[k]||(candidate_weights[j]==candidate_weights[k]&&candidate_joints[j]<candidate_joints[k])){float w=candidate_weights[k];candidate_weights[k]=candidate_weights[j];candidate_weights[j]=w;uint32_t joint=candidate_joints[k];candidate_joints[k]=candidate_joints[j];candidate_joints[j]=joint;}
            out->count=(uint32_t)(candidate_count<8?candidate_count:8);float kept=0;
            for(size_t k=0;k<out->count;k++){out->joints[k]=candidate_joints[k];out->weights[k]=candidate_weights[k];kept+=candidate_weights[k];}
            float loss=(sum-kept)/sum;if(loss>a->discarded_weight)a->discarded_weight=loss;
            for(size_t k=0;k<out->count;k++)out->weights[k]/=kept;
        }
        graphene_anim_section *section=&a->sections[s];
        if(!anim_gltf_material(a,d,p->material,path,&section->material_index))goto primitive_done;
        anim_name(section->name,NULL,"Section",s);anim_name(section->material,p->material?p->material->name:NULL,"Material",p->material?(size_t)(p->material-d->materials):s);snprintf(section->material,sizeof(section->material),"%s",a->materials[section->material_index].name);section->first=(uint32_t)io;section->count=(uint32_t)(p->indices?p->indices->count:count);
        if(p->indices){
            if(p->indices->component_type!=cgltf_component_type_r_8u&&p->indices->component_type!=cgltf_component_type_r_16u&&p->indices->component_type!=cgltf_component_type_r_32u){anim_fail(a,"Invalid skin index encoding");goto primitive_done;}
            indices=anim_unpack(a,p->indices,1);if(!indices)goto primitive_done;
        }
        for(size_t k=0;k<section->count;k++){float ix=indices?indices[k]:(float)k;if(ix<0||ix>=(float)count||ix!=(uint32_t)ix){anim_fail(a,"Skin index out of range");goto primitive_done;}a->indices[io+k]=(uint32_t)vo+(uint32_t)ix;}
        if(!normals){
            for(size_t k=0;k<section->count;k+=3){graphene_anim_vertex *v0=&a->vertices[a->indices[io+k]],*v1=&a->vertices[a->indices[io+k+1]],*v2=&a->vertices[a->indices[io+k+2]];float u[3],v[3];for(int c=0;c<3;c++){u[c]=v1->data[c]-v0->data[c];v[c]=v2->data[c]-v0->data[c];}float n[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};for(int c=0;c<3;c++){v0->data[3+c]+=n[c];v1->data[3+c]+=n[c];v2->data[3+c]+=n[c];}}
            for(size_t v=0;v<count;v++)model_normalize(a->vertices[vo+v].data+3);
        }
        for(size_t m=0;m<nm;m++){
            cgltf_morph_target *target=&p->targets[m];float *values[3]={0};bool good=true;
            cgltf_attribute_type types[3]={cgltf_attribute_type_position,cgltf_attribute_type_normal,cgltf_attribute_type_tangent};
            for(int c=0;c<3;c++){cgltf_accessor *attr=anim_attribute(target->attributes,target->attributes_count,types[c],0);if(attr){if(attr->count!=count){good=false;anim_fail(a,"Morph accessor count mismatch");break;}values[c]=anim_unpack(a,attr,3);if(!values[c]){good=false;break;}}}
            if(good)for(size_t v=0;v<count;v++){
                bool nonzero=false;for(int c=0;c<3;c++)if(values[c])for(int k=0;k<3;k++)if(values[c][v*3+k]!=0)nonzero=true;
                if(nonzero){graphene_anim_delta *delta=&a->morphs[m].deltas[a->morphs[m].count++];delta->vertex=(uint32_t)(vo+v);if(values[0])memcpy(delta->position,values[0]+v*3,sizeof(float)*3);if(values[1])memcpy(delta->normal,values[1]+v*3,sizeof(float)*3);if(values[2])memcpy(delta->tangent,values[2]+v*3,sizeof(float)*3);}
            }
            for(int c=0;c<3;c++)free(values[c]);if(!good)goto primitive_done;
        }
        io+=section->count;vo+=count;ok=true;
primitive_done:
        free(positions);free(normals);free(tangents);free(uvs);free(indices);for(int set=0;set<8;set++){free(joints[set]);free(weights[set]);}if(!ok)return false;
    }
    *mesh_node=node;return true;
}
static bool anim_gltf_clips(graphene_animation_source *a,cgltf_data *d,const uint32_t *map,cgltf_node *mesh_node) {
    if(d->animations_count>4096)return anim_fail(a,"Too many animation clips");
    size_t tracks=0;
    for(size_t i=0;i<d->animations_count;i++)for(size_t c=0;c<d->animations[i].channels_count;c++){
        cgltf_animation_channel *channel=&d->animations[i].channels[c];size_t amount=channel->target_path==cgltf_animation_path_type_weights?a->morph_count:1;
        if(amount>65536-tracks)return anim_fail(a,"Too many animation tracks");tracks+=amount;
    }
    a->clip_count=d->animations_count;if(a->clip_count)a->clips=anim_allocate(a,a->clip_count,sizeof(*a->clips));
    if(tracks)a->tracks=anim_allocate(a,tracks,sizeof(*a->tracks));if((a->clip_count&&!a->clips)||(tracks&&!a->tracks))return false;
    for(size_t i=0;i<d->animations_count;i++){
        cgltf_animation *animation=&d->animations[i];graphene_anim_clip *clip=&a->clips[i];anim_name(clip->name,animation->name,"Clip",i);clip->first=a->track_count;
        for(size_t c=0;c<animation->channels_count;c++){
            cgltf_animation_channel *channel=&animation->channels[c];cgltf_animation_sampler *sampler=channel->sampler;
            if(!channel->target_node||!sampler||!sampler->input||!sampler->output||channel->target_node->has_matrix)return anim_fail(a,"Invalid animation channel or matrix-animated node");
            bool morph=channel->target_path==cgltf_animation_path_type_weights;
            uint32_t path=channel->target_path==cgltf_animation_path_type_translation?0:channel->target_path==cgltf_animation_path_type_rotation?1:channel->target_path==cgltf_animation_path_type_scale?2:morph?3:UINT32_MAX;
            if(path==UINT32_MAX||(morph&&(channel->target_node!=mesh_node||!a->morph_count)))return anim_fail(a,"Unsupported animation channel target");
            uint32_t interpolation=sampler->interpolation==cgltf_interpolation_type_step?0:sampler->interpolation==cgltf_interpolation_type_linear?1:sampler->interpolation==cgltf_interpolation_type_cubic_spline?2:UINT32_MAX;
            if(interpolation==UINT32_MAX)return anim_fail(a,"Unsupported animation interpolation");
            size_t components=morph?1:path==1?4:3,channels=morph?a->morph_count:1,n=sampler->input->count,mult=interpolation==2?3:1;
            if(sampler->input->component_type!=cgltf_component_type_r_32f||sampler->output->component_type!=cgltf_component_type_r_32f||n>GRAPHENE_ANIM_MAX_KEYS||n>SIZE_MAX/(mult*channels)||sampler->output->count!=n*mult*channels)return anim_fail(a,"Invalid animation key encoding or count");
            float *times=anim_unpack(a,sampler->input,1),*values=anim_unpack(a,sampler->output,components);
            if(!times||!values){free(times);free(values);return false;}
            bool ok=true;
            for(size_t k=0;k<n;k++)if(times[k]<0||times[k]>86400||(k&&times[k]<=times[k-1])){ok=anim_fail(a,"Animation times must be increasing in [0,86400]");break;}
            for(size_t component=0;ok&&component<channels;component++){
                graphene_anim_track *track=&a->tracks[a->track_count++];track->target=morph?(uint32_t)component:map[channel->target_node-d->nodes];track->path=path;track->interpolation=interpolation;track->count=n;
                for(size_t previous=clip->first;previous+1<a->track_count;previous++)if(a->tracks[previous].path==path&&a->tracks[previous].target==track->target){ok=anim_fail(a,"Duplicate animation channel");break;}
                track->keys=anim_allocate(a,n,sizeof(*track->keys));if(!track->keys){ok=false;break;}
                for(size_t k=0;ok&&k<n;k++){
                    graphene_anim_key *key=&track->keys[k];key->time=times[k];
                    memcpy(key->value,values+((k*mult+(mult==3?1:0))*channels+component)*components,components*sizeof(float));
                    if(mult==3){memcpy(key->incoming,values+(k*3*channels+component)*components,components*sizeof(float));memcpy(key->outgoing,values+((k*3+2)*channels+component)*components,components*sizeof(float));}
                    if(path==1){float norm=0;for(int j=0;j<4;j++)norm+=key->value[j]*key->value[j];if(norm<1e-12f||fabsf(norm-1)>1e-3f){ok=anim_fail(a,"Animation rotation key is not normalized");break;}for(int j=0;j<4;j++)key->value[j]/=sqrtf(norm);}
                    if(path==2)for(int j=0;j<3;j++)if(fabsf(key->value[j])<1e-6f)ok=anim_fail(a,"Singular animated scale");
                }
            }
            if(n&&times[n-1]>clip->duration)clip->duration=times[n-1];free(times);free(values);if(!ok)return false;
        }
        clip->count=a->track_count-clip->first;
    }
    return true;
}
/* Bound parser metadata too: tiny JSON can describe large accessor arrays. */
typedef struct {size_t used;} graphene_anim_parser_budget;
typedef union {max_align_t alignment;size_t size;} graphene_anim_allocation;
static void *anim_parser_allocate(void *user,cgltf_size size){
    graphene_anim_parser_budget *b=user;
    if(size>GRAPHENE_ANIM_BUDGET-b->used||size>SIZE_MAX-sizeof(graphene_anim_allocation))return NULL;
    graphene_anim_allocation *p=malloc(sizeof(*p)+size);if(!p)return NULL;
    p->size=size;b->used+=size;return p+1;
}
static void anim_parser_free(void *user,void *pointer){
    if(!pointer)return;graphene_anim_parser_budget *b=user;graphene_anim_allocation *p=(graphene_anim_allocation*)pointer-1;b->used-=p->size;free(p);
}
static bool anim_gltf(graphene_animation_source *a,const char *path) {
    graphene_anim_parser_budget budget={0};cgltf_options options={0};options.memory.alloc_func=anim_parser_allocate;options.memory.free_func=anim_parser_free;options.memory.user_data=&budget;cgltf_data *data=NULL;
    if(cgltf_parse_file(&options,path,&data)!=cgltf_result_success)return anim_fail(a,"Unable to parse skeletal glTF/GLB");
    bool ok=false;size_t bytes=0;uint32_t *map=NULL;cgltf_node *mesh_node=NULL;
    for(size_t i=0;i<data->buffers_count;i++){
        if(data->buffers[i].size>GRAPHENE_ANIM_BUDGET-bytes){anim_fail(a,"Skeletal glTF buffers exceed 512 MiB");goto done;}bytes+=data->buffers[i].size;
    }
    if(cgltf_load_buffers(&options,data,path)!=cgltf_result_success){anim_fail(a,"Unable to load skeletal glTF buffers");goto done;}
    if(!anim_validate_gltf(a,data)||!anim_gltf_skeleton(a,data,&map)||!anim_gltf_mesh(a,data,map,&mesh_node,path)||!anim_gltf_clips(a,data,map,mesh_node))goto done;
    ok=true;
done:free(map);cgltf_free(data);return ok;
}
uintptr_t graphene_animation_open(encore_str input) {
    graphene_animation_source *a=calloc(1,sizeof(*a));if(!a)return 0;
    char *path=graphene_c_string(input);if(!path){anim_fail(a,"Unable to allocate animation source path");return (uintptr_t)a;}
    FILE *file=fopen(path,"rb");if(!file){free(path);anim_fail(a,"Unable to open animation source");return (uintptr_t)a;}
    bool bounded=fseek(file,0,SEEK_END)==0;long size=bounded?ftell(file):-1;fclose(file);
    if(size<0||size>GRAPHENE_ANIM_BUDGET){free(path);anim_fail(a,"Animation source exceeds 512 MiB");return (uintptr_t)a;}
    const char *extension=strrchr(path,'.');char lower[8]={0};
    if(extension&&strlen(extension)<sizeof(lower))for(size_t i=0;extension[i];i++)lower[i]=extension[i]>='A'&&extension[i]<='Z'?extension[i]+32:extension[i];
    if(!strcmp(lower,".gltf")||!strcmp(lower,".glb"))anim_gltf(a,path);else anim_fail(a,"Skeletal FBX import is not implemented yet; use glTF/GLB");
    free(path);return (uintptr_t)a;
}
encore_str graphene_animation_error(uintptr_t handle){graphene_animation_source *a=(void*)handle;return graphene_string(a?a->error:"Unable to allocate animation importer");}
/* Bounds-checked POD bridge. No cgltf pointer survives the import operation. */
size_t graphene_animation_count(uintptr_t handle,uint32_t kind,size_t owner){
    graphene_animation_source *a=(void*)handle;if(!a||a->error[0])return 0;
    switch(kind){case 20:return a->material_count;case 21:case 22:if(owner<a->material_count){graphene_source_model *m=kind==21?&a->materials[owner].base_image:&a->materials[owner].normal_image;return m->pixels?(size_t)m->width*m->height:1;}return 0;case 0:return a->bone_count;case 1:return a->vertex_count;case 2:return a->index_count;case 3:return a->section_count;case 4:return a->morph_count;case 5:return a->clip_count;case 6:return owner<a->clip_count?a->clips[owner].count:0;case 7:return owner<a->track_count?a->tracks[owner].count:0;case 8:return owner<a->morph_count?a->morphs[owner].count:0;case 9:return owner<a->vertex_count?a->vertices[owner].count:0;default:return 0;}
}
uint32_t graphene_animation_integer(uintptr_t handle,uint32_t kind,size_t owner,size_t element){
    graphene_animation_source *a=(void*)handle;if(!a||a->error[0])return 0;
    switch(kind){case 25:return owner<a->material_count?(element?(a->materials[owner].normal_image.pixels!=NULL):(a->materials[owner].base_image.pixels!=NULL)):0;
    case 21:case 22:if(owner<a->material_count){graphene_source_model *m=kind==21?&a->materials[owner].base_image:&a->materials[owner].normal_image;return element?(m->height?m->height:1):(m->width?m->width:1);}return 0;
    case 23:case 24:if(owner<a->material_count){graphene_source_model *m=kind==23?&a->materials[owner].base_image:&a->materials[owner].normal_image;if(!m->pixels)return kind==23?UINT32_MAX:0xffff8080u;return element<(size_t)m->width*m->height?m->pixels[element]:0;}return 0;
    case 0:return owner<a->bone_count?(uint32_t)a->bones[owner].parent:UINT32_MAX;case 1:return owner<a->vertex_count&&element<a->vertices[owner].count?a->vertices[owner].joints[element]:0;case 2:return owner<a->index_count?a->indices[owner]:0;case 3:return owner<a->section_count?(element?a->sections[owner].count:a->sections[owner].first):0;case 5:return owner<a->clip_count?(uint32_t)a->clips[owner].first:0;case 6:return owner<a->track_count?(element==0?a->tracks[owner].target:element==1?a->tracks[owner].path:a->tracks[owner].interpolation):0;case 8:return owner<a->morph_count&&element<a->morphs[owner].count?a->morphs[owner].deltas[element].vertex:0;default:return 0;}
}
float graphene_animation_number(uintptr_t handle,uint32_t kind,size_t owner,size_t element,size_t component){
    graphene_animation_source *a=(void*)handle;if(!a||a->error[0])return 0;
    switch(kind){case 20:if(owner<a->material_count){graphene_anim_material *m=&a->materials[owner];switch(component){case 0:case 1:case 2:case 3:return m->base_color[component];case 4:return m->metallic;case 5:return m->roughness;case 6:return m->normal_scale;case 7:return m->unlit?1:0;case 8:return m->double_sided?1:0;case 9:return (float)m->alpha_mode;case 10:return m->alpha_cutoff;}}return 0;case 11:return a->discarded_weight;case 0:return owner<a->bone_count&&component<10?a->bones[owner].trs[component]:0;case 1:return owner<a->vertex_count&&component<12?a->vertices[owner].data[component]:0;case 2:return owner<a->bone_count&&component<16?a->bones[owner].inverse_bind[component]:0;case 3:return owner<a->vertex_count&&component<a->vertices[owner].count?a->vertices[owner].weights[component]:0;case 4:return owner<a->morph_count?a->morphs[owner].weight:0;case 5:return owner<a->clip_count?a->clips[owner].duration:0;case 7:if(owner<a->track_count&&element<a->tracks[owner].count){graphene_anim_key *key=&a->tracks[owner].keys[element];if(component==0)return key->time;if(component<=4)return key->value[component-1];if(component<=8)return key->incoming[component-5];if(component<=12)return key->outgoing[component-9];}return 0;case 8:if(owner<a->morph_count&&element<a->morphs[owner].count){graphene_anim_delta *delta=&a->morphs[owner].deltas[element];if(component<3)return delta->position[component];if(component<6)return delta->normal[component-3];if(component<9)return delta->tangent[component-6];}return 0;default:return 0;}
}
encore_str graphene_animation_name(uintptr_t handle,uint32_t kind,size_t owner){
    graphene_animation_source *a=(void*)handle;const char *name="";if(a&&!a->error[0])switch(kind){case 20:if(owner<a->material_count)name=a->materials[owner].name;break;case 0:if(owner<a->bone_count)name=a->bones[owner].name;break;case 3:if(owner<a->section_count)name=a->sections[owner].name;break;case 4:if(owner<a->morph_count)name=a->morphs[owner].name;break;case 5:if(owner<a->clip_count)name=a->clips[owner].name;break;case 10:if(owner<a->section_count)name=a->sections[owner].material;break;}return graphene_string(name);
}
void graphene_animation_close(uintptr_t handle){graphene_animation_source *a=(void*)handle;if(!a)return;for(size_t i=0;i<a->morph_count;i++)if(a->morphs)free(a->morphs[i].deltas);for(size_t i=0;i<a->track_count;i++)if(a->tracks)free(a->tracks[i].keys);for(size_t i=0;i<a->material_count;i++){free(a->materials[i].base_image.pixels);free(a->materials[i].normal_image.pixels);}free(a->materials);free(a->bones);free(a->vertices);free(a->indices);free(a->sections);free(a->morphs);free(a->clips);free(a->tracks);free(a);}
