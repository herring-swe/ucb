# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import bisect
from enum import IntEnum
from functools import total_ordering
from typing import (
    Any,
    Iterable,
    Iterator,
    List,
    Optional,
    Tuple,
    Type,
    TypeVar,
    Union,
)

from .util import fmt_char, fmt_cp, fmt_cp_key_list, get_global_prefix

UcEnumT = TypeVar("UcEnumT", bound="UcEnum")

GLOBAL_PREFIX = ""


class UcEnum(IntEnum):
    """
    Enum to support parsing from UCD data field and provide
    attributes for use with templating
    Entry values are automatically numbered from 0

    Define as:
    class MyClass(UcEnum):
        # For parsing from UCD using short name
        MyVal = (short_name, description)
        # Using optional parse value
        MyVal = (short_name, description, "parse_val")
    """

    short_name: str
    description: str
    parse_val: Union[str, None]

    def __new__(cls, short_name: str, description: str, parse_val: Optional[str] = None):
        value = len(cls.__members__)
        obj = int.__new__(cls, value)
        obj._value_ = value
        obj.short_name = short_name
        obj.description = description
        obj.parse_val = parse_val
        return obj

    @classmethod
    def parse(cls: Type[UcEnumT], s: str) -> UcEnumT:
        for member in cls:
            if member.parse_val is not None and member.parse_val == s:
                return member
            elif member.short_name == s:
                return member
        raise ValueError(f"Could not parse {s} as {cls.__name__}")

    @classmethod
    def description_vals(cls: Type[UcEnumT]) -> List[str]:
        return [member.description for member in cls]

    def __repr__(self):
        s = f", short_name={self.short_name!r}, description={self.description!r}"
        if self.parse_val is not None:
            s += f", parse_val={self.parse_val!r}"
        return super().__repr__().replace(">", s + ">")

    @classmethod
    def enum_prefix(cls) -> str:
        """Return the prefix for enum type."""
        return cls.__name__.upper()

    @classmethod
    def prefix(cls) -> str:
        """Return the prefix, prefix + enum_prefix"""
        return (get_global_prefix() + cls.enum_prefix()).upper()

    @classmethod
    def c_name_max(cls) -> int:
        val = 0
        for member in cls:
            val = max(len(member.c_ename()), val)
        return val

    def c_ename(self) -> str:
        """Return the C enum name"""
        return f"{self.prefix()}{self.name}".upper()

    def c_edef(self) -> str:
        """Return a C enum definition for the enum value."""
        return f"{self.c_ename()} = {self.value},"

    def c_comment(self) -> str:
        """Return a C comment for the enum value."""
        return f"// {self.description}"


class UcFlag(IntEnum):
    """
    Enum to support rising enum values.
    Not parsed.
    Entry values are automatically numbered
    0000, 0001, 0010, 0100, 1000, etc.
    First should be a DEFAULT or NONE flag.

    Define as:
    class MyClass(UcFlag):
        MyVal = (short_name, description)
    """

    description: str

    def __new__(cls, description: str):
        if len(cls.__members__) == 0:
            value = 0
        else:
            value = 1 << len(cls.__members__) - 1
        obj = int.__new__(cls, value)
        obj._value_ = value
        obj.description = description
        return obj

    def __repr__(self):
        s = f", name={self.name!r}, description={self.description!r}"
        return super().__repr__().replace(">", s + ">")

    @classmethod
    def flag_prefix(cls) -> str:
        """Return the prefix for enum type."""
        return cls.__name__.upper()

    @classmethod
    def prefix(cls) -> str:
        """Return the prefix, prefix + flag_prefix"""
        return (get_global_prefix() + cls.flag_prefix()).upper()

    @classmethod
    def c_name_max(cls) -> int:
        val = 0
        for member in cls:
            val = max(len(member.c_fname()), val)
        return val

    def c_fname(self) -> str:
        """Return the C enum name"""
        return f"{self.prefix()}{self.name}".upper()

    def c_fdef(self) -> str:
        """Return a C preprocessor definition for the enum value."""
        return f"#define {self.c_fname()} {self.value}"

    def c_comment(self) -> str:
        """Return a C comment for the enum value."""
        return f"// {self.description}"

    def c_val(self) -> str:
        """Return hex value with max number of digits."""
        max_val = max(member.value for member in self.__class__)
        max_w = len(f"{max_val:X}")
        return f"0x{self.value:0{max_w}X}"


