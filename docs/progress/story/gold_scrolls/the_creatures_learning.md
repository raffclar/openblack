# The Creature's Learning

The first half of the first land's creature story: once the creature is chosen, Sable the creature trainer comes out of
the temple four times to teach the player to look after it — its new home and stroking, feeding it, slapping it, the
Leash of Learning (pulling it and making it pick up a cow) and tying the leash to a tree, where she also shows the
Leashes of Aggression and Compassion before saying farewell. The same gold scroll then carries on with the big creature
that wanders the valley; that half is in [creature_guide.md](../creature_guide.md).

**Land:** 1 · **Giver:** Sable the creature trainer, at the creature pen by the temple · **Script:** CreatureDevelopment (CreatureDevSeeHome, CreatureDevLearnToEat, CreatureDevPunishment, CreatureDevLeashIntro, CreatureDevLeashAttachToHouse) · **Reward:** none (the lessons hand over the three leashes) · **Repeatable:** no

Sources: the lessons' script source (checked against the shipped compiled program), the land's control script and the
game's text table. openblack is judged on the physics work tree (`ob-wt-physics`): the land's control script never gets
this far (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and nearly every command the lessons use
— making the trainer, the scroll, the dialogue, the camera moves, the creature's actions, stages and desires — only
logs "not implemented" in `src/CHLApi.cpp`. A few work: the leash questions and the leash-granting developer functions,
drawing the leash, setting the game time, music, fades and the moveable/pick-up/indestructible flags. Rows are todo
unless the notes say otherwise.

**Progress: 0/136 done, 17 partial — 6%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lessons are a gold scroll (a story highlight) titled "The Creature's Learning", the same title the guide's chapters use after them | todo | `CreateHighlight`, `Snapshot`, `UpdateSnapshot` are stubs in `src/CHLApi.cpp` |
| They start once the creature is chosen ([Choose Your Creature](choose_your_creature.md)) and the breeder, explorers and the guide's wandering have been started | todo | the land's control script stops long before |
| The land's control script runs them one after another, each waiting for the last: the new home, eating, punishment, then the leash introduction, then tying the leash | todo | |
| Between lessons, other quests start: after punishment the immersion mushrooms and the singing stones; after the leash introduction the hermit; after tying the leash the lost flock is wanted and the saviour waits for the creature to grow | todo | [the_immersion_mushrooms.md](../silver_scrolls/the_immersion_mushrooms.md), [the_singing_stones.md](../silver_scrolls/the_singing_stones.md), [the_hermit.md](../silver_scrolls/the_hermit.md), [the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [the_saviour.md](../silver_scrolls/the_saviour.md) |
| What comes next: the guide's meeting starts straight after the last lesson | todo | [creature_guide.md](../creature_guide.md) |
| The scroll's progress mark rises by lesson: 0 for the new home, 0.08 eating, 0.15 punishment, 0.23 the leash, 0.31 tying the leash; the guide's chapters carry on from 0.5 | todo | `Snapshot`/`UpdateSnapshot` stubs |
| A quiet pause follows each lesson before the next trainer visit: 31 seconds after the new home, 61 after eating, 61 after punishment, 31 after the leash introduction | todo | the timed waits are never reached |
| Each lesson after the first switches the creature's own development script on while it runs and off when it ends | todo | `CreatureInDevScript` is a stub |
| A file loaded with the lessons marks the temple's entrance, where the trainer comes and goes | todo | |

## Skipping the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game that skips the creature guide still runs the new-home step, but only as a wake-up camera at the pen with a two-second fade in; no trainer, no scroll | partial | the fade works (`SetFadeIn`, `src/CHLApi.cpp`); the camera and the skip question (`CanSkipCreatureTraining`) are not |
| The creature is put in its pen and that becomes its home | todo | `SetCreatureHome` is a stub |
| The other lessons are skipped: the creature is made fully mature at once, given the Leash of Learning and the other two leashes, its development script switched off and released | partial | granting the leashes works (`DevFunction` 2 and 3, `src/CHLApi.cpp`); setting the stage (`SetCreatureDevStage`) is a stub |
| The quests that start between lessons still start as usual | todo | |

## How the trainer calls the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The trainer (a special villager) walks slowly (speed 0.3) out of the temple to her spot by the pen; while teaching she can't be moved, picked up or hurt | partial | the three flags work (`SetIdMoveable`, `SetIdPickupable`, `SetIndestructable`); creating the trainer doesn't (`CreateScriptObject` only makes mobile statics and rocks) |
| A gold scroll stands over the pen, raised six above the ground | todo | `CreateHighlight` stub |
| Until it is clicked she faces the camera and beckons, then points and talks, in turn | todo | |
| Every 40 seconds, or every 12 when the camera is more than 30 from the scroll, the player's creature points the scroll out | todo | the developer function for this isn't handled by `DevFunction` |
| When the camera is near the scroll (within 200 for the eating lesson, 100 for the others) and it is on screen, the evil advisor steps out every 30 seconds, points at it and says "Hey. Sable the Creature Trainer is trying to attract our attention." | todo | `SpiritEject` and the dialogue commands are stubs |
| Clicking the scroll or the trainer starts the lesson | todo | |
| The first lesson needs no click: it starts as a cut scene | todo | |

## Reminders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the active scroll again repeats its reminder; an advisor line is said by the advisor who owns it, stepping out to say it, and a trainer line by the trainer's voice | todo | the shared reminder script; advisors are stubs |
| While the player must do something with the creature, a nag repeats its line every 30 seconds — only while the camera is within 150 of the creature, the creature is on screen and the player isn't holding it; the good advisor steps out, points at it, says it and goes home | todo | |

## Lesson 1: the new home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is set to just before sunrise and game time runs | partial | `SetGameTime` and `GameTimeOnOff` work; the scripts never get here |
| A did-you-know sign by the pen: "This is your Creature's home. He feels safe here and when he rests he'll grow in size faster…" | todo | did-you-know signs are stubs; see [../../creature/home_and_pen.md](../../creature/home_and_pen.md) |
| The creature is put at its first stage of growing up | todo | `SetCreatureDevStage` stub; see [../../creature/development_phases.md](../../creature/development_phases.md) |
| The creature is put in the pen, which becomes its home, and it faces the camera | todo | |
| A cut scene opens with the guide's music; the creature is made exhausted and put to sleep | partial | the music command works (`StartMusic`); the rest is stubbed |
| After six seconds the picture fades in over five seconds on a sunrise view of the pen, which pans up over seven seconds | partial | the fade works; the camera moves are stubs |
| The camera cuts to the pen as the creature wakes and moves in on the trainer | todo | |
| The scroll is logged at 0 with the reminder "Hold down the Action Button over the Creature to zoom in on him." (trainer) | todo | |
| The creature points at the camera; the trainer: "This is your Creature's new home." The creature yawns tiredly and loses its tiredness | todo | |
| The camera turns to the creature, which looks frightened; trainer: "He's small at the moment but look after him and one day he'll be big and powerful." | todo | |
| It cries; the camera goes back to the trainer: "He isn't comfortable in his new home yet. In fact he's a little frightened." — the creature looks frightened again | todo | |
| "You can stroke him to reassure him." The creature looks sad and the camera rises; the music stops | todo | |
| The reminder becomes "Click and hold the Action Button, then move your hand to stroke him." | todo | |
| The creature is left wanting only to be sad; the trainer says the same line again | todo | `SetCreatureOnlyDesire` stub |
| The good advisor nags "Let's click on the Creature so we can stroke him." | todo | |
| The player must take hold of the creature (zoom in on it) and keep hold for two seconds; letting go sooner gets "Holy One, you're clicking and letting go too quickly." the first time and "I humbly suggest you click and hold the mouse button for longer." after that | todo | there is no time limit: the lesson waits for this |
| Then: "Now stroke him by holding the Action Button and moving your hand over him gently." and "Stroking tells him you approve of him and it makes him happy." | todo | |
| The player has two minutes to stroke or slap; the good advisor nags "Click on the Creature and stroke him some more." | todo | `GetInteractionMagnitude` is a stub that always answers no feedback |
| A slap: the creature looks sad, the trainer is unimpressed: "Hey, remember he's only little."; a stroke: it looks happy, she is impressed: "Excellent. See how he likes that? He feels much better now."; neither: no comment, the lesson goes on | todo | what the creature learns from these is the engine's own: [../../creature/learning_from_feedback.md](../../creature/learning_from_feedback.md) |
| The creature is let go; the trainer walks back to the temple: "I'll leave you two alone for a while. I'll return later to teach you more." | todo | |
| The scroll goes; she fades away at the temple entrance | todo | |

## Lesson 2: learning to eat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature moves to the stage of taking things from the player and eating them, and its energy is set to nothing (starving) | todo | `SetCreatureDevStage` stub; energy property is a stub |
| The trainer comes out under a new scroll and calls the player as above | todo | |
| The scroll is logged at 0.08: "You need to give your Creature something to eat to move to the next stage." (good advisor) | todo | |
| A cut scene: the creature walks to a spot in front of the pen; the camera moves in; it looks at the camera and acts hungry | todo | |
| Trainer: "Your Creature is getting hungry." then "I suggest you find him something to eat."; the camera turns to the creature, which acts hungry again | todo | |
| "Choose carefully though - some things are better for him than others."; the camera goes back to where the player had it | todo | |
| A did-you-know sign: "If you want your Creature to eat something, rub his tummy straight after you give it to him. He'll eat it even if he's not hungry." | todo | |
| The reminder becomes "I suggest you go and get the Creature some food." (good advisor) | todo | |
| While the player looks for food, the advisors comment on what the hand is over (within 5.5): a pig — evil: "Hey. What about some meat? Pork chops? He'd love them!" (again after 3 minutes at most) | todo | the food comments are also listed in [../../creature/feeding_and_thrown_things.md](../../creature/feeding_and_thrown_things.md) |
| A food store — good: "Now that sounds tasty. Corn from the fields." (every 190 seconds at most) | todo | |
| A cow — evil: "What about some beef steak? Meat-eaters are more aggressive, after all." (every 190 seconds at most) | todo | |
| A villager, only shortly after a pig or cow comment — evil: "Ooh. That would be deeply evil. Yeah." for a child under 11, else "Eating people? Yep. That's evil, all right." | todo | |
| The village's storage pit (hand within 15.5) — good: "Why not take some grain from the Village Store to feed the Creature?" | todo | |
| Scripted objects are never commented on | todo | |
| As soon as the hand holds anything within 50 of the creature the comments stop; reminder "Give him the food by keeping your hand in front of him and holding the Action Button." and the good advisor says nearly the same line | todo | `GetObjectHeld` is a stub |
| The creature is starved again and kept wanting only food, its energy held where it is, until it eats once — something alive, something after looking it over, something from the hand or from a magic food pile | todo | `GetActionCount` stub; how it eats: [../../creature/feeding_and_thrown_things.md](../../creature/feeding_and_thrown_things.md) |
| There is no time limit; whatever it eats is accepted, good or evil | todo | |
| A cut scene: the good advisor points at it: "He seemed to like that."; the camera turns to the trainer: "Try feeding him some more. I'll be back later." | todo | |
| She walks back into the temple and fades away; the scroll goes; the creature's energy is filled up | todo | |

## Lesson 3: punishment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature moves to the punishment stage; from here on slaps and strokes teach it lessons | todo | `SetCreatureDevStage` stub; [../../creature/development_phases.md](../../creature/development_phases.md) |
| The trainer comes out under a new scroll; the scroll is logged at 0.15: "You must punish your Creature to move on. Click on him with the Action Button." (good advisor) | todo | |
| The creature walks to its spot; the cut scene moves the camera beside the pen and the creature looks at the camera | todo | |
| For this scene the trainer can be moved and picked up | partial | the flags work; the trainer isn't made |
| She beckons, turns to the creature and points: "Your Creature already has his own personality." as the camera turns to it | todo | |
| A tiny rock (1% of its size, so all but invisible) is made by the pen | partial | making a scaled rock works (`CreateWithAngleAndScale`) |
| A two-shot of creature and trainer: it acts happy twice; "But you can alter and shape it by… hey what's going on?" as the creature is made to pick her up and hurl her at the rock | todo | the forced creature action is a stub |
| A second after she is flying: "Aiee!" | todo | |
| The creature is put back in place: "Punish your Creature to stop him doing things like that."; it looks at the camera, asks for attention and acts happy while she walks back to her spot | todo | |
| The camera goes back to the player; she is made fixed and unhurtable again, faces the creature and shakes her fist | todo | |
| The creature's record of the player's feedback is cleared | todo | the developer function for this isn't handled by `DevFunction` |
| Every two seconds: when the player has hold of the creature, "Now slap him by moving your hand quickly from side to side across him." (once per hold); when not, "Hold down the Action Button over the Creature to zoom in on him." at most once a minute | todo | |
| The lesson waits, with no time limit, for any slap or stroke | todo | a one-minute timer is made but never used |
| A slap: she shakes her fist: "Thanks. He won't try that again in a hurry." then "I'll come back soon to explain more about your wondrous, slightly headstrong beast." | todo | the creature learning not to throw villagers is the engine's: [../../creature/learning_from_feedback.md](../../creature/learning_from_feedback.md) |
| A stroke: she is impressed: "Oh, you want to encourage him to do things like that, do you?" then shakes her fist: "Well, it's up to you. I will return when your Creature is behaving a bit better." — she comes back for the next lesson all the same | todo | |
| She walks back into the temple and fades away; the scroll goes | todo | |

## Lesson 4: the Leash of Learning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A palm tree is planted east of the pen; it can't be moved, picked up, hurt or burned | partial | the flags work; making a tree from a script doesn't |
| The trainer comes out under a new scroll; the scroll is logged at 0.23: "Click the Action Button on the Leash." (good advisor) | todo | |
| The creature walks to its spot and is put at the leash stage, so it can't be leashed before the lesson | todo | `SetCreatureDevStage` stub |
| A cut scene looks down on the leash post at the pen: "It's time to take him for a walk."; she points at the leash as it is switched on | partial | switching the Leash of Learning on works (`DevFunction` 2, see [../../creature/leash.md](../../creature/leash.md)) |
| "This is the Leash of Learning." (the good advisor points at it) and "Please click on it and I'll show you how to use it with your Creature." | todo | |
| The lesson waits until the player holds the Leash of Learning | partial | the leash-type question works (`GetObjectLeashType`); it never gets here |
| "Good! Now click on your Creature."; reminder "Now click the Action Button on the Creature to attach the Leash to him." (trainer); she points at the creature | todo | |
| The lesson waits until the creature is on the leash | partial | `IsLeashed` works |
| A did-you-know sign on the leash: "Shake your hand when you've got the Leash and you'll release it. You can Leash your Creature to any object by double-clicking…" | todo | |
| A cut scene with the leash drawn: she is impressed: "Now you've got some direct control of the little beast!" | partial | drawing the leash in cut scenes works (`SetDrawLeash`) |
| She beckons; the camera turns to her: "You can pull the Creature about with the Leash." | todo | |
| The camera turns to the tree: "You see that tree over there?"; a target sparkle marks it for 30 seconds and the good advisor steps out and points at it | todo | special effects are stubs |
| "Try pulling your Creature towards it. Click the Action Button on the ground to hurry him up."; reminder "Now walk your Creature over to the tree over there." (good advisor) | todo | |
| She walks (speed 0.5) to the tree and beckons the creature from there | todo | |
| The good advisor nags "Click the Action Button on the ground to get him to move faster." | todo | |
| As the creature nears the tree the good advisor calls out once each: "Yes." within 40, "Yes." within 30, "You've got it. Very well done!" within 20; "No." once if it gets 10 further away than it started | todo | |
| The step ends when the creature is on the leash within 20 of the tree and she has reached it | todo | how leading works in openblack: [../../creature/leash.md](../../creature/leash.md) |
| A cut scene at the tree: "Excellent! Well done."; then "Did you know that you can use a Leash to make your Creature interact with objects?" | todo | |
| A cow appears south of the tree; it can't be hurt, moved or picked up; "Try getting him to pick up that cow over there." with a sparkle on it and the camera on it | todo | making an animal is a stub |
| "Move your hand with the Leash over the cow." and "Then click the Action Button."; reminder "Click the Action Button on the cow to get the Creature to interact with it." (good advisor), which also becomes the nag | todo | |
| The creature is kept wanting only to look at things | todo | |
| Once the hand comes within 10 of the cow it can be picked up; the step ends when the creature holds it | todo | |
| If the player picks the cow up with the hand it vanishes and a new one appears in its place | todo | |
| A cut scene at the tree: "Well done. You see! The Leash is very useful.", "Why not take him for a walk? It'll do him good." and "You can drop the Leash by shaking your hand left and right quickly." | todo | |
| The trainer and the scroll vanish there and then, and the cow fades away if it is still there | todo | |

## Lesson 5: tying the leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is put at the stage of tying the leash; the trainer appears by the tree, fixed and unhurtable | todo | |
| Two gold scrolls: one over the tree (raised 12) and one at the pen (raised 6); clicking either, or the trainer, starts the lesson | todo | |
| The scroll is logged at 0.31: "Attach your Creature to that sparkly tree." (good advisor) | todo | |
| A cut scene at the tree with the leash drawn: "There's more to learn about Leashes." | todo | |
| A sparkle on the tree for 30 seconds: "I didn't tell you that you can attach the Leash to objects." then, looking at her hand, "Try Leashing the Creature to this tree!" | todo | |
| The camera goes back: "With the Leash attached to your hand, move it over the tree." | todo | |
| The good advisor nags "With the Leash attached to your Creature, double-click on the tree."; with the hand within 10 of the tree, the trainer: "Double-click the Action Button to attach the Leash." at most every 30 seconds | todo | |
| The step ends when the creature is tied to that tree; the creature moves on to the good-and-evil-leash stage | partial | the tied-to question works (`IsLeashedToObject`) and tying works ([../../creature/leash.md](../../creature/leash.md)); the stage is a stub |
| A cut scene at the tree: "Good. The Leash is very useful. Your Creature is tied to the tree now. He'll interact with whatever he's Leashed to, as well." then "If you click on a Leash when the Creature's attached to something you can adjust its length." | todo | |
| The other two leashes are switched on: "There are two other Leashes for you here." | partial | `DevFunction` 3 grants them |
| The camera looks at each by the pen: "This is the Leash of Aggression." "It encourages your Creature to get angry." then "This is the fluffy Leash of Compassion " "It makes your Creature behave nicely." | todo | what they do: [../../creature/leash.md](../../creature/leash.md) |
| A did-you-know sign on the leash's length by the tree | todo | |
| Back at the tree: "Why not experiment with these Leashes? They're powerful tools." and "I'll bid you farewell. You now know as much about Creatures as I do." | todo | |
| The guide's music starts as the camera closes in over 15 seconds: "One thing remains. Legends tell of an ancient Creature somewhere on this land." and, looking around, "I've heard him but I've never seen him. He hides from us humans. But he has great knowledge, I think." | partial | the music command works |
| The camera goes back and the music stops; the good advisor appears, points at the leash sign: "There are advanced techniques for using the Leash. Find them on this signpost." | todo | |
| A did-you-know sign on the pen and the leashes | todo | |

## The trainer's exit and aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the last lesson the trainer doesn't go back to the temple: she joins the player's Norse village, walks to it (giving up after 30 seconds) and becomes an ordinary villager who can be moved, picked up and hurt | todo | |
| The village store's food is cut down to 1000 | todo | `GetResource`, `RemoveResource` are stubs |
| The creature keeps its home in the pen, the three leashes and the stage reached | todo | |
| The scroll stays on at 0.31 until the guide's meeting moves it on | todo | |
| The two scrolls of the last lesson are never removed by the script (undetermined whether they stay visible: the scroll commands are not researched past being clicked) | todo | |

## Failing and soft locks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No lesson can be failed: each waits, with no time limit, for the one thing it asks (holding the creature, any slap or stroke, eating once, holding the rope, leashing, reaching the tree, the cow, the tie) — only the stroking step after the first hold has a two-minute limit | todo | |
| Killing or losing the creature isn't handled; every step that waits on the creature would then wait for ever (undetermined whether the game can lose the creature here at all) | todo | |
| The trainer can't be hurt while she teaches, so she can't stall the lessons | todo | |
| The story can't go on until the last lesson ends, as the guide's meeting waits for it | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the cow step the near-the-cow "Yes." calls reuse the flags already set while walking to the tree, so they almost never play; only "No." can | todo | the same counters serve both steps |
| In the cow step a check turns the trainer to the camera if she is at a spot in the north village, where she never is | todo | a left-over position |
| The thing the leash is tied to is called a house in the script but is the palm tree planted in the leash lesson | todo | |
| The punishment step's one-minute timer is never used | todo | |

## Music, sound and voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide's theme plays over the first scene and the trainer's farewell | partial | `StartMusic`/`StopMusic` work (`src/Audio/GameMusic.cpp`); the scripts never reach them |
| The trainer speaks in her own narrator voice; the advisors in theirs | todo | dialogue commands are stubs; see [../advisors.md](../advisors.md) |
| The creature's moods are played as animations: tired, frightened, crying, sad, happy, hungry, look-at-me | todo | forced creature animations are stubs; see [../../creature/animation.md](../../creature/animation.md) |

## Saves and other modes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the lesson reached, the trainer and the scroll | todo | openblack has no saved games |
| The lessons are only in the first land of the story, not in skirmish, multiplayer or the Creature Isle | n/a | nothing to do beyond the story itself |

## Unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A compiled but never started good-and-evil leash lesson: back at the pen the trainer shows the fluffy Leash of Compassion and the spiky Leash of Aggression, a dying farmer appears, the player leads the creature to him on either leash; "That's it! I can break the curse now!", "I have nothing more to teach you.", "He's all yours. Now it's up to you." and she walks off to join the village (scroll at 0.92) | n/a | nothing runs it (only marked "old" in the source); the shipped tying lesson shows the two leashes instead |
| A compiled script that runs the five lessons in a row, started only by a commented-out line of the land's control script and an unshipped test launcher | n/a | |
| Older, shorter drafts of the five lessons as separate files, with placeholder lines typed into the script and a man as trainer | n/a | not in the game's program |
| An unshipped lesson where the creature needs to relieve itself and is led to a spot ("Er, Mighty One? Your Creature needs to relieve himself.", advisors' bowel jokes), and a leash version of it | n/a | not in the game's program |
| An unshipped thirst lesson: nothing to drink at home, so the creature is leashed and dragged to water ("I think your Creature is thirsty, powerful one."), and a leash version of it | n/a | not in the game's program |
| An unshipped anger lesson: villagers crowd the creature, it gets angry, and stroking or slapping changes how easily it angers ("Remember, punishment and reward is the path to having the Creature you want.") | n/a | not in the game's program |
| Unshipped leash drafts: tying the leash to an object (with the three leashes explained), tying it to a walking villager so the creature follows and learns from them, pulling to a tree and picking up a cow | n/a | not in the game's program; the shipped leash lesson merges the pull and pick-up drafts |
| A misnamed "learn to play" file that holds a draft of the guide's help-a-town lesson | n/a | the guide's unshipped lessons are listed in [creature_guide.md](../creature_guide.md) |
