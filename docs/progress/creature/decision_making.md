# Decision making

How a creature decides what to do next: it plans for its desires, picks the thing to act on and the action to take,
turns the action into an agenda of small steps, carries them out, and copes when they fail. Scripts and the computer
gods can also take control of a creature.

**Progress: 21/63 done, 18 partial — 48%**

## Planning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each desire gets a plan: a thing to act on (the goal) and an action to take | done | `src/Creature/CreaturePlanner.h` (`PlanDesire`); test `CreaturePlanner.PlansTheMostUsefulGoal` |
| A couple of desires are planned again each turn, going round the ones that can be planned | done | `creature_planner::NextGoals` (two a turn); test `CreaturePlanner.GoesRoundTheEligibleDesires` |
| The goal is the thing the creature has learnt is most useful for the desire, the nearer the better, up to 200 away | done | `DistancePriority`, decision trees in `CreatureMindLearning.cpp`; test `CreaturePlanner.DistancePriority` |
| Something it already holds is preferred as a goal | done | `k_HeldPriority` (1.6) |
| A thing it knows nothing about is worth a little | done | `k_DefaultUsefulness` (0.1) |
| Among the actions that satisfy the desire, its opinion of each decides | done | `ActionPriority`; test `CreaturePlanner.OpinionBreaksTies` |
| An action not done for a long time gets a little extra novelty | done | `Novelty`, `k_NoveltyTurns`; test `CreaturePlanner.ActionPriorityAndNovelty` |
| A plan's priority is the desire's strength times how useful the goal, action and things used are | done | `creature_planner::Priority`; test `CreaturePlanner.PriorityAndSwitching` |
| The creature changes what it does only for a plan more than twice as pressing | done | `ShouldSwitch`; test `CreaturePlanner.ChoosesOnlyWhatIsPressingEnough` |
| A plan must reach a minimum priority to be taken up at all | partial | `k_MinPlanPriority` (15) in `CreatureMindLearning.cpp` (unconfirmed against the game) |
| Actions are weighed by the kind of thing they are done to, so each has a goal it can be done to | done | groups in `PlanCreature` |
| Actions it can't do now (no water near, no target, can't afford a miracle) are left out | done | `creature_plan_actions::Possible`, `MayCast` |
| Only actions the creature knows can be chosen | partial | miracles must be learnt (see [creature casting](creature_casting.md)); ordinary skills are learnt by watching but most skill actions have nothing to carry them out |
| The planner can carry out the game's whole range of actions | partial | about seventy of the game's nearly three hundred actions can be carried out (`src/Creature/CreaturePlanActions.cpp`); town work, building, dancing, friends, games, stealing and most miracles have none; see [object actions](object_actions.md) and [town actions](town_actions.md) |
| Something shown to it on the leash is the only goal it plans for | done | `mind.leash.actOn` in `PlanCreature`; see [leash](leash.md) |
| When free, the creature plans every desire at once before falling back on idling | done | `PlanCreature(..., true)` in `CreatureMindSystem::ProcessTurn` |
| The idle choices (sitting, hanging around, being idle) are weighed through the planner as the game does | partial | a stand-in random choice (`creature_mind::k_ActivityLots`); see [idle behaviour](idle_behaviour.md) |
| Needs (hunger, thirst, sleep, poo) are weighed against everything else through the planner | partial | a stand-in: a need over 0.3 is seen to first (`k_ActOnNeed`) |
| What it chose for itself runs to its end unless it was only idling | partial | `Interruptible` in `CreatureMindLearning.cpp` (unconfirmed) |
| The plan it takes up is remembered, with what it acted on, for the player's feedback to be credited to | done | `creature_learning::Remember`; see [learning](learning_from_feedback.md) |
| How far it can see limits what it plans for | partial | goals are gathered within 200 of it; the game's vision range by size (`creature_look::LookRange`) is only used for looking |

