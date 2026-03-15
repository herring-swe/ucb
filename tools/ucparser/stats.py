# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import sys
from typing import Any, Dict, List

from .parser import UnicodeDataParser
from .types import CodepointInfo, DecompositionType, GeneralCategory
from .util import Align, pprint


def print_stats(parser: UnicodeDataParser, fd=sys.stdout) -> None:
    class SetCounter(object):
        def __init__(self, st: Dict[str, int], *args):
            self.count = 0
            self.st = st
            self.keys = []
            name = ""
            for arg in args:
                self.keys.append(arg)
                if arg.startswith("to_"):
                    arg = arg[3:]
                name += "_" + arg
            self.name_isect = "all" + name
            self.name_union = "any" + name
            self.name_compl = "unique" + name
            for name in [self.name_isect, self.name_union, self.name_compl]:
                if name in st:
                    raise RuntimeError(f"Duplicate key {name} in st")
                st[name] = 0

        def check(self, cpinfo: CodepointInfo):
            if all(getattr(cpinfo, key) is not None for key in self.keys):
                self.st[self.name_isect] += 1
            if any(getattr(cpinfo, key) is not None for key in self.keys):
                self.st[self.name_union] += 1
            if sum(getattr(cpinfo, key) is not None for key in self.keys) == 1:
                self.st[self.name_compl] += 1

    # Remain compatible with Python 3.6+
    # Should have used TypedDict, but let's just split the dicts and combine later
    counts: Dict[str, int] = {
        "num_entries": 0,
        "num_unique_entries": 0,
        "num_ranges": 0,
        "num_ranged_entries": 0,
        "num_casefold": 0,
        "num_to_lower": 0,
        "num_to_upper": 0,
        "num_to_title": 0,
        "num_decomp": 0,
        "max_decomp_len": 0,
        "max_casefold_len": 0,
    }

    errors: List[str] = []

    cats = {member.name: 0 for member in GeneralCategory}
    decomp = {member.name: 0 for member in DecompositionType}

    combinations: Dict[str, int] = {}
    set_counters = [
        SetCounter(combinations, "to_lower", "to_upper", "to_title"),
        SetCounter(combinations, "to_upper", "to_title"),
        SetCounter(combinations, "to_lower", "casefold"),
    ]

    for cpinfo in parser.data.values():
        counts["num_unique_entries"] += 1
        cats[cpinfo.gc.name] += 1
        for name in [
            "casefold",
            "to_lower",
            "to_upper",
            "to_title",
        ]:
            if getattr(cpinfo, name) is not None:
                counts["num_" + name] += 1
        for sc in set_counters:
            sc.check(cpinfo)
        if cpinfo.casefold is not None:
            counts["max_casefold_len"] = max(
                counts["max_casefold_len"], len(cpinfo.casefold.value)
            )
        if cpinfo.decomp is not None:
            dc = cpinfo.decomp
            counts["num_decomp"] += 1
            counts["max_decomp_len"] = max(counts["max_decomp_len"], len(dc.dcvals))
            decomp[dc.dctype.name] += 1

        if cpinfo.codepoints:
            counts["num_ranges"] += 1
            counts["num_ranged_entries"] += len(cpinfo.codepoints)
            
    counts["num_entries"] = counts["num_unique_entries"] + counts["num_ranged_entries"]

    # Combine all stats
    all_stats: Dict[str, Any] = {
        **counts,
        "categories": cats,
        "decomp": decomp,
        "combinations": combinations,
        "errors": errors,
    }
    pprint(all_stats, file=fd, align=Align.NONE)
