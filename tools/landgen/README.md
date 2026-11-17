# Land measuring tool

Read only tools that measure lands with the same numbers: the lands of the story, and the lands of the generator in
`components/landgen`. They read the game data and never write a landscape file.

Python 3 with numpy and scipy is needed.

## Files

- `lndread.py`: reads a `.lnd`, a land script and a `.fot` file.
- `analyze.py`: the full report of the vanilla lands (`measurements.md`, `objects.json`, `summary.json`). Run it in an
  empty folder: `python -I /path/to/tools/landgen/analyze.py`. `coast.py`, `terraces.py`, `density.py` and
  `formations.py` print more tables from its output.
- `measure.py`: the numbers of one land, as a table next to the vanilla range.
- `vanilla_targets.json`: the lowest and highest value of each number over the eight vanilla lands.
- `test_measure.py`: tests of the pure measures (ctest name `test_landgen_measure`).

The game data folder is `C:/projects/black`. Set `OPENBLACK_GAME_DATA` to use another folder.

## Measure a generated land

1. Write the dump of the seeds. The test program writes one JSON file for each seed and does nothing without the
   folder:

   ```
   OPENBLACK_LANDGEN_DUMP_DIR=<folder> OPENBLACK_LANDGEN_SEEDS=1,7,42 \
     openblack_tests --openblack-test-source=landgen/test_landgen_dump.cpp
   ```

2. Compare: `python -I tools/landgen/measure.py compare <folder>/seed1.json <folder>/seed7.json`.
   `generated` prints the numbers only.

The status of a number is `ok` inside the vanilla range, `near` within a tenth of the range width outside it, and `OUT`
beyond that.

## Make the targets again

`python -I tools/landgen/measure.py vanilla` measures the eight vanilla lands again and writes `vanilla_targets.json`.

## What the numbers are

Heights are in metres (the altitude times 0.67), slopes in degrees from the corner gradient with cells of 10 m, and
distances in metres. A generated land can be higher than the lands of the story (the user allowed it on 2026-11-17,
deviation 233): its dump has the height of each corner in units of 0.67 m ("units") and the metres of each altitude
("metres"). For such a land, the height targets and the thresholds of `above_100m_pct` and `peaks_60m` are the vanilla
altitudes put through the curve. The slope, cliff, flat and summit-count numbers keep the vanilla ranges. The area and
the totals that grow with the area (`land_km2`, `coast_km`, `trees`, `rocks`, `rock_clumps`, `big_forests`) show as
`free` for a generated land: the user allowed lands over 4 km2 on 2026-11-17. A group of trees links trees that are 15 m or less apart. A clump of rocks links rocks that are
20 m or less apart. The rock numbers also give the clumps on each square kilometre, the median distance to the
nearest rock and to the nearest tree, the lean of the rocks (the larger of the two horizontal angles, in degrees) and
their height above the land (median and 90th percentile). `pillars` counts the pillar features. A dump feature has
11 values: kind, type, x, z, angle, size, largest size, forest, the two lean angles and the height above the land
(older dumps have the first 8). The big forest numbers count the big forests, the median distance to the nearest big forest and
the median distance from a big forest to the nearest tree. A dump mist has 6 values: x, z, the height above the land, the
colour (0xAARRGGBB), the size and the edge shrink. The generator dump has no footpaths yet, so this number is zero for
it.

`cursed_patches` counts the cursed places: the groups of burnt trees, craters and dark mists that are 150 m or less
apart. A dark mist has no colour channel above 0x50 (the near black mist of Land 1, the dark mist of Land 4 and the grey
smoke of the Land 5 craters). `dark_mists` counts the dark mists.
