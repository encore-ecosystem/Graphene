"""Packaging must preserve libraries mapped by an already running process."""
import json
import mmap
from pathlib import Path
import subprocess
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "tools/package/bundle_reconstruction.py"
ARTIFACTS = [
    "libencore_reconstruction.so", "libnvidia-ngx-dlss.so.310.9.16",
    "libnvidia-ngx-dlssg.so.310.9.16", "dlss-LICENSE.txt",
    "fidelityfx-LICENSE.txt", "vulkan-headers-LICENSE.txt",
]


class BundleTest(unittest.TestCase):
    def test_running_mapping_and_rejected_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output = root / "runtime", root / "application"
            source.mkdir()
            output.mkdir()
            manifest = {"dlss": {"tag": "v310.9.16"}, "fidelityfx": {"tag": "v1.1.4"}}
            (source / "sdk-versions.json").write_text(json.dumps(manifest))
            for name in ARTIFACTS:
                (source / name).write_bytes(b"original artifact")
            command = ["python", str(TOOL), str(output / "game"), "--runtime", str(source)]
            subprocess.run(command, check=True, capture_output=True)
            target = output / ARTIFACTS[0]
            with target.open("rb") as opened, mmap.mmap(opened.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
                inode = target.stat().st_ino
                (source / ARTIFACTS[0]).write_bytes(b"updated artifact")
                subprocess.run(command, check=True, capture_output=True)
                self.assertEqual(mapped[:], b"original artifact")
                self.assertEqual(target.read_bytes(), b"updated artifact")
                self.assertNotEqual(inode, target.stat().st_ino)
            link = output / "libnvidia-ngx-dlss.so"
            self.assertTrue(link.is_symlink())
            self.assertEqual(link.read_bytes(), b"original artifact")
            link.unlink()
            link.write_bytes(b"user file")
            (source / ARTIFACTS[0]).write_bytes(b"must not be copied")
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(target.read_bytes(), b"updated artifact")
            self.assertEqual(link.read_bytes(), b"user file")


if __name__ == "__main__":
    unittest.main()
