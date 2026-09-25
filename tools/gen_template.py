#!/usr/bin/env python3
r"""
Generate a C header and source file from a template header with preprocessor defines.
The script acts as a simplified C preprocessor with specific rules using the new py-c-preprocessor.

Capabilities:
- Simplified macro parsing
- Macro replacement
- Branch support for #if/#ifdef/#else/#endif

Rules:
- #error, #pragma, #include etc are left as-is
- Uses provided #define values to expand macros
  - Some values are predefined
- Unknown macros don't expand (left as-is)
- Comments are stripped, except:
  - When generating header and in a #(if/else/ifdef/endif) blocks
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

from py_c_preprocessor.preprocessor import (
    CommentAction,
    DirectiveAction,
    IncludeAction,
    Preprocessor,
)


def get_clang_format() -> Optional[str]:
    # Find clang in path first, then via python
    mydir = Path(__file__).parent
    binary = None
    if sys.platform == "win32":
        binary = mydir / "clang-format" / "clang-format.exe"
    else:
        binary = mydir / "clang-format" / "clang-format-linux"

    if binary.exists():
        return str(binary)
    else:
        raise FileNotFoundError(f"Could not find clang-format at {binary}")


def format_file(filename: str) -> None:
    # Find clang in path first, then via python
    clang_format = get_clang_format()

    if clang_format is None:
        return

    cmd = [clang_format, "-i", filename, "--style=file"]
    print(f"Executing: {' '.join(cmd)}")

    # Format file, using .clang-format rules
    subprocess.check_call(cmd)


def format_stream(contents: str) -> str:
    clang_format = get_clang_format()
    if clang_format is None:
        return contents

    # Format from pipe, using .clang-format rules
    p = subprocess.Popen(
        [clang_format, "--style=file"],
        universal_newlines=True,
        encoding="utf-8",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
    )
    stdout, stderr = p.communicate(contents)
    if p.returncode != 0 or stdout is None:
        print("Formatting failed", file=sys.stderr)
        if stderr:
            print(stderr, file=sys.stderr)
        return contents
    return stdout


def parse_template(
    template: str,
    defines: List[str],
    output_file: str = "",
    extra_inc: Optional[List[str]] = None,
    is_header: bool = False,
) -> None:
    pp = Preprocessor()
    # pp.ignore_missing_includes = True
    pp.include_action = IncludeAction.KEEP
    pp.pragma_action = DirectiveAction.KEEP
    pp.error_action = DirectiveAction.KEEP
    pp.comment_action = CommentAction.KEEP_IN_CONDITIONS if is_header else CommentAction.STRIP
    pp.undef_rule = lambda name: not (name.startswith("UCB") or name.startswith("_UCB"))  # type: ignore

    gen_filename = "stdout"
    if output_file:
        gen_filename = Path(output_file).name

    pp.define("_GEN_TEMPLATE_PARSER")
    pp.define("_GEN_FILENAME", gen_filename)
    pp.define("UCB_SNAKE", "a##_##b", ["a", "b"])

    for d in defines:
        if "=" in d:
            name, value = d.split("=", 1)
            pp.define(name.strip(), value.strip())
        else:
            pp.define(d.strip())
    pp.include(template)

    # print("Remain defined:")
    # for macro in pp.macros:
    #     print(f"{macro} = {pp.macros[macro]}")

    # Further pre-process comments
    # Replace @def MACRO with @fn MACRO_EXPANDED
    # Evaluate Doxygen-style @if/@else/@endif conditionals. The preprocessor
    # expands the condition macros (e.g. _UCB_T_IS_POD to 0/1) before this step.

    re_cdef = re.compile(r"(\s*\*\s*@def\s+)(\w+)(.*)")
    re_cif = re.compile(r"\s*\*\s*@if\s+(.+?)\s*$")
    re_celse = re.compile(r"\s*\*\s*@else\s*$")
    re_cendif = re.compile(r"\s*\*\s*@endif\s*$")

    t_type = pp.expand("_UCB_T")

    def branch_ignore(entry: dict) -> bool:
        if entry["parent_ignore"]:
            return True
        return entry["taken"] if entry["in_else"] else not entry["taken"]

    lines = []
    first_include = False
    cond_stack: List[dict] = []
    for _, line in enumerate(pp.source_lines):
        line = line.rstrip()

        m = re_cif.match(line)
        if m:
            parent_ignore = bool(cond_stack) and branch_ignore(cond_stack[-1])
            taken = False if parent_ignore else bool(pp.evaluate(m.group(1)))
            cond_stack.append(
                {"parent_ignore": parent_ignore, "taken": taken, "in_else": False}
            )
            continue
        if re_celse.match(line):
            if cond_stack:
                cond_stack[-1]["in_else"] = True
            continue
        if re_cendif.match(line):
            if cond_stack:
                cond_stack.pop()
            continue

        if cond_stack and branch_ignore(cond_stack[-1]):
            continue

        line = line.replace("type T", t_type)
        m = re_cdef.match(line)
        if m:
            prefix = m.group(1).replace("@def", "@fn")
            fn = pp.expand(m.group(2)) or ""
            suffix = m.group(3) or ""
            lines.append(prefix + fn + suffix)
            continue

        # Skip all
        if not line and not lines:
            continue
        if not first_include and line.startswith("#include"):
            first_include = True
            if extra_inc:
                for inc in extra_inc:
                    lines.append(f"#include {inc}")
                lines.append("")

        lines.append(line)

    if output_file:
        with open(output_file, "w", encoding="utf-8", newline="\n") as f:
            for line in lines:
                f.write(line + "\n")
        format_file(output_file)
    else:
        print(format_stream("\n".join(lines)))


def main():
    parser = argparse.ArgumentParser(
        description="Generate C header and source files from a template header."
    )
    parser.add_argument("template", help="Template file")
    parser.add_argument("-o", "--output", help="Output file. Otherwise will write to stdout")
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        dest="defines",
        help="Define a macro (key=value)",
    )
    parser.add_argument(
        "-I",
        "--include",
        action="append",
        default=[],
        help="Extra includes to add to the generated file. Angle brackets will be added "
        "if it is not wrapped in quotes or angle brackets.",
    )
    parser.add_argument(
        "-H", "--header", action="store_true", help="Flag to generate header. Include comments"
    )

    args = parser.parse_args()

    extra_inc = []
    for inc in args.include:
        inc = inc.strip()
        if not len(inc) > 2:
            raise ValueError(f"Invalid include: {inc}")
        if inc[0] not in '<"' and inc[-1] not in '>"':
            inc = f"<{inc}>"
        extra_inc.append(inc)

    defines = []
    for define in args.defines:
        defines.append(define.strip())

    parse_template(
        args.template, defines, output_file=args.output, extra_inc=extra_inc, is_header=args.header
    )


def test_template():
    root = Path(__file__).parents[1]
    template = root / "include/ucb/container/vector/template.h"
    defines = [
        "UCB_T_DECLARE=int",
        "UCB_T_IS_POD=1",
    ]
    parse_template(str(template), defines, "vector_int.h")
    defines = [
        "UCB_T_DEFINE=int",
        "UCB_T_IS_POD=1",
    ]
    parse_template(str(template), defines, "vector_int.c", extra_inc=['"vector_int.h"'])


if __name__ == "__main__":
    main()
