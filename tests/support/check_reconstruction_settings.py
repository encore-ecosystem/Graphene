#!/usr/bin/env python3
"""Run small, actual-source settings/scheduling fixtures without building the editor."""
from pathlib import Path
import argparse, shutil, subprocess
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--compiler',default='encore');args=p.parse_args()
def run(argv,**kwargs):subprocess.run([str(x) for x in argv],check=True,**kwargs)
work=root/'target/reconstruction/settings-fixture';src=work/'src';(src/'scene').mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'src/render_settings.enq',src/'render_settings.enq')
shutil.copy2(root/'src/scene/json_value.enq',src/'scene/json_value.enq')
(src/'main.enq').write_text((root/'tests/reconstruction_settings.enq').read_text().replace('import graphene::','import mod::render_settings::'))
shutil.copy2(root/'native/json_number.c',work/'json_number.c')
shutil.copy2(root/'build.enq',work/'build.enq')
(work/'runtime.c').write_text('#define _GNU_SOURCE\n#include <stddef.h>\ntypedef struct {size_t ref_count,len;char data[];} encore_str_object;\ntypedef struct {encore_str_object *object;} encore_str;\n#include "json_number.c"\n')
(work/'encore.toml').write_text('[project]\nname="reconstruction-settings-fixture"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","index@linalg@0.0.1"]\n')
shutil.copy2(root/'encore.lock',work/'encore.lock')
run([args.compiler,'run'],cwd=work)
print('Settings codecs, persistence, unknown keys, capability fallbacks: PASS')
work=root/'target/reconstruction/schedule-fixture';src=work/'src';src.mkdir(parents=True,exist_ok=True)
shutil.copy2(root.parent/'Vulkan/src/presentation_schedule.enq',src/'schedule.enq')
(src/'main.enq').write_text((root.parent/'Vulkan/tests/presentation_schedule.enq').read_text().replace('import vulkan::','import mod::schedule::'))
(work/'encore.toml').write_text('[project]\nname="presentation-schedule-fixture"\nversion="0.0.0"\n')
run([args.compiler,'run'],cwd=work)
print('Bounded presentation schedule and successful-submit accounting: PASS')
