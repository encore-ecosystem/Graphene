#!/usr/bin/env python3
"""Bundle the optional, pinned Linux reconstruction SDK runtime next to a binary."""
from pathlib import Path
import argparse, json, os, shutil, tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('binary',type=Path);p.add_argument('--runtime',type=Path,default=root/'target/reconstruction/runtime');args=p.parse_args()
source=args.runtime.resolve();destination=args.binary.resolve().parent
manifest=json.loads((source/'sdk-versions.json').read_text())
if manifest['dlss']['tag']!='v310.9.16' or manifest['fidelityfx']['tag']!='v1.1.4':raise SystemExit('Unexpected SDK bundle versions')
files=['libencore_reconstruction.so','libnvidia-ngx-dlss.so.310.9.16','libnvidia-ngx-dlssg.so.310.9.16','sdk-versions.json','dlss-LICENSE.txt','fidelityfx-LICENSE.txt','vulkan-headers-LICENSE.txt']
for name in files:
 if not (source/name).is_file():raise SystemExit('Missing SDK artifact/license: '+str(source/name))
destination.mkdir(parents=True,exist_ok=True)
for feature in ['dlss','dlssg']:
 link=destination/f'libnvidia-ngx-{feature}.so'
 if link.exists() and not link.is_symlink():raise SystemExit('Refusing to replace a non-symlink: '+str(link))
# Never truncate a shared library that a running editor has mapped. Prepare on
# the destination filesystem, then replace directory entries; existing mappings
# retain their original inodes. Symlinks are replaced without an unlink gap too.
with tempfile.TemporaryDirectory(prefix='.reconstruction-',dir=destination) as temporary:
 staging=Path(temporary)
 for name in files:
  if (source/name).resolve()!=(destination/name).resolve():shutil.copy2(source/name,staging/name)
 for feature in ['dlss','dlssg']:
  (staging/f'libnvidia-ngx-{feature}.so').symlink_to(f'libnvidia-ngx-{feature}.so.310.9.16')
 for artifact in staging.iterdir():os.replace(artifact,destination/artifact.name)
print('Reconstruction runtime bundled in',destination)
