#!/usr/bin/env python3

import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List, Tuple, Dict
from collections import defaultdict

class QrcValidator:
    DEFAULT_QRC_FILE = "toonz.qrc"

    def __init__(self):
        self.base_dir = os.getcwd()
        self.qrc_path = os.path.join(self.base_dir, self.DEFAULT_QRC_FILE)
        self.missing_files: List[str] = []
        self.invalid_paths: List[Tuple[str, str]] = []
        self.file_types: Dict[str, int] = defaultdict(int)
        self.total_files = 0

    def validate_paths(self) -> bool:
        try:
            if not os.path.isfile(self.qrc_path):
                print(f"Error: QRC file not found at {self.qrc_path}")
                print("Please run this script from the toonz directory containing toonz.qrc")
                return False

            tree = ET.parse(self.qrc_path)
            root = tree.getroot()

            print(f"\nValidating all file paths...")
            print(f"Working directory: {self.base_dir}")
            print(f"QRC file: {self.qrc_path}")

            for file_elem in root.findall(".//file"):
                file_path = file_elem.text
                if file_path:
                    self.total_files += 1
                    extension = os.path.splitext(file_path)[1].lower()
                    self.file_types[extension] += 1
                    self._validate_path(file_path)

            if self.total_files == 0:
                print(f"\nWarning: No files found in {self.qrc_path}")
                return True

            return len(self.missing_files) == 0 and len(self.invalid_paths) == 0

        except ET.ParseError as e:
            print(f"\nError: Failed to parse {self.qrc_path}")
            print(f"XML Parse Error: {str(e)}")
            return False
        except Exception as e:
            print(f"\nError: An unexpected error occurred while processing {self.qrc_path}")
            print(f"Error: {str(e)}")
            return False

    def _validate_path(self, file_path: str) -> None:
        if '//' in file_path:
            self.invalid_paths.append((file_path, "Contains double slashes"))
            return

        file_path = file_path.lstrip('/').replace('/', os.sep)
        full_path = os.path.join(self.base_dir, file_path)
        
        if not os.path.isfile(full_path) and not self.missing_files:
            print("\nDebug info for first missing file:")
            print(f"File path from QRC: {file_path}")
            print(f"Full path: {full_path}")
            print(f"Exists: {os.path.isfile(full_path)}")
            
            parent_dir = os.path.dirname(full_path)
            print(f"Parent directory ({parent_dir}) exists: {os.path.isdir(parent_dir)}")
            
            if os.path.isdir(parent_dir):
                print(f"\nContents of {parent_dir}:")
                for item in sorted(os.listdir(parent_dir)):
                    print(f"  - {item}")

        if not os.path.isfile(full_path):
            self.missing_files.append(file_path)

    def print_report(self) -> None:
        print("\nQRC Validation Report")
        print("===================")
        print(f"QRC File: {self.qrc_path}")
        
        print(f"\nFile Statistics:")
        print(f"Total files referenced: {self.total_files}")
        print("\nBreakdown by file type:")
        for ext, count in sorted(self.file_types.items()):
            print(f"  {ext or '(no extension)'}: {count} files")
        
        if not self.missing_files and not self.invalid_paths:
            print("\nStatus: All file paths are valid ✓")
            return

        if self.missing_files:
            print(f"\nMissing Files ({len(self.missing_files)}):")
            # Group missing files by extension for easier review
            by_extension = defaultdict(list)
            for file_path in self.missing_files:
                ext = os.path.splitext(file_path)[1].lower() or '(no extension)'
                by_extension[ext].append(file_path)
            
            for ext in sorted(by_extension.keys()):
                print(f"\n  {ext}:")
                for file_path in sorted(by_extension[ext]):
                    print(f"    - {file_path}")

        if self.invalid_paths:
            print(f"\nInvalid Paths ({len(self.invalid_paths)}):")
            for path, reason in sorted(self.invalid_paths):
                print(f"  - {path} ({reason})")

def main():
    validator = QrcValidator()
    if validator.validate_paths():
        validator.print_report()
        sys.exit(0)
    else:
        validator.print_report()
        sys.exit(1)

if __name__ == "__main__":
    main()