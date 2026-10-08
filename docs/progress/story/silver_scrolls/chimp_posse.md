# Chimp Posse

A cut challenge for the last land: three small, angry chimp creatures on a hilltop have lost their ball in "the
neighbour's garden", and Nemesis's creature takes it. Bringing the ball back (with their die and teddy still at home)
makes the chimps friends of the player's creature, until Nemesis's creature comes to take the ball again. It makes a
challenge record but no silver scroll, has no reward, and was never compiled into the game.

**Land:** 5 (never started) · **Giver:** none (a cut scene when the camera nears the chimps) · **Script:** ChimpPosse · **Reward:** none (the chimps become the creature's friends) · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and its single-quest test launcher, the script compiler's project and quest menu
lists, Land 5's map script and the game's text table. Every row is n/a: the game never runs this quest. To restore it
openblack would need making creatures by script, creature desires, friendships and forced actions, held-object checks
and the challenge record, which are all stubs in `src/CHLApi.cpp` (its `Create` makes the toys, being mobile statics,
but not the chimps).

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it | n/a | never started by the game |
| Only its single-quest test launcher starts it, after Land 5's setup script; the developers' quest menu compiles that launcher with Land 5's setup, volcano and returning-explorers scripts | n/a | never started by the game: the launcher is not compiled into the shipped game either |
| It opens a challenge record but never makes a silver scroll, so even if run there would be no scroll to click; the quest starts on its own when the camera comes near | n/a | never started by the game |
| Its title "Chimp Posse" and both its reminders exist in the text table, but all fourteen of its spoken lines are missing; their wording survives only in the source's comments (for ten of them) | n/a | never started by the game |
| Its spots fit Land 5: the spot "just outside Nemesis's citadel" is about 50 from Nemesis's citadel on Land 5, and the "big fight arena" is about 95 from Nemesis's Aztec village there | n/a | never started by the game: checked against every `Land<N>.txt` |
| Not reachable at all in the shipped game | n/a | never started by the game |

## The chimps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three chimp creatures are made at three spots on the hill's edge, about 45 from their home on the hilltop, at half size | n/a | never started by the game |
| They are fully grown, taught the fireball and lightning miracles, and friends with each other | n/a | never started by the game |
| Their only desire is anger, with the desires to play and to make friends set to full | n/a | never started by the game |
| They are kept within 75 of their home | n/a | never started by the game |
| A die and a cuddly toy, at half size, are put 7 either side of their home | n/a | never started by the game |
| Until the ball is moved, the chimps stand facing the spot where it lies: one points at it and looks confused in turn, one is sad then embarrassed, one angry then confused | n/a | never started by the game |
| Once the ball has moved more than 10, they are let go; from then until the quest ends their energy is kept full every second, a comment says so they can cast miracles | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Starts when the camera is within 100 of the chimps' home and looking at it | n/a | never started by the game |
| Cut scene: the camera glides to 20 above and 20 off the middle chimp over 3 seconds | n/a | never started by the game |
| A half-size ball appears about 215 away, down near sea level | n/a | never started by the game |
| The good advisor points at the chimp; the record opens as "Chimp Posse", success and alignment 0, reminder "It looks like these chimps have lost a toy." | n/a | never started by the game |
| Good advisor (comment's wording): "Oooh look at that. A group of chimps. Aaah sweet." | n/a | never started by the game: not in the text table |
| After 2 seconds the camera moves to the ball over 4 seconds; the evil advisor points at it: "Ahh diddums. They seem to have lost their ball in the neighbours garden." | n/a | never started by the game: not in the text table |
| Nemesis's creature is put about 80 from the ball and made to pick it up; the camera follows him, rising to 60 above over 12 seconds | n/a | never started by the game |
| With the ball in hand he walks off towards the big fight arena, about 320 east | n/a | never started by the game |
| "Uh Oh. Getting the ball back for the chimps has just got harder." After 4 seconds and the line, the camera returns over 4 seconds | n/a | never started by the game: not in the text table |

## Getting the toys home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When Nemesis's creature is within 20 of the arena, he puts the ball down and is let go (once) | n/a | never started by the game |
| Any toy that no longer exists is made again at the chimps' home | n/a | never started by the game |
| The first time the player's creature comes within 50 of the home while the chimps aren't friendly and it isn't carrying the ball, the evil advisor points at it: "Woah. The Chimps aren't very friendly." | n/a | never started by the game: not in the text table |
| A toy counts as home when it is within 50 of the home, not in the hand and not in Nemesis's creature's hand | n/a | never started by the game |
| The ball home for the first time moves the record to 0.5, reminder "These Chimps seem to be friendly towards our Creature." | n/a | never started by the game |
| A toy coming home with the other two there brings two good advisor lines whose wording is lost; with one still missing: "The chimps will only be happy with you if ALL of the toys are returned." | n/a | never started by the game: not in the text table |
| The die or the cuddly toy coming home when the others are there moves the record to 1.0 | n/a | never started by the game: the ball never does, even if it is the last one home |
| A toy that ends up more than 100 from home counts as missing again; if the player's creature or the hand has it: "The chimps are not happy with you." "They want the toy back." | n/a | never started by the game: not in the text table |
| If Nemesis's creature has it: the good advisor points at him: "Oh dear. Nemesis' Creature has taken back his toy from the chimps." "They're not happy about it either" | n/a | never started by the game: not in the text table |

## Friends

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With all three toys home, the three chimps and the player's creature become friends both ways | n/a | never started by the game |
| As soon as any toy is missing again they stop being friends | n/a | never started by the game |
| There is no reward, no alignment change, no timer and no failure | n/a | never started by the game |

## Nemesis's revenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two minutes after the chimps first become friends, Nemesis's creature comes back for the ball: every 10 seconds he is made to grab it until it is in his hand | n/a | never started by the game |
| Each of those 10-second tries while he is within 200 of the chimps' home, the good advisor points at him: "I don't think the toy belongs to the chimps." "Nemesis' Creature looks mighty pissed." | n/a | never started by the game: not in the text table |
| With the ball he walks to a spot just outside his citadel, being sent again every 20 seconds until within 5, and puts it down there | n/a | never started by the game |
| The chimps then lose their friendship (the ball is missing) until the player brings it back again | n/a | never started by the game |

## The end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is meant to end once the revenge is over and the chimps are friends again: the chimps are marked active and the record closes at 1.0 with the opening reminder | n/a | never started by the game |
| What "active" chimps were to do is never written; nothing else reads it | n/a | never started by the game |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The revenge never ends: it loops until a flag that nothing sets, so it never reports back, the chimps never become active and the challenge can never close | n/a | never started by the game |
| Once Nemesis's creature has put the ball down at his citadel, that loop goes round with no pause | n/a | never started by the game |
| The main loop also has no pause | n/a | never started by the game |
