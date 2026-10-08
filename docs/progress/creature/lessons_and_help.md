# Lessons and creature help

The game explains the creature to the player as it goes. When the creature learns something, moves to a new stage of
growing up, is put under a spell, gets into trouble or fails at something, the good advisor comes to the edge of the
screen and says so in a line or two of text. These messages wait their turn on a stack, aren't repeated too often, and
can be switched off in the options. The general help system and advisors belong to [../interface/](../interface/) and
[../story/](../story/); this file covers what the creature has to say.

**Progress: 0/84 done, 10 partial — 6%**

## How creature help is shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creature help is run by the challenge script's creature help scripts, which open a dialogue with the good advisor | todo | the scripts are in `Scripts/Quests/challenge.chl`; nothing in openblack asks for them |
| Help about the creature acting on a thing: the advisor clings to the bottom left, reads out what the creature is doing, waits, then gives the lesson in a line | todo | needs the dialogue, advisor clinging and the creature's action text (`START_DIALOGUE`, `TEXT_READ`, `SPIRIT_HOME`, `GET_ACTION_TEXT_FOR_OBJECT` are stubs in `src/CHLApi.cpp`) |
| Help about the creature acting on a place, worded the same way | todo | as above |
| Help as a single line of text | todo | as above |
| Help as two lines, one after the other | todo | as above |
| Help on reaching a new stage of growing up, after which that stage's script is switched off | todo | see [development_phases.md](development_phases.md) |
| Help that just focuses on the creature | todo | as above |
| Help where the advisor flies out and looks at the creature as it speaks | todo | as above |
| Messages wait on a stack and are shown one at a time; some kinds stack, others replace the one waiting | todo | not modelled |
| A message isn't shown again until enough time has passed since it was last shown | todo | not modelled |
| Only the player's own creature gives help | todo | not modelled |
| The creature help option in the game's options turns it all on or off | partial | the option is in `src/Gui/GameMenu.cpp` (`creatureHelp`), but nothing reads it |
| Scripts can turn creature help on or off | todo | `SET_CREATURE_HELP` is a stub in `src/CHLApi.cpp` |

## Lessons from feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It wants a desire more | partial | the words are made (`creature_learning::DesireLessonText`) and kept as a thought, shown only in the debug spawner |
| It wants a desire less | partial | as above |
| A source drives a desire more readily (for example, it will eat when only a little hungry) | todo | not worded |
| A source drives a desire less readily | todo | not worded |
| It thinks better of an action | partial | `creature_learning::ActionLessonText`, debug only |
| It thinks worse of an action | partial | as above |
| It will choose, or avoid, a kind of thing for a desire | partial | `creature_learning::ObjectLessonText`, debug only |
| A stroke or slap taught it nothing | todo | not shown; see [learning_from_feedback.md](learning_from_feedback.md) |

## Lessons from watching

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has learnt an ordinary skill | partial | kept as a thought ("I've learnt to ..."), debug only |
| It has learnt a miracle | partial | kept as a thought ("I've learnt the miracle ..."), debug only |
| It has nearly learnt a skill | todo | not shown |
| It has nearly learnt a miracle | todo | worked out (`LearningEvent::NearlyLearnt`) but not shown |
| It can't learn a skill yet | todo | not shown |
| It can't learn a miracle yet | todo | worked out (`LearningEvent::TooYoung`) but not shown |
| It hasn't learnt a skill it is being shown | todo | not shown |
| It can't learn this at its stage of growing up | todo | not shown |
| It is too young to copy the player | todo | not shown |

## Stages of growing up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each stage has its own message as the creature reaches it: the start, learning to eat from the hand, punishment, leash pulling and picking up, leashing to a house, meeting the guide, friends with the guide, the guide's history, the guide's spells, impressing a town, learning to fight, helping a town, the good and evil leashes, and grown up | todo | fourteen messages; see [development_phases.md](development_phases.md) |
| What it still has to do to reach the next stage is explained | todo | not shown |

