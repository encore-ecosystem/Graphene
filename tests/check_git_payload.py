"""Exercise the commit guard against a disposable Git index and working tree."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PayloadTest(unittest.TestCase):
    def test_staged_bytes_ignore_rules_and_source_meshes(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            (repo / "tools").mkdir()
            shutil.copy2(ROOT / "tools/check_git_payload.py", repo / "tools/check_git_payload.py")
            (repo / ".gitignore").write_text("target/\n")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)

            def git(*args):
                subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True)

            def guard(*args):
                return subprocess.run(["python3", "tools/check_git_payload.py", *args], cwd=repo, capture_output=True, text=True)

            mesh = repo / "small mesh.obj"
            mesh.write_text("v 0 0 0\n")
            git("add", "small mesh.obj")
            self.assertEqual(guard().returncode, 0)
            self.assertEqual(guard("--worktree").returncode, 0)

            staged = repo / "large asset.dat"
            staged.write_bytes(b"x" * 8192)
            git("add", "large asset.dat")
            staged.write_bytes(b"small working copy")
            result = guard("--max-file-mib", "0.001")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("large asset.dat", result.stdout)
            self.assertIn("Too large", result.stdout)
            # Guard must not modify the index to match the smaller working file.
            content = subprocess.check_output(["git", "show", ":large asset.dat"], cwd=repo)
            self.assertEqual(len(content), 8192)

            ignored = repo / "target/forced.txt"
            ignored.parent.mkdir()
            ignored.write_text("local artifact")
            git("add", "-f", "target/forced.txt")
            result = guard()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Ignored local artifact included", result.stdout)
            self.assertIn("target/forced.txt", result.stdout)
            self.assertNotEqual(guard("--max-total-mib", "0.001").returncode, 0)


if __name__ == "__main__":
    unittest.main()
