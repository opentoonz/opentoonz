import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

SCRIPT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "import_container.py"))
PY = sys.executable


def run_script(args, cwd=None):
    cmd = [PY, SCRIPT] + args
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return proc


def test_import_as_file():
    td = tempfile.mkdtemp(prefix="flare_test_as_")
    try:
        as_file = os.path.join(td, "hello.as")
        with open(as_file, "w", encoding="utf-8") as f:
            f.write("// sample ActionScript\nvar x = 1;\n")

        out = os.path.join(td, "out")
        proc = run_script(["--input", as_file, "--output", out])
        assert proc.returncode == 0, proc.stderr.decode("utf-8")
        mf = os.path.join(out, "manifest.json")
        assert os.path.exists(mf)
        with open(mf, "r", encoding="utf-8") as f:
            data = json.load(f)
        assert data["input"].endswith("hello.as")
        assert "hello.as" in data["files"]
    finally:
        shutil.rmtree(td)


def test_import_swc_with_as():
    td = tempfile.mkdtemp(prefix="flare_test_swc_")
    try:
        swc = os.path.join(td, "lib.swc")
        with zipfile.ZipFile(swc, "w") as z:
            z.writestr("library/foo.as", "// foo as")
            z.writestr("assets/img.png", "")

        out = os.path.join(td, "out_swc")
        proc = run_script(["--input", swc, "--output", out])
        assert proc.returncode == 0, proc.stderr.decode("utf-8")
        mf = os.path.join(out, "manifest.json")
        assert os.path.exists(mf)
        with open(mf, "r", encoding="utf-8") as f:
            data = json.load(f)
        assert data["input"].endswith("lib.swc")
        # should have extracted at least the .as file
        has_as = any(x.lower().endswith(".as") for x in data.get("files", []))
        assert has_as
    finally:
        shutil.rmtree(td)
