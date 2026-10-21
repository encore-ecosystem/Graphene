#!/usr/bin/env python3
"""Check staged Git blobs, or the files a normal `git add -A` would include."""
import argparse
import math
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MIB = 1024 * 1024


def git(*args, data=None):
    return subprocess.check_output(["git", *args], cwd=ROOT, input=data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worktree", action="store_true", help="check the entire prospective source tree without staging it")
    parser.add_argument("--max-file-mib", type=float, default=10)
    parser.add_argument("--max-total-mib", type=float, default=50)
    args = parser.parse_args()
    if any(not math.isfinite(limit) or limit <= 0 for limit in (args.max_file_mib, args.max_total_mib)):
        parser.error("size limits must be finite and positive")
    if args.worktree:
        names = set(git("ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0")) - {b""}
        rows = []
        for name in names:
            path = ROOT / os.fsdecode(name)
            try:
                if path.is_file() or path.is_symlink():
                    rows.append((path.lstat().st_size, name))
            except FileNotFoundError:
                continue
    else:
        names = set(git("diff", "--cached", "--name-only", "--diff-filter=ACMRT", "-z").split(b"\0")) - {b""}
        entries = []
        for entry in git("ls-files", "--stage", "-z").split(b"\0"):
            if not entry:
                continue
            meta, name = entry.split(b"\t", 1)
            mode, oid, stage = meta.split()
            if name in names and stage == b"0" and mode != b"160000":
                entries.append((oid, name))
        sizes = git("cat-file", "--batch-check=%(objectsize)", data=b"".join(oid + b"\n" for oid, _ in entries)).splitlines()
        rows = [(int(size), name) for size, (_, name) in zip(sizes, entries)]
    failed = False
    total = sum(size for size, _ in rows)
    for size, name in sorted(rows, reverse=True):
        if size > args.max_file_mib * MIB:
            print(f"Too large: {size / MIB:.2f} MiB  {os.fsdecode(name)!r}")
            failed = True
    # --no-index also detects accidentally force-added ignored files.
    if rows:
        result = subprocess.run(
            ["git", "check-ignore", "--no-index", "--stdin", "-z"], cwd=ROOT,
            input=b"\0".join(name for _, name in rows) + b"\0", capture_output=True,
        )
        if result.returncode not in (0, 1):
            sys.stderr.buffer.write(result.stderr)
            return 2
        for name in result.stdout.split(b"\0"):
            if name:
                print(f"Ignored local artifact included: {os.fsdecode(name)!r}")
                failed = True
    if total > args.max_total_mib * MIB:
        print(f"Payload exceeds total limit: {total / MIB:.2f} > {args.max_total_mib:g} MiB")
        failed = True
    label = "Prospective source tree" if args.worktree else "Staged changes"
    print(f"{label}: {len(rows)} files, {total / MIB:.2f} MiB; limits {args.max_file_mib:g} MiB/file, {args.max_total_mib:g} MiB total")
    for size, name in sorted(rows, reverse=True)[:5]:
        print(f"  {size / MIB:.2f} MiB  {os.fsdecode(name)!r}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
