# Info tables

Almost every number behind Black & White's objects, from a tree's wood to a miracle's cost or a creature's desires,
comes from one file of tables, `Scripts/info.dat`, loaded once when the game starts. The scripts' constant names come
from the same tables. A table is counted done here only when openblack reads it and its systems use it; how well each
system follows it is in that system's own domain.

**Progress: 6/36 done, 19 partial — 43%**

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tables are read once at start from the info file and shared by the whole game | done | `src/Parsers/InfoFile.cpp`, `Locator::infoConstants`, `Game::Initialize` |
| Each table's layout matches the game's, field by field | partial | `src/InfoConstants.h`; many fields are still unnamed |
| Things are looked up in the tables by their type names, ignoring case; an unknown name is logged and skipped | done | port notes (case-insensitive lookups) |
| The script language's constant names for objects and types come from the tables | done | `components/lhvmlang`; `ChlLanguage.ConstantsFromInfoTables` |

## Magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle's general table (cost, timers, gestures, cast type, power-ups, hold) | done | `MagicSystem`; `test_magic_tables`, `test_spell_chants`; see ../miracles/ |
| Each miracle family's own table (heal, teleport, forest, food, storm and tornado, shields, wood, water, flocks, creature spells) | partial | read; used miracle by miracle, see ../miracles/ |
| Magic effects and spell seeds (the orbs in the hand and the one-shot ones) | done | `test_magic_tables`; see ../miracles/ |
| Spot visuals and effects | partial | used by the particle and miracle effects; see ../rendering/ |
| Map shields and spell icons | todo | read, not used |

## Creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species' table and the differences between species | partial | species tables used widely; the per-species differences table unused; see ../creature/ |
| Creature actions and the desire tables (desires, sources, dependencies, attributes, starting values) | partial | used by the creature's mind; see ../creature/desires.md |
| Development phases and their lengths | partial | used for growth; see ../creature/ |
| Actions the creature copies from the player | partial | see ../creature/ |
| Creature pens | todo | read, not used |

## People and towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Abodes, town centres and storage pits | partial | used for building and placing them; see ../building/ |
| Villagers, their tribes and their state table (animations and flags per state) | partial | see ../villager/ |
| Special villagers | todo | read, not used |
| Jobs | todo | read, not used; see ../villager/ |
| The town, its desires and belief | partial | see ../town/ |
| Worship sites, prayer sites and icons, worship site upgrades | partial | worship sites used; prayer sites, icons and upgrades not; see ../worship/ |
| Alignment, influence, the player and reactions | partial | used; the miracle audit found values read wrongly in places; see ../worship/ |
| Dances | todo | read, not used |
| Totem statues | todo | read, not used |

## Nature and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, big forests and flowers | partial | see ../nature/ |
| Features, animated statics, mobile objects and mobile statics | partial | used to create and draw them; see ../nature/ and ../physics/ |
| Pots of food and wood | partial | see ../resources/ |
| Fields, field types and fish farms | partial | fields used; fish farms not; see ../resources/ |
| Animals and their state table | partial | the animals' table used, their state table not; see ../animal/ |
| Furniture, scaffolds and single-cell fixed objects | todo | read, not used |
| Terrain materials (what each ground type does to objects and sounds) | partial | see ../terrain/ |
| Weather climates | done | `WeatherSystem`; see ../weather/ |
| Speed thresholds | todo | read, not used |

## Help, sound and play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The help system, help spirits and help sprite guidance | todo | read, not used; see ../interface/ |
| The sound table | partial | used by the audio; see ../audio/ |
| Arrows, script highlights and showing a town's needs | todo | read, not used |
| The ball, football and playtime tables for the creature's games | todo | read, not used; see ../creature/ |
