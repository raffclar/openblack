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

   Add `OPENBLACK_LANDGEN_FORMATIONS=0` for the same land with no fairy rings and no lone pillars (the land before
   them). Add `OPENBLACK_LANDGEN_TREES=<hundredths>` for a land with more stands of trees, as `--landgen "7;trees=2600"`
   gives in the game (the tree benchmarks).

2. Compare: `python -I tools/landgen/measure.py compare <folder>/seed1.json <folder>/seed7.json`.
   `generated` prints the numbers only.

The status of a number is `ok` inside the vanilla range, `near` within a tenth of the range width outside it, and `OUT`
beyond that.

## Make the targets again

`python -I tools/landgen/measure.py vanilla` measures the eight vanilla lands again and writes `vanilla_targets.json`.

## What the numbers are

The distance from the sea starts at the water of altitude 1 or lower, so that a lake above the sea on a generated land does not count as the sea for the coast numbers. Heights are in metres (the altitude times 0.67), slopes in degrees from the corner gradient with cells of 10 m, and
distances in metres. A generated land can be higher than the lands of the story (the user allowed it on 2026-11-17,
deviation 233): its dump has the height of each corner in units of 0.67 m ("units") and the metres of each altitude
("metres"). For such a land, the height targets and the thresholds of `above_100m_pct` and `peaks_60m` are the vanilla
altitudes put through the curve. The slope, cliff, flat and summit-count numbers keep the vanilla ranges. The area and
the totals that grow with the area (`land_km2`, `coast_km`, `trees`, `rocks`, `rock_clumps`, `big_forests`, `mushroom_clumps`, `toadstools`) show as
`free` for a generated land: the user allowed lands over 4 km2 on 2026-11-17. A group of trees links trees that are 15 m or less apart. A clump of rocks links rocks that are
20 m or less apart. The rock numbers also give the clumps on each square kilometre, the median distance to the
nearest rock and to the nearest tree, the lean of the rocks (the larger of the two horizontal angles, in degrees) and
their height above the land (median and 90th percentile). `pillars` counts the pillar features. A dump feature has
11 values: kind, type, x, z, angle, size, largest size, forest, the two lean angles and the height above the land
(older dumps have the first 8). The big forest numbers count the big forests, the median distance to the nearest big forest and
the median distance from a big forest to the nearest tree. A dump mist has 6 values: x, z, the height above the land, the
colour (0xAARRGGBB), the size and the edge shrink.

The path numbers: `footpaths` counts the footpaths of a land of the story, and the paths between the towns and the
temple of a generated land. `path_length_median_m`, `path_length_max_m`, `path_winding_median` and `path_winding_p90`
give their length and their winding (the length over the straight distance between the ends). They are `free` for a
generated land: its paths join the towns, and are longer and wind more than the footpaths of the story (deviation 233,
"LGX-4 yes"). `lanterns` counts the street and country lanterns, and `lantern_spacing_median_m` is the median distance
from a lantern to the nearest lantern. A dump path is 1 when it has lanterns (else 0), then x and z in metres of each
point. A dump lantern is 0 for a street lantern or 1 for a country lantern, then x and z in metres.

The ground numbers of the trees, the rocks and the cursed places (`tree_height_median_m`,
`_p90_m`, `_max_m` and `_slope_p90_deg`, the same for `rock_` and `cursed_`) are the height in metres and the slope
of the corner nearest each thing, as the game draws the land. The burnt trees count as cursed things, not as trees.
A clump of mushrooms links the magic mushrooms, Champi mushrooms and toadstools that are 25 m or less apart, and is
not a fairy ring: `mushroom_clumps(_per_km2)`, `mushroom_clump_size_max` and `toadstools(_per_km2)`.

