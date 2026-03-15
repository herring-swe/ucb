#!/usr/bin/env python3
# This file is part of the UCB project
# SPDX-FileCopyrightText: 2026 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import argparse
import platform
import subprocess
import sys
from enum import Enum
from pathlib import Path
from typing import Iterable, List, Sequence

C_SUFFIXES = {".c", ".cpp", ".h", ".hpp"}
C_DIRS = sorted(["src", "include", "tests", "!tests/doctest"])

PYTHON_SUFFIXES = {".py"}
PYTHON_DIRS = sorted(["tools"])


class FormatAction(Enum):
    CHECK = "check"
    FIX = "fix"
    LIST = "list"


class FormatType(Enum):
    ALL = "all"
    C = "c"
    PYTHON = "python"


def get_clang_format() -> str:
    my_dir = Path(__file__).parent
    if platform.system() == "Windows":
        binary = my_dir / "clang-format" / "clang-format.exe"
    elif platform.system() == "Darwin":
        binary = my_dir / "clang-format" / "clang-format-mac"
    else:
        binary = my_dir / "clang-format" / "clang-format-linux"

    if not binary.exists():
        print(f"clang-format binary not found at {binary}")
        sys.exit(1)

    return str(binary.resolve())


def find_project_root() -> Path:
    root_dir = Path(__file__).resolve().parent
    markers = [".clang-format", "CMakeLists.txt"]
    while root_dir != root_dir.parent:
        if all((root_dir / marker).exists() for marker in markers):
            return root_dir
        root_dir = root_dir.parent
    print("Could not determine project root")
    sys.exit(1)


def _resolve_paths(root_dir: Path, raw_entries: Sequence[str], label: str) -> List[Path]:
    resolved: List[Path] = []
    for entry in raw_entries:
        path = Path(entry)
        full = (root_dir / path).resolve() if not path.is_absolute() else path.resolve()
        if not full.exists():
            print(f"Skipping missing {label} path: {entry}")
            continue
        if root_dir not in [full, *full.parents]:
            print(f"Skipping {label} path outside repository root: {entry}")
            continue
        resolved.append(full)
    return resolved


def _resolve_dir_rules(root_dir: Path, dirs: Sequence[str]) -> tuple[List[Path], List[Path]]:
    include_entries: List[str] = []
    exclude_entries: List[str] = []

    for entry in dirs:
        if entry.startswith("!"):
            exclude_entries.append(entry[1:])
        else:
            include_entries.append(entry)

    includes = _resolve_paths(root_dir, include_entries, "include")
    excludes = _resolve_paths(root_dir, exclude_entries, "exclude")
    return includes, excludes


def _relative_posix(root_dir: Path, path: Path) -> str:
    return path.relative_to(root_dir).as_posix()


def _discover_files_with_git(
    root_dir: Path,
    dirs: Sequence[Path],
    suffixes: Iterable[str],
    exclude_dirs: Sequence[Path] = (),
) -> List[Path]:
    relative_dirs = [_relative_posix(root_dir, directory) for directory in dirs]
    cmd = [
        "git",
        "-C",
        str(root_dir),
        "ls-files",
        "-z",
        "--cached",
        "--others",
        "--exclude-standard",
        "--",
        *relative_dirs,
    ]
    completed = subprocess.run(cmd, capture_output=True, check=False)
    if completed.returncode != 0:
        stderr = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(stderr or "git ls-files failed")

    valid_suffixes = {suffix.lower() for suffix in suffixes}
    resolved_excludes = tuple(exclude_dirs)
    files: List[Path] = []
    for rel in completed.stdout.decode("utf-8", errors="replace").split("\0"):
        if not rel:
            continue
        path = root_dir / rel
        if any(path == exclude or exclude in path.parents for exclude in resolved_excludes):
            continue
        if path.suffix.lower() in valid_suffixes:
            files.append(path)
    return sorted(set(files))


def discover_files(
    root_dir: Path,
    dirs: Sequence[Path],
    suffixes: Iterable[str],
    exclude_dirs: Sequence[Path] = (),
) -> List[Path]:
    try:
        return _discover_files_with_git(root_dir, dirs, suffixes, exclude_dirs)
    except Exception as exc:
        print(f"git-based discovery failed: {exc}")
        exit(1)


def run_format_c(files: Sequence[Path], fix: bool) -> bool:
    c_files = [str(file) for file in files if file.suffix.lower() in C_SUFFIXES]
    if not c_files:
        print("No C/C++ files to format")
        return True

    binary = get_clang_format()
    print(f"Using clang-format binary at: {binary}")

    cmd = [binary]
    if fix:
        cmd.append("-i")
    else:
        cmd.extend(["--dry-run", "--Werror"])
    cmd.append("--style=file")
    cmd.extend(str(path) for path in c_files)

    print(f"Running clang-format on {len(c_files)} files...")
    completed = subprocess.run(cmd, check=False)
    return completed.returncode == 0


