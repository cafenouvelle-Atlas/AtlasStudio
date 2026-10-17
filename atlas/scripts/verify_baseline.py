"""Reject application/profile changes before the baseline build gate. AGPL-3.0."""
from pathlib import Path
import json
import subprocess

root = Path(__file__).resolve().parents[2]
lock = json.loads((root / "atlas/upstream.json").read_text())
base = lock["commit"]
subprocess.run(["git", "cat-file", "-e", base + "^{commit}"], cwd=root, check=True)
paths = ["src", "resources", "deps", "lib", "cmake", "tests", "CMakeLists.txt",
         "version.inc", "build_win.bat", "LICENSE"]
subprocess.run(["git", "diff", "--exit-code", base, "--", *paths], cwd=root, check=True)
print("PASS: upstream application, profiles, dependencies and license unchanged at " + base)