class GeneralCategory(UcEnum):
    Lu = ("Lu", "Uppercase letter")
    Ll = ("Ll", "Lowercase letter")
    Lt = ("Lt", "Titlecase letter")
    Lm = ("Lm", "Modifier letter")
    Lo = ("Lo", "Other letter")
    Mn = ("Mn", "Nonspacing mark")
    Mc = ("Mc", "Spacing combining mark")
    Me = ("Me", "Enclosing mark")
    Nd = ("Nd", "Decimal digit number")
    Nl = ("Nl", "Letter like number")
    No = ("No", "Other number")
    Pc = ("Pc", "Connector punctuation")
    Pd = ("Pd", "Dash punctuation")
    Ps = ("Ps", "Open punctuation")
    Pe = ("Pe", "Close punctuation")
    Pi = ("Pi", "Initial quote punctuation")
    Pf = ("Pf", "Final quote punctuation")
    Po = ("Po", "Other punctuation")
    Sm = ("Sm", "Math symbol")
    Sc = ("Sc", "Currency symbol")
    Sk = ("Sk", "Modifier symbol")
    So = ("So", "Other symbol")
    Zs = ("Zs", "Whitespace")
    Zl = ("Zl", "Line separator")
    Zp = ("Zp", "Paragraph separator")
    Cc = ("Cc", "Control character")
    Cf = ("Cf", "Control Format")
    Cs = ("Cs", "Control Surrogate")
    Co = ("Co", "Control private Use")
    Cn = ("Cn", "Control unassigned")

    @classmethod
    def enum_prefix(cls):
        return "GC_"


class DecompositionType(UcEnum):
    # fmt: off
    Canon   = ("canon",     "Canonical decomposition (not tagged in UnicodeData.txt)", "")
    Font    = ("font",      "Font variant")
    NoBrk   = ("noBreak",   "No-break version of a space or hyphen")
    Init    = ("initial",   "Initial presentation form (Arabic)")
    Medial  = ("medial",    "Medial presentation form (Arabic)")
    Final   = ("final",     "Final presentation form (Arabic)")
    Isol    = ("isolated",  "Isolated presentation form (Arabic)")
    Circle  = ("circle",    "Encircled form")
    Super   = ("super",     "Superscript form")
    Sub     = ("sub",       "Subscript form")
    Vert    = ("vertical",  "Vertical layout presentation form")
    Wide    = ("wide",      "Wide (or zenkaku) compatibility character")
    Narrow  = ("narrow",    "Narrow (or hankaku) compatibility character")
    Small   = ("small",     "Small variant form (CNS compatibility)")
    Square  = ("square",    "CJK squared font variant")
    Fract   = ("fraction",  "Vulgar fraction form")
    Compat  = ("compat",    "Otherwise unspecified compatibility character")
    # fmt: on

    @classmethod
    def enum_prefix(cls):
        return "DC_"


class PropertyFlags(UcFlag):
    # fmt: off
    Default = ("No special flags")
    CompExcl = ("Excluded from composition")
    # fmt: on

    @classmethod
    def flag_prefix(cls):
        return "PROP_"


class Freezable:
    def __init__(self):
        self._frozen = False

    def freeze(self) -> None:
        """Make this object immutable."""
        object.__setattr__(self, "_frozen", True)

    def __setattr__(self, name: str, value: Any) -> None:
        """Prevent attribute assignment when frozen."""
        if self._frozen:
            raise AttributeError(f"Cannot set attribute {name} on frozen object")
        object.__setattr__(self, name, value)


# class SetOnlyDict(dict):
#     """
#     Dict prevents modification of existing keys, but allows new keys to be added.
#     """

#     def __setitem__(self, key, value):
#         if key in self:
#             raise RuntimeError(f"Key {key} already exists in {type(self)}")
#         return super().__setitem__(key, value)

#     def __delitem__(self, key):
#         raise RuntimeError(f"Cannot delete from {type(self)}")

#     def pop(self, key, default=None):
#         raise RuntimeError(f"Cannot pop from {type(self)}")

#     def clear(self):
#         raise RuntimeError(f"Cannot clear {type(self)}")

#     def setdefault(self, key, default=None):
#         raise RuntimeError(f"Cannot setdefault in {type(self)}")

