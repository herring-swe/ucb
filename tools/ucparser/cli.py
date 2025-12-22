# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

import os
import sys
import logging
from argparse import ArgumentParser

from .parser import UnicodeDataParser, ParseError
from .stats import print_stats


def main():
    ap = ArgumentParser(description="Unicode Character Database parser")
    ap.add_argument("--debug", action="store_true", help="Enable debug output")

    args = ap.parse_args()

    logging.basicConfig()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    required_files = ["UnicodeData.txt", "CaseFolding.txt"]
    if any(not os.path.exists(fn) for fn in required_files):
        print(
            f"Error: Required files not found. Please ensure {', '.join(required_files)} are in the current directory.",
            file=sys.stderr,
        )
        exit(1)

    try:
        ucparser = UnicodeDataParser()
        ucparser.parse_all()
        ucparser.write_templates()
        print_stats(ucparser)
    except ParseError as e:
        print(f"Parse error: {e}")
        exit(1)


if __name__ == "__main__":
    main()