The formation numbers: `magic_mushrooms` counts the magic mushrooms (loose object 18; a dump feature of kind 4 is a
loose object) and `fairy_rings` the groups of six or more magic mushrooms 10 m or less apart that make a ring round their
middle (`fairy_rings_per_km2` for each km2). `lone_pillars` counts the fat pillars, the pillars and the second and third
pillars (not the spiky sea stacks), `lone_pillars_per_km2` and `lone_pillar_groups_per_km2` (pillars 50 m or less apart
are one group) give them for each km2, and `lone_pillar_group_gap_median_m` is the median distance from a group to the
nearest other group. Only three of the eight lands of the story have lone pillars and two have a fairy ring, so the
lowest value of each of these numbers is 0. The four pillars on the summit of the third land count as one group.

The cliff numbers come from `cliffs.py`. A cliff corner has a slope of 40 degrees or more, and a cliff band is a group of
20 or more cliff corners that touch. `cliff_face_median_deg`, `cliff_face_p90_deg` and `cliff_over_60_pct` give the
slope of the faces. `cliff_gorge_pct` is the share of the cliff corners that face another cliff across a valley of 80 m
or less. `cliff_bands_per_km2`, `cliff_length_median_m`, `cliff_length_p90_m` and `cliff_width_median_m` give the size of
the bands (the length along the main axis of a band), and `cliff_rise_p90_m` the rise of the high bands. Each band is on
the coast (30% of it or more within 60 m of the sea), on a mountain side (its top at 100 m of the story or higher) or on
a low terrace: `cliff_coast_pct`, `cliff_mountain_pct` and `cliff_terrace_pct` give the share of the cliff corners.

`cursed_patches` counts the cursed places: the groups of burnt trees, craters and dark mists that are 150 m or less
apart. A dark mist has no colour channel above 0x50 (the near black mist of Land 1, the dark mist of Land 4 and the grey
smoke of the Land 5 craters). `dark_mists` counts the dark mists.

The swamp numbers: `wet_pct` is the share of the land whose main ground is wet (river mud, wet earth, wet gravel, wet
solid rock, wet grass or rushes on a land file; mud and wet grass on a generated land). `wet_trees_per_km2` counts the
trees on that wet ground for each square kilometre of it. `green_mists_per_km2` and `green_mist_sheets_per_km2` count
the puffs of the dark green mist of Land 4 (colour 50632F) and their sheets (puffs 70 m or less apart) for each square
kilometre of land. The cover of whole-swamp mist on generated lands (deviation 279) puts `green_mists_per_km2` far
above the vanilla range on purpose.

The town numbers: `towns` counts the towns with the player's towns, `neutral_towns` the towns of no player and
`town_tribes` the tribes. `town_nn_median_m` and `town_nn_min_m` are the median and the least distance from a town to
the nearest other town. `town_height_median_m` and `town_sea_median_m` are the medians of the height of the town
centres and of their distance from the sea. `town_houses_median`, `town_villagers_median` and `town_fields_median` are
the medians over the towns of five buildings or more (the lands of the story also have hamlets of one to three
buildings). A dump town has 8 values: x, z, the tribe, 1 for a neutral town, the buildings, the houses, the fields and
the people. `towns_per_km2` is free for a generated land: its towns keep the vanilla count and the vanilla distances,
so a land larger than the lands of the story has fewer towns for each km2.

The lake shore numbers (generated lands only, from the `fresh` and `lake_level` lists of a newer dump): the lake water
is the still fresh water at the sea level and the water of the high lakes at their own levels. `lake_edge_sand_pct`
is the share of the dry cells next to the lake water whose main ground is sand (also wet sand), and
`sea_edge_sand_pct` the same next to the sea. `lake_sand_width_median_m` and `lake_sand_width_p90_m` are the median
and the 90th percentile of the distance from the water of the sand within 200 m that the lakes are nearer to than the
sea (`sea_...` for the sea). `lake_strip_height_m` is the median height over the level of the nearest lake of the
ground 10 to 40 m from the lake water, as `shore_strip_height_m` measures the sea shore. The lands of the story have
no targets for these numbers: compare the lake numbers with the sea numbers of the same land.
