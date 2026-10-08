# Creature casting

A creature can cast the miracles it has learnt. It chooses a miracle the way it chooses any action, to satisfy a desire:
lightning or a fireball when angry, a heal or food when kind, a spell on another creature when playful. It walks to a
good distance, turns, may draw the miracle's gesture, then takes up its casting pose while the miracle flows from its
hands. It pays with its own energy, not the player's prayer power, and a miracle it has only half learnt may fizzle.
What each miracle does belongs to [../miracles/](../miracles/); how miracles are learnt is in
[learning_by_observation.md](learning_by_observation.md); casting in a fight is in [fighting.md](fighting.md).

**Progress: 26/62 done, 5 partial — 46%**

## Which miracles it may try

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It may try a miracle once it has seen it half the times it needs to learn it | done | `creature_spell_casting::MayTry`, `CreatureMindSystem::MayCast`; test `CreatureSpellCasting.ItTriesAtHalfTheSightingsAndFizzlesShortOfAllButOne` |
| A try short of all but one of the sightings it needs fizzles, and counts as one more sighting, so trying teaches it | done | `creature_spell_casting::TrySucceeds`, `CreatureMindSystem::TryMiracle` |
| A fizzled try shows a light bulb over its head | todo | no light bulb effect |
| After a fizzle it is embarrassed, and one time in two sad as well | partial | `CreatureMindSystem::ShowFizzle`; the original's own reaction (a messed-up action with its help line) is not checked (unconfirmed) |
| It tries a power-up of a miracle only once grown past a stage | partial | the gate is in `MayCast` (`k_PowerUpPhase`), but no power-up action is carried out |
| How skilled it is at a miracle can make it miss its aim | todo | not modelled |
| Scripts can give it the most skill at a miracle | todo | not modelled |
| It can't cast a miracle at something it can't be cast at; trying leaves it frustrated, wanting food above all | done | `MagicSystem::CanCreatureCastAt`, `TryMiracle` |

## Paying for it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature pays with its body: chants for its size and strength over the energy it has above a floor | done | `creature_spell_casting::MostChants`, `ChantsToEnergy`; test `CreatureSpellCasting.ItPaysWithItsEnergyAndTires` |
| Paying costs energy (by a share for the kind of miracle) and tires it, by no more than seven tenths at a time | done | `creature_spell_casting::MaintainSpell`; test `CreatureSpellCasting.NoMoreThanItHasNorTiredByMoreThanSevenTenths` |
| It can't cast a miracle that would leave it too exhausted to pay for making it | done | `creature_spell_casting::CanCast`; test `CreatureSpellCasting.TooExhaustedItCantCast` |
| A miracle that lasts keeps drawing on the creature while it runs | done | `MagicSystem` charges the creature through its caster (`MaintainSpell`) |
| The player's prayer power is not spent on the creature's miracles | done | `MagicSystem::CastByCreature` starts the miracle with its own chants |
| In a fight each miracle costs stamina, and it can't cast one it hasn't the stamina for | done | `creature_spell_casting::StaminaCost`, `TryMiracle`; test `CreatureSpellCasting.FightStaminaAndMagnitude` |

