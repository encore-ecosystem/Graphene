#!/usr/bin/env python3
"""Compile the isolated GenAI PBR preview, retaining small Encore wrappers."""
from pathlib import Path
import struct,subprocess
root=Path(__file__).resolve().parents[2]
out=root/'target/generated-shaders';out.mkdir(parents=True,exist_ok=True)
source='import core::vec::Vec\n\n'
for stage,name in [('vert','genai_preview_vertex_spirv'),('frag','genai_preview_fragment_spirv')]:
 binary=out/('genai_preview.'+stage+'.spv')
 subprocess.run(['glslc','-O','--target-env=vulkan1.2','-I'+str(root/'shaders'),str(root/'shaders'/('genai_preview.'+stage)),'-o',str(binary)],check=True)
 subprocess.run(['spirv-val','--target-env','vulkan1.2',str(binary)],check=True)
 data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
 source+='pub fn '+name+'() -> Vec[u32] {\n    let mut words = Vec[u32]::new()\n'
 source+=''.join('    words.push('+str(w)+'_u32)\n' for w in words)+'    ret words\n}\n'
(root/'src/shaders/genai.enq').write_text(source)
subprocess.run(['python3',str(root/'tools/shaders/pack_shader_tables.py')],check=True)
