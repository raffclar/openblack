# Leash

The leash is how the player leads their creature. Held in the hand it makes the creature follow; tied to something it
keeps the creature there and has it act on that thing. There are three leashes: the learning leash (a plain rope), and
the aggression and compassion leashes, which make the creature angry or kind for as long as it wears them.

**Progress: 36/51 done, 6 partial — 76%**

## Which creature and which leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player can lead exactly one creature, their own; other players' creatures and creatures of nobody can't be leashed | done | `src/Creature/LeashOwnership.*`, `LeashSystem::WhyNot`; tests `LeashOwnership.*` |
| A player's first creature becomes the one they lead; giving a creature away takes its leash off | done | `LeashSystem::ClaimOnArrival`, `SetOwner`; test `APlayersFirstCreatureClaimsTheLeash` |
| The creature must know a leash before it can wear it, the learning leash first | done | `LeashSystem::Knows`/`SetKnown`; tests `ItMustKnowTheLeashes`, `TheLeashKeyNeedsTheLearningLeash` |
| The creature is given the learning leash as it starts to grow up, and the aggression and compassion leashes at the stage of growing up that teaches good and evil | partial | the tutorial's developer script calls set them known (`CHLApi.cpp`, the dev function); the stages themselves are in [development_phases.md](development_phases.md); the shipped lessons: the Leash of Learning in its own lesson, the other two shown in the tying lesson ([../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-4-the-leash-of-learning)) |
| The leash can only be tied to things once the creature has grown past its early stages | done | `LeashSystem` (tying refused before the tying stage, logged) |
| The citadel hangs a post for each leash the player has; tapping a post picks that leash, tapping it again unpicks it | partial | `LeashSystem::PlacePosts`/`TapPost` place three posts at each temple and pick on tap with the click sound; the game makes posts only for leashes the creature knows and uses the citadel's own leash meshes (unconfirmed which) |
| The temple shows the collar of the leash worn | todo | see [../temple](../temple/) |
| The hand's tooltip over a post or the leashed creature says what a tap does | todo | |

## Putting it on and taking it off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the player's own creature with the right button puts the picked leash on | done | `LeashSystem::TapCreature`, `CreatureHandRules` click rule; test `AQuickPressIsAClickAndALongOneAHold` |
| L toggles the leash: puts the picked leash on, unties a tied leash back to the hand, or takes it off | done | `src/Creature/LeashKeys.*`; tests `TheLeashKeyPutsOnUntiesAndTakesOff`, `TheGameBindsLVAndB` |
| V and B step the picked leash through the leashes the creature knows, swapping the one worn | done | `LeashKeys`; tests `VStepsUpAndBDownThroughTheLeashNumbers`, `AStepOntoAnUnknownLeashDoesNothing` |
| Drawing the leash gesture opens a picker of the known leashes, each drawn as a gesture to choose | partial | the gesture requests are handled in [../gesture](../gesture/); the picker's on-screen leash pictures are todo |
| A scribble drawn with the empty hand shakes off a leash held in the hand; a tied leash stays | done | `LeashSystem::Shake` (from `GestureSystem`) |
| After putting a leash on there is a short delay before the hand can hand out another | todo | |
| The leash goes on with its own sounds, and the two tying sounds play in turn | done | `LeashSystem` (`G_LeashAttach` sounds) |
| Leashes can't be put on a creature in a fight or knocked out, and don't pull it about | done | `LeashSystem::ProcessTurn` skips fighting or knocked-out creatures |

## The rope

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rope is a chain of masses on springs from the hand (or the thing tied to) to the creature's collar, swinging and sagging | done | `src/Creature/LeashRope.*`; tests `LeashRope.*` |
| It is stepped 200 times a second however long the frame, with a cap on catching up | done | `leash_rope::k_StepSeconds`, `k_MaxSteps` |
| It is kept off the ground and inside the world | done | tests `StaysOffTheGround`, `EndsAreKeptWithinTheWorld` |
| It is drawn as a ribbon facing the camera, with a shadow strip on the land below | done | `src/Graphics/RendererLeash.cpp`; test `RibbonFacesTheEyeAndShadowLiesOnTheLand` |
| Each leash looks its own (its band of the leash texture), the compassion leash thicker | done | `creature_leash::LookFor`; test `EachLeashLooksItsOwn` |
| Held in the hand, the leash is as long as the creature is big | done | `creature_leash::InHand`; test `HandLengthsGrowWithTheCreature` |
| Tied to something still, it reaches one and a half times the distance to it, within limits; tied to something moving, seven times the creature's height | done | `TiedToStatic`, `TiedToMobile`; test `TiedLengths` |
| Its tension, slack to taut, is what pulls the creature | done | test `ASlackRopeHasNoTension`, `StretchedToItsFullLengthIsTaut` |
| Scripts can hide all leashes | done | `SET_DRAW_LEASH` in `CHLApi.cpp` |
| In a network game the leash's pull is sent to the other players | todo | see [../multiplayer](../multiplayer/) |