#     def popitem(self):
#         raise RuntimeError(f"Cannot popitem from {type(self)}")

#     def update(self, *args, **kwargs):
#         raise RuntimeError(f"Cannot update {type(self)}")


# class RoDict(SetOnlyDict):
#     """
#     Dict that prevents modification of the dict after initialization
#     """

#     def __setitem__(self, key, value):
#         raise RuntimeError(f"Cannot setitem in {type(self)}")


@total_ordering
class Codepoint(int):
    """
    Basically just an int but we can validate it and serialize it

    NOTE: Cannot be compared directly against int (or str) unless
          you cast the other value, or override all comparison
          functions
    """

    def __new__(cls, value: Union[int, str]):
        if isinstance(value, int):
            if not cls.is_valid(value):
                raise ValueError(f"Value is outside of valid codepoint range: {hex(value)}")
            return int.__new__(cls, value)
        elif isinstance(value, str):
            value = int(value, 16)
            if not cls.is_valid(value):
                raise ValueError(f"Value is outside of valid codepoint range: {hex(value)}")
            return int.__new__(cls, value)
        else:
            raise ValueError(f"Invalid value type {type(value)}")

    @staticmethod
    def is_valid(value: int) -> bool:
        return value >= 0 and value <= 0x10FFFF

    @property
    def char(self) -> str:
        """Return the Unicode character for this codepoint."""
        return chr(self)

    def __str__(self) -> str:
        return fmt_cp(self)

    def __repr__(self) -> str:
        return f"Codepoint({self})"

    def __hash__(self) -> int:
        return int.__hash__(self)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, (Codepoint, int)):
            return int.__eq__(self, other)
        return False

    def __lt__(self, other: object) -> bool:
        if isinstance(other, (Codepoint, int)):
            return int.__lt__(self, other)
        raise TypeError(f"Cannot compare Codepoint with {type(other)}")


class CodepointSet(object):
    """
    A set of Unicode codepoints.
    Which can be either single codepoints or ranges of codepoints.

    When codepoints or ranges are added, they are merged with existing ranges
    if they overlap or are adjacent.
    """

    def __init__(self) -> None:
        self.ranges: List[Tuple[int, int]] = []  # Sorted list of (start, end) tuples

    def add(self, start: int, end: Optional[int] = None) -> None:
        """Add a single codepoint or a range of codepoints."""
        if end is None:
            end = start  # Single codepoint

        if not self.ranges:
            self.ranges.append((start, end))
            return

        # Find insertion point using binary search
        insert_pos = bisect.bisect_left(self.ranges, (start, end))

        # Check if we can merge with previous range
        if insert_pos > 0:
            prev_start, prev_end = self.ranges[insert_pos - 1]
            if prev_end >= start - 1:  # Overlapping or adjacent
                start = min(start, prev_start)
                end = max(end, prev_end)
                insert_pos -= 1  # We'll replace the previous range

        # Check if we can merge with next range(s)
        merge_end = end
        while insert_pos < len(self.ranges):
            current_start, current_end = self.ranges[insert_pos]
            if current_start <= end + 1:  # Overlapping or adjacent
                merge_end = max(merge_end, current_end)
                del self.ranges[insert_pos]
            else:
                break

        # Insert the merged range
        self.ranges.insert(insert_pos, (start, merge_end))

    def update(self, other: "CodepointSet"):
        """
        Update this set with ranges from another CodepointSet.
        """
        for start, end in other.ranges:
            self.add(start, end)

    def __contains__(self, cp: Codepoint) -> bool:
        """Check if a codepoint is in any of the ranges using binary search."""
        # Find the first range that might contain the codepoint
        idx = bisect.bisect_right(self.ranges, (cp, float("inf"))) - 1
        if idx >= 0:
            range_start, range_end = self.ranges[idx]
            return range_start <= cp <= range_end
        return False

    @property
    def first(self) -> int:
        """Return the first codepoint in the set."""
        return self.ranges[0][0] if self.ranges else 0xFFFF

    @property
    def last(self) -> int:
        """Return the last codepoint in the set."""
        return self.ranges[-1][1] if self.ranges else 0xFFFF

    def num_ranges(self) -> int:
        """Return the number of ranges held."""
        return len(self.ranges)

    def __len__(self) -> int:
        """Return the total number of codepoints in all ranges."""
        return sum(end - start + 1 for start, end in self.ranges)

    def __iter__(self) -> Iterator[int]:
        """Iterate through all individual codepoints in all ranges."""
        for start, end in self.ranges:
            yield from range(start, end + 1)

    def __repr__(self) -> str:
        return f"CodepointSet({self.ranges})"

    def get_ranges(self) -> List[Tuple[int, int]]:
        """Return the list of ranges."""
        return self.ranges.copy()

    def __bool__(self) -> bool:
        return bool(self.ranges)

    def __copy__(self):
        obj = CodepointSet()
        obj.ranges = self.ranges.copy()
        return obj


