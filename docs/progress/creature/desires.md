# Desires

A creature is driven by about forty desires (hunger, anger, compassion, the wish to play, to sleep, to be friends …).
Each desire grows from its sources (low energy, darkness, the player's strokes, its own innate character …) once they
pass their thresholds, fades otherwise, and the strongest desires decide what the creature does and what it shows the
player.

**Progress: 24/71 done, 27 partial — 53%**

## How desires work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every desire has its own value, maximum, decay and weight, set per species from the game's creature tables | done | `src/Creature/CreatureDesires.h` (`Create`), `CreatureMindSystem.cpp` (`SetupFor`); test `CreatureDesires.CreatedFromTheSetup` |
| A desire grows from up to eight sources, each with a value and a threshold | done | `src/Creature/CreatureDesires.h` (`Source`, `k_MaxSources`) |
| How hard a source drives its desire follows the game's sigmoid of how far its value is past its threshold (a table of 41 steps) | done | `creature_desires::Sigmoid`; test `CreatureDesires.TheSigmoidSteps` |
| A desire grows by its sources' drive over its species' growing time, never past its maximum | done | `creature_desires::UpdateDesires`; test `CreatureDesires.ADesireGrowsByItsDriveOverItsIncreaseTime` |
| A desire nothing drives fades by its decay each turn | done | `UpdateDesires`; test `CreatureDesires.ADesireWithoutDriveFades` |
| Each creature's decay is picked at random within its species' range when it is made | done | `creature_desires::Create` takes a uniform draw |
| Sources that follow the creature's state are read again every turn; the rest fade by their own multiplier | done | `creature_desires::UpdateSources`; test `CreatureDesires.SourcesReadTheirStateThenFade` |
| Events push sources up or down (a stroke, a slap, a nasty miracle, a fizzled miracle) | partial | `creature_desires::ChangeSource`; only the stroke, slap, scary miracle, fizzle and copying pushes exist (`CreatureMindLearning.cpp`, `CreatureMindReactions.cpp`, `CreatureMindCasting.cpp`); seeing the player's deeds, villagers or deserving objects push nothing yet |
| A desire can be held back for a while, during which it only fades | done | `creature_desires::Suppress`; test `CreatureDesires.ASuppressedDesireOnlyFades` |
| Having drunk it doesn't want water for a while; having had a poo, it doesn't want another for longer | done | `CreatureMindSystem.cpp` (20 and 60 seconds) (unconfirmed against the game's times) |
| Deciding on a desire holds back the desires opposed to it | done | `creature_learning::SuppressOpposed`; test `CreatureLearning.DecidingSuppressesOpposedDesires` |
| Doing an action multiplies the desire it serves down by the action table's factor | done | `Satisfied` in `CreatureMindSystem.cpp`, from the game's action table |
| A desire's weight (how much it matters) and its sources' thresholds change with the player's lessons | done | `creature_learning::LearnDesireLesson`; see [learning](learning_from_feedback.md) |
| Lessons about one desire spread to the desires that depend on it | done | `creature_learning::Spread`; test `CreatureLearning.SpreadsLessonsByDependency` |
| The sum of all active desires is kept and feeds the wish to show how it feels | done | `Desires::sum`, `ReadSource` in `CreatureMindSystem.cpp` |
| Only desires the creature's stage of growing up has brought are active | done | `creature_desires::ActivateForPhase`; test `CreatureDesires.GrowingUpBringsAndTakesDesires`; see [development phases](development_phases.md) |
| The dominant desire is the strongest active one | done | `StrongestShowable`, `BeliefOf` (dominant desire attribute) |
| The game finds the dominant desire that can be helped, and the one that can be shown, separately | partial | only "strongest showable" exists, above a stand-in minimum of 0.2 (`creature_mind::k_MinDesireShown`) |
| The strongest physical desire (hunger, thirst, tiredness, poo) is found for emergencies | partial | the idle mind sees to the strongest need above a stand-in threshold of 0.3 (`creature_mind::k_ActOnNeed`) rather than the game's rule |
| A desire can be made fully dominant, its source changed, as the mood spells and lessons do | done | `creature_learning::MakeFullyDominant`, `creature_spell_mind`; tests `CreatureLearning.DominanceAndActions`, test_creature_spell_mind |
| A desire can be made least dominant (below the weakest of the others), as a slap for it does | done | `creature_learning::MakeLeastDominant` |
| The mood and need spells make one desire dominant and hold the others down while they last | done | `src/Creature/CreatureSpellMind.h`; test_creature_spell_mind; see [creature spells](../miracles/) |
| Leashes force a desire (anger on the aggression leash, compassion on the compassion leash, obedience when led) | partial | `mind.leash.forcedDesire` in `PlanCreature`; see [leash](leash.md) |
| Each source has the species' starting value and threshold: innate niceness, aggression, lethargy, friendliness and communicativeness are a species' character | done | `SetupFor` reads the per-species source tables |
| A creature's desires and sources are saved in and restored from its mind file | done | `src/Creature/CreatureMindModel.cpp`; see [saves and files](saves_and_files.md) |

## The desires and what drives them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To impress: grows from watching the player impress and from seeing things that deserve it; shown by impressive poses, throws and miracles | partial | the desire and its actions (`ShowImpressiveAnimation`, `ThrowToImpress`) exist; neither source is fed, so it only holds its starting value |
| Compassion: from watching the player be kind, from seeing those who deserve it, from being content, and from innate niceness | partial | stroking pushes the watching source and innate niceness is set; seeing the deserving and contentment are not fed; acted on by stroking villagers and casting heals |
| Anger: from watching the player be cruel, from those who deserve it, from being dissatisfied, from being hurt, from sadness, and innate aggression | partial | slaps push "from being hurt", sadness is read, innate aggression set; real damage, dissatisfaction and watching don't push it; acted on by hurling, stomping, kicking and lightning |
| To play: from watching the player play and from watching villagers play | partial | strokes push the first; villagers playing feed nothing; acted on by throwing things about, kicking balls, silly faces and playful spells |
| Hunger: from low energy, from watching villagers eat, and from sadness | partial | energy and sadness are read each turn (`creature_physiology::SourceValue`); watching villagers eat is not fed |
| Fear: from darkness, from being hurt, and from seeing frightening miracles | partial | darkness reads night as fully on or off (`IsNight`), slaps and nasty miracles push it; real damage doesn't; acted on by running away and being frightened on the spot |
| Curiosity | partial | the desire and its actions (examining by picking up, looking and following) exist; nothing feeds its source beyond its starting value |
| To poo, from the poo built up by eating | done | `creature_physiology::SourceValue`; test `CreatureNeedsMind.APooTakesFourSecondsAndDropsAsItEnds` |
| Tiredness: from exhaustion, laziness, night time, sadness, and innate lethargy | partial | exhaustion, night and sadness read; laziness not fed |
| To idle about with the player | partial | the desire exists; its source isn't fed and its actions (following the hand, going to the middle of the screen) are missing |
| Wanderlust | todo | no exploring actions (coast, towns, hills) exist; see [idle behaviour](idle_behaviour.md) |
| To be sick | partial | the action exists (`Puke`); nothing makes it want to be sick |
| To build its home | todo | see [home and pen](home_and_pen.md) |
| To bring things home | todo | see [home and pen](home_and_pen.md) |
| Thirst, from dehydration | done | `creature_physiology::SourceValue`; test `CreatureNeedsMind.ItDrinksAtTheWatersEdge` |
| To restore its health, from lost life | partial | the source reads life; resting to get better and healing itself aren't actions yet (sleep heals) |
| To be friends, and innate friendliness | partial | the sources exist; only smiling and waving at a friend are acted on; see [friends](friends_and_other_creatures.md) |
| To get the player's attention, from loneliness and from lack of interaction | partial | read from seconds alone, fully after one and two minutes (`ReadSource`), measures not confirmed against the game; acted on by being pathetic, howling and pointing at the camera |
| To show how it feels, and innate communicativeness | partial | read from the sum of desires, fully at 3 (unconfirmed); the communicate-state action plays |
| To get warmer, and to get colder | partial | read from warmth (`creature_physiology`); shown by shivering and showing it's hot; starting fires and warming or cooling spells are missing |
| To scratch, from itchiness | done | read from itchiness; the scratch action plays |
| To run away from the player | partial | read from a poor attitude to the player (unconfirmed); the run-away action works; forgiven when stroked for it (`LearnFromFeedback`) |
| To rest | partial | the desire exists; resting on the spot and going home to recover are missing |
| To obey the player | partial | forced by the leash; see [leash](leash.md) |
| Illness | partial | the ill spell drives it; sneezing plays; curing illness is missing |
| To obey another creature | todo | |
| Sadness | partial | a fizzled miracle pushes it, strokes hold it back; the be-sad action plays; other causes (losing a fight, losing a friend) are missing |
| To go home | todo | see [home and pen](home_and_pen.md) |
| To tell the player what it thinks of him | todo | showing how nice it thinks the player is, being cross with the player |
| To play with the player | todo | the throwing game with the player and being silly with the player |
| To tell another creature what it thinks of him | todo | see [friends](friends_and_other_creatures.md) |
| To teach a friend | todo | see [friends](friends_and_other_creatures.md) |
| To follow what the player seems to want | partial | copying the player pushes it (`k_FollowPlayerSource`); see [learning by watching](learning_by_observation.md) |
| To get high | todo | |
| To hang around at home | partial | hanging around walks somewhere nearby rather than near its home |
| Mental illness | todo | |
| To miss a friend | todo | |
| To look around | partial | looking about plays; looking at particular sights is generic, see [idle behaviour](idle_behaviour.md) |
| To steal | todo | |

## Scripts and desires

The script language itself is the story domain's: see [../story/](../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can set a desire's value | todo | stub in `src/CHLApi.cpp` |
| A script can turn a desire on or off | todo | two stubs in `src/CHLApi.cpp` |
| A script can set a desire's maximum | todo | stub in `src/CHLApi.cpp` |
| A script can turn off every desire | todo | stub in `src/CHLApi.cpp` |
| A script can make one desire the creature's only one, and turn that off again | todo | stubs in `src/CHLApi.cpp` |
| A script can ask whether a creature's desire is a given one | todo | stub in `src/CHLApi.cpp` |
| A script can turn a desire on and make it as strong as it gets | todo | |
