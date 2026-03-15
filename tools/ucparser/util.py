# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import re
import sys
from enum import Enum
from typing import Any, Dict, Iterable, List, TextIO

RE_C_IDENT = re.compile(r"^[a-zA-Z_][a-zA-Z0-9_]*$")

GLOBAL_PREFIX = ""


def set_global_prefix(prefix: str) -> None:
    global GLOBAL_PREFIX
    GLOBAL_PREFIX = prefix


def get_global_prefix() -> str:
    return GLOBAL_PREFIX


def is_valid_c_ident(name: str) -> bool:
    return RE_C_IDENT.match(name) is not None


def get_option_str(name: str, default: str, data: Dict[str, Any]) -> str:
    val = data.get(name, default)
    if not isinstance(val, str):
        raise ValueError(f"Expected string for option '{name}'")

    if name.startswith("prefix"):
        if val and not is_valid_c_ident(val):
            raise ValueError(f"Invalid C identifier for option '{name}': {val}")
    return val


def get_option_bool(name: str, default: bool, data: Dict[str, Any]) -> bool:
    val = data.get(name, default)
    if not isinstance(val, bool):
        raise ValueError(f"Expected boolean for option '{name}'")
    return val


def get_option_int(name: str, default: int, data: Dict[str, Any]) -> int:
    val = data.get(name, default)
    if not isinstance(val, int):
        raise ValueError(f"Expected boolean for option '{name}'")
    return val


# ------------------- Functions exposed to jinja2 templates ------------------ #


def cstduint(maxval: int):
    """
    Return the C fixed stdint type needed to hold the given value.
    For instance 256 -> uint8_t, 65536 -> uint16_t, etc.
    maxval must be >= 0
    """
    # Unsigned case: maxval must be positive
    if maxval < 0:
        raise ValueError("maxval must be positive for unsigned types")
    b = 8
    while maxval > ((1 << b) - 1):
        b *= 2
        if b > 64:
            raise ValueError("maxval too large")
    return f"uint{b}_t"


def cstdint(maxval: int):
    """
    Return the C fixed intXX_t type needed to hold the given signed value.
    maxval can be negative (e.g., -128 requires int8_t).
    """
    if maxval >= 0:
        # Positive case: check against INTN_MAX
        b = 8
        while maxval > ((1 << (b - 1)) - 1):
            b *= 2
            if b > 64:
                raise ValueError("maxval too large")
    else:
        # Negative case: check against INTN_MIN
        b = 8
        while maxval < -(1 << (b - 1)):
            b *= 2
            if b > 64:
                raise ValueError("maxval too large")
    return f"int{b}_t"


def printed_width(val: Any) -> int:
    """
    Returns the printed width of the given value.
    """
    return len(str(val))


def max_printed_width(vals: Iterable[Any]) -> int:
    """
    Returns the maximum printed width of the given values.
    """
    return max(printed_width(val) for val in vals)


def fmt_char(cp: int, cat: int) -> str:
    """
    Returns a string representation of the character with the given codepoint and
    general category index (See GeneralCategory in types.py).
    If the codepoint is not printable, return a hex-representation instead.

    Some characters were assumed to be printable but for instance vscode showed
    flow control issues. Instead of checking individual codepoints, we just ban
    the whole category.
    """

    # Exclude full categories C, Z, S
    # And specifically Lo, Mn, Mc, Me, Pd, Po
    if cat >= 19 or cat in (4, 5, 6, 7, 12, 17):
        return fmt_cp(cp)

    # Handle only BMP
    if cp > 0xFFFF:
        return fmt_cp(cp)

    return f"'{chr(cp)}'"


def fmt_cp(cp: int, just: bool = False) -> str:
    if cp <= 0xFFFF:
        val = f"0x{cp:04X}"
        return val if not just else val.rjust(8)
    elif cp <= 0x10FFFF:
        return f"0x{cp:06X}"
    else:
        raise ValueError(f"Invalid codepoint: {cp}")


def fmt_cp_list(cps: List[int], fixed_len: int = 0, just: bool = False) -> str:
    if fixed_len <= 0:
        return "{ " + ", ".join(fmt_cp(cp, just) for cp in cps) + " }"

    # Pad with 0xFFFF up to fixed len
    if len(cps) > fixed_len:
        raise ValueError(f"Too many values: {len(cps)} > {fixed_len}")
    vals = []
    for cp in cps:
        vals.append(fmt_cp(cp, just))
    for _ in range(fixed_len - len(cps)):
        vals.append(fmt_cp(0xFFFF, just))
    return "{ " + ", ".join(vals) + " }"


def fmt_cp_key(cp: int) -> str:
    return f"{cp:06x}"


def fmt_cp_key_list(cps: List[int]) -> str:
    return ",".join(fmt_cp_key(cp) for cp in cps)


def fmt_idx(cp: int, just: bool = False) -> str:
    val = f"{cp}"
    return val if not just else val.rjust(4)


def fmt_idx_list(indices: List[int], fixed_len: int = 0, just: bool = False) -> str:
    if fixed_len <= 0:
        return "{ " + ", ".join(fmt_idx(idx, just) for idx in indices) + " }"

    # Pad with 0 up to fixed len
    if len(indices) > fixed_len:
        raise ValueError(f"Too many values: {len(indices)} > {fixed_len}")
    vals = []
    for idx in indices:
        vals.append(fmt_idx(idx, just))
    for _ in range(fixed_len - len(indices)):
        vals.append(fmt_idx(0, just))
    return "{ " + ", ".join(vals) + " }"


# -------------------- End of functions exposed to jinja2 -------------------- #


def jinja_functions_dict() -> Dict[str, Any]:
    return {
        "fmt_cp": fmt_cp,
        "fmt_cp_list": fmt_cp_list,
        "fmt_cp_key": fmt_cp_key,
        "fmt_cp_key_list": fmt_cp_key_list,
        "fmt_idx": fmt_idx,
        "fmt_idx_list": fmt_idx_list,
        "cstduint": cstduint,
        "cstdint": cstdint,
    }


# ---------------------------------- pprint ---------------------------------- #


class Align(Enum):
    NONE = 0
    LEFT = 1
    RIGHT = 2


def pprint(d: Dict, file: TextIO = sys.stdout, indent=0, align=Align.NONE):
    indstr = " " * indent
    if align != Align.NONE:
        maxw = max([len(k) for k in d.keys()])

    for key, value in d.items():
        if align == Align.LEFT:
            key = key.ljust(maxw)
        elif align == Align.RIGHT:
            key = key.rjust(maxw)
        file.write(f"{indstr}{key}:")
        if isinstance(value, dict):
            file.write("\n")
            if align == Align.NONE:
                next_indent = indent + 4
            else:
                next_indent = indent + maxw + 5
            pprint(value, file=file, indent=next_indent, align=align)
        elif isinstance(value, list):
            file.write("\n")
            for item in value:
                file.write(f"{indstr}    - {item}\n")
        else:
            file.write(f" {value}\n")
