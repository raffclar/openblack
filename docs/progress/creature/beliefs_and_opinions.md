# Beliefs and opinions

What a creature knows and thinks about the world: what each kind of thing is like and whether it is good to act on for
each desire (its decision trees), its opinion of each action, how it feels about the player and about other creatures,
and how the towns feel about it.

**Progress: 22/56 done, 12 partial — 50%**

## Decision trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| For each desire it keeps two decision trees: which things to act on, and which things to use doing so | done | `src/Creature/CreatureDecisionTree.h`, `src/Creature/CreatureMindModel.h` (`TreeKind`) |
| Each tree learns from the newest sixteen examples of things acted on and the feedback for them | done | `creature_tree::AddEpisode`; test `CreatureDecisionTree.KeepsTheNewestSixteenExamples` |
| Feedback is sorted into eleven steps from -1 to 1 | done | `BucketOf`; test `CreatureDecisionTree.BucketsFeedbackByTheFirstStepWithinAQuarter` |
| The tree is grown again from scratch after each new example, testing at each step the attribute that best separates the feedback | done | `creature_tree::Build`, `Gain`, `Entropy`; tests `CreatureDecisionTree.GainPicksTheAttributeThatSeparatesFeedback`, `BuildsAndEvaluates` |
| A tree with no examples knows nothing, and a thing it knows nothing about is worth a little | done | tests `CreatureDecisionTree.EmptyTreeKnowsNothing`, `UsefulnessFromUtility` |
| Each desire's trees may only test the attributes the game's tables allow it | done | `tables->attributes` in `CreatureMindLearning.cpp` |
| The trees decide which food it eats first, and which thing it picks for every plan | done | `FoodUsefulness`, `Usefulness` in `CreatureMindLearning.cpp` |
| The trees are saved in and restored from the mind file | done | `creature_mind_model`; test `CreatureMindModel.LearningRebuildsTrees` |
| The trees can be read as text, as the game's own mind viewer shows them | done | `creature_tree::Describe` (used by the debug creature spawner) |

## What it knows about each kind of thing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every thing has seven common attributes: whose it is, natural or made, alive or not, which player, whether stronger than the creature, which species, and its kind | done | `BeliefOf` in `CreatureMindLearning.cpp`; test `CreatureDecisionTree.SlotsFollowTheKindOfThing` |
| Villagers: sex, job, alive, on fire, tribe | partial | all are set; whose a villager is is always "nobody's" rather than the player whose town it lives in |
| Other creatures: stronger than it, species, height, the miracles they know, what they carry, their dominant desire | partial | the miracles a creature knows are always read as none |
| Homes: their kind, alive, on fire, being built | partial | "being built" is always no |
| Trees, features, and loose objects | done | `BeliefOf` |
| The temple | partial | the temple itself is known; the temple's parts are not |
| Towns: their belief in the gods, what they need most, their size | todo | towns have no belief built for them |
| Forests: their size | todo | |
| Flocks of animals | todo | |
| Single animals | todo | animals give no belief, so a creature can't learn about them |
| Fields | todo | |
| Miracles and spell seeds | todo | |
| The place and situation it acts in (its context) | todo | |
| It remembers what it believes about particular things it has met, not just their kind | partial | beliefs are worked out afresh each time from the thing (`BeliefOf`); the game keeps a list per thing |
| It weighs how edible, how damaged, how dangerous, how interesting and how impressed a thing is | partial | food value and interest by kind exist; damage, danger and impressedness are not weighed |
| It weighs how useful a thing is for a nice or a nasty purpose | todo | |
| A script teaches it a distinction about a kind of thing | todo | stub in `src/CHLApi.cpp` |

## Opinions of actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an opinion of every action, from -1 to 1, which the player's feedback changes | done | `creature_learning::OpinionAfter`; test `CreatureLearning.OpinionsAndFeedbackStrength` |
| Opinions break ties between actions for a desire | done | test `CreaturePlanner.OpinionBreaksTies` |
| It remembers how long since it last did each action | done | `turnsSinceDone` in `CreatureMindModel.h` |
| It counts how often it did each action | todo | the count the scripts ask for |
| It has a skill at each action, which rises with practice | todo | |

## How it feels about the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an attitude to the player, from hating to loving, moved by strokes and slaps | partial | `creature_feedback::AttitudeAfter` (mostly what it was, nudged by the feedback) (unconfirmed against the game) |
| A poor attitude makes it want to run away from the player | partial | `ReadSource` (unconfirmed) |
| Its attitude shows on its face when it reflects on the player | done | `creature_face` (smile or sad); see [face, eyes and hair](face_eyes_hair.md) |
| It judges how abusive the player is | todo | |
| It judges how neglectful the player is | todo | |
| It remembers whether the player has been nasty to it | todo | |
| It keeps the most recent feedback the player gave it | done | `GetLastFeedbackSum` in `CreatureHandSystem` |
| It guesses what the player wants from what it sees the player do, and that guess fades | done | `creature_perceived_desires`; test `PerceivedDesires.SeenDesiresAddUpHeldToOneAndFade` |
| It sees the player's deeds only within two thirds of a half turn either way, or in its own cell | done | test `PerceivedDesires.ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell` |
| It is cross with the player and shows it | todo | |
| It shows the player how nice it thinks he is | todo | |
| Its opinion of its god shows in the Creature Cave | done | `creature_cave` (`opinionOfGod`); see [creature cave](creature_cave.md) |
| Its attitude is saved in its mind file | done | `CreatureMindModel.cpp` |

## How it feels about other creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps how nice it thinks each creature it has met is, from -1 to 1 | done | `creature_learning::AttitudeTo`, `ChangeHowNice`; test `CreatureLearning.AttitudesToCreatures` |
| How nice it thinks a creature is moves its desires towards being friends or angry with it | partial | the desire changes exist (`ChangeHowNice`); nothing in the world changes the attitude yet |
| It judges whether another creature seems friendly | todo | |
| It judges whether another creature is the dominant one | todo | |
| It judges how much stronger an opponent is, how much more life it has and how many dangerous miracles it knows | todo | see [fighting](fighting.md) |
| It reacts to another creature only so often, by how it feels about it | todo | |
| It shows a creature it hates it | todo | see [friends](friends_and_other_creatures.md) |

## Its thoughts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps its newest thoughts (what it learnt, what it saw the player do) | done | `creature_mind_model::Think`, `k_MaxThoughts` (8); test `CreatureLearning.ThoughtsAndLikes` |
| It knows the thing it likes most, with its picture and name, for the Creature Cave | partial | likes are worked out (test `CreatureLearning.ThoughtsAndLikes`); see [creature cave](creature_cave.md) |

## How the towns feel about it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Towns watch what a creature does with fear or respect, and change their view of it | partial | `creature_object_actions::TownAttitude` in `CreatureObjectActionSystem`; see [town actions](town_actions.md) and [../town/](../town/) |
| A creature impresses villagers by what it does, which adds to belief | partial | see [town actions](town_actions.md) and [../worship/](../worship/) |
| The creature is told how a town feels about it | todo | see [lessons and help](lessons_and_help.md) |