class Decomp(object):
    __slots__ = ("_cp", "_dctype", "_dcvals")

    def __init__(
        self,
        cp: Codepoint,
        dctype: DecompositionType,
        dcvals: Iterable[Codepoint],
    ):
        super().__init__()
        self._cp = cp
        self._dctype = dctype
        self._dcvals = tuple(x for x in dcvals)  # Immutable

    @property
    def cp(self) -> Codepoint:
        return self._cp

    @property
    def dctype(self) -> DecompositionType:
        return self._dctype

    @property
    def dcvals(self) -> Tuple[Codepoint, ...]:
        return self._dcvals

    # def __contains__(self, key):
    #     if key in self.__slots__:
    #         return True
    #     return super().__contains__(key)


@total_ordering
class CombinerEntry(object):
    """
    Combiner Entry
    Sorted by combiner
    """

    __slots__ = ("_combiner", "_composed")

    def __init__(self, combiner: Codepoint, composed: Codepoint):
        self._combiner = combiner
        self._composed = composed

    @property
    def combiner(self) -> Codepoint:
        return self._combiner

    @property
    def composed(self) -> Codepoint:
        return self._composed

    def __str__(self):
        return f"{self._combiner} -> {self._composed}"

    def __lt__(self, other):
        return self._combiner < other._combiner

    def __eq__(self, other):
        return self._combiner == other._combiner

    def __hash__(self):
        return hash(self._combiner)


class Combiners(object):
    __slots__ = ("_index", "_starter", "_entries")

    def __init__(self, starter: Codepoint) -> None:
        self._starter = starter
        self._entries: List[CombinerEntry] = []

    def add_entry(self, combiner: Codepoint, composed: Codepoint) -> None:
        entry = CombinerEntry(combiner, composed)
        idx = bisect.bisect_left(self._entries, entry)
        if idx < len(self._entries) and self._entries[idx] == entry:
            # debug print
            print(f"starter: {self._starter}")
            print(f"idx: {idx}, len: {len(self._entries)}")
            print(f"entry: {entry}")
            print(f"self._entries[idx]: {self._entries[idx]}")
        assert idx == len(self._entries) or self._entries[idx] != entry
        self._entries.insert(idx, entry)

    def set_index(self, index: int) -> None:
        self._index = index

    @property
    def index(self) -> int:
        return self._index

    @property
    def starter(self) -> Codepoint:
        return self._starter

    @property
    def entries(self) -> List[Tuple[Codepoint, Codepoint]]:
        return [(x.combiner, x.composed) for x in self._entries]

    @property
    def num_entries(self) -> int:
        return len(self._entries)


class Mapping(object):
    __slots__ = ("_val", "_idx")

    def __init__(self, value: Union[Codepoint, Iterable[Codepoint]]) -> None:
        self._idx = -1
        if isinstance(value, Codepoint):
            self._val = [value]
        else:
            self._val = [x for x in value]

    @property
    def value(self) -> List[Codepoint]:
        return self._val

    @property
    def index(self) -> int:
        return self._idx

    @property
    def key(self) -> str:
        return fmt_cp_key_list([x for x in self.value])

    @property
    def is_multi(self) -> bool:
        return len(self.value) > 1

    def set_index(self, idx: int) -> None:
        if self._idx != -1:
            raise ValueError("Index already set")
        if idx < 0:
            raise ValueError("Index must be positive")
        self._idx = idx