## How it casts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One time in five it first shows how it feels: angry before lightning, kind before a helpful miracle, playful before a spell on a creature | done | `src/Creature/CreatureCastAgenda.cpp`; test `CreatureCastAgenda.OneTimeInFiveItShowsHowItFeelsFirst` |
| Lightning is cast from fifty away, backing off to ten more than its height | done | test `CreatureCastAgenda.LightningFromFiftyBackingOffToTenMoreThanItsHeight` |
| A helpful miracle is cast from twice its height; a spell on a creature from five times, backing off to twice | done | `src/Creature/CreatureCastAgenda.h` |
| Getting away from the thing, it goes to the nearest clear area as wide as it is tall | done | `src/Creature/CreatureCastMoves.cpp`; test `CreatureCastMoves.TheNearestClearAreaIsTheMiddleOfAClearSquare` |
| It turns to face the thing, within an eighth of a turn | done | `creature_cast_moves` |
| Going near gets stuck: it gives up and holds back that desire for a while | done | `CreatureMindCasting.cpp` (`GoNear`, `StepSubMove`) |
| It draws the miracle's gesture in the air before casting | partial | the agenda has a gesture step (`CastAt`), but the game's tables give the creature's miracles no gesture, and the drawn shapes (circle, star, spiral, square wave, kiss, square, triangle, S, V, moon, heart, bow tie) aren't played (unconfirmed which are used) |
| It takes up its casting pose: start, loop, end; the miracle is cast as the loop begins, held three seconds, and let go as the pose ends | done | test `CreatureCastAgenda.TheMiracleIsCastAsThePoseLoopsAndHeldThirtyTurns` |
| Water is cast with a scattering pose instead | todo | not modelled |
| A move the body can't make gives up the rest of the casting | done | test `CreatureCastAgenda.AMoveTheBodySaysFailedGivesUpTheRest` |
| The miracle is as big as what it is cast at (a little bigger than its radius, a creature by its height, a fire miracle by the caster's height) | done | `creature_spell_casting::CastMagnitude` |
| The miracle's effect flows from between the creature's hands towards what it is cast at, following its hands as it moves | done | `MagicSystem::CastByCreature`, `UpdateCreatureCast` |
| Food, wood and water are cast from a point above the target, with a beam from each of its hands to that point | done | `MagicSystem::CastByCreature`, `KeepCreatureBeams` |
| A miracle that lasts only while held, such as lightning, stops when it lets go | done | `MagicSystem::ReleaseCreatureCast` |
| The miracle runs for the creatures' own time from the tables | done | `magic::GetTimerWhenCreatureCasting` |
| Its town fears or respects it for what it casts | partial | towns watch what creatures do (`CreatureObjectActionSystem`); casting's share is unconfirmed; see [town_actions.md](town_actions.md) |

## Choosing a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Angry, it casts lightning at something | done | `src/Creature/CreaturePlanActions.cpp` ("CastLightningBolt") |
| Kind, it heals a villager hurt to seven tenths of its life or less | done | "CastHealSpell", `k_HurtEnoughToHeal` |
| Playful or spiteful, it casts a spell on another creature: freeze, small, big, weak, strong, fat, thin, invisible, nice, angry, hungry, frightened, tired, ill, thirsty, itchy | done | "CastMakeCreature..." executors; the spells' effects in [../miracles/](../miracles/) |
| It casts a spell meant to amuse another creature | partial | spells on creatures are cast, but the choice of an amusing one is not modelled separately |
| Angry, it casts a fireball or an explosion | todo | the body can cast them when told (debug), but the mind never chooses them |
| It casts a lightning storm or a tornado | todo | as above |
| It casts the power-ups of lightning, fireball, explosion, food, heal and shield | todo | no power-up actions are carried out |
| It casts a miracle to impress villagers | todo | not modelled |
| It casts food by a worship site, or into the storehouse | todo | see [town_actions.md](town_actions.md) |
| It casts wood by a building site or the workshop | todo | see [town_actions.md](town_actions.md) |
| It sprinkles water on crops or waters trees for a town | todo | see [town_actions.md](town_actions.md) |
| It puts out a fire with water | todo | see [town_actions.md](town_actions.md) |
| It casts a forest | todo | not modelled |
| It shields a town, or casts a physical shield | todo | not modelled |
| It looks for a town of its own that needs a miracle, and decides whether the nearest town should be helped or attacked | todo | not modelled |
| It heals itself | todo | not modelled |
| On fire, it puts the fire out on itself with water | todo | not modelled |
| It casts teleport to travel to something, or explores and casts teleport | todo | not modelled; it does walk between teleport stones (`src/ECS/Systems/Implementations/TeleportSystem.cpp`) |
| It copies a miracle it saw its god cast | todo | not modelled; see [learning_by_observation.md](learning_by_observation.md) |
| It shows a friend how to cast fireball, lightning, the storm, food or wood | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It casts warming or cooling spells, or cures illness, on a friend | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It swaps minds with another creature | todo | not modelled |

## One-off miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a one-off miracle, it casts it to attack | todo | not modelled |
| Holding a one-off miracle, it casts it to help | todo | not modelled |
| Holding a one-off miracle, it casts it in play | todo | not modelled |
| Holding a one-off miracle, it casts it to restore its health | todo | not modelled |
| It picks up a one-off miracle lying about and casts it, for any of those four | todo | not modelled |
| The player can hand it a one-off miracle | todo | giving things to the creature isn't modelled; see [object_actions.md](object_actions.md) |
| It steals miracles and miracle seeds | todo | not modelled |
| Catching a fireball, it throws it back | todo | not modelled |

## Telling it to cast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The debug tools and testbed can tell a creature to cast any miracle at something | done | `CreatureMindSystem::TellCast`, `src/Debug/TestbedScenarioCreatureCasting.cpp` (openblack only) |
| Scripts can make a creature cast a miracle | todo | the script functions that force creature actions are stubs in `src/CHLApi.cpp` |
