# Creature spells

Sixteen miracles that only work on a creature (freeze, small, big, weak, strong, fat, thin, invisible, compassion,
angry, hungry, frightened, tired, ill, thirsty, itchy). Each eases the creature into a changed state, holds it for a
time and eases it back. This file also covers creatures learning miracles by watching and casting miracles themselves.
The creature's own body, mind and leash are in [../creature/](../creature/).

**Progress: 60/72 done, 8 partial — 89%**

## Casting a creature spell

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature spell can only be cast on a creature (any creature: own, rival or wild), never at a point | done | `src/ECS/Systems/Implementations/MagicCreatureSpells.cpp` (CanCastOn) |
| It is refused when more than five spells already wait on that creature | done | `src/Creature/CreatureSpells.cpp` (QueueFull); `test/test_creature_spells.cpp` NoMoreThanFiveWait |
| It lasts 25 seconds times the power of its tribes (a different tribe or pair per spell) | done | `MagicCreatureSpells.cpp`; tribal powers are 1.0 until worship exists (see [prayer_cost.md](prayer_cost.md)) |
| The miracle stays open until the creature lets the spell go, then closes | done | `MagicCreatureSpells.cpp` |
| The player's hand pours a stream of magic onto the creature for four seconds | done | `MagicCreatureSpells.cpp` (StartHandGrain) |
| Creature spells reach the player from worship-site icons in some towns | todo | No spell icons or worship sites (see [../worship/](../worship/)) |
| Some come as one-off rewards from fireflies on Lands 2, 4 and 5 (freeze, small, big, weak, strong, invisible, compassion, angry, itchy; about 1% each); fat, thin and the five need spells are never in a firefly table | todo | `FIRE_FLY_SPELL_REWARD_PROB` is a stub in `src/LHScriptX/FeatureScriptCommands.cpp`; see [../nature/fireflies.md](../nature/fireflies.md) |

## How a spell takes hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell waits two seconds, eases in over its start time, holds, then eases out over its finish time | done | `src/Creature/CreatureSpells.cpp`; `test/test_creature_spells.cpp` ASpellStartsTwoSecondsAfterItIsCast, ASpellEasesInHoldsForItsTimeAndEasesOut |
| Start and finish times differ per spell (freeze 2 s, size and strength 4 s, invisible 5 s, compassion 3 s, others 1 s) | done | Read from the miracle tables; `test/test_creature_spells.cpp` TimesAreWholeTurns |
| Casting the same spell again adds its time and takes over from the older miracle | done | `test/test_creature_spells.cpp` CastingTheSameSpellAgainAddsItsTimeAndTakesTheNewMiracle |
| The same spell cast while it is wearing off waits and starts again afterwards | done | `src/Creature/CreatureSpells.cpp` |
| An opposing spell of the same kind cuts the first short and waits for it | done | `test/test_creature_spells.cpp` TheOpposingSpellCutsTheFirstShortAndWaitsForIt |
| Spells of different kinds run together | done | `test/test_creature_spells.cpp` SpellsOfDifferentKindsRunTogether |
| A spell whose miracle disappears ends early | done | `src/Creature/CreatureSpells.cpp` |
| A creature that faints brings every spell to its end | done | `test/test_creature_spells.cpp` AFaintBringsEverySpellToItsEnd |
| With reversion turned off by a script, a spell stops where it is and never puts the creature back | done | `test/test_creature_spells.cpp` WithoutReversionASpellStopsWhereItIs; `CREATURE_SPELL_REVERSION` in `src/CHLApi.cpp` |
| The creature's mind is told when a spell starts and when it ends (for the advisor's help) | todo | No spell-started/finished events in the creature mind, and no creature help messages |
| A creature with spells on it is saved as it was before them (size, strength, alignment) | partial | Mind files save the "before" values (`30564213`; ItIsSavedAsItWasBeforeItsSpells); full game saves don't exist |

