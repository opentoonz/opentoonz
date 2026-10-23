import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("enable", Path(__file__).with_name("enable.py"))
enable = importlib.util.module_from_spec(spec)
spec.loader.exec_module(enable)


class InjectionTest(unittest.TestCase):
    def make_checkout(self, root):
        target = root / "toonz/sources/toonz"
        target.mkdir(parents=True)
        source = target / "main.cpp"
        source.write_text('#include "mainwindow.h"\n  int ret = a.exec();', encoding="utf-8")
        cmake = target / "CMakeLists.txt"
        cmake.write_text("# existing build\n", encoding="utf-8")
        return source, cmake

    def make_external_recorder(self, root):
        recorder = root / "infrastructure ${not_a_variable} ]=] with spaces"
        recorder.mkdir()
        (recorder / "otdevrecorder.cpp").write_text("// recorder source\n", encoding="utf-8")
        (recorder / "otdevrecorder.h").write_text("// recorder header\n", encoding="utf-8")
        return recorder

    def test_inject_once_and_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, cmake = self.make_checkout(root)
            enable.inject(root, "test-build-123")
            result = source.read_text(encoding="utf-8")
            cmake_result = cmake.read_text(encoding="utf-8")
            self.assertEqual(result.count("new OtDevRecorder("), 1)
            self.assertIn('filePath("portablestuff")', result)
            self.assertIn('TFilePath("preferences.ini")', result)
            self.assertIn('filePath("otdev-recorder/ffmpeg.exe")', result)
            with self.assertRaises(RuntimeError):
                enable.inject(root, "test-build-123")
            self.assertEqual(result, source.read_text(encoding="utf-8"))
            self.assertEqual(cmake_result, cmake.read_text(encoding="utf-8"))

    def test_external_sources_do_not_require_recorder_in_pr_checkout(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "clean-pr"
            source, cmake = self.make_checkout(checkout)
            recorder = self.make_external_recorder(root)
            enable.inject(checkout, "external-build-123", recorder)
            self.assertFalse((checkout / ".github").exists())
            self.assertIn("new OtDevRecorder(", source.read_text(encoding="utf-8"))
            result = cmake.read_text(encoding="utf-8")
            self.assertIn((recorder / "otdevrecorder.cpp").resolve().as_posix(), result)
            self.assertNotIn("CMAKE_SOURCE_DIR", result)

    @unittest.skipUnless(shutil.which("cmake"), "CMake is needed to parse literal source paths")
    def test_cmake_receives_external_paths_without_expansion(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "clean-pr"
            _, cmake = self.make_checkout(checkout)
            recorder = self.make_external_recorder(root)
            enable.inject(checkout, "external-build-123", recorder)
            harness = root / "parse.cmake"
            harness.write_text('''
set(WIN32 TRUE)
function(target_sources target visibility path)
  if(NOT path STREQUAL "$ENV{OTDEV_EXPECTED_SOURCE}" OR NOT EXISTS "${path}")
    message(FATAL_ERROR "Injected source path was expanded or split")
  endif()
  set(source_checked TRUE PARENT_SCOPE)
endfunction()
function(target_include_directories target visibility path)
  if(NOT path STREQUAL "$ENV{OTDEV_EXPECTED_INCLUDE}" OR NOT IS_DIRECTORY "${path}")
    message(FATAL_ERROR "Injected include path was expanded or split")
  endif()
  set(include_checked TRUE PARENT_SCOPE)
endfunction()
function(target_link_libraries)
endfunction()
include("$ENV{OTDEV_INJECTED_CMAKE}")
if(NOT source_checked OR NOT include_checked)
  message(FATAL_ERROR "Recorder sources and include path were not configured")
endif()
''', encoding="utf-8")
            environment = os.environ.copy()
            environment.update({
                "OTDEV_EXPECTED_SOURCE": (recorder / "otdevrecorder.cpp").resolve().as_posix(),
                "OTDEV_EXPECTED_INCLUDE": recorder.resolve().as_posix(),
                "OTDEV_INJECTED_CMAKE": cmake.resolve().as_posix(),
            })
            result = subprocess.run([shutil.which("cmake"), "-P", str(harness)],
                                    env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_incomplete_external_sources_leave_checkout_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, cmake = self.make_checkout(root / "clean-pr")
            original = source.read_bytes(), cmake.read_bytes()
            recorder = self.make_external_recorder(root)
            (recorder / "otdevrecorder.h").unlink()
            with self.assertRaises(FileNotFoundError):
                enable.inject(root / "clean-pr", "test-build-123", recorder)
            self.assertEqual(original, (source.read_bytes(), cmake.read_bytes()))

    def test_missing_cmake_leaves_source_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, cmake = self.make_checkout(root)
            original = source.read_bytes()
            cmake.unlink()
            with self.assertRaises(FileNotFoundError):
                enable.inject(root, "test-build-123")
            self.assertEqual(original, source.read_bytes())
            self.assertFalse(cmake.exists())

    def test_partial_previous_injection_is_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, cmake = self.make_checkout(root)
            cmake.write_text('target_sources(OpenToonz PRIVATE otdevrecorder.cpp)\n', encoding="utf-8")
            original = source.read_bytes(), cmake.read_bytes()
            with self.assertRaises(RuntimeError):
                enable.inject(root, "test-build-123")
            self.assertEqual(original, (source.read_bytes(), cmake.read_bytes()))

    def test_bad_identity_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                enable.inject(Path(temporary), 'bad"identity')


if __name__ == "__main__":
    unittest.main()
