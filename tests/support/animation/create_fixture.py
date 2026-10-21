#!/usr/bin/env python3
"""Original tiny rig: two bones, one triangle, sparse morph and three channels."""
import json, struct
from pathlib import Path
out=Path(__file__).resolve().parents[3]/'target/animation/fixtures'
out.mkdir(parents=True,exist_ok=True)
blob=bytearray(); views=[]; accessors=[]
def accessor(values,components,component=5126,type=None,minimum=None,maximum=None):
    while len(blob)%4:blob.append(0)
    offset=len(blob); fmt={5126:'f',5123:'H',5121:'B',5125:'I'}[component]
    blob.extend(struct.pack('<'+fmt*len(values),*values)); view=len(views)
    views.append({'buffer':0,'byteOffset':offset,'byteLength':len(blob)-offset})
    a={'bufferView':view,'componentType':component,'count':len(values)//components,'type':type or {1:'SCALAR',2:'VEC2',3:'VEC3',4:'VEC4',16:'MAT4'}[components]}
    if minimum is not None:a['min']=minimum
    if maximum is not None:a['max']=maximum
    accessors.append(a); return len(accessors)-1
p=accessor([0,0,0,1,0,0,0,1,0],3,minimum=[0,0,0],maximum=[1,1,0])
n=accessor([0,0,1]*3,3)
j=accessor([0,1,0,0]*3,4,5123)
w=accessor([.5,.5,0,0]*3,4)
ix=accessor([0,1,2],1,5123)
ib=accessor([1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,-1,0,1],16)
t=accessor([0,2],1,minimum=[0],maximum=[2])
translation=accessor([0,0,0,4,0,0],3)
rotation=accessor([0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0],4)
weights=accessor([0,1],1)
sparse_index=accessor([2],1,5121);sparse_value=accessor([0,0,2],3)
accessors.append({'componentType':5126,'count':3,'type':'VEC3','sparse':{'count':1,'indices':{'bufferView':accessors[sparse_index]['bufferView'],'componentType':5121},'values':{'bufferView':accessors[sparse_value]['bufferView']}}})
morph=len(accessors)-1
# Sparse JOINTS verifies that no cgltf_accessor_read_uint shortcut is used.
accessors[j]['sparse']={'count':1,'indices':{'bufferView':accessors[sparse_index]['bufferView'],'componentType':5121},'values':{'bufferView':accessors[j]['bufferView']}}
g={'asset':{'version':'2.0','generator':'Graphene original animation regression fixture'},'scene':0,'scenes':[{'nodes':[0,2]}],
   'nodes':[{'name':'Root','children':[1]},{'name':'Tip','translation':[0,1,0]},{'name':'Mesh','mesh':0,'skin':0}],
   'skins':[{'joints':[0,1],'inverseBindMatrices':ib}],
   'meshes':[{'name':'Triangle','weights':[0],'extras':{'targetNames':['Smile']},'primitives':[{'attributes':{'POSITION':p,'NORMAL':n,'JOINTS_0':j,'WEIGHTS_0':w},'indices':ix,'material':0,'targets':[{'POSITION':morph}]}]}],
   'materials':[{'name':'Original'}],
   'animations':[{'name':'Move','samplers':[{'input':t,'output':translation,'interpolation':'LINEAR'},{'input':t,'output':rotation,'interpolation':'CUBICSPLINE'},{'input':t,'output':weights,'interpolation':'STEP'}], 'channels':[{'sampler':0,'target':{'node':0,'path':'translation'}},{'sampler':1,'target':{'node':1,'path':'rotation'}},{'sampler':2,'target':{'node':2,'path':'weights'}}]}],
   'buffers':[{'byteLength':len(blob)}],'bufferViews':views,'accessors':accessors}
def glb(name,doc):
    raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4)
    data=bytes(blob)+b'\0'*((-len(blob))%4)
    total=12+8+len(raw)+8+len(data)
    (out/name).write_bytes(struct.pack('<III',0x46546c67,2,total)+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(data),0x004e4942)+data)
glb('rig.glb',g)
import copy
bad=copy.deepcopy(g);bad['accessors'][t]['count']=0;glb('bad-count.glb',bad)
bad=copy.deepcopy(g);bad['nodes'][0]['children']=[0];glb('bad-cycle.glb',bad)
bad=copy.deepcopy(g);bad['meshes'][0]['primitives'][0]['attributes'].pop('WEIGHTS_0');glb('bad-weights.glb',bad)
bad=copy.deepcopy(g);bad['animations'][0]['channels'].append(copy.deepcopy(bad['animations'][0]['channels'][0]));glb('bad-duplicate.glb',bad)
bad=copy.deepcopy(g);bad['accessors'][morph]['sparse']['count']=4;glb('bad-sparse.glb',bad)
print(out)

bad=copy.deepcopy(g);bad["materials"][0]["pbrMetallicRoughness"]={"roughnessFactor":2};glb("bad-material.glb",bad)
duplicate=copy.deepcopy(g);duplicate["animations"].append(copy.deepcopy(duplicate["animations"][0]));glb("duplicate-clips.glb",duplicate)

# Valid authored weights can be negative and cubic curves can overshoot keys.
wide=copy.deepcopy(g)
cubic_weights=accessor([0,0,6,-6,0,0],1)
wide['accessors']=copy.deepcopy(accessors);wide['bufferViews']=copy.deepcopy(views)
wide['buffers']=[{'byteLength':len(blob)}]
wide['meshes'][0]['weights']=[-0.5]
wide['animations'][0]['samplers'][2]={'input':t,'output':cubic_weights,'interpolation':'CUBICSPLINE'}
glb('wide-morph.glb',wide)