def run_format_python(root_dir: Path, fix: bool) -> bool:
    python_dirs, _python_excludes = _resolve_dir_rules(root_dir, PYTHON_DIRS)
    if not python_dirs:
        print("No Python directories configured")
        return True

    targets = [_relative_posix(root_dir, directory) for directory in python_dirs]

    ruff_cmd = [sys.executable, "-m", "ruff", "check"]
    if fix:
        ruff_cmd.append("--fix")
    ruff_cmd.extend(targets)

    mypy_cmd = [sys.executable, "-m", "mypy", *targets]

    print("\nRunning ruff...")
    ruff_ok = subprocess.run(ruff_cmd, check=False, cwd=root_dir).returncode == 0
    print("\nRunning mypy...")
    mypy_ok = subprocess.run(mypy_cmd, check=False, cwd=root_dir).returncode == 0
    return ruff_ok and mypy_ok


def discover_files_by_type(
    root_dir: Path, format_type: FormatType
) -> tuple[List[Path], List[Path]]:
    c_files: List[Path] = []
    python_files: List[Path] = []

    if format_type in (FormatType.ALL, FormatType.C):
        c_dirs, c_excludes = _resolve_dir_rules(root_dir, C_DIRS)
        if c_dirs:
            c_files = discover_files(root_dir, c_dirs, C_SUFFIXES, exclude_dirs=c_excludes)

    if format_type in (FormatType.ALL, FormatType.PYTHON):
        python_dirs, python_excludes = _resolve_dir_rules(root_dir, PYTHON_DIRS)
        if python_dirs:
            python_files = discover_files(
                root_dir, python_dirs, PYTHON_SUFFIXES, exclude_dirs=python_excludes
            )

    return c_files, python_files


def list_files_grouped(
    root_dir: Path, c_files: Sequence[Path], python_files: Sequence[Path]
) -> None:
    print("Files grouped by formatter:")
    print("clang-format:")
    if c_files:
        for file in c_files:
            print(f"  - {_relative_posix(root_dir, file)}")
    else:
        print("  (none)")

    print("python formatter:")
    if python_files:
        for file in python_files:
            print(f"  - {_relative_posix(root_dir, file)}")
    else:
        print("  (none)")


def format_code(action: FormatAction, format_type: FormatType = FormatType.ALL) -> bool:
    root_dir = find_project_root()

    c_files, python_files = discover_files_by_type(root_dir, format_type)
    if not c_files and not python_files:
        print("No matching files found")
        return True

    print(f"Action: {action.value}")
    print(f"Format type: {format_type.value}")

    if action == FormatAction.LIST:
        list_files_grouped(root_dir, c_files, python_files)
        return True

    print(f"Project root: {root_dir}")
    if format_type in (FormatType.ALL, FormatType.C):
        c_dirs, c_excludes = _resolve_dir_rules(root_dir, C_DIRS)
        print("C/C++ directories:")
        for directory in c_dirs:
            print(f"  - {_relative_posix(root_dir, directory)}")
        if c_excludes:
            print("C/C++ excluded directories:")
            for directory in c_excludes:
                print(f"  - {_relative_posix(root_dir, directory)}")
    if format_type in (FormatType.ALL, FormatType.PYTHON):
        python_dirs, python_excludes = _resolve_dir_rules(root_dir, PYTHON_DIRS)
        print("Python directories:")
        for directory in python_dirs:
            print(f"  - {_relative_posix(root_dir, directory)}")
        if python_excludes:
            print("Python excluded directories:")
            for directory in python_excludes:
                print(f"  - {_relative_posix(root_dir, directory)}")

    fix = action == FormatAction.FIX
    ok = True
    if format_type in (FormatType.ALL, FormatType.C):
        if not run_format_c(c_files, fix):
            print("clang-format check failed for C/C++ files")
            ok = False
    if format_type in (FormatType.ALL, FormatType.PYTHON):
        if not run_format_python(root_dir, fix):
            print("Python checks failed")
            ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="Run clang-format for project files")
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument(
        "--list", action="store_true", help="List files that would be formatted"
    )
    mode_group.add_argument("--fix", action="store_true", help="Apply formatting in-place")
    parser.add_argument(
        "--type",
        default=FormatType.ALL.value,
        choices=sorted([ft.value for ft in FormatType]),
        help="File types to include (default: %(default)s)",
    )
    args = parser.parse_args()

    action = FormatAction.CHECK
    if args.list:
        action = FormatAction.LIST
    elif args.fix:
        action = FormatAction.FIX

    if format_code(action, format_type=FormatType(args.type)):
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
