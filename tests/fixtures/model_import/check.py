#!/usr/bin/env python3
"""Run against the native_model_import.c probe (optionally ASan/UBSan)."""
from pathlib import Path
import argparse,json,struct,subprocess,tempfile,copy
parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True);args=parser.parse_args();probe=str(Path(args.probe).resolve())
with tempfile.TemporaryDirectory(prefix='graphene-model-formats-') as directory:
 root=Path(directory);blob=struct.pack('<9f',0,0,0,1,0,0,0,1,0)
 doc={'asset':{'version':'2.0'},'buffers':[{'byteLength':len(blob)}],'bufferViews':[{'buffer':0,'byteLength':len(blob)}],'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3'}],'meshes':[{'primitives':[{'attributes':{'POSITION':0}}]}],'nodes':[{'translation':[2,0,0],'children':[1]},{'mesh':0,'translation':[0,3,0],'scale':[-2,3,1]}],'scenes':[{'nodes':[0]}],'scene':0}
 def glb(name,d):
  js=json.dumps(d,separators=(',',':')).encode();js+=b' '*((-len(js))%4)
  binary=blob+b'\0'*((-len(blob))%4)
  data=struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(binary))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary
  path=root/name;path.write_bytes(data);return path
 def run(path,valid):
  r=subprocess.run([probe,str(path)],text=True,capture_output=True)
  assert 'Sanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
  assert r.returncode==(0 if valid else 1),(path.name,r.returncode,r.stdout,r.stderr)
  if valid:
   result=json.loads(r.stdout);assert result['triangles']==1 and result['bounds_min']==[0,3,0] and result['bounds_max']==[2,6,0],result
 run(glb('mirrored.glb',doc),True)
 d=copy.deepcopy(doc);d['buffers'][0]['uri']='mesh.bin';(root/'mesh.bin').write_bytes(blob);path=root/'external.gltf';path.write_text(json.dumps(d));run(path,True)
 for name,change in [('empty',{'count':0}),('overflow',{'count':18446744073709551615}),('offset',{'byteOffset':18446744073709551615})]:
  d=copy.deepcopy(doc);d['accessors'][0].update(change);run(glb(name+'.glb',d),False)
 d=copy.deepcopy(doc);d['nodes'][1]['children']=[0];run(glb('cycle.glb',d),False)
 d=copy.deepcopy(doc);d['extensionsRequired']=['KHR_draco_mesh_compression'];run(glb('required-extension.glb',d),False)
 print('7 model parser cases passed: world transforms, mirrored winding, external buffers, malformed layouts and cycles')
