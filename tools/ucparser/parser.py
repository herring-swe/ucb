# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

# Requires UnicodeData.txt and CaseFolding.txt from unicode.org
# In the current working directory

import logging
import os
from pathlib import Path
from typing import Dict, List, Optional, Set, TextIO, Tuple, Union

from jinja2 import Environment, FileSystemLoader

from .types import (
    Codepoint,
    CodepointInfo,
    Combiners,
    Decomp,
    DecompositionType,
    GeneralCategory,
    Mapping,
    PropertyFlags,
)
from .util import (
    cstduint,
    get_option_bool,
    get_option_int,
    get_option_str,
    jinja_functions_dict,
    max_printed_width,
    set_global_prefix,
)

WITH_VERIFICATION = True
MAX_CP_CODEPOINTS = 3
MAX_DC_CODEPOINTS = 3

# Fields in UnicodeData.txt
UDF_CP = 0
UDF_NAME = 1
UDF_GENERAL_CATEGORY = 2
UDF_CANONICAL_COMBINING_CLASS = 3
UDF_BIDI_CLASS = 4
UDF_DECOMPOSITION = 5
UDF_DECIMAL_DIGIT_VALUE = 6
UDF_DIGIT_VALUE = 7
UDF_NUMERIC_VALUE = 8
UDF_BIDI_MIRRORED = 9
UDF_UNICODE_1_NAME = 10  # Obsolete
UDF_ISO_COMMENT = 11  # Obsolete
UDF_SIMPLE_UPPERCASE_MAPPING = 12
UDF_SIMPLE_LOWERCASE_MAPPING = 13
UDF_SIMPLE_TITLECASE_MAPPING = 14  # Empty when same as UPPERCASE
# See SpecialCasing.txt for full mapping for fields 12-14


# Fields in CaseFolding.txt (Simple and full case folding)
CFDF_CP = 0
CFDF_STATUS = 1
CFDF_MAPPING = 2
CFDF_COMMENT = 3

# Fields in SpecialCasing.txt
SCDF_CP = 0
SCDF_LOWERCASE_MAPPING = 1
SCDF_TITLECASE_MAPPING = 2
SCDF_UPPERCASE_MAPPING = 3
SCDF_CONDITIONAL_MAPPING = 4


log = logging.getLogger(__name__)


class ParseError(Exception):
    def __init__(
        self, msg: str, filename: str = "", lineno: int = -1, e: Optional[Exception] = None
    ):
        super().__init__(msg)
        self.message = msg
        self.filename = filename
        self.lineno = lineno
        self.exception = e

    def __str__(self):
        msg = self.message
        if self.filename:
            msg += f" in {self.filename}"
        if self.lineno >= 0:
            msg += f" at line {self.lineno}"
        if self.exception:
            msg += f"\nCaused by: {self.exception}\n"
        return msg