## Spells, needs and troubles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell has been cast on it | todo | not shown; spells themselves in [../miracles/](../miracles/) |
| A spell on it has worn off | todo | not shown |
| What source drives its current desire | todo | not shown |
| Its current desire | todo | not shown |
| A bodily problem (hungry, tired, needs a poo, thirsty, ill) | todo | not shown |
| A problem of the mind | todo | not shown |
| How a town feels about it | todo | not shown; town attitudes in [town_actions.md](town_actions.md) |
| Something interesting is happening off screen | todo | not shown |

## Things it messed up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A throw that missed | todo | not shown |
| A miracle that fizzled | partial | it plays its embarrassed and sad actions (`CreatureMindSystem::ShowFizzle`), but no message; see [creature_casting.md](creature_casting.md) |
| Fishing that went wrong | todo | not shown |
| Dancing with villagers that went wrong | todo | not shown |
| Making a fire that went wrong | todo | not shown |
| Impressing villagers that went wrong | todo | not shown |
| Raising a totem that went wrong | todo | not shown |

## Why it failed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It won't fight because it is hurt | todo | not shown; see [fighting.md](fighting.md) |
| It can't find a way there | todo | not shown; routes in [locomotion.md](locomotion.md) |
| Its way is blocked | todo | not shown |
| Where it wants to go can't be reached | todo | not shown |
| The other creature refused to fight | todo | not shown |
| The other creature refused to play | todo | not shown |
| The other creature refused to be friends | todo | not shown |
| The leash stopped it | todo | not shown; see [leash.md](leash.md) |
| It lost sight of what it was following | todo | not shown |

## Other remarks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nice music is playing | todo | not shown |
| Nasty music is playing | todo | not shown |
| Frightening music is playing | todo | not shown |
| It has fainted | todo | not shown; fainting itself in [physiology.md](physiology.md) |
| The player stroked it but it isn't learning | todo | not shown |
| The player slapped it but it isn't learning | todo | not shown |
| It has been carried home | todo | not shown |
| It can't pick that up | todo | not shown |

## The first lessons (tutorial prompts)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| How to select the creature | todo | not shown |
| How to make it run | todo | not shown |
| Holding the hand over it | todo | not shown |
| How to let it go | todo | not shown |
| Its home needs more rocks | todo | not shown; see [home_and_pen.md](home_and_pen.md) |
| How to make it pick something up | todo | not shown |
| How to make it help build | todo | not shown |
| How to make it drop something | todo | not shown |
| How to make it eat | todo | not shown |
| How to reward and punish it | todo | not shown |
| How to make it drink | todo | not shown |
| How to make it poo | todo | not shown |

## Signs the creature gives

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its thoughts (the last few things it learnt or decided) are kept | partial | `creature_mind_model::Think`, newest eight; shown only in `src/Debug/CreatureSpawnerMind.cpp` |
| What the creature is doing can be read as a line of text | todo | the action text scripts ask for is a stub (`GET_ACTION_TEXT_FOR_OBJECT`) |
| A light bulb appears over its head when it tries a miracle it hasn't learnt | todo | no light bulb effect |
| It makes a sound that shows its mood | todo | not modelled; its voice in general is in [animation.md](animation.md) |
| Music changes its mood, and its mood changes the music | todo | not modelled; see [../audio/](../audio/) |
| It talks: speech items it says to villagers, friends and the player | todo | not modelled |
| A screen to choose which lesson the last feedback teaches (the desire, the thing acted on, the thing used, the action) and step through recent actions | todo | not modelled; may be a developer tool rather than part of the released game (unconfirmed) |
| A web page of the creature's statistics can be written out | todo | `Scripts/creature_html.tmpl` is the template; not modelled |
| Creatures could be uploaded to the game's online creature service | n/a | the service no longer exists |
