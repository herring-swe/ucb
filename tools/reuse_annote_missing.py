#!/usr/bin/env python3
# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable, List, Set

from reuse.project import Project

# Define your default license and copyright
DEFAULT_LICENSE = "MIT"
DEFAULT_COPYRIGHT = "Åke Svedin <ake@svedin.org>"

# Might be needed to force a style. Shouldn't matter since we use
# commented templates...
COMMENT_STYLE_AUTO = ""
COMMENT_STYLE_C = "c"
COMMENT_STYLE_HASH = "python"


# Template and regex patters for matching
class Template:
    def __init__(
        self,
        name: str,
        template_name: str,
        patterns: List[re.Pattern],
        style: str = COMMENT_STYLE_AUTO,
    ):
        self.name = name
        self.template_name = template_name
        self.patterns = patterns
        self.style = style

    def exists(self, root: Path) -> bool:
        template_file = (
            root / ".reuse" / "templates" / f"{self.template_name}.commented.jinja2"
        )
        return template_file.is_file()

    def matches_filename(self, filename: str) -> bool:
        return any(pattern.match(filename) for pattern in self.patterns)


# Using case sensitive, no uppercase files should exist in the project
RE_C_FILES = re.compile(r"^.*\.(h|c|cpp|hpp|cxx|hxx|cc|hh|c\+\+|h\+\+|cppm)$")
RE_SCRIPT_FILES = re.compile(r"^.*\.(py|sh)$")
RE_CMAKE_FILES = re.compile(r"^(.*\.cmake|CMakeLists\.txt)$")
TEMPLATES = [
    Template("C/C++ code", "cstyle", [RE_C_FILES]),
    Template("Script files", "hash", [RE_SCRIPT_FILES]),
    Template("CMake files", "hash", [RE_CMAKE_FILES]),
]


def annotate(cwd: Path | str, files: Iterable[str], template: Template | None = None):
    if not files:
        return

    cmd: list[str] = [
        sys.executable,
        "-m",
        # Use the local reuse
        "reuse",
        "annotate",
        "-c",
        DEFAULT_COPYRIGHT,
        "-l",
        DEFAULT_LICENSE,
        "--skip-existing",
        "--no-replace",
    ]
    if template and template.template_name:
        cmd.extend(["--template", template.template_name])
    if template and template.style:
        cmd.extend(["--style", template.style])
    else:
        cmd.append("--skip-unrecognized")
    cmd.extend(files)

    print(f"Running command: {' '.join(cmd)}")
    subprocess.run(cmd, check=True, cwd=cwd)
    print()


def get_reuse_files(root: Path) -> Set[Path]:
    project = Project.from_directory(root)

    files: Set[Path] = set()
    for file in project.all_files():
        info = project.reuse_info_of(file)
        if info is None or not any(i.contains_info() for i in info):
            files.add(file)
    return files


def main():
    root = Path(__file__).parents[1]
    toml = root / "REUSE.toml"

    if not toml.exists():
        print("Could not determine root directory")
        exit(1)

    for template in TEMPLATES:
        if not template.exists(root):
            print(f"Could not find template file {template.name}")
            exit(1)

    print(f"Discovered templates: {', '.join(t.name for t in TEMPLATES)}")
    print()

    reuse_files = get_reuse_files(root)
    if not reuse_files:
        print("No files completely lack annotations")
        print("This tool won't replace partial or invalid annotations")
        print("If 'reuse lint' fails, you need to manualy fix the files")
        exit(0)

    for template in TEMPLATES:
        # Iterate and fetch matching files, remove from reuse_files
        check_files = []
        for file in reuse_files.copy():
            if template.matches_filename(file.name):
                check_files.append(str(file.relative_to(root)))
                reuse_files.remove(file)
        if not check_files:
            continue

        print(f"Found files to annotate with {template.name} template")
        for file in check_files:
            print(f"  {file}")
        print()

        annotate(root, check_files, template=template)

    if reuse_files:
        print("Files without templates:")
        for file in reuse_files:
            print(f"  {file}")
        print()


if __name__ == "__main__":
    main()