class CodepointInfo(object):
    __slots__ = (
        "cp",
        "gc",
        "flags",
        "ccc",
        "mapping_idx",
        "decomp_idx",
        "combiner_idx",
        "casefold",
        "to_upper",
        "to_lower",
        "to_title",
        "decomp",
        "codepoints",
        "propindex",
    )

    def __init__(self, cp: Codepoint, gc: GeneralCategory):
        # cp is the codepoint, or when codepoints are set, the
        # first codepoint of all with same data
        self.cp = cp
        self.gc = gc
        self.flags: int = 0
        self.ccc: int = 0
        self.mapping_idx: int = 0
        self.decomp_idx: int = 0
        self.combiner_idx: int = 0

        # Data to hold for other tables
        self.casefold: Optional[Mapping] = None
        self.to_upper: Optional[Mapping] = None
        self.to_lower: Optional[Mapping] = None
        self.to_title: Optional[Mapping] = None
        self.decomp: Optional[Decomp] = None
        # Codepoints sharing the same properties,
        # either through ranges in UnicodeData.txt or
        # through table compression
        self.codepoints: CodepointSet = CodepointSet()
        self.propindex = 0  # No index

    @property
    def data_key(self) -> str:
        return (
            f"{self.flags}-{self.gc.value}-{self.ccc}-{self.mapping_idx}-"
            f"{self.decomp_idx}-{self.combiner_idx}"
        )

    @property
    def has_mapping(self) -> bool:
        return (
            self.casefold is not None
            or self.to_upper is not None
            or self.to_lower is not None
            or self.to_title is not None
        )

    @property
    def casefold_index(self) -> int:
        if self.casefold is None:
            return 0
        return self.casefold.index

    @property
    def to_upper_index(self) -> int:
        if self.to_upper is None:
            return 0
        return self.to_upper.index

    @property
    def to_lower_index(self) -> int:
        if self.to_lower is None:
            return 0
        return self.to_lower.index

    @property
    def to_title_index(self) -> int:
        if self.to_title is None:
            return 0
        return self.to_title.index

    @property
    def has_decomp(self) -> bool:
        return self.decomp is not None

    @property
    def dctype(self) -> Optional[DecompositionType]:
        if self.decomp is None:
            return None
        return self.decomp.dctype

    @property
    def dcvals(self) -> Optional[Tuple[Codepoint, ...]]:
        if self.decomp is None:
            return None
        return self.decomp.dcvals

    @property
    def has_combiners(self) -> bool:
        return self.combiner_idx != 0

    @property
    def is_template_printable(self) -> bool:
        """
        Printable as a stand-alone character for use in templates
        Whitespace and other problematic categories are removed
        even though they contain printable characters.
        """
        return not (
            self.gc >= GeneralCategory.Zs
            # or (GeneralCategory.Mn <= self.gc <= GeneralCategory.Me)
            or self.gc == GeneralCategory.Po
            or self.gc == GeneralCategory.So
        )

    @property
    def cp_comment(self) -> str:
        num = len(self.codepoints)
        if num == 0:
            assert self.propindex == 0
            return "Default properties, no assigned codepoints"
        if num == 1:
            return fmt_char(self.codepoints.first, self.gc)
        else:
            return (
                f"{num} codepoints in range [{fmt_cp(self.codepoints.first)}, "
                f"{fmt_cp(self.codepoints.last)}]"
            )

    def __contains__(self, key):
        if key in self.__slots__:
            return True
        return super().__contains__(key)

    def equal_data(self, other: "CodepointInfo") -> bool:
        """Only compare data that will end up in property table"""
        for key in ["gc", "ccc", "mapping_idx", "decomp_idx"]:
            if getattr(self, key) != getattr(other, key):
                return False
        return True

    def __str__(self) -> str:
        return f"CodepointInfo({self.cp:x}, {self.gc})"

    def copy(self) -> "CodepointInfo":
        """
        For shallow copy for all but codepoints, which must use copy
        """
        new = CodepointInfo(self.cp, self.gc)
        new.flags = self.flags
        new.ccc = self.ccc
        new.mapping_idx = self.mapping_idx
        new.decomp_idx = self.decomp_idx
        new.combiner_idx = self.combiner_idx
        # Share references
        new.casefold = self.casefold
        new.to_upper = self.to_upper
        new.to_lower = self.to_lower
        new.to_title = self.to_title
        new.decomp = self.decomp
        # Explicitly copy codepoints
        new.codepoints = self.codepoints.__copy__()
        new.propindex = self.propindex
        return new
