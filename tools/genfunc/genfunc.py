#!/usr/bin/env python3
# This file is part of the UCB project
# SPDX-FileCopyrightText: 2026 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import os
import sys
from pathlib import Path
from typing import IO, Any

BEGIN = "GENERATE_BEGIN"
END = "GENERATE_END"

FLAG_SIGNED = 0x01
FLAG_INT = 0x02
FLAG_FLOAT = 0x04

TYPES = {
    "signed char": {"short": "hhi", "mid": "SCHAR", "flags": FLAG_SIGNED | FLAG_INT},
    "unsigned char": {"short": "hhu", "mid": "UCHAR", "flags": FLAG_INT},
    "short": {"short": "hi", "mid": "SHORT", "flags": FLAG_SIGNED | FLAG_INT},
    "unsigned short": {"short": "hu", "mid": "USHORT", "flags": FLAG_INT},
    "int": {"short": "i", "mid": "INT", "flags": FLAG_SIGNED | FLAG_INT},
    "unsigned int": {"short": "u", "mid": "UINT", "flags": FLAG_INT},
    "long": {"short": "li", "mid": "LONG", "flags": FLAG_SIGNED | FLAG_INT},
    "unsigned long": {"short": "lu", "mid": "ULONG", "flags": FLAG_INT},
    "long long": {"short": "lli", "mid": "LLONG", "flags": FLAG_SIGNED | FLAG_INT},
    "unsigned long long": {"short": "llu", "mid": "ULLONG", "flags": FLAG_INT},
    "float": {"short": "f", "mid": "FLT", "flags": FLAG_FLOAT},
    "double": {"short": "d", "mid": "DBL", "flags": FLAG_FLOAT},
}

FUNCTIONS = {
    "tgmath": {
        "min": [
            "all",
            "$TYPE ucb_min_$tshort(const $TYPE a, const $TYPE b)",
            "return a < b ? a : b;",
        ],
        "max": [
            "all",
            "$TYPE ucb_max_$tshort(const $TYPE a, const $TYPE b)",
            "return a > b ? a : b;",
        ],
        "clamp": [
            "all",
            "$TYPE ucb_clamp_$tshort(const $TYPE val, const $TYPE min_val, const $TYPE max_val)",
            "return val < min_val ? min_val : val > max_val ? max_val : val;",
        ],
        "lerp": [
            "all",
            "$TYPE ucb_lerp_$tshort(const $TYPE a, const $TYPE b, const $TYPE t)",
            "return a + t * (b - a);",
        ],
        "swap": [
            "all",
            "void ucb_swap_$tshort($TYPE *a, $TYPE *b)",
            "$TYPE tmp = *a; *a = *b; *b = tmp;",
        ],
        "in_range": [
            "all",
            "bool ucb_in_range_$tshort(const $TYPE val, const $TYPE min_val, const $TYPE max_val)",
            "return val >= min_val && val <= max_val;",
        ],
        "abs": [
            "signed",
            "$TYPE ucb_abs_$tshort(const $TYPE a)",
            "return a < 0 ? -a : a;",
        ],
        "sign": [
            "signed",
            "$TYPE ucb_sign_$tshort(const $TYPE a)",
            "return a < 0 ? -1 : 1;",
        ],
        "equal_eps": [
            "float",
            "bool ucb_equal_eps_$tshort(const $TYPE a, const $TYPE b, const $TYPE eps)",
            "return ucb_abs_$tshort(a - b) < eps;",
        ],
        "equal": [
            "float",
            "bool ucb_equal_$tshort(const $TYPE a, const $TYPE b)",
            "return ucb_equal_eps_$tshort(a, b, UCB_EPSILON_$TMID);",
        ],
        "comp_eps": [
            "float",
            "int ucb_comp_eps_$tshort(const $TYPE a, const $TYPE b, const $TYPE eps)",
            "if (ucb_equal_eps_$tshort(a, b, eps)) return 0;",
            "return a < b ? -1 : 1;",
        ],
        "comp": [
            "float",
            "int ucb_comp_$tshort(const $TYPE a, const $TYPE b)",
            "return ucb_comp_eps_$tshort(a, b, UCB_EPSILON_$TMID);",
        ],
        "approx_equal_eps": [
            "float",
            "bool ucb_approx_equal_$tshort(const $TYPE a, const $TYPE b, const $TYPE eps)",
            "return ucb_abs_##TNAME(a - b) <= ",
            "    ucb_max_##TNAME(ucb_abs_##TNAME(a), ucb_abs_##TNAME(b)) * eps;",
        ],
        "approx_equal": [
            "float",
            "bool ucb_approx_equal_$tshort(const $TYPE a, const $TYPE b)",
            "return ucb_approx_equal_eps_$tshort(a, b, UCB_EPSILON_$TMID);",
        ],
        "approx_comp_eps": [
            "float",
            "int ucb_approx_comp_eps_$tshort(const $TYPE a, const $TYPE b, const $TYPE eps)",
            "if (ucb_approx_equal_eps_$tshort(a, b, eps)) return 0;",
            "return a < b ? -1 : 1;",
        ],
        "approx_comp": [
            "float",
            "int ucb_approx_comp_$tshort(const $TYPE a, const $TYPE b)",
            "return ucb_approx_comp_eps_$tshort(a, b, UCB_EPSILON_$TMID);",
        ],
    }
}


def repl_type(s: str, typename: str, data: dict[str, Any]) -> str:
    s = s.replace("$TYPE", typename.upper())
    s = s.replace("$TSHORT", data["short"].upper())
    s = s.replace("$TMID", data["mid"].upper())
    s = s.replace("$type", typename.lower())
    s = s.replace("$tshort", data["short"].lower())
    s = s.replace("$tmid", data["mid"].lower())
    return s


def generate_tgmath(fd, header: bool):
    for func in FUNCTIONS["tgmath"]:
        ftype = func[0]
        for typename, data in TYPES.items():
            flags: int = data["flags"]  # type: ignore
            if ftype == "signed" and (flags & FLAG_SIGNED) == 0:
                continue
            if ftype == "float" and (flags & FLAG_FLOAT) == 0:
                continue

            decl = repl_type(func[1], typename, data)
            impl = repl_type(func[2:], typename, data)

            if header:
                fd.write(f"UCB_EXPORT {decl};\n")
            else:
                fd.write(f"{decl}\n{{\n")
                for line in impl:
                    fd.write(f"    {line}\n")
                fd.write("}\n")


def generate_func(fn: Path, fd: IO[Any]):
    if fn.name == "tgmath.h":
        generate_tgmath(fd, True)


def generate_file(fn: Path):
    got_marker = False
    # Binary to force LF newlines
    with open(fn, "rwb") as f:
        for line in f:
            if line.startswith(BEGIN):
                got_marker = True
                continue
            if line.startswith(END):
                if got_marker:
                    generate_func(fn, f)
                else:
                    print(f"Error: {fn}: {END} without {BEGIN}")
                got_marker = False
            f.write(line)


if __name__ == "__main__":
    # Find root path with src and include

    curdir = Path(__file__).parent
    while curdir != Path("/"):
        if (curdir / "src").exists() and (curdir / "include").exists():
            break
        curdir = curdir.parent
