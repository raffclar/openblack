# Wounds, burns and blood

Fights and fire leave marks on a creature's skin: cuts, bruises, grazes and burns that start fresh and red and fade
over time to old scars, and drops of blood that darken as they dry. The marks heal as the creature does and go once
they are old enough.

**Progress: 17/23 done, 1 partial — 76%**

## The marks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wounds are small pictures from an atlas of damage, 8 kinds by 8 pictures of 32 texels | done | `creature_marks`; damage atlases loaded in `Loaders.cpp` |
| Each wound has a place on one of the base mesh's skins, a kind and a picture | done | `creature_marks::Mark` |
| A wound starts with its fresh look and fades towards its old look as it ages | done | test `WoundsFadeFromFreshToOld` |
| Each kind of wound lasts its own number of healing steps | done | `k_WoundLifetimes`; test `MarksGoOnceTheirKindHasLasted` |
| Wounds are centred on their place and cut off at the skin's edges | done | test `WoundsAreCentredAndCutAtTheEdges` |
| A wound's colour is blended over the skin at 8 bits and cut back to 4 | done | test `ColoursBlendAt` |
| Blood is painted a texel at a time, red when fresh and browner as it dries, covering three quarters of the skin under it | done | test `BloodDarkensAsItDries` |
| Blood goes after a set number of steps | done | `k_BloodLifetime` |
| A creature keeps at most 1024 of each; a new one past that takes the oldest's place | done | test `AFullListLosesItsOldestMark` |
| The skin is painted tattoos first, then wounds, then blood | done | test `SkinsArePaintedTattoosThenWoundsThenBlood` |

## Where marks come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Blows in fights leave wounds where they land | done | `CreatureFightSystem` adds a wound for each blow; see [fighting.md](fighting.md) |
| Blows cut, bruise or graze by where they land; some kinds also bleed | partial | the kind of wound each blow leaves is guessed (`CreatureFight.h`), not known from the game |
| Bleeding wounds leave drops of blood | done | `CreatureFightSystem` |
| Fire and fireballs leave burns | todo | the burn kind exists (spawner `CreatureSpawnerAppearance.cpp`) but fire never adds one |
| Lightning and other harmful miracles cut and scar the creature | done | A miracle's crush cuts along the line from the map's corner to the groin, as the game does, through the game's skin-mark ray (`05c66517`, `6afe6217`) |
| Being hit by thrown objects marks it (unconfirmed) | todo | |
| The game keeps a map of where on the body damage has landed (unconfirmed what it drives) | todo | |

## Healing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Marks age a step every 600 counts of healing; a game turn heals by one count | done | `creature_marks::Heal`; test `MarksAgeEvery` |
| Healing goes fifty times faster while the creature heals quickly (asleep or resting, unconfirmed) | todo | the fast rate is defined but not used |
| The heal miracle heals marks by its strength | done | `MagicLiving.cpp` `HealMarks` |
| Old marks go once their kind has lasted, and the skin is repainted | done | `CreatureSkinSystem::ProcessTurn` |
| Marks are kept in saved games | todo | no saved games |
| The debug spawner can add wounds, burns and blood | done | `CreatureSpawnerAppearance.cpp`; testbed scenario `appearance.tattoos_wounds` |
