# Reactions

A creature reacts to what happens around it: miracles cast near it, fires, deaths, fights, things flying past, food,
balls, teleport stones, the player's hand at work, things crushed and things that hit it, and other creatures. Each kind
of reaction has a priority that decides whether it drops what it is doing, and a time before it reacts to the same
kind again.

**Progress: 21/42 done, 6 partial — 57%**

## How reactions are taken up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reactions reach every creature within their range, as they do villagers | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp` (shared with villagers); test fixture `Reactions` in test_reactions |
| A creature takes a reaction up only after its species' reaction time from the game's tables | done | `numGameTurnsForCreatureToReact` in `ReactionSystem.cpp` |
| After reacting to a kind of thing, it won't react to that kind again for a time from the game's tables, longer or shorter than villagers' | done | `numGameTurnsForCreatureBeforeReactingAgain`, `Memory::MayReactAgain` |
| A more urgent reaction, nearer, takes over from what it reacts to, but only after a while | done | `magic::ChangesReaction`; tests `ReactionRules.MoreUrgentCloserAndToFleeingMore`, `ADifferentMoreUrgentKindTakesOverOnlyAfterAWhile` |
| A creature fighting or knocked out takes no reactions up | done | `Available` in `ReactionSystem.cpp` |
| While copying the player it takes up only the most urgent reactions | done | `k_LeastPriorityWhileMimicking` in `ReactionSystem.cpp` |
| A script can turn reactions off for a creature | todo | noted as missing in `ReactionSystem.cpp` |
| No reactions during the dance editor, while forced to faint or carried by a teleport | todo | noted as missing in `ReactionSystem.cpp` |
| A shield it is under keeps reactions from outside off it | done | `KeepsReactionOff` in `ReactionSystem.cpp`; test `ShieldRules.WhatIsSurelyWithinAShield` |
| Reacting, it stops what it was doing and picks up again afterwards | partial | the reaction replaces its agenda (`Replan`); the game's reset after reacting and return to the plan is missing |
| Its priority for each kind of reaction depends on the creature (how frightened or curious it is) | partial | only the nasty-miracle reaction looks at its fear; the other kinds' own priority rules are missing |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A nasty miracle near it frightens it: its fear rises | done | `CreatureMindSystem::ReactToNastyMagic` (`CreatureMindReactions.cpp`) |
| Frightened by a nasty miracle, it may start back in fright, then runs away from where it struck | done | `creature_mind::RunAwayFromMiracle`; test `CreatureMiracleReactions.ItRunsFromANastyMiracleSometimesStartingInFright` |
| Not afraid, or on the learning leash, it goes to look at a nasty miracle instead | done | `ReactToNastyMagic` |
| A nice miracle of its own player's draws it to look: it may turn and point, goes up to it, points, or waits there puzzled | done | `creature_mind::ExamineMiracle`; test `CreatureMiracleReactions.ItGoesToLookAndPointsOrIsPuzzled` |
| It takes no notice of another player's nice miracle or shield | done | `ReactionSystem::Start`; tests `Reactions.AnotherPlayersMiracleImpressesACreatureNotAtAllWhoeverCastIt`, `ACreatureIgnoresAnotherPlayersShield` |
| A shield of its own player's draws it to look, as a nice miracle does | partial | `ReactionSystem::Start`; test `Reactions.AShieldIsLeftToTheShieldsForVillagersButACreatureTakesItUp`; its own way of reacting to a shield is not modelled |
| A shield struck or destroyed means nothing to it | done | `ReactionSystem::Start` |
| A magic tree is taken up but does nothing | done | `ReactionSystem::Start` |
| It is impressed by its own player's miracles and those of its player's other creature, not by its own | done | tests `Reactions.TheCreatureIsImpressedByItsOwnPlayersMiracle`, `AnotherCreatureOfItsPlayerImpressesItAsItsPlayer`, `ACreatureIsNotImpressedByItsOwnMiracle` |
| It learns a miracle it reacts to, if cast by a creature or a human player, and a seed's miracle teaches only the first to learn it | done | `ReactionSystem::Start`; see [learning by watching](learning_by_observation.md) |
| Frozen, its mind learns nothing | done | `WatchMiracle` |
| A teleport stone used near it draws it aside into the stone | partial | `TeleportSystem::SetupReact`; see [../miracles/teleport.md](../miracles/teleport.md) |
| Caught by a tornado it faints | done | `CreatureFightSystem::ForceFaint`; test_tornado_creature; see [../miracles/tornado.md](../miracles/tornado.md) |
| A miracle hitting it during a fight makes it reel | done | `CreatureFightSystem::Recoil` from `MagicLiving.cpp`; see [fighting](fighting.md) |
| Fleeing a miracle, it weighs how urgent fleeing is against what it does | partial | the shared fleeing priority is used; the creature's own fleeing priority is not |

## Other things that happen near it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A fire near it: it goes to look, flees, or puts it out | todo | |
| A death near it | todo | |
| A fight between others near it: it goes to watch | todo | |
| Something flying through the air: it watches it, and may try to catch it | partial | Ported (`PhysicsGameHooks::OfferToCatchingCreatures`, `src/Creature/CreatureCatch.*`, `CreatureObjectActionSystem` catch, test `test_creature_catch`, `cf273ae0`, `6afe6217`); open: R20's start checks (body action, held by a creature), the waiting stand pose and the step's acceptance test, see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Food dropped or thrown near it | todo | see [feeding and thrown things](feeding_and_thrown_things.md) |
| A ball near it: it goes to play | todo | |
| Another creature near it: it goes to greet, inspect or confront it | todo | see [friends](friends_and_other_creatures.md) |
| Another creature fainting near it | todo | |
| Something crushed near it | todo | |
| Something it can't make sense of: it looks confused | todo | |
| Hit by something thrown, it reacts to the blow | done | Hurt, angered, frightened and swayed (`HurtCreature`, `CreatureSway`); see [feeding and thrown things](feeding_and_thrown_things.md) |

## The player's hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks something up near it: it watches, and may come to see | todo | the reaction exists for villagers only (`src/Magic/ReactionRules.cpp`) |
| The hand puts things in a store: it watches, and may copy it | todo | |
| The hand uses a totem: it watches | todo | |
| The hand comes near it: it looks at the hand, or runs from it if it fears the player | todo | |
| Stroked or slapped, it reacts at once, by the part of the body touched | done | see [learning from feedback](learning_from_feedback.md) and `src/Creature/CreatureFeedback.h` |
