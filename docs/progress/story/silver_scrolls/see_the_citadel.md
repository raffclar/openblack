# See The Citadel

A cut early draft of the first land's opening at the temple (called the citadel here): the advisors teach the player
to rotate the camera, then to carry wood and food to the villagers building the temple, and once it is built they show
the three kinds of scroll and explain the temple, its info orb and its entrance. It is a tutorial, not a silver scroll:
it never opens a challenge record and only shows a silver scroll for two seconds as an example. It was never compiled
into the game; the shipped opening does this part of Land 1 its own way.

**Land:** 1 (never started) · **Giver:** the advisors, then a villager with an information scroll · **Script:** SeeTheCitadel · **Reward:** none · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, the script compiler's project and quest menu lists, Land 1's map script, the shipped
Land 1 opening script, the game's text table and the camera zone files. The quest's title is "See The Citadel" from its
script name, since its title text is missing from the text table. Every row is n/a: the game never runs this script.
The shipped equivalents are tracked in [../tutorial.md](../tutorial.md) and
[../../temple/temple_exterior.md](../../temple/temple_exterior.md).

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file: it is missing from the compiler's project list, and no script starts it | n/a | never started by the game |
| The developers' quest menu lists it to be compiled alone, but even then its own line to start itself is commented out | n/a | never started by the game |
| Its title text (9) and every text-table line it uses (the rotate lessons and two "follow us" extras) are missing from the shipped text table; the rest of its lines are typed straight into the script as draft English | n/a | never started by the game |
| The only record it would make, a gold-scroll (quest) record, is commented out, so it would not appear as a challenge at all | n/a | never started by the game |
| Its temple is exactly Land 1's planned temple site, and it uses Land 1's real camera zone files (the building zone, then zone 2) | n/a | never started by the game: checked against `Land1.txt` and `Data/Zones` |
| The village it looks up (about 90 from Land 1's player village) matches no town, but it never uses it | n/a | never started by the game |
| The shipped Land 1 opening also locks the camera to the building zone while the temple is finished and widens it to zone 2 afterwards, but with different lines and no rotate lesson or scroll demonstration | n/a | never started by the game: see [../tutorial.md](../tutorial.md) |
| Not reachable at all in the shipped game | n/a | never started by the game |

## Reaching the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple is set to 30% built | n/a | never started by the game |
| Until the camera is within 100 of the temple, the good advisor points at it and says a "follow us" line every 30 seconds | n/a | never started by the game: the line is missing from the text table |
| Then the camera is kept to the building zone | n/a | never started by the game |
| A wood store is made about 50 east of the temple and a food store about 55 south of it, each given 6,000 | n/a | never started by the game |

## The rotate lesson

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: the camera rises to a view of the temple over 3 seconds; the good advisor clings to the bottom of the screen and says "*** HAND DEMO INITIATED ***", then the first rotate line | n/a | never started by the game: a comment asks for the rotate hand animation here; the rotate lines are missing from the text table |
| The evil advisor clings beside him for the second rotate line | n/a | never started by the game |
| The player may then only rotate the camera | n/a | never started by the game |
| While the player has rotated fewer than 15 times, both rotate lines are repeated every 30 seconds | n/a | never started by the game |
| After the first rotation, a third rotate line is said once | n/a | never started by the game |
| After more than 30 rotations, the evil advisor says a fifth rotate line (a fourth line and a wait for 60 rotations are commented out, as are two more lines) | n/a | never started by the game |
| The lesson only ends after more than 90 rotations | n/a | never started by the game |

## Helping the builders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player may now rotate and use the hand | n/a | never started by the game |
| Cut scene: the camera moves to the wood store (3 seconds); evil advisor: "They'll build the temple a lot quicker if you supply them with the wood they need.", pointing at the store | n/a | never started by the game |
| Evil advisor, clinging: "*** HAND DEMO INITIATED ***" then "Pick up some wood from the supplies here and drop it near the builders." | n/a | never started by the game: a comment asks for a carry-wood hand animation |
| The camera moves to the food store; good advisor: "Dont you forget to feed them though." then, pointing, "They need food if they're going to be happy little workers." | n/a | never started by the game |
| Good advisor, clinging: "*** HAND DEMO INITIATED ***", "Pick up some food from the supplies here and drop it near the builders." and "It's up to you what help you give the villagers." as the camera returns to the temple view | n/a | never started by the game |
| Until the temple is fully built: each handful of food let go within 20 of the building site brings the good advisor's "Gotta keep up your strength chaps." | n/a | never started by the game |
| Each handful of wood let go there brings the evil advisor's "Work, bitches, work!" | n/a | never started by the game |
| The script counts the handfuls but never uses the counts; comments plan reminders that were never written | n/a | never started by the game |
| The script does nothing to build the temple itself; it waits for the villagers to finish it | n/a | never started by the game |

## The three scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera is widened to zone 2 and the player gets the full controls back | n/a | never started by the game |
| Cut scene: camera low by the temple (3 seconds); the good advisor says another "follow us" line | n/a | never started by the game: missing from the text table |
| A villager appears in front of the temple with an information scroll beside him; the camera moves to him over 4 to 5 seconds | n/a | never started by the game |
| Good advisor, pointing at it: "This is an information scroll. There's lots more to be learnt from these scrolls" | n/a | never started by the game |
| After 2 seconds it is replaced by a silver scroll: "This kind of scroll will give you rewards." | n/a | never started by the game: the only silver scroll in this script, shown for about 2 seconds |
| Then by a gold scroll: "And these scrolls are special scrolls. Keep an eye out for them." | n/a | never started by the game |
| An information scroll appears 10 above the villager: "Click on the one above this bloke's head to learn about the citadel." | n/a | never started by the game |
| It waits until that scroll or the villager is clicked | n/a | never started by the game |

## The temple explained

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Camera high over the temple (4 seconds): "The citadel is the centre of your power. The citadel blah blah." | n/a | never started by the game: placeholder text |
| Camera to the info orb at the building site; good advisor, pointing: "This is the info orb. Pick it up and click on things for info." | n/a | never started by the game |
| Camera to the entrance: "This is the entrance. Click on it to go inside or press F at any time." then "You can go in now or take a look around the village first." | n/a | never started by the game |
| The script ends there; the villager and the information scroll above him are left in the world | n/a | never started by the game |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A handful counts only if it is within 20 of the site at the moment it leaves the hand, so food or wood thrown there from afar never counts | n/a | never started by the game |