## Agendas and steps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An action becomes an agenda of small steps (go near, turn to face, play an action, wait, let go …) | done | `src/Creature/CreatureIdleMind.h` (`Step`), `creature_plan_actions::Agenda`; test `CreaturePlanActions.BuildsAgendas` |
| The game has well over a hundred kinds of step; all of them are available | partial | going, following, fleeing, turning, picking up, holding, eating, throwing, destroying, pointing, casting, sleeping, sitting and emotes exist; dancing, building, repairing, kissing, talking, fishing, setting on fire, putting out fire, raising totems, making disciples, swapping minds, teleporting and the like do not |
| Going near a thing, keeping clear of it by both their sizes | done | `src/Creature/CreatureCastMoves.h` |
| Getting away from a thing to a clear area | done | `creature_cast_moves` (nearest clear area) |
| Turning to face a thing, the camera or the hand | partial | facing things and the camera exist; facing the hand and another player's camera do not |
| Steps start only when the body is free | done | test `CreatureIdleMind.NothingStartsWhileTheBodyIsBusy` |
| A step with a time limit, such as following, ends when its time is up | done | test `CreatureIdleMind.AFollowStepStopsWhenItsTimeIsUp` |
| An action finishes successfully and satisfies its desire, or unsuccessfully and doesn't | partial | an agenda that fails is given up (`gaveUp`); the game's separate unsuccessful ending (with a lesson and help) is missing |
| Being slapped stops what it is doing | done | `CreatureMindSystem::ReceiveFeedback` |
| Stopping what it does lets go of a miracle it holds and ends its animation | done | `Replan`, `ReleaseCast` |
| Waiting for another creature to be free, or for a thing to come within reach | todo | |
| Repeating its last action | todo | |

## When things go wrong

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't find a route to its goal | partial | the locomotion system refuses (`MoveResult::InvalidDestination`) and the agenda is given up; the explanation shown to the player is missing (see [lessons and help](lessons_and_help.md)) |
| Its route is blocked | partial | routes plan round things in the way (`src/Creature/CreatureRoute.h`); no reaction to a blocked route |
| Its goal is out of reach (cut off land) | partial | unreachable land is known (`creature_route`); no explanation |
| The opponent refused to fight | todo | see [fighting](fighting.md) |
| The other creature refused to play | todo | |
| The other creature refused to be friends | todo | |
| The leash stopped it | partial | the leash stops it (see [leash](leash.md)); no explanation |
| It lost sight of what it was tracking | todo | |
| Stuck in one place for a while, it gives up and holds back that desire | partial | only while casting: after 50 turns without moving the desire is held back 10 seconds (`CreatureMindCasting.cpp`) |
| Trapped in an enclosed space, it gets itself out | todo | |
| Something in the way, it finds an action to clear it (knocking it down, moving it) | todo | |
| It responds to emergencies first: being on fire, fainting, being in danger | partial | fainting when exhausted, starved or out of life works (`CreatureMindSystem::ProcessTurn`); putting out fire on itself and fleeing danger first do not |
| It weighs how dangerous a thing is and whether anything scary is near it, its home or where it is going | partial | frightening things (creatures, bats, vultures, lions, miracles) are a target kind (`Target::Frightening`); danger isn't weighed for other plans |
| Lacking skill, it sometimes messes an action up (a throw, a miracle, fishing, a dance, making fire, impressing, a totem) | partial | only miracles fizzle (see [creature casting](creature_casting.md)); the rest are missing |
| It answers requests from other creatures (to fight, to play, to be friends, to learn) | todo | see [friends](friends_and_other_creatures.md) |

## Control by scripts and the computer gods

The script language is the story domain's: see [../story/](../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script makes a creature do an action, to a thing or at a place | todo | stub in `src/CHLApi.cpp` |
| A script moves a creature to a place | todo | |
| A script plays one of a creature's animations, once or held | todo | |
| A script sets the priority of what the creature is doing | todo | stub in `src/CHLApi.cpp` |
| A script forces a creature to finish what it is doing | todo | stub in `src/CHLApi.cpp` |
| A creature under a script's control does nothing of its own until released | todo | |
| A script teaches a creature everything, or a single action | todo | stubs in `src/CHLApi.cpp` |
| A script asks how often a creature did an action, and resets the count | todo | stubs in `src/CHLApi.cpp` |
| A script asks whether a thing is in a creature's hand | todo | stub in `src/CHLApi.cpp` |
| A script creates a creature next to another | todo | stub in `src/CHLApi.cpp` |
| A script gives a creature to another player, or swaps creatures | todo | stubs in `src/CHLApi.cpp` |
| A script calls the player's creature to a place | todo | stub in `src/CHLApi.cpp` |
| A script puts a creature in its mind's development stage | todo | see [development phases](development_phases.md) |
| The computer gods command their creatures to do things and teach them | todo | |
| The computer gods' creatures have their knowledge balanced for the difficulty | todo | |
| Multiplayer locked selection of a creature across the network | n/a | network play is not supported; see [../multiplayer/](../multiplayer/) |