## Leading the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pulled taut, the creature stops what it is doing and walks to the hand | done | `ShouldPull`, `DecideLead`, `CreatureLocomotionSystem::LeadTo`; tests `PullsOnlyWhenTaut`, `LeadsToTheHand` |
| The harder the pull the faster it goes, up to twice its running pace; the pull fades once it is under way | done | `FadePull`; test `PullFades` |
| Close to the hand it stays where it is | done | `k_CloseToHand` |
| Pulled away from acting on the same desire twice, it goes off that desire for a while | done | `RecordPull`; test `SecondPullHoldsTheDesireBack` |
| While leashed it can be dragged over land it could not walk | todo | (unconfirmed) |
| A leashed creature that strays out of reach of the hand or tied thing is brought back | done | `LeashSystem::ProcessTurn` (confinement to the rope's reach) |
| The leash overrides what a script has the creature doing | todo | (unconfirmed which scripts it overrides) |
| Scripts can make the leash do nothing while it is worn | done | `SET_LEASH_WORKS` → `LeashSystem::SetWorks` |
| The creature is kept near home while it starts to grow up, unless led on a working leash | done | `ConfineToHome`, `IsConfined`; test `Confinement` |
| Away from home and its player has no temple, it isn't free to roam | done | `FreeOfHome` |

## Feelings and lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The aggression leash makes the creature angry, the compassion leash kind, above every other desire, while worn | done | `ForcedDesireFor`, `CreatureMindLearning`; test `LeashesForceTheirFeelings` |
| The learning leash makes the creature watch the player more keenly: each miracle seen counts three times | done | `MiracleSightingWeight`; see [learning_by_observation.md](learning_by_observation.md) |
| On the learning leash in the hand, the creature copies what the player does | partial | `learningInHand` reaches the copying rules; see [learning_by_observation.md](learning_by_observation.md) |
| Tied to something, the creature acts on it, choosing an action for the desire the leash gives it | done | `actOn` hook in `CreatureMindLearning.cpp` |
| Tying the leash to something teaches the creature which desire to act on it with: anger over compassion on the aggression leash, the reverse on the compassion leash | done | `LessonsFor`; test `LessonsOfWhatThePlayerShows` |
| Tied to a village centre on any leash but aggression, the creature wants to impress that village | partial | `k_ImpressTownValue` is passed on; impressing towns is mostly todo in [town_actions.md](town_actions.md) |
| Holding something in the hand while the creature is on the leash has it act on that thing | todo | |
| Tied to another creature on the aggression leash, the two fight | done | `CreatureFightSystem` takes the tied creature (see [fighting.md](fighting.md)) |
| Tied to another creature, the other gets angry too if near enough, and they warm (compassion) or cool (aggression) to each other over time | done | `LeashSystem::ProcessTurn`, `AttitudeChange`; test `LeashedCreaturesWarmOrCoolToEachOther` |
| At the tying stage of growing up the creature is taught to stay where its leash is tied; the stage was planned around a house, but the shipped lesson ties it to a palm tree by the pen | partial | tying works; the stage's lesson and help text are in [development_phases.md](development_phases.md); the shipped lesson: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |

## Scripts and computer players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can tie a creature's leash to an object, back to the hand, or take it off | done | `ATTACH_OBJECT_LEASH_TO_OBJECT`, `ATTACH_OBJECT_LEASH_TO_HAND`, `DETACH_OBJECT_LEASH` in `CHLApi.cpp` |
| Scripts can ask whether a creature is leashed, to what, and on which leash | done | `IS_LEASHED`, `IS_LEASHED_TO_OBJECT`, `GET_OBJECT_LEASH_TYPE`; the Pied Piper is caught by tying the creature's leash to him, a person, and dragging him from his cave: [the_pied_piper.md](../story/silver_scrolls/the_pied_piper.md) |
| Scripts can toggle a player's leash as the key does | done | `TOGGLE_LEASH` |
| Computer players leash their creatures to things and to other creatures to teach or fight | todo | see [../multiplayer](../multiplayer/) and [learning_by_observation.md](learning_by_observation.md) |
| Leash state is saved and loaded with the game | todo | no saved games; see [../engine](../engine/) |
