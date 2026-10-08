# Prayer power

The magic the worshippers make. Each worship site stores it in a battery that its dancers fill, and the miracles draw on
it to be summoned, kept going and powered up.

**Progress: 1/22 done, 14 partial — 36%**

## Making it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each dancer chants a set amount a turn, from its tribe's worship site table | partial | `src/Magic/WorshipBattery.cpp`, test `test_worship_battery`; runs only in the debug sandbox (`src/Debug/Magic.cpp`) |
| The player's tribal power multiplies what is chanted; all sites use the same multiplier | partial | same rules, sandbox only |
| The battery's size is its tribe's base plus a share for each dancer | partial | same |
| An idle site fills its battery up to its size | partial | same |
| The dance speeds up as more is drawn and slows as the battery fills | partial | same |
| Demand beyond what the dancers can give is strain, which stops the icons charging | partial | same |

## Spending it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Summoning a miracle from an icon charges its cost to create from the site | partial | one store per player stands in for the sites: `src/ECS/Components/PrayerPower.h`, `src/Magic/PrayerRules.cpp` |
| A running miracle draws its upkeep each turn, topping itself up to its safety level | partial | from the per-player store, not the site |
| The icons charge between uses, sharing evenly what the site can give | partial | rules only (`WorshipIconShare`); no icons in the world |
| A reserve is kept for miracles already cast while any icon's seed is out | partial | rules only |
| A seed dropped or shaken off gives back what it holds to its site | partial | refunds go to the per-player store |
| Powering a miracle up costs more; any surplus goes back to the site | partial | drawn from the per-player store |
| Scripts' miracles (the neutral player's) cost nothing | done | `PlayerMaintainSpell` in `src/Magic/PrayerRules.cpp` |
| The prayer power the player has is shown (in the temple and on the icons) | todo | see `../interface/` |
| The player's statistics count the prayer power used | todo | shown as Total Chants Used ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |
| Prayer power can pay for influence away from the citadel (unconfirmed how it's used in the story) | todo | |

## Sacrifice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A living thing dropped on the site's altar is sacrificed for prayer power | todo | the hand can't hold villagers or animals yet; Land 2's sacrifice quest: [the_sacrifice.md](../story/silver_scrolls/the_sacrifice.md) |
| What a sacrifice gives is its kind's sacrifice value, from half to all of it by how much life it has left (unconfirmed that the scale is its life) | todo | |
| Sacrifice is an evil deed and moves the player's alignment | todo | see [alignment](alignment.md) |
| Scripts can read a player's sacrifice total | todo | stub in `src/CHLApi.cpp`; used by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) |

## Cheats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Infinite prayer power | partial | the testbed's and debug window's infinite store (`PrayerPower::infinite`) |
| Free upkeep for running miracles | partial | sandbox only (`WorshipBattery::freeMaintenance`) |
