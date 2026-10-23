"""CI-only source injection. Normal checkouts/builds never include the recorder."""

import argparse
import os
from pathlib import Path


def cmake_path(path: Path):
    # Bracket arguments keep spaces and ${...} in external CI paths literal.
    value = path.as_posix()
    delimiter = "="
    while f"]{delimiter}]" in value:
        delimiter += "="
    return f"[{delimiter}[{value}]{delimiter}]"


def inject(root: Path, build_id: str, recorder_dir=None):
    if not build_id or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_" for c in build_id):
        raise ValueError("Invalid CI build identity")
    recorder_dir = (Path(recorder_dir) if recorder_dir is not None
                    else Path(__file__).parent).resolve(strict=True)
    for name in ("otdevrecorder.cpp", "otdevrecorder.h"):
        if not (recorder_dir / name).is_file():
            raise FileNotFoundError(f"Recorder source not found: {recorder_dir / name}")
    source = root / "toonz/sources/toonz/main.cpp"
    cmake = root / "toonz/sources/toonz/CMakeLists.txt"
    text = source.read_text(encoding="utf-8")
    cmake_text = cmake.read_text(encoding="utf-8")
    anchor = "  int ret = a.exec();"
    include = '#include "mainwindow.h"'
    if (text.count(anchor) != 1 or text.count(include) != 1
            or "otdevrecorder" in text.lower()
            or "otdevrecorder" in cmake_text.lower()):
        raise RuntimeError("Recorder injection anchors changed or already patched")
    hook = f'''  // OT-Dev Windows CI only; not present in normal source builds.
  auto *otDevRecorder = new OtDevRecorder(
      &w, (ToonzFolder::getMyModuleDir() + TFilePath("preferences.ini")).getQString(),
      QStringLiteral("{build_id}"),
      QDir(QCoreApplication::applicationDirPath()).filePath("portablestuff"),
      QDir(QCoreApplication::applicationDirPath()).filePath("otdev-recorder/ffmpeg.exe"));
  QTimer::singleShot(0, otDevRecorder, [otDevRecorder] {{ otDevRecorder->initialize(); }});

'''
    patched_source = (text.replace(include, include + '\n#include "otdevrecorder.h"\n#include <QTimer>')
                      .replace(anchor, hook + anchor))
    patched_cmake = cmake_text + f'''
# Injected only by .github/ot-dev/recorder/enable.py in OT-Dev Windows CI.
if(NOT WIN32)
    message(FATAL_ERROR "The OT-Dev recorder injection is Windows CI only")
endif()
target_sources(OpenToonz PRIVATE {cmake_path(recorder_dir / "otdevrecorder.cpp")})
target_include_directories(OpenToonz PRIVATE {cmake_path(recorder_dir)})
target_link_libraries(OpenToonz user32 gdi32)
'''
    source.write_text(patched_source, encoding="utf-8")
    cmake.write_text(patched_cmake, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--recorder-dir", type=Path,
                        default=Path(__file__).resolve().parent,
                        help="Directory containing recorder sources, including an external CI checkout")
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    if os.environ.get("GITHUB_REPOSITORY", "").lower() != "openanimationlibrary/opentoonz-dev" or os.environ.get("RUNNER_OS") != "Windows":
        raise SystemExit("Only the OT-Dev Windows CI workflow may inject this feature")
    inject(args.root, args.build_id, args.recorder_dir)
