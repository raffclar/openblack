# Dispensers and one-off seeds

Where miracles come from other than the player's own worship: miracle dispensers that grow a globe every so often,
one-shot globes placed by the lands' scripts, and the miracle icons at the worship site and village centres that
charge a miracle for the hand. Taking a globe and holding it is in [casting_and_globes.md](casting_and_globes.md).
Debug and testbed tools that make dispensers and globes don't count here.

Silver scrolls that give a dispenser are listed in [../story/rewards.md](../story/rewards.md) (among them the flying-flock
dispenser of [The Magic Dragon](../story/silver_scrolls/the_magic_dragon.md), the food dispenser of
[The Singing Stones](../story/silver_scrolls/the_singing_stones.md), the big dispenser of [The Sea](../story/silver_scrolls/the_sea.md) and the wolf-pack dispenser of
[The Slavers](../story/silver_scrolls/the_slavers.md)), with the reward script's refill-time quirk. Scrolls that give seeds: the itchy
creature seed of [The Heavenly Fire](../story/silver_scrolls/the_heavenly_fire.md), the heal chest of
[Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md) and the water seeds of [Throwing Stones](../story/silver_scrolls/throwing_stones.md).
Gold scrolls that hand out seeds or dispensers: the three fire seeds of [Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md),
the three shield seeds of [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md), the monk's two water one-shot
miracles in [Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md), the half-built shield dispenser of the fourth
land's arrival ([The Defending Ogres](../story/gold_scrolls/the_defending_ogres.md)) and the heal dispenser of
[I have a surprise for you.](../story/gold_scrolls/i_have_a_surprise_for_you.md#the-reward)

**Progress: 7/26 done, 2 partial — 31%**

## Dispensers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A dispenser is a building with the miracle-creator model, holding one miracle, which may be an extreme version | done | `MagicSystem::CreateDispenser` in `src/ECS/Systems/Implementations/MagicSystem.cpp` |
| It counts game turns only while active, holding a miracle, built and repaired, and makes its first globe after its full period (300 turns) | done | `src/Magic/DispenserRules.cpp`; tests `DispenserRules.ADispenserWaitsForItsBubbleToBeTakenThenMakesAnotherAfterItsPeriod`, `DispenserRules.AnInactiveDispenserOrOneWithoutAMiracleMakesNothing` |
| The globe floats at 1.2 times the dispenser's height and appears with a sparkle | done | test `DispenserRules.TheBubbleFloatsAboveItsDispenserAndFacesTheCamera` |
| Once the globe is taken or moved off, the count starts again | done | `DispenserRules.cpp` |
| A faint starry disk turns on the ground under every dispenser, always on, the same for every miracle | done | vortex effect in `MagicSystem.cpp`, surface-of-revolution drawing in `src/Particles/ParticleSurfaceRules.cpp`; testbed `miracles.dispenser_vortex` |
| The disk is hidden while the dispenser is being built | n/a | openblack dispensers are never under construction |
| Dispensers and globes make no sound of their own apart from the pop when taken | done | no dispenser sound, as the game |
| The lands' scripts place dispensers with their tribe, miracle, angle, size and period | todo | `CREATE_SPELL_DISPENSER` is a stub in `src/LHScriptX/FeatureScriptCommands.cpp`, so no dispenser appears in a real land |
| A dispenser belongs to a town | todo | nothing |
| Challenge scripts put a miracle into a dispenser or object or take it out | todo | `SET_MAGIC_IN_OBJECT` is a stub in `src/CHLApi.cpp` |
| Dispensers are saved and loaded with the game | todo | no save system |

## One-shot globes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lands' scripts place one-shot globes, plain or powered up | todo | `CREATE_ONE_SHOT_SPELL` and `CREATE_ONE_SHOT_SPELL_PU` are stubs in `src/LHScriptX/FeatureScriptCommands.cpp` |
| A globe is made for a miracle at its seed's power-up level | done | `MagicSystem::CreateOneOffSeedFor` |
| Lifting a tree or rock that a firefly hides in leaves a one-shot globe in its place, drawn by weight from the land's firefly reward table | todo | `CREATE_FIRE_FLY` and `FIRE_FLY_SPELL_REWARD_PROB` are empty stubs (`src/LHScriptX/FeatureScriptCommands.cpp`); owned by [../nature/fireflies.md](../nature/fireflies.md) |
| The scripted "falling miracle" sequence: a challenge script starts it with the same command that plays the intro film; it runs only when the player has a creature, and the screen fades back to normal as it starts | todo | `SET_AVI_SEQUENCE` is a stub in `src/CHLApi.cpp` (the `Falling` value exists in `src/ScriptHeaders/ScriptEnums.h`); what the sequence shows is not researched yet |
| Skirmish set-up chooses the one-shot miracles and miracles on offer | todo | see [../multiplayer/](../multiplayer/) |

## Seeds hidden in trees and rocks

No land hides a miracle seed under a tree, and no tree carries one: the one-shot globes the land scripts place all lie
in the open (none of the 244 placed on the story lands and skirmish maps is within 3 m of a tree), and shaking, burning
or felling a tree, or the creature lifting one, never reveals anything. The seed players remember finding under a tree
or rock comes from a firefly: by day fireflies hide in the trees and rocks nearest the houses and street lights they
hovered by, and when the player's hand uproots or lifts the one a firefly hides in, the firefly is gone and a one-shot
globe lies where it stood (the row above; [../nature/fireflies.md](../nature/fireflies.md)). Reward chests can also hold
a one-shot seed ([../hand/clicking_and_activating.md](../hand/clicking_and_activating.md)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Hermit's rock on Land 1: a strong creature miracle seed appears where the rock lay once the hand lifts it, the creature holds it or it is moved more than 5 m, and the advisors explain that fireflies hide under rocks at dawn and become seeds | todo | creating a seed from a challenge script does nothing yet; owned by [../story/silver_scrolls/the_hermit.md](../story/silver_scrolls/the_hermit.md) |

## Worship site and village-centre icons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's worship site shows one icon per miracle they have, in a fixed placement | todo | blocked: no worship sites in openblack's world |
| Converting a town adds its village-centre miracle (and power-ups) to the player's icons | todo | nothing; see [../town/](../town/) |
| Tapping an icon starts it charging, with a click whose pitch rises with the icon's place | todo | blocked on worship sites |
| Each charging icon takes an equal share of what the site can give each turn until it holds the miracle's cost to create | partial | pure rules in `src/Magic/WorshipBattery.cpp`; test `WorshipBattery.strainAndIconShare`; not used in the world |
| A charging icon shows a ring of light filling with the charge, a pulsing shine and a flash as it starts | todo | blocked on worship sites |
| A fully charged icon puts the seed into an empty hand, with a voice naming the miracle | todo | blocked on worship sites |
| Tapping a charging icon cancels it and gives the prayer power back | todo | blocked on worship sites |
| Scripts enable or disable a player's miracles and ask whether they have one | todo | `SET_PLAYER_MAGIC` and `HAS_PLAYER_MAGIC` are stubs in `src/CHLApi.cpp` |
| Scripts ask whether a miracle is charging and clear all charging | todo | `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING`, `CLEAR_PLAYER_SPELL_CHARGING` are stubs in `src/CHLApi.cpp` |
| In the testbed and debug tools a seed can be summoned straight into the hand as if from an icon | partial | `MagicSystem::SummonSeed` charges it from prayer power at once as a stand-in for the icon |
