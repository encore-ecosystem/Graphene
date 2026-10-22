#!/usr/bin/env python3
"""Build the actual command dispatcher with a small entry point, excluding renderer Encore modules."""
from pathlib import Path
import argparse, shutil, subprocess, tomllib, json
root=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--compiler',default='encore');args=p.parse_args()
work=root/'target/mcp/gateway-fixture';src=work/'src';src.mkdir(parents=True,exist_ok=True)
# Keep only the real transitive command modules. Unrelated renderer modules
# contain self-library imports and are deliberately excluded from this fixture.
import re
shutil.rmtree(src);src.mkdir(parents=True)
todo=['mcp_gateway.enq','automation.enq','command_bus.enq','scene/world.enq','scene/transform.enq','mcp/protocol.enq'];seen=set()
while todo:
 relative=todo.pop()
 if relative in seen:continue
 seen.add(relative);original=root/'src'/relative
 destination=src/relative;destination.parent.mkdir(parents=True,exist_ok=True)
 destination.write_text(original.read_text().replace('graphene::','refrain::'))
 for match in re.finditer(r'\bimport\s+([\w:]+)',original.read_text()):
  value=match.group(1)
  if value.startswith('mod::'):base=original.parent;parts=value[5:].strip(':').split('::')
  elif value.startswith('refrain::'):base=root/'src';parts=value[9:].strip(':').split('::')
  elif value.startswith('graphene::'):base=root/'src';parts=value[10:].strip(':').split('::')
  else:continue
  while parts:
   candidates=[base.joinpath(*parts).with_suffix('.enq'),base.joinpath(*parts,'mod.enq')]
   found=next((candidate for candidate in candidates if candidate.exists()),None)
   if found:todo.append(str(found.relative_to(root/'src')));break
   parts.pop()
(src/'main.enq').write_text((root/'tests/mcp_commands.enq').read_text())
manifest=tomllib.loads((root/'encore.toml').read_text())['project']
deps=[('path@../../../../'+x[len('path@../'):]) if x.startswith('path@../') else x for x in manifest['dependencies']]
(work/'encore.toml').write_text('[project]\nname="mcp-command-fixture"\nversion="0.0.0"\nbuild="build.enq"\ndependencies='+json.dumps(deps)+'\n')
(work/'runtime.c').write_text('#define _GNU_SOURCE\n#include '+json.dumps(str(root/'runtime.c'))+'\n')
shutil.copy2(root/'build.enq',work/'build.enq');shutil.copy2(root/'encore.lock',work/'encore.lock')
subprocess.run([args.compiler,'run'],cwd=work,check=True)
print('PASS: production gateway, 1000 actors, paging, trusted edits, component settings, batch rollback, Undo/Redo')
