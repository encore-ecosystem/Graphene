#!/usr/bin/env python3
"""Bundle the pinned optional HRTF SDK next to a Graphene executable."""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
binary = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'target/release/graphene'
sdk = root.parent / 'Audio/native/third_party/steamaudio'
destination = binary.parent / 'lib'
destination.mkdir(parents=True, exist_ok=True)
shutil.copy2(sdk / 'libphonon.so', destination / 'libphonon.so')
shutil.copy2(sdk / 'LICENSE.md', destination / 'SteamAudio-LICENSE.md')
print(destination / 'libphonon.so')
