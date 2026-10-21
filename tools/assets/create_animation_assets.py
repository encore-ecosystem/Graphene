#!/usr/bin/env python3
"""Original smooth humanoids with different proportions, clips and facial morphs.
No downloaded geometry or animation is used. Coordinates are metres, RH Y-up.
"""
from pathlib import Path
import json, math, struct
repo=Path(__file__).resolve().parents[2]

def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def scale(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a):return scale(a,1/math.sqrt(dot(a,a)))
def quat(axis,angle):return (*scale(axis,math.sin(angle/2)),math.cos(angle/2))
def qmul(a,b):return (*add(add(scale(b[:3],a[3]),scale(a[:3],b[3])),cross(a[:3],b[:3])),a[3]*b[3]-dot(a[:3],b[:3]))

def create(name,height,width,leg,arm,color):
    bones=[];positions=[]
    def bone(label,parent,offset):
        index=len(bones);bones.append((label,parent,offset));positions.append(add(positions[parent],offset) if parent>=0 else offset);return index
    root=bone('Root',-1,(0,0,0));hips=bone('Hips',root,(0,.95*leg,0))
    spine=bone('Spine',hips,(0,.21*height,0));chest=bone('Chest',spine,(0,.20*height,0))
    neck=bone('Neck',chest,(0,.15*height,0));head=bone('Head',neck,(0,.12*height,0))
    arms=[];legs=[]
    for side,sign in [('L',1),('R',-1)]:
        upper=bone('UpperArm.'+side,chest,(sign*.24*width,.045*height,0))
        lower=bone('LowerArm.'+side,upper,(sign*.31*arm,0,0));hand=bone('Hand.'+side,lower,(sign*.27*arm,0,0));arms.append((upper,lower,hand,sign))
    for side,sign in [('L',1),('R',-1)]:
        upper=bone('UpperLeg.'+side,hips,(sign*.115*width,-.05*leg,0))
        lower=bone('LowerLeg.'+side,upper,(0,-.43*leg,0));foot=bone('Foot.'+side,lower,(0,-.42*leg,.04));legs.append((upper,lower,foot,sign))
    # Three independent materials also exercise per-section VG cooking.
    sections=[dict(p=[],n=[],uv=[],j=[],w=[],indices=[],breath=[],blink=[]) for _ in range(3)]
    def ellipsoid(material,center,axis,length,ru,rv,start,end=None,breath=False,blink=False):
        section=sections[material];base=len(section['p']);axis=norm(axis)
        u=norm(cross((0,0,1) if abs(axis[2])<.9 else (0,1,0),axis));v=cross(axis,u)
        sides=16;rings=10
        def vertex(theta,phi):
            sn=math.sin(theta);cs=math.cos(theta)
            offset=add(scale(axis,cs*length),add(scale(u,sn*math.cos(phi)*ru),scale(v,sn*math.sin(phi)*rv)))
            point=add(center,offset)
            normal=norm(add(scale(axis,cs/length),add(scale(u,sn*math.cos(phi)/ru),scale(v,sn*math.sin(phi)/rv))))
            # The bone at the beginning owns most of the limb. A short smooth
            # transition near the distal joint avoids a hard rigid hinge.
            weight=0 if end is None else max(0,min(1,(cs+.15)/1.15))*.55
            section['p'].append(point);section['n'].append(normal);section['uv'].append((phi/(2*math.pi),theta/math.pi))
            section['j'].append((start,end if end is not None else start,0,0));section['w'].append((1-weight,weight,0,0))
            section['breath'].append((offset[0]*.045,0,offset[2]*.08) if breath else (0,0,0))
            section['blink'].append((0,-offset[1]*.88,0) if blink else (0,0,0))
        vertex(0,0)
        for ring in range(1,rings):
            for side in range(sides+1):vertex(math.pi*ring/rings,2*math.pi*side/sides)
        bottom=len(section['p'])-base;vertex(math.pi,0)
        triangles=[]
        for side in range(sides):triangles.append((base,base+1+side,base+2+side))
        for ring in range(rings-2):
            for side in range(sides):
                a=base+1+ring*(sides+1)+side;b=a+sides+1
                triangles.extend([(a,b,b+1),(a,b+1,a+1)])
        for side in range(sides):triangles.append((base+bottom,base+1+(rings-2)*(sides+1)+side+1,base+1+(rings-2)*(sides+1)+side))
        for triangle in triangles:
            a,b,c=(section['p'][i] for i in triangle)
            normal=cross(sub(b,a),sub(c,a))
            assert dot(normal,normal)>1e-16
            assert dot(normal,add(add(section['n'][triangle[0]],section['n'][triangle[1]]),section['n'][triangle[2]]))>0
            section['indices'].extend(triangle)
    def limb(material,a,b,radius):
        vector=sub(positions[b],positions[a]);center=scale(add(positions[a],positions[b]),.5)
        ellipsoid(material,center,vector,math.sqrt(dot(vector,vector))*.57,radius,radius*.88,a,b)
    ellipsoid(0,add(positions[hips],(0,.04,0)),(0,1,0),.18*height,.19*width,.12*width,hips,spine,breath=True)
    ellipsoid(0,scale(add(positions[spine],positions[chest]),.5),(0,1,0),.24*height,.21*width,.125*width,spine,chest,breath=True)
    limb(1,neck,head,.065*width)
    ellipsoid(1,add(positions[head],(0,.10*height,0)),(0,1,0),.16*height,.115*width,.105*width,head)
    for upper,lower,hand,sign in arms:
        limb(0,upper,lower,.075*width);limb(0,lower,hand,.06*width)
        ellipsoid(1,add(positions[hand],(sign*.055*arm,0,0)),(1,0,0),.09*arm,.045*width,.055*width,hand)
    for upper,lower,foot,sign in legs:
        limb(0,upper,lower,.10*width);limb(0,lower,foot,.075*width)
        ellipsoid(0,add(positions[foot],(0,-.025,.09)),(0,0,1),.16,.075*width,.05*width,foot)
    for sign in [-1,1]:
        ellipsoid(2,add(positions[head],(sign*.044*width,.13*height,.095*width)),(0,1,0),.018,.018,.018,head,blink=True)
    blob=bytearray();views=[];accessors=[]
    def accessor(rows,kind,component=5126,minimum=False):
        while len(blob)%4:blob.append(0)
        offset=len(blob);flat=[c for row in rows for c in row]
        fmt={5126:'f',5123:'H',5125:'I'}[component];blob.extend(struct.pack('<'+fmt*len(flat),*flat));view=len(views)
        views.append(dict(buffer=0,byteOffset=offset,byteLength=len(blob)-offset))
        item=dict(bufferView=view,componentType=component,count=len(rows),type=kind)
        if minimum:item.update(min=[min(row[c] for row in rows) for c in range(len(rows[0]))],max=[max(row[c] for row in rows) for c in range(len(rows[0]))])
        accessors.append(item);return len(accessors)-1
    primitives=[]
    for material,section in enumerate(sections):
        attributes=dict(POSITION=accessor(section['p'],'VEC3',minimum=True),NORMAL=accessor(section['n'],'VEC3'),TEXCOORD_0=accessor(section['uv'],'VEC2'),JOINTS_0=accessor(section['j'],'VEC4',5123),WEIGHTS_0=accessor(section['w'],'VEC4'))
        primitives.append(dict(attributes=attributes,indices=accessor([(i,) for i in section['indices']],'SCALAR',5125),material=material,targets=[{'POSITION':accessor(section['breath'],'VEC3')},{'POSITION':accessor(section['blink'],'VEC3')}]))
    matrices=[]
    for x,y,z in positions:matrices.append((1,0,0,0,0,1,0,0,0,0,1,0,-x,-y,-z,1))
    inverse_bind=accessor(matrices,'MAT4')
    nodes=[]
    for i,(label,parent,offset) in enumerate(bones):
        node=dict(name=label,translation=offset);children=[j for j,b in enumerate(bones) if b[1]==i]
        if children:node['children']=children
        nodes.append(node)
    mesh_node=len(nodes);nodes.append(dict(name='CharacterMesh',mesh=0,skin=0))
    animations=[]
    for label,duration,swing in [('Idle',3,.025),('Walk',1.2,.55),('Run',.72,.95)]:
        times=[(i/32*duration,) for i in range(33)];input_index=accessor(times,'SCALAR',minimum=True);samplers=[];channels=[]
        def channel(node,path,values,kind):
            samplers.append(dict(input=input_index,output=accessor(values,kind),interpolation='LINEAR'))
            channels.append(dict(sampler=len(samplers)-1,target=dict(node=node,path=path)))
        translations=[]
        for i in range(33):
            phase=2*math.pi*i/32;translations.append(add(bones[hips][2],(0,(.008 if label=='Idle' else .035 if label=='Walk' else .065)*(1-math.cos(phase*2)),0)))
        channel(hips,'translation',translations,'VEC3')
        for upper,lower,hand,sign in arms:
            channel(upper,'rotation',[qmul(quat((1,0,0),sign*math.sin(2*math.pi*i/32)*swing),quat((0,0,1),-sign*math.pi/2)) for i in range(33)],'VEC4')
            channel(lower,'rotation',[quat((0,1,0),sign*(.15+max(0,math.sin(2*math.pi*i/32))*swing*.6)) for i in range(33)],'VEC4')
        for upper,lower,foot,sign in legs:
            channel(upper,'rotation',[quat((1,0,0),sign*math.sin(2*math.pi*i/32)*swing) for i in range(33)],'VEC4')
            channel(lower,'rotation',[quat((1,0,0),.08+max(0,-sign*math.sin(2*math.pi*i/32))*swing) for i in range(33)],'VEC4')
        channel(head,'rotation',[quat((0,1,0),math.sin(2*math.pi*i/32)*.10) for i in range(33)],'VEC4')
        channel(mesh_node,'weights',[(w,) for i in range(33) for w in [(.5-.5*math.cos(2*math.pi*i/32)),max(0,1-abs(i-16)/2)]],'SCALAR')
        animations.append(dict(name=label,samplers=samplers,channels=channels))
    doc=dict(asset=dict(version='2.0',generator='Graphene original procedural humanoid'),scene=0,scenes=[dict(nodes=[0,mesh_node])],nodes=nodes,skins=[dict(joints=list(range(len(bones))),inverseBindMatrices=inverse_bind)],meshes=[dict(name=name,weights=[0,0],extras=dict(targetNames=['Breath','Blink']),primitives=primitives)],materials=[dict(name='Suit',pbrMetallicRoughness=dict(baseColorFactor=[*color,1],metallicFactor=.05,roughnessFactor=.65)),dict(name='Skin',pbrMetallicRoughness=dict(baseColorFactor=[.65,.36,.22,1],metallicFactor=0,roughnessFactor=.7)),dict(name='Eyes',pbrMetallicRoughness=dict(baseColorFactor=[.02,.02,.025,1],metallicFactor=0,roughnessFactor=.4))],animations=animations,buffers=[dict(byteLength=len(blob))],bufferViews=views,accessors=accessors)
    raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4);blob+=b'\0'*((-len(blob))%4)
    data=struct.pack('<III',0x46546c67,2,12+8+len(raw)+8+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob
    directory=repo/'Assets/Models/Animation/Humanoids';directory.mkdir(parents=True,exist_ok=True);(directory/(name+'.glb')).write_bytes(data)
    print(name,sum(len(s['p']) for s in sections),'vertices',sum(len(s['indices'])//3 for s in sections),'triangles',len(data),'bytes')

if __name__=='__main__':
    create('Tall',1.08,1,1.10,1.07,(.07,.24,.62))
    create('Compact',.86,1.18,.74,.83,(.55,.16,.08))
    (repo/'Assets/Models/Animation/Humanoids/NOTICE.md').write_text('Original procedural geometry, rig, morphs and animation created for Graphene.\nGenerated by `tools/assets/create_animation_assets.py`; distributed under the project MIT license.\nNo third-party model or motion data is included.\n')