## The sixteen spells

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Freeze: the creature gives up what it was doing, stops, and its mind stops planning and learning | done | `MagicCreatureSpells.cpp` |
| Freeze: its animation slows to a statue as it freezes and speeds up as it thaws | done | `MagicCreatureSpells.cpp` (playback scale) |
| Freeze: an icy environment-mapped sheen is added over it by how frozen it is | done | `assets/shaders/fs_object.sc`, `src/ECS/Systems/Implementations/RenderingSystem.cpp`; TheFrozenLookTintsTowardsIcyBlue |
| Small: shrinks the creature to its smallest size (less in a fight, by the size it began the fight with) | done | `src/Creature/CreatureSpells.cpp` (SizeTarget); TheBodySpellsTargets |
| Big: grows the creature to its largest size (more in a fight), never shrinking it | done | `src/Creature/CreatureSpells.cpp` (SizeTarget) |
| Small and Big put the creature back to its size from before, losing growth made meanwhile | done | `src/Creature/CreatureSpells.cpp` |
| Weak takes strength down to a tenth; Strong raises it to full; both put it back | done | `src/Creature/CreatureSpells.cpp`; TheBodySpellsTargets |
| Fat and Thin move how fat the creature wants to be up or down, then back | done | `src/Creature/CreatureSpells.cpp` |
| Invisible: the creature dissolves through static up to three quarters of the way | done | `assets/shaders/fs_object.sc` (static dissolve); `MagicCreatureSpells.cpp` |
| Invisible: villagers neither worship nor flee an invisible creature | partial | The invisible flag is set, but openblack villagers don't react to creatures at all yet |
| A creature frozen or fizzed a fifth of the way or more casts no shadow | done | `src/ECS/Systems/Implementations/RenderingSystem.cpp` |
| Compassion and Angry: the creature gives up its action and wants only to be kind (and make friends) or angry, everything else held down | done | `src/Creature/CreatureSpellMind.cpp`; `test/test_creature_spell_mind.cpp` |
| Compassion and Angry swing the creature's alignment fully good or evil, then back | done | `MagicCreatureSpells.cpp` |
| Compassion and Angry switch a leashed creature to the compassion or aggression leash | done | `MagicCreatureSpells.cpp` (ChangeType) |
| As they wear off the mood is wanted least, and the hold on the other desires is released (kept on a player's creature led on a mood leash) | done | `MagicCreatureSpells.cpp` (ClearCheat); WearingOffItIsWantedLeast |
| Hungry, Frightened, Tired, Ill and Thirsty make that need the only one wanted, without changing the body | done | `MagicCreatureSpells.cpp`; TheMoodSpellsMakeTheirDesireDominant |
| The need spells' hold on the other desires lasts its own time after they wear off | done | `src/Creature/CreatureSpellMind.cpp`; TheCheatLastsItsTimeThenLetsEverythingGo |
| Itchy: the creature gives up its action and wants only to scratch, again each turn | done | `MagicCreatureSpells.cpp` |
| Itchy: the leash comes off each turn while it lasts | done | `MagicCreatureSpells.cpp` (TakeOff) |

## Look and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three wisps fly from the hand to the creature and wind round its body, trailing light | partial | `src/Particles/ParticleCreatureSpellRules.cpp`; WispsWindRoundTheBoxOfTheBones; user reported missing creature-miracle visuals, in-game re-check still owed |
| The wisps fade with an invisible creature and fade out after the spell ends | done | `ParticleCreatureSpellRules.cpp`; WispsFlyFromTheHandOverTwoSecondsAndFadeIn |
| Compassion sends hearts rising off the creature's body | done | `ParticleCreatureSpellRules.cpp` (compassion rule) |
| Itchy brings a flock of flies that waits at the hand, then circles the creature's head | done | `ParticleCreatureSpellRules.cpp`, `src/Particles/ParticleFlockingRules.cpp`; TheFliesCircleTheHeadHalfAsWideAgainAsTheEyes |
| An itchy spell held in the hand has flies circling the hand | done | `src/Particles/ParticleHandRules.cpp` (follow the local hand); SF_CreatureSpellItchInHand |
| Holder effects: glints on a frozen-creature phial, hearts and flies on their holders | done | `src/Particles/ParticleGlintRules.cpp` |
| A whispering loop plays on the creature while the effect lasts and softly stops at the end | done | `ParticleCreatureSpellRules.cpp`; `4f4878a5` |
| A cast sound plays once for a player's cast, none for a script's or another creature's | done | `ParticleCreatureSpellRules.cpp` |
| Each spell's own sound plays as it takes hold (freeze, shrink, grow, invisible, compassion, itchy); fat, thin and the needs are silent | done | `MagicCreatureSpells.cpp` from `spells.sad`; verified in game (creature_big, creature_freeze) |
| The itch sound stops when out of hearing range | done | `MagicCreatureSpells.cpp`; fixed in fix-core |
| There is no special animation for receiving a spell | done | Nothing plays, as in the game |

## Creatures learning miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature learns a miracle by seeing it cast enough times; the number depends on the miracle and the species | done | `src/Creature/CreatureWatching.cpp`; LearnsAMiracleBySightingsAndSpecies |
| Sightings count only when more than five seconds apart, and every sighting resets that gap | done | `CreatureWatching.cpp`; fixed in fix-core |
| Watching on the learning leash counts extra | done | `CreatureWatching.cpp` (leash sighting weight) |
| A power-up can't be learnt before its miracle; thirsty and itchy need the creature to know building | done | `CreatureWatching.cpp`; APowerUpTeachesNothingBeforeItsMiracle |
| A young creature sees but learns nothing | done | `CreatureWatching.cpp` |
| The second lightning power-up is never learnt from watching | done | `src/ECS/Systems/Implementations/CreatureMindReactions.cpp` |
| One creature learning from a seed's miracle stops any creature learning from that seed again | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp` |
| The creature shows a thought when it has nearly learnt or has just learnt a miracle | partial | The events are worked out but not shown: no creature help messages or floating thoughts (re-audit item 37 open) |

## Creatures casting miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature may try a miracle once it has seen it half the times needed, and fizzles until nearly learnt | done | `src/Creature/CreatureSpellCasting.cpp` (MayTry); ItTriesAtHalfTheSightingsAndFizzlesShortOfAllButOne |
| A fizzled try embarrasses the creature and sometimes saddens it | done | `src/ECS/Systems/Implementations/CreatureMindCasting.cpp` |
| It walks near the target, backs off by its height, turns to face it, makes the gesture and casts | done | `src/Creature/CreatureCastAgenda.cpp`, `CreatureCastMoves.cpp`; `test/test_creature_casting.cpp` |
| One time in five it shows how it feels first | done | OneTimeInFiveItShowsHowItFeelsFirst |
| Casting costs the creature energy and tires it; too tired, it can't cast | done | `CreatureSpellCasting.cpp`; ItPaysWithItsEnergyAndTires, TooExhaustedItCantCast |
| A cast it can't make makes it frustrated (one desire fully dominant) | done | `CreatureMindCasting.cpp` |
| In a fight a cast costs fight stamina, and a shortfall quietly does nothing | done | FightStaminaAndMagnitude |
| The miracle's size follows the target's size; a fire miracle's follows the caster's height | done | `CreatureSpellCasting.cpp`; FightStaminaAndMagnitude |
| A creature's earlier held miracle is let go when it casts again | done | `src/ECS/Systems/Implementations/MagicCreatureCasting.cpp` (ReleaseCreatureCast) |
| Food, wood and water are cast from above with two beams from its hands | done | `MagicCreatureCasting.cpp`, `src/Particles` simple beam (`18fd1f7f`) |
| A creature's cast gives up when it swings too far away from its target | done | `ParticleCreatureSpellRules.cpp`; ACreaturesCastGivesUpOnceItSwingsTooFar |
| In a fight, at the end of its cast pose it casts an attack spell at its opponent or a defence spell on itself | partial | `src/Creature/CreatureFight.cpp`; fight spells in place, the arena queue from scripts is not (unconfirmed coverage) |
| Glints show on the miracle in the creature's hands | partial | `src/Particles/ParticleGlintRules.cpp`; the on-screen gate is still TODO |
| On its way to the target the creature keeps its current pace rather than walking | partial | It always walks: no reset of its pace when it stops what it is doing |
| A personal speed factor for each creature affects its casting walk | partial | Worked out and tested but not applied in game (no creation index, no villager food) |
| Creatures cast power-up miracles with their power-up gestures | todo | No power-up gestures for creature casts |