class UnicodeDataParser(object):
    def __init__(self, *args, **kwargs) -> None:
        """
        Create a parser

        The following keyword arguments are supported:
        - prefix: Prefix for generated symbols (default: "ucb_uc_")
        - prefix_filename: Prefix for generated files (default: prefix)
        - prefix_header_guard: Prefix for header guard symbols (default: prefix_filename)
        - with_verif: Include verification code
        - with_comments: Include comments
        - with_decomp: Include decomposition data

        All prefixes will be cased properly

        """
        self.prefix = get_option_str("prefix", "ucb_uc_", kwargs)
        self.prefix_filename = get_option_str("prefix_filename", "unicode_", kwargs)
        self.prefix_header_guard = get_option_str(
            "prefix_header_guard", "ucb_data_unicode_", kwargs
        )

        set_global_prefix(self.prefix)
        # FIXME: When arguments are properly set
        # self.prefix_filename = get_option_str("prefix_filename", self.prefix, kwargs)
        # self.prefix_header_guard = get_option_str(
        #     "prefix_header_guard", self.prefix_filename, kwargs
        # )

        self.with_verif = get_option_bool("with_verif", True, kwargs)
        self.with_comments = get_option_bool("with_comments", True, kwargs)
        self.with_decomp = get_option_bool("with_decomp", True, kwargs)

        self.blocksize = get_option_int("blocksize", 256, kwargs)
        n = self.blocksize
        if n <= 0 or (n & (n - 1) != 0):
            raise ValueError(f"Blocksize must be a power of two: {self.blocksize}")

        self.no_codepoint_val = get_option_int("blocksize", 0xFFFF, kwargs)

        # Generated from parsing UCD
        self.data: Dict[Codepoint, CodepointInfo] = {}
        self.comp_exclusions: Set[Codepoint] = set()

        # Generated from primary data tables
        self.prop_table: List[CodepointInfo] = []
        self.stage1: List[int] = []
        self.stage2: List[int] = []
        self.smap: List[Codepoint] = []
        self.mmap: List[List[Codepoint]] = []
        self.special_casefold: Dict[Codepoint, Codepoint] = {}
        self.combiners: List[Combiners] = []

        # Non-trivial counts only
        self.num_codepoints = 0
        self.num_mapping = 0
        self.num_decomp = 0
        self.max_mmap_len = 0
        self.max_combiner_vals = 0

        # Internal state
        self._parse_finalized = False

    def parse_unicode_data(self, fd: TextIO, filename: str) -> bool:
        lines_parsed = 0
        lineno = 0
        range_start: None | Codepoint = None
        range_desc: None | str = None

        for line in fd:
            lineno += 1
            line = line.strip()

            # Skip empty and commented lines
            if not line or line.startswith("#"):
                continue

            try:
                fields = line.split(";")

                cp = Codepoint(fields[UDF_CP])
                if cp in self.data:
                    raise ParseError("Duplicate codepoint", filename, lineno)

                # Parse ranges
                name = fields[UDF_NAME]
                if name.startswith("<") and name.endswith("First>"):
                    if range_start is not None:
                        raise ParseError(f"Nested range {cp}, {name}", filename, lineno)
                    range_start = cp
                    range_desc = name[1:-7]
                elif name.startswith("<") and name.endswith("Last>"):
                    if range_start is None or range_desc != name[1:-6]:
                        raise ParseError(
                            f"Unmatched range: {cp}, {name}, previous {range_start}, {range_desc}",
                            filename,
                            lineno,
                        )

                    # Range of exact duplicates
                    # range_start is already added, so duplicate it until and including cp
                    for range_cp in range(int(range_start) + 1, int(cp) + 1):
                        info = self.data[range_start].copy()
                        info.cp = Codepoint(range_cp)
                        self.data[info.cp] = info
                    range_start = None
                    range_desc = None
                    continue

                gc = GeneralCategory.parse(fields[UDF_GENERAL_CATEGORY])
                info = CodepointInfo(cp, gc)

                # CCC is only relevant for normalization, so we keep it
                ccc = int(fields[UDF_CANONICAL_COMBINING_CLASS], 10)
                if ccc < 0 or ccc > 255:
                    raise ValueError(f"Invalid canonical combining class: {ccc}")
                info.ccc = ccc

                if fields[UDF_DECOMPOSITION]:
                    parts = fields[UDF_DECOMPOSITION].split()
                    if parts[0].startswith("<"):  # Tagged
                        if not parts[0].endswith(">"):
                            raise ParseError(
                                f"Invalid decomposition type: {parts[0]}",
                                filename,
                                lineno,
                            )
                        dctype = DecompositionType.parse(parts[0][1:-1])
                        parts = parts[1:]  # Remaining parts are codepoints
                    else:  # Untagged
                        dctype = DecompositionType.Canon

                    dcvals = [Codepoint(x) for x in parts]
                    info.decomp = Decomp(cp, dctype, dcvals)

                if fields[UDF_SIMPLE_UPPERCASE_MAPPING]:
                    info.to_upper = Mapping(Codepoint(fields[UDF_SIMPLE_UPPERCASE_MAPPING]))
                if fields[UDF_SIMPLE_LOWERCASE_MAPPING]:
                    info.to_lower = Mapping(Codepoint(fields[UDF_SIMPLE_LOWERCASE_MAPPING]))
                if fields[UDF_SIMPLE_TITLECASE_MAPPING]:
                    info.to_title = Mapping(Codepoint(fields[UDF_SIMPLE_TITLECASE_MAPPING]))

                self.data[cp] = info
                lines_parsed += 1
            except ParseError as e:
                raise e
            # except Exception as e:
            #     raise ParseError("Caught exception", filename, lineno, e)
        return lines_parsed > 0

    def parse_casefolding(self, fd: TextIO, filename: str) -> bool:
        lineno = 0

        for line in fd:
            lineno += 1
            line = line.strip()

            # Skip empty and commented lines
            if not line or line.startswith("#"):
                continue

            cp_str, status, mapping, _ = [part.strip() for part in line.split(";", 3)]
            cp = Codepoint(cp_str)
            value = [Codepoint(x) for x in mapping.split()]
            if status == "F":
                if len(value) < 2:
                    raise ParseError(
                        f"Multi case folding assumed to have at least two codepoints: {line}",
                        filename,
                        lineno,
                    )
                if self.data[cp].casefold is not None:
                    raise ParseError(
                        f"Duplicate full case folding for codepoint {cp}",
                        filename,
                        lineno,
                    )
                self.data[cp].casefold = Mapping(value)
            elif status == "C":
                if len(value) != 1:
                    raise ParseError(
                        f"Simple or common case folding must have exactly one codepoint: {line}",
                        filename,
                        lineno,
                    )
                if self.data[cp].casefold is not None:
                    raise ParseError(f"Duplicate case folding for codepoint {cp}", filename, lineno)
                self.data[cp].casefold = Mapping(value[0])
            elif status == "S":
                # Skip simple case folding in favor for full
                pass
            elif status == "T":
                if len(value) != 1:
                    raise ParseError(
                        f"Special case folding must have exactly one codepoint: {line}",
                        filename,
                        lineno,
                    )
                if cp in self.special_casefold:
                    raise ParseError(
                        f"Duplicate special case folding for codepoint {cp}",
                        filename,
                        lineno,
                    )
                self.special_casefold[cp] = value[0]
            else:
                raise ParseError(f'Unknown status character "{status}"', filename, lineno)
        return True

    def parse_composition_exclusions(self, fd: TextIO, filename: str) -> bool:
        """
        Parse the CompositionExclusions.txt file
        """
        lineno = 0

        for line in fd:
            lineno += 1
            line = line.strip()

            # Skip empty and commented lines
            if not line or line.startswith("#"):
                continue

            # Parse first value as single codepoint
            # or split it on .. to get range
            # Note: no such range in 17.0.0, but could be in the future

            try:
                val = line.split()[0]
                if ".." in val:
                    start, end = val.split("..")
                    for cp in range(int(start, 16), int(end, 16) + 1):
                        self.comp_exclusions.add(Codepoint(cp))
                else:
                    self.comp_exclusions.add(Codepoint(val))

            except Exception as e:
                raise ParseError(
                    f"Could not parse codepoint(s) from line: {line}", filename, lineno
                ) from e

        return True

    def _parse_ranges(self) -> int:
        """
        Parse additional ranges of continuous codepoints with the same data
        This is scrapped from the final design and will be removed
        """
        ranges: List[Tuple[Codepoint, Codepoint]] = []
        first: Union[CodepointInfo, None] = None
        last: Union[CodepointInfo, None] = None
        min_range = 50
        for cp in sorted(self.data.keys()):
            cur = self.data[cp]

            # Start or not continuous range
            if first is None:
                first = last = cur
                continue
            assert last is not None  # Mypy can't infer this
            if cur.cp - last.cp > 1 or not last.equal_data(cur):
                # End of range
                if last.cp - first.cp >= min_range:
                    ranges.append((first.cp, last.cp))
                first = last = cur
                continue
            last = cur

        # Sort ranges according to size
        ranges.sort(key=lambda x: x[1] - x[0], reverse=False)
        log.debug("Additional ranges:")
        total_cp = len(self.data)
        for cp1, cp2 in ranges:
            log.debug(f"Range: {cp1:04X} - {cp2:04X} ({cp2 - cp1 + 1} codepoints)")
            total_cp -= cp2 - cp1 + 1
        log.debug(f"Original codepoints: {len(self.data)}")
        log.debug(f"Additional ranges: {len(ranges)} with {len(self.data) - total_cp} codepoints")
        log.debug(f"Resulting codepoints: {total_cp}")
        log.debug(f"Reduction to {total_cp / len(self.data) * 100:.2f}%")
        return len(ranges)

    def _create_mapping_tables(self, smap: Dict[str, Mapping], mmap: Dict[str, Mapping]) -> None:
        """
        Create tables for looking up mapping values
        smap -> single value
        mmap -> multiple values
        These contain unique replacements only and sorted according to codepoint
        """

        # Build mapping tables, pointing to unique mapping values
        for info in self.data.values():
            for field in ("casefold", "to_upper", "to_lower", "to_title"):
                mapping = getattr(info, field)
                if mapping is None:
                    continue
                dest = mmap if mapping.is_multi else smap
                if mapping.key not in dest:
                    dest[mapping.key] = Mapping(mapping.value)
                # dest[mapping.key].add_ref(info.cp, field)

        # Sort mapping tables according to replacement value
        smap_list = [x for x in smap.values()]
        mmap_list = [x for x in mmap.values()]
        smap_list.sort(key=lambda x: x.key)
        mmap_list.sort(key=lambda x: x.key)

        # Set indices, starting from 1
        for idx, mapping in enumerate(smap_list):
            mapping.set_index(idx + 1)
        for idx, mapping in enumerate(mmap_list):
            mapping.set_index(idx + 1)

        # Set tables that will be written to templates
        self.mmap = [x.value for x in mmap_list]
        self.smap = [x.value[0] for x in smap_list]

    def _is_valid_composition(self, info: CodepointInfo):
        # See https://www.unicode.org/reports/tr15/#Primary_Exclusion_List_Table

        if info.decomp is None:
            return False
        # Only entries with one starter and combiner can be composed
        # Only canonical decompositions must be composed (even with of NFKC)
        if info.decomp.dctype != DecompositionType.Canon or len(info.decomp.dcvals) != 2:
            return False

        starter = info.decomp.dcvals[0]
        starter_info = self.data[starter]
        if starter_info.ccc != 0:
            # This codepoint is not a valid starter
            return False

        # Adhere to explicit exclusions, covering the final composition exclusion list
        # See https://www.unicode.org/Public/UCD/17.0.0/ucd/CompositionExclusions.txt
        if info.cp in self.comp_exclusions:
            return False
        return True

    def _create_combiner_table(self, combiners: Dict[Codepoint, Combiners]) -> None:
        """
        Create a table of all combiners, sorted by codepoint
        """

        max_combiner_vals = 0

        # Update indices in codepoint table
        for info in self.data.values():
            if info.decomp is None:
                continue

            if self._is_valid_composition(info):
                # Composition is possible
                starter = info.decomp.dcvals[0]
                combiner = info.decomp.dcvals[1]
                composed = info.cp

                if starter not in combiners:
                    combiners[starter] = Combiners(starter)
                combiners[starter].add_entry(combiner, composed)
                max_combiner_vals = max(max_combiner_vals, combiners[starter].num_entries)

        clist = [x for x in combiners.values()]
        clist.sort(key=lambda x: x.starter)
        for idx, mapping in enumerate(clist):
            mapping.set_index(idx + 1)

        # Set tables that will be written to templates
        self.combiners = clist
        self.max_combiner_vals = max_combiner_vals

    def _update_property_flags(self, info: CodepointInfo) -> None:
        if info.cp in self.comp_exclusions or (
            info.decomp
            and (len(info.decomp.dcvals) == 1 or self.data[info.decomp.dcvals[0]].ccc != 0)
        ):
            info.flags |= PropertyFlags.CompExcl

    def _create_property_table(self) -> None:
        """
        Remove all duplicate properties. Key data fields must have
        been set before calling this function

        We also count all codepoints, including those ranges
        that have already been parsed

        Special entry 0 is added first, this holds default properties
        for all codepoints not explicitly listed in the property table
        """

        dataset: Dict[str, CodepointInfo] = {}

        default = CodepointInfo(Codepoint(0xFFFF), GeneralCategory.Cn)
        default.propindex = 0
        dataset[default.data_key] = default

        prop_index = 1

        for info in self.data.values():
            # Update flags
            self._update_property_flags(info)

            if info.data_key in dataset:
                propinfo = dataset[info.data_key]
                assert len(propinfo.codepoints) != 0
                propinfo.codepoints.add(info.cp)
                info.propindex = propinfo.propindex
                continue

            # Update data entry
            info.propindex = prop_index
            prop_index += 1

            # Create a copy for property table
            propinfo = info.copy()
            assert len(propinfo.codepoints) == 0
            propinfo.codepoints.add(info.cp)
            dataset[info.data_key] = propinfo

        num_ranges = 0
        for info in dataset.values():
            if info.codepoints:
                num_ranges += info.codepoints.num_ranges()

        self.num_codepoints = len(self.data)
        self.prop_table = [x for x in dataset.values()]

    def _create_lookup_tables(self) -> None:
        """
        Create lookup tables for codepoint properties
        """
        stage2: List[int] = []  # Indices to prop_table
        stage1: List[int] = []  # Indices to stage2 blocks

        block: List[int] = []  # Current block
        unique_blocks: Dict[Tuple[int, ...], int] = {}

        # added_indices: Set[int] = set()

        def add_block() -> None:
            tblock = tuple(block)
            if tblock in unique_blocks:
                block_index = unique_blocks[tblock]
            else:
                # Add a new block
                block_index = len(stage2) // self.blocksize
                unique_blocks[tblock] = block_index
                stage2.extend(block)
            stage1.append(block_index)
            block.clear()

        for cp in range(0, 0x110000):
            if cp not in self.data:
                # Default property
                prop_index = 0
            else:
                info = self.data[Codepoint(cp)]
                prop_index = info.propindex

            block.append(prop_index)
            if len(block) < self.blocksize:
                continue

            add_block()

        # Append the last block
        if block:
            add_block()

        # This is magic... did it work?
        log.debug(f"Stage1 size: {len(stage1)}")
        log.debug(f"Stage2 size: {len(stage2)}")
        log.debug(f"Prop size: {len(self.prop_table)}")

        self.stage2 = stage2
        self.stage1 = stage1

    def _verify(self) -> None:
        # Do simple queries for a few codepoints to verify that tables are correct

        def lookup_func(cp: int) -> int:
            assert cp >= 0 and cp < 0x110000
            block_offset = self.stage1[cp // self.blocksize] * self.blocksize
            return self.stage2[block_offset + cp % self.blocksize]

        for cp in [
            0xFFFF,
            0x0041,
            0x0061,
            0x00DF,
            0x03A3,
            0x03C2,
            0x1F600,
            0xD800,
            0xD850,
            0xE000,
            0x0,
            0xE012,
            0x10FFFF,
            0x10FFFE,
        ]:
            info = self.prop_table[lookup_func(cp)]
            try:
                if Codepoint(cp) in self.data:
                    assert int(info.cp) == cp or Codepoint(cp) in info.codepoints
                    log.debug(f"Found {cp:x} with lookup index {lookup_func(cp)}")
                    if int(info.cp) == cp:
                        log.debug("Match by direct comparison")
                    else:
                        log.debug("Match by checking shared data")
                else:
                    log.debug(f"Did not find {cp:x} with lookup index {lookup_func(cp)}")
                    assert info.cp == 0xFFFF and info.propindex == 0
            except AssertionError as e:
                log.debug(f"   Verifying: {cp:x}")
                log.debug(f"         Got: {info.cp:x}")
                log.debug(f"       Index: {info.propindex}")
                log.debug(f"Lookup index: {lookup_func(cp)}")

                raw_info = self.data.get(Codepoint(cp))
                if raw_info:
                    log.debug(f"Found codepoint in raw data: {raw_info}")
                    log.debug(f"                      Index: {raw_info.propindex}")
                    log.debug(f"    Raw data comparison key: {raw_info.data_key}")
                    log.debug(f"  Found data comparison key: {info.data_key}")
                if info.codepoints:
                    log.debug("Expected to be in one of:")
                    for cp in info.codepoints:
                        log.debug(f"     {cp:x}")
                else:
                    log.debug("Expected to be in no codepoints")
                raise e

            if info.combiner_idx != 0:
                combiner = self.combiners[info.combiner_idx - 1]
                if info.cp != combiner.starter:
                    log.fatal(f"Combiner starter: {combiner.starter:x}")
                    log.fatal(f"Codepoint: {info.cp:x}")
                    log.fatal(f"Combiner index: {info.combiner_idx}")
                    log.fatal(f"Combiner: {combiner}")
                assert info.cp == combiner.starter

    def parse_finalize(self) -> None:
        if self._parse_finalized:
            raise RuntimeError("parse_finalize() already called")
        self._parse_finalized = True

        smap: Dict[str, Mapping] = {}
        mmap: Dict[str, Mapping] = {}
        # comp: Dict[Codepoint, List[Tuple(Codepoint, Codepoint)]] = {}
        self._create_mapping_tables(smap, mmap)

        combiners: Dict[Codepoint, Combiners] = {}
        self._create_combiner_table(combiners)

        mapping_idx = 1
        decomp_idx = 1
        max_mmap = 0

        # Update indices in codepoint table
        for info in self.data.values():
            has_mapping = False
            for mapping in (info.casefold, info.to_upper, info.to_lower, info.to_title):
                if mapping is None:
                    continue
                has_mapping = True
                max_mmap = max(max_mmap, len(mapping.value))
                if mapping.is_multi:
                    dest = mmap
                    mapping.set_index(dest[mapping.key].index + len(smap))
                else:
                    dest = smap
                    mapping.set_index(dest[mapping.key].index)
                assert mapping.index != -1

            # Update mapping table indices
            if has_mapping:
                info.mapping_idx = mapping_idx
                mapping_idx += 1

            # Update decomp table indices
            if info.decomp is not None:
                info.decomp_idx = decomp_idx
                decomp_idx += 1

            if info.cp in combiners:
                info.combiner_idx = combiners[info.cp].index

        self.num_decomp = decomp_idx - 1
        self.num_mapping = mapping_idx - 1
        self.max_mmap_len = max_mmap

        self._create_property_table()
        self._create_lookup_tables()
        self._verify()

        # After this, self.data can be discarded
        self.data.clear()

    def parse(self, filename: str) -> bool:
        path = Path(filename)
        if not path.is_file():
            raise ParseError("File not found", filename, -1)
        if path.name == "UnicodeData.txt":
            with open(filename, "r", encoding="utf-8") as fd:
                return self.parse_unicode_data(fd, filename)
        elif path.name == "CaseFolding.txt":
            with open(filename, "r", encoding="utf-8") as fd:
                return self.parse_casefolding(fd, filename)
        elif path.name == "CompositionExclusions.txt":
            with open(filename, "r", encoding="utf-8") as fd:
                return self.parse_composition_exclusions(fd, filename)
        raise ParseError("Unknown file type", filename, -1)

    def parse_all(self) -> None:
        self.parse("UnicodeData.txt")
        self.parse("CaseFolding.txt")
        self.parse("CompositionExclusions.txt")
        self.parse_finalize()

    def _write_template(self, comp: str, tfile: Path, ofile: Path):
        log.debug(f'Writing component "{comp}" using template {tfile} to {ofile}')

        data = jinja_functions_dict()
        defs = {
            "component": comp,
            # User settings
            "prefix": self.prefix,
            "prefix_lc": self.prefix.lower(),
            "prefix_uc": self.prefix.upper(),
            "prefix_filename": self.prefix_filename,
            "prefix_header_guard": self.prefix_header_guard.upper(),
            "with_verif": self.with_verif,
            "with_comments": self.with_comments,
            "with_decomp": self.with_decomp,
            "block_size": self.blocksize,
            "no_codepoint_val": self.no_codepoint_val,
            # Enums and flags
            "enum_gc": GeneralCategory,
            "enum_decomp": DecompositionType,
            "flags_prop": PropertyFlags,
            # Data tables
            "props": self.prop_table,
            "smap": self.smap,
            "mmap": self.mmap,
            "combiners": self.combiners,
            "stage1": self.stage1,
            "stage2": self.stage2,
            # Types, for dynamic data in order to minimize memory usage
            "stage1_type": cstduint(max(self.stage1)),
            "stage2_type": cstduint(max(self.stage2)),
            # Max width info for tables row data, if applicable
            "stage1_mw": max_printed_width(self.stage1),
            "stage2_mw": max_printed_width(self.stage2),
            # Sizes, should add all relevant ones
            "num_codepoints": self.num_codepoints,
            "num_props": len(self.prop_table),
            "num_smap": len(self.smap),
            "num_mmap": len(self.mmap),
            "num_stage1": len(self.stage1),
            "num_stage2": len(self.stage2),
            "num_mapping_table": self.num_mapping,
            "num_decomp_table": self.num_decomp,
            "num_combiner_table": len(self.combiners),
            "max_mmap_len": self.max_mmap_len,
            "num_cf_special": len(self.special_casefold),
        }
        data.update(defs)
        # data.update(template_data)

        env = Environment(
            loader=FileSystemLoader(str(tfile.parent)),
            trim_blocks=True,
            lstrip_blocks=True,
            keep_trailing_newline=True,
            # line_statement_prefix="%",
        )
        env.filters["ljust"] = lambda value, width: str(value).ljust(width)
        env.filters["rjust"] = lambda value, width: str(value).rjust(width)
        env.filters["center"] = lambda value, width: str(value).center(width)
        template = env.get_template(str(tfile.name))
        with open(str(ofile), "w", encoding="utf-8", newline="\n") as f:
            f.write(template.render(**data))

    def write_templates(self, template_dir: str = "", output_dir: str = ""):
        if not self._parse_finalized:
            raise RuntimeError("Data not finalized")

        if template_dir:
            tdir = Path(template_dir)
        else:
            tdir = Path(os.path.join(os.path.dirname(__file__), "templates"))
        if output_dir:
            odir = Path(output_dir)
        else:
            odir = Path.cwd()

        if not tdir:
            raise RuntimeError("Template directory not found")
        if not odir:
            raise RuntimeError("Output directory not found")

        components = ["defines", "props", "mapping", "combine"]
        if self.with_decomp:
            components.append("decomp")

        for comp in components:
            try_tfn = []
            if self.prefix_filename:
                try_tfn.append(f"{self.prefix_filename}{comp}.h.jinja")
            try_tfn.append(f"{comp}.h.jinja")

            tfile: Optional[Path] = None
            for fn in try_tfn:
                tfile = tdir / fn
                if tfile.is_file():
                    break
                tfile = None

            if tfile is None:
                raise FileNotFoundError(
                    f"Could not find template file for component {comp}. Tried: {try_tfn}"
                )

            ofile = odir / f"{self.prefix_filename}{comp}.h"

            self._write_template(comp, tfile, ofile)
