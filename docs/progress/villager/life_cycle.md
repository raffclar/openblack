# Villager life cycle

Villagers are born, grow up, pair off, have children and die of old age. Each belongs to a tribe and a town, and lives
in an abode with a family.

**Progress: 4/28 done, 6 partial — 25%**

## Creation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers placed by the land script at a position, near an abode, with a tribe, job and age | partial | `src/LHScriptX/FeatureScriptCommands.cpp` (CREATE_VILLAGER_POS) and `src/ECS/Archetypes/VillagerArchetype.cpp`; the creation-time draws of age, scale and food that the game makes are not kept |
| Town villagers created straight into a town by script (by town, by type, special ones) | todo | CREATE_VILLAGER, CREATE_TOWN_VILLAGER and CREATE_SPECIAL_TOWN_VILLAGER only log "not implemented" |
| A new villager joins the nearest town and an abode in it with room | partial | `src/ECS/Systems/Implementations/TownSystem.cpp`; the abode's list of inhabitants is not filled for scripted villagers (only the testbed fills it), so room is never used up |
| A villager made over water starts drowning instead of standing | todo | |
| A newly made villager waits a random while before its first decision | done | `src/ECS/Systems/Implementations/LivingActionSystem.cpp` (the created state) |
| Sex follows the job: housewives are women, the other jobs men | done | `src/ECS/Archetypes/VillagerArchetype.cpp` |
| Each villager has its own small difference in speed and size from when it was made | partial | the speed factor is ported in `src/ECS/VillagerSpeed.cpp` (test `test_villager_speed.cpp`); size by age is not |

## Age and growing up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Age counts in game time, a year every fixed number of turns, with birthdays | todo | `Villager::age` is set once at creation and never advances |
| Children are smaller, scaled up with age until adult size | todo | all villagers draw at scale 1 |
| Children use the child models of their tribe | todo | every villager draws its job's adult model |
| A child becomes an adult at the age of growing up, takes a job and the town's counts change | todo | the child/adult split is only read at creation (`VillagerArchetype.cpp`) |
| Old villagers walk slower; young ones too | done | `src/ECS/VillagerSpeed.cpp` (age factor), tested in `test_villager_speed.cpp` |
| Villagers die of old age, more likely the older they get, checked at bed time | todo | `VillagerHome.cpp` notes it as TODO in going to bed |

## Families and children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A couple shares an abode; a man and a woman are spouses | todo | |
| Couples have sex at home at night, more often when the town wants children | todo | states for sex at home are unported |
| Couples meet outside and have sex when promiscuous or waiting for a mate | todo | |
| A woman falls pregnant and gives birth after a pregnancy, a child appearing at home | todo | |
| The town's desire for children drives how many are born | partial | the desire is worked out (`src/ECS/TownDesire.cpp`), nothing acts on it |
| Children follow their mother about | todo | |
| Children go to the crèche when the town has one, and play there | todo | See ../building/civic_buildings.md |
| Children whose mother dies are orphaned and mourn her | todo | |
| Children play in the town rather than work | todo | See play_and_gossip.md |
| Breeder disciples raise the birth rate of the town they are put in | todo | See disciples.md |
| Abodes swap a man for a woman between them so couples can form | todo | |

## Tribes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine tribes, each with its own villager models | done | `src/Enums.h`, models from the villager info table in `VillagerArchetype.cpp` |
| Each tribe has its own speed and need tables in the info data | partial | read through `src/InfoConstants.h`; only speed is used |
| A villager that moves into a town of another tribe changes tribe | todo | |
| A town's tribe decides its buildings and its tribal power to the player | partial | the tribe is kept on the town (`TownArchetype.cpp`); tribal powers are only read for the Indian speed and the Aztec chant (see ../town/wonder.md) |
