# How a computer god uses miracles

A computer god casts the miracles its towns and temple give it, as a person does: it picks the best miracle of a kind
for the job, makes sure it has the prayer power, charges the icon if needed and casts it at the right place. The land
scripts also cast miracles in a god's name, from where its hand is, for staged attacks.

**Progress: 1/38 done, 9 partial — 14%**

See [../miracles/](../miracles/) for each miracle, and [ai.md](ai.md) for when a god wants to cast.

## Which miracles it has

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god has the miracles of the towns that believe in it, as the person does | todo | `CREATE_NEW_TOWN_SPELL`/`CREATE_TOWN_SPELL` are not implemented; see [../worship/](../worship/) |
| Its miracles sit as icons in its temple, charged from its prayer power | todo | see [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| It needs prayer power like any player | todo | each player can carry a `PrayerPower` component, but nothing gives a computer god one or spends it |
| It looks for an icon needing charge and charges it up when its worshippers can | todo | the "charge up" desire under worshippers and defeating players |
| Scripts can top up an icon's prayer power, as Khazar's is for six wood miracles | todo | `GAME_SET_MANA` is a stub |
| Scripts ask how much prayer power a miracle costs | todo | `GET_MANA_FOR_SPELL` is a stub |

## Choosing a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It picks its best aggressive miracle for hurting a town or people | todo | |
| It picks its best aggressive miracle against a creature | todo | |
| It picks its best compassionate miracle | todo | |
| It picks its best impressive miracle, and judges how well one would impress where it is cast | todo | |
| It picks its best shield, judging whether a shield is possible at all | todo | |
| For each kind (food, heal, water, wood, shield, physical shield, aggressive, compassionate, impressive) it asks whether it has enough prayer power | todo | |
| It weighs how well a miracle would work at a place before casting it there | todo | |
| A desire to cast is weighed with how long until the miracle is ready | todo | |

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It casts a miracle either from an icon or as if by gesture | todo | |
| Miracles can be cast in another player's name, belonging to them | done | `MagicSystem::CastAtPoint` takes the casting player; testbed scenarios cast as the second player (`src/Debug/TestbedScenarioMiracles.cpp`) |
| A miracle a computer god casts is timed by its own timer from the miracle tables, not the person's | partial | the timer is read (`src/Debug/Magic.cpp` shows it) but nothing casts as a computer god |
| Food on a town's storehouse, on a worship site, or on its starving creature | todo | |
| Wood on a building site, on the workshop, or on a storehouse | todo | Khazar's wood on the player's storehouse is this, queued four times |
| Water on a burning building | todo | |
| Heal at its worship site, and on its hurt creature | todo | |
| A shield over an unprotected building, a town object, or against a belief attack | todo | |
| A physical shield against things thrown at it | todo | |
| Aggressive miracles at a town it wants to destroy | todo | |
| Aggressive miracles at a creature it is attacking | todo | |
| Impressive miracles at a town it wants to win | todo | |
| Miracles to meet a town's needs (food, wood) to win it | todo | |
| A miracle on its own creature for a laugh (starts at weight 0) | todo | |
| A creature that sees a computer god's ordinary miracle reacts to it but doesn't learn it, unlike a person's | partial | openblack treats every player but the first and the neutral one as a computer god here, where the game asks whether the player is a computer god (see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)) |

## Miracles the scripts cast for a god

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts cast a miracle from a god's hand position at a target, with radius, time and curl | partial | `SPELL_AT_POS` casts it, but the hand position is a stub (it comes from the land's origin) and the miracle belongs to no one |
| Lethys's fake attacks on the second land: three level-two fireballs | partial | see [lethys.md](lethys.md) |
| Nemesis's fireball volleys on Khazar's land from the vortex, curling left, straight and right | partial | `SPELL_AT_POS`; see [khazar.md](khazar.md) |
| Explosions on temples and wonders from a point above them | partial | as above |
| Nemesis's physical shield over his last town every 30 seconds | partial | `SPELL_AT_POS` casts a physical shield; the loop around it waits on other stubs (see [nemesis.md](nemesis.md)) |
| A miracle on a creature (strength on Lethys's creature, invisibility on Nemesis's) | todo | `SPELL_ON_*` for creature miracles; see [../miracles/creature_spells.md](../miracles/creature_spells.md) |

## What it remembers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It records the miracles other players cast near it, by kind | partial | each player keeps its last cast and a count by type (`Player::lastCast`, `castsOfType`); no god reads them |
| It reacts to a player casting at it by using its creature as a diversion | todo | |
| It records the damage each player's miracles did to what is its own | partial | `Player::damageFrom`; no god reads it |
