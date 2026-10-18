#!/usr/bin/env python3
# EXPERIMENT (not for merge): makes the star catalogue of the water experiment's night sky.
#
# Source: the Yale Bright Star Catalogue, 5th revised edition (Hoffleit and Warren, 1991), the file "catalog" of
# CDS/VizieR catalogue V/50 (https://cdsarc.cds.unistra.fr/ftp/V/50/), also at NASA HEASARC as BSC5. The catalogue is
# NASA public-domain data, free to use with a citation.
#
# The output (graphics::stars::ParseCatalogue reads it) is little-endian:
#   4 bytes   "OBSC"
#   uint32    version (1)
#   uint32    the number of stars
#   then for each star, 12 bytes:
#   float32   right ascension in radians (J2000)
#   float32   declination in radians (J2000)
#   int16     V magnitude times 100
#   int16     B-V colour times 1000, or -32768 when the catalogue does not give it
#
# Usage: python -I make_star_catalogue.py <catalog or catalog.gz> <output .bin>

import gzip
import math
import struct
import sys

NO_COLOUR = -32768


def field(line, first, last):
    """The text of the 1-based columns first to last, stripped"""
    return line[first - 1:last].strip()


def parse(line):
    """The star of a catalogue line as (ra, dec, vmag, bv), or None when it has no place or no magnitude"""
    ra_h, ra_m, ra_s = field(line, 76, 77), field(line, 78, 79), field(line, 80, 83)
    de_sign, de_d, de_m, de_s = field(line, 84, 84), field(line, 85, 86), field(line, 87, 88), field(line, 89, 90)
    vmag = field(line, 103, 107)
    if not ra_h or not de_d or not vmag:
        return None
    hours = int(ra_h) + int(ra_m) / 60.0 + float(ra_s) / 3600.0
    degrees = int(de_d) + int(de_m) / 60.0 + int(de_s) / 3600.0
    if de_sign == "-":
        degrees = -degrees
    bv = field(line, 110, 114)
    colour = int(round(float(bv) * 1000.0)) if bv else NO_COLOUR
    return math.radians(hours * 15.0), math.radians(degrees), int(round(float(vmag) * 100.0)), colour


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: make_star_catalogue.py <catalog[.gz]> <output.bin>")
    source = sys.argv[1]
    opener = gzip.open if source.endswith(".gz") else open
    with opener(source, "rt", encoding="ascii", errors="replace") as handle:
        stars = [star for star in (parse(line.rstrip("\n").ljust(197)) for line in handle) if star is not None]
    with open(sys.argv[2], "wb") as out:
        out.write(b"OBSC")
        out.write(struct.pack("<II", 1, len(stars)))
        for ra, dec, vmag, bv in stars:
            out.write(struct.pack("<ffhh", ra, dec, vmag, bv))
    print(f"{len(stars)} stars written to {sys.argv[2]}")


if __name__ == "__main__":
    main()
